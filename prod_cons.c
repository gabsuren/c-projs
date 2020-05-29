/*
Procucer-consumer solution based on POSIX Standart library.
The application must be stopped by pressing Ctrl+C
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#define N 10
#define M 10

#define Q_SIZE 100
#define THRESHOLD 80
#define FILE_NAME "data.txt"

typedef struct c_queue_t {
    int buff[Q_SIZE];
    int count;
    int saturation;
    int idx_prod;                 // write pos for producer
    int idx_cons;                 // read pos for consumer
    int run;                      // breaks cons/prod infinite loop when Ctrl+C is pressed;
    pthread_mutex_t queue_locker; // mutex for all queue related operations
    pthread_cond_t prod_locker;   // locks the producer when upper limit is riched, resumed by consumers signal
    pthread_cond_t cons_locker;   // locks the consumer when buffer is empty, resumed by producers signal 
} c_queue;

c_queue* queue = NULL;
FILE* file = NULL;

static c_queue* create_queue() {
    c_queue* q = (c_queue*)malloc(sizeof(c_queue));
    q->idx_prod = q->idx_cons = 0;
    q->count = 0;
    q->run = 1;
    q->saturation = Q_SIZE;
    pthread_mutex_init(&q->queue_locker, NULL);
    pthread_cond_init(&q->prod_locker, NULL);
    pthread_cond_init(&q->cons_locker, NULL);
    return q;
}

void ctrl_c(int sig) {
    queue->run = 0;
}

// Producer function. Generates random numbers and puts inside queue. 
// Suspends when queue is filled.
void* producer(void* c) {
    fprintf(stderr, "Producer started\n");
    while(queue->run) { 
        usleep((rand() % 101) * 1000);	
        pthread_mutex_lock(&queue->queue_locker);
        if(queue->count > queue->saturation) {
            queue->saturation = THRESHOLD;
            pthread_cond_wait(&queue->prod_locker, &queue->queue_locker);
            pthread_mutex_unlock(&queue->queue_locker);
            continue;
        }
        queue->saturation = Q_SIZE;
        queue->buff[queue->idx_prod++] = rand() % 101;
        queue->idx_prod %= Q_SIZE;
        queue->count++;
        pthread_cond_signal(&queue->cons_locker);
        pthread_mutex_unlock(&queue->queue_locker);
    }
    fprintf(stderr, "Producer stopping\n");
    return 0;
}

void* consumer(void* c) {
    fprintf(stderr, "Consumer started\n");
    while(1) {
        usleep((rand() % 101) * 1000);
        pthread_mutex_lock(&queue->queue_locker);
        if(queue->count < 1) {
            if(queue->run) {
                pthread_cond_wait(&queue->cons_locker, &queue->queue_locker);
                pthread_mutex_unlock(&queue->queue_locker);
                continue;
            } else {
                pthread_mutex_unlock(&queue->queue_locker);
                break;
            }
        }
        fprintf(file, "%d,", queue->buff[queue->idx_cons++]);
        queue->idx_cons %= Q_SIZE;	
        queue->count--;
        pthread_cond_signal(&queue->prod_locker);
        pthread_mutex_unlock(&queue->queue_locker);
    }
    fprintf(stderr, "Consumer stopping\n");
    return 0;
}

static void free_queue(c_queue* queue) {
    pthread_cond_destroy(&queue->prod_locker);
    pthread_cond_destroy(&queue->cons_locker);
    pthread_mutex_destroy(&queue->queue_locker);
    free(queue);
}

static void wait_for_stop_and_report(c_queue* queue) {
    while(queue->run) {
        sleep(1);
        fprintf(stderr, "==========Count: %d==========\n", queue->count);
    }
}

static void join_all_workers(pthread_t* first, pthread_t* end) {
    while(first < end) {
        pthread_join(*first++, 0);
    }
}

int main() {
    pthread_t thread_pool[N + M];
    pthread_t* pp = thread_pool;
    file = fopen(FILE_NAME, "w");

    if(NULL == file) {	
        perror(FILE_NAME);
        return 1;
    }
    srand(time(0));
    queue = create_queue();
    if(NULL == queue) {
        perror("Ring queue allocation failed");
        return 1;
    }
    fprintf(stderr, "To terminate the progran use Ctrl+C.\n");
    signal(SIGINT, ctrl_c);
    // Create producer threads
    for(int i = 0; queue->run && i < N; i++) {
        if (pthread_create(pp++, 0, producer, 0)) {
            perror("Create producer thread");
            queue->run = 0;
        }
    }
    // Create consumer threads
    for(int i = 0; queue->run && i < M; i++) {
        if(pthread_create(pp++, 0, consumer, 0)) {
            perror("Create consumer thread");
            queue->run = 0;
        }
    }
    wait_for_stop_and_report(queue);
    join_all_workers(thread_pool, pp);
    free_queue(queue);
    fclose(file);
    return 0;
}
