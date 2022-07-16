// ======================================================================
// You must modify this file and then submit it for grading to D2L.
// ======================================================================
//
// count_pi() calculates the number of pixels that fall into a circle
// using the algorithm explained here:
//
// https://en.wikipedia.org/wiki/Approximations_of_%CF%80
//
// count_pixels() takes 2 paramters:
//    r                 =    the radius of the circle
//    n_threads =    the number of threads you should create
//
// Currently the function ignores the n_threads parameter. Your job is to
// parallelize the function so that it uses n_threads threads to do
// the computation.

#include "calcpi.h"
#include <pthread.h>
#include <stdio.h>

struct Task {
    int tid;
    uint64_t partial_count;
};
Task tasks[256];

int rad;
int n_thr;
double rsq;

void* thread_count_pixels(void* ptr) {
    struct Task* task = (struct Task*)ptr;
    uint64_t count = 0;
    //partial sum - thread does its share of x numbers
    for( double x = task->tid; x <= rad ; x += n_thr) { //starts at thread id + 1, increments by # of threads
        for( double y = 0 ; y <= rad ; y++) {
            if( x*x + y*y <= rsq) count++;
        }
    }
    task->partial_count = count;
    return NULL;
}

uint64_t count_pixels(int r, int n_threads)
{
    if(r < n_threads) //in this case we won't actually need that many threads
        n_threads = r;

    pthread_t threads[n_threads];
    //assigns arguments to global variables, aka lazy
    rad = r;
    n_thr = n_threads;
    rsq = double(r) * r;

    for(int i = 0; i < n_threads; i++) { //creates and executes n threads
        tasks[i].tid = i+1;
        pthread_create(&threads[i], NULL, thread_count_pixels, &tasks[i]);
    }

    for(int i = 0; i < n_threads; i++) { //joins threads, waiting until all are done
        pthread_join(threads[i], NULL);
    }

    //sums up total count of partial counts
    uint64_t total_count = 0;
    for(int i = 0; i < n_threads; i++) {
        total_count += tasks[i].partial_count;
    }
    total_count = total_count * 4 + 1; //final part of the algorithm
    return total_count;
}