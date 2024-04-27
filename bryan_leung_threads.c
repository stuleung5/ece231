#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // Header file for sleep(). 
#include <pthread.h> // Header file for thread library
#include <time.h>
#include <sys/epoll.h>

#define SIZE 100

int buffer[SIZE];
int timestamp[SIZE];

int count = 0; // tracks the number of characters inside the buffer
int putIndex = 0; // tracks the buffer index for next write
int getIndex = 0; // tracks buffer index for next read
// variable to identify/instantiate lock
pthread_mutex_t lock;


void configure_gpio_input(int gpio_number){
    // converting gpio number from integer to string
    char gpio_num[10];
    sprintf(gpio_num, "export%d", gpio_number);
    const char* GPIOExport="/sys/class/gpio/export";
    // exporting the GPIO to user space
    FILE* fp = fopen(GPIOExport, "w");
    fwrite(gpio_num, sizeof(char), sizeof(gpio_num), fp);
    fclose(fp);
    // setting gpio direction as input
    char GPIODirection[40];
    sprintf(GPIODirection, "/sys/class/gpio/gpio%d/direction", gpio_number);
    // setting GPIO as input
    fp = fopen(GPIODirection, "w");
    fwrite("in", sizeof(char), 2, fp);
    fclose(fp);
}

// sets up an interrupt on the given GPIO pin
void configure_interrupt(int gpio_number){
    configure_gpio_input(gpio_number); // set gpio as input
    // configuring interrupts on rising, falling or both edges
    char InterruptEdge[67];
    sprintf(InterruptEdge, "/sys/class/gpio/gpio%d/edge", gpio_number);
    // configures interrupt on falling edge
    FILE* fp = fopen(InterruptEdge, "w");
    fwrite("falling", sizeof(char), 7, fp);
        // configures interrupt on both edges
    // fwrite("both", sizeof(char), 4, fp);
    fclose(fp);
}

void* inputThread(void* input) {

    //In one thread, upon detecting an edge at P8_8 pin, take a timestamp
    //using clock_gettime() API, store the timestamp in a shared array, and
    //wait for the next edge [Timestamp using the clock,
    //CLOCK_MONOTONIC]

    int gpio_number = 67;
    configure_interrupt(gpio_number);
    char InterruptPath[22];
    sprintf(InterruptPath, "/sys/class/gpio/gpio%d/value", gpio_number);
    int epfd;
    struct epoll_event events, ev;
    FILE* fp = fopen(InterruptPath, "r");
    int fd = fileno(fp);
    epfd = epoll_create(1);
    //printf("epfd=", epfd);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

    int interrupt;
    time_t seconds;

    struct timespec start, end;
    unsigned int sec = 1;
    long time_nanosec;
    long time_microsec;
    // Open a file to write time values
    FILE *fptr;
    fptr = fopen("bryan_leung_pwmperiods.txt","w");
    
    int i = 0;

    while(i<100) // Print and Save 10 time values
    {   pthread_mutex_lock(&lock);
        while (count == 10) {
            pthread_mutex_unlock(&lock); //if buffer is full; release the lock so that other thread can empty the buffer
            pthread_mutex_lock(&lock);
        }

        interrupt = epoll_wait(epfd, &events, 1, -1);

        clock_gettime(CLOCK_MONOTONIC, &start); // record start time
        sleep(sec);
        clock_gettime(CLOCK_MONOTONIC, &end); // record end time
        time_nanosec = (end.tv_sec - start.tv_sec)*1000000000 + (end.tv_nsec - start.tv_nsec);

        time_microsec = time_nanosec/1000;
        fprintf(fptr,"%ld,\n", time_microsec);

        seconds = time(NULL);
        printf("Interrupt received: %d at %ld ms\n", interrupt, time_microsec);
         
        timestamp[putIndex] = time_microsec;

        count++;
        buffer[putIndex] = i;
        putIndex++;
        
        if (putIndex == SIZE) {
            putIndex = 0;
        }

        i++;
        pthread_mutex_unlock(&lock); // releasing the lock; marking the end of critical section
    
    }
    fclose(fptr);
    close(epfd);
    return 0;
 
 }

void* outputThread(void* input) {
    int c=0; // initializing using NULL character
    // print all characters till "z"
    while(c <= 74){
        pthread_mutex_lock(&lock); // acquires the lock;i.e. marks the start of critical section
        while (count == 0) {
            pthread_mutex_unlock(&lock); // if buffer isempty; release the lock so that other thread can fill thebuffer
            pthread_mutex_lock(&lock);
        }
        // read the character from buffer, update variablesand print the character to terminal
        count--;
        c = buffer[getIndex];
        getIndex++;

        printf("Emptying buffer: %c, %d\n", c, c);
        // return index to zero after reading the lastcharacter in the buffer
        if (getIndex == SIZE) {
        getIndex = 0;
        }
        pthread_mutex_unlock(&lock); // releasing the lock;marking the end of critical section
        // exit the thread after printing last character
        if(c == 74){
            break;
        }

    } 
    int sum = 0;
    int a = 0;
    for(a=0; a<SIZE; a++) {
        sum += timestamp[a];
        printf("value = %ld\n",timestamp[a]);
    }
    int avg = sum/SIZE;

}

int main(){

    int data = 6;
    pthread_t thread_id[2]; // instantiating argument required for thread creation
    printf("Before Threads \n");
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
    return 1;
    }
    pthread_create(&(thread_id[1]), NULL, inputThread, (void*)(&data) ); //P8_08  gpio22
    pthread_create(&(thread_id[0]), NULL, outputThread, (void*)(&data) ); //P8_19   gpio67
    
    pthread_join(thread_id[1], NULL);
    pthread_join(thread_id[0], NULL);

    pthread_mutex_destroy(&lock); //Uninitialize Lock
    printf("After Threads \n");
    pthread_exit(0);
}
