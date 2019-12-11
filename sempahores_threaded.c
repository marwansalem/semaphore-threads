#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

unsigned int buffer_max_size  = 16;
unsigned int buffer_current_size = 0;//increases when monitor reads the shared counter,,decreases when collector takes an item from buffer
unsigned int message_counter = 0;
unsigned int buffer_last = 0;//position of initial front of the buffer
unsigned int buffer_begin = 0;//position of intial back of the buffer  at the beginning the front and back are the same
unsigned int buffer[256];
sem_t counter_sem, empty, full ,not_busy;//semaphores used
//prototypes
void increment(int*);
void monitor();
void collector();
int main()
{

    srand(time(0));//seed for the following uses of rand()

    int number_of_threads = 5 ;// let it be 500 for example
    int thread_idx = 0;
    pthread_t mCounter[number_of_threads]; // N mCounter threads
    pthread_t mMonitor; //1 monitor thread
    pthread_t mCollector; // 1 collector thread
    int shared = 1;
    sem_init(&counter_sem, shared , 1);//initialize counter semaphore to 1
    //meaning only 1 counter thread can access the counter at a time

    sem_init(&full, shared , buffer_max_size); //initiallized to capacity of buffer
    sem_init(&empty, shared , 0); // it is empty so cannot be read by collector
    //empty initialized to 0 so when the collector waits on it it gets blocked ( so as not to read from empty buffer)
    sem_init(&not_busy, shared , 1); // do not access it if it is used



    while(thread_idx < number_of_threads) {
        int index_copy = thread_idx;// a copy of the index of the counter thread
        int* idx_arg = &index_copy;// pointer to be passed to pthread_create as an argument for increment function

        pthread_create(&mCounter[thread_idx++],NULL,increment, idx_arg);

        usleep(300);//sleep after each thread  300 microseconds

    }
    sleep(5);//wait after creation of counter threads
    //then activate the monitor
    pthread_create(&mMonitor, NULL, monitor, NULL);
    sleep(15);
    // give the monitor some time to fill the buffer
    //then activate the collector
    pthread_create(&mCollector, NULL, collector, NULL);

    while(1);
    //keep waiting forever to watch the threads

    //pthread_join(mCounter[thread_idx-1],NULL);
    /*
    while(1){
        int choice = rand() % 5;

        switch( choice) {
            case 4:
            case 0: {
                int threads_to_create = (buffer_max_size/20 ) %number_of_threads;

                int ctr = 0;
                while(ctr < threads_to_create){
                    int* idx_arg = &thread_idx;
                    pthread_create(&mCounter[thread_idx], NULL, increment, idx_arg );
                    thread_idx ++;
                    thread_idx = thread_idx% number_of_threads;
                    if(ctr %5)
                        sleep(1);
                }
                break;

            }
            case 3:
            case 1: { //monitor

                pthread_create(&mMonitor ,NULL , monitor, NULL);

                break;
            }case 2: {

                pthread_create(&mCollector, NULL, collector, NULL);

            }
        }

        sleep(rand()%5);

    thread_idx = 0;

    */
    return 0;
}
void increment(int* thread_idx) {
    int idx = *thread_idx; // get the counter index passed as a parameter when the thread is created

    while(1){



        printf("Counter thread %d: received a message\n", idx);

        printf("Counter thread %d: waiting to write\n", idx);
        //enforcing mutual exclusion on the shared variable message_counter
        sem_wait(&counter_sem);
        message_counter++; // critical section
        int copy = message_counter;
        printf("Counter thread %d: now adding to counter, counter value = %u\n", idx, copy);
        sem_post(&counter_sem);

        sleep(rand()%13);

    }
}
void monitor() {
    while(1) {
        int counter_copy = 0;
        printf("Monitor Thread: waiting to read counter\n");

        sem_wait(&counter_sem);
         // first critical section reading counter
        printf("Monitor Thread: reading a count value of %u\n",message_counter);
        counter_copy = message_counter; //making a copy of the counter before clearing
        // and so it can be placed in the buffer
        message_counter = 0;//clear the counter after reading its value

        sem_post(&counter_sem);//allow other counters (or monitors to access the shared counter

        //if( counter_copy == 0)
          //  return;
             // to prevent buffer overflowing if the counter has not been updated

        sem_wait(&full); //if the buffer is full the monitor gets blocked until the collector frees one spot in it
        sem_wait(&not_busy);//do not use buffer if anyone else is using it ( collector or if there are multiple monitors)

        //second critical section writing to buffer
        printf("Monitor Thread: writing to buffer at position %d\n",buffer_last);
        //buffer enqueue
        buffer[buffer_last] = counter_copy; //put the read value in the buffer at the `back of the queue
        buffer_last = (buffer_last +1  ) % buffer_max_size;// go to next available spot in the circular array
        buffer_current_size++;
        //buffer enqueuing done
        if(buffer_current_size == buffer_max_size) { // buffer became full the monitor will be blocked until it gets collected
            printf("Monitor Thread: Buffer full!\n"); // if the next operation on the buffer is monitor then it will be blocked
        }

        sem_post(&not_busy);
        sem_post(&empty);

        sleep(rand()%5);
    }
}
void collector() {

    while(1) {
        sem_wait(&empty);//wait no the empty so as not to collect fron empty buffer
        sem_wait(&not_busy);//if the buffer is not empty ,, either the collector or the monitor may access it at a time
        //so if it is being used you get blocked
        printf("Collector Thread: Reading at position %d value:%u\n",buffer_begin,buffer[buffer_begin]);//
        //buffer 'dequeuing'
        buffer_begin = (buffer_begin + 1 ) % buffer_max_size; // advance the pointer to the last added item in the circular array
        buffer_current_size--;
        //buffer dequeuing complete

        if(buffer_current_size == 0) { // if buffer is empty the semaphore will block the collector until an item is added again
            printf("Collector Thread: Buffer Empty!\n");
            //if next operation on the buffer is the collector then it will be blocked because buffer is now empty.
        }
        sem_post(&not_busy);//allow any other process or thread to use the buffer
        sem_post(&full);//free one of the spots in the buffer, to unblock the monitor if it was blocked for attempting to overfill the buffer

        sleep(rand()%7);

    }
}
