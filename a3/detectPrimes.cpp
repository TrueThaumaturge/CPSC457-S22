/// ============================================================================
/// Copyright (C) 2022 Pavol Federl (pfederl@ucalgary.ca)
/// All Rights Reserved. Do not distribute this file.
/// ============================================================================
///
/// You must modify this file and then submit it for grading to D2L.
///
/// You can delete all contents of this file and start from scratch if
/// you wish, as long as you implement the detect_primes() function as
/// defined in "detectPrimes.h".

#include "detectPrimes.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

std::atomic<bool> global_finished(false); //set global_finished to false and atomic
std::atomic<bool> global_cancel(false); //thread cancellation flag

std::vector<int64_t> result; //initialize empty result vector
bool * thread_results; //individual thread results

int64_t n; //global var that tracks the current number

// C++ barrier class (from lecture notes).
// -----------------------------------------------------------------------------
class simple_barrier {
    std::mutex m_;
    std::condition_variable cv_;
    int n_remaining_, count_;
    bool coin_;

    public:
    simple_barrier(int count = 1) { init(count); }
    void init(int count)
    {
        count_ = count;
        n_remaining_ = count_;
        coin_ = false;
    }
    bool wait()
    {
        if (count_ == 1) return true;
        std::unique_lock<std::mutex> lk(m_);
        if (n_remaining_ == 1) {
            coin_ = ! coin_;
            n_remaining_ = count_;
            cv_.notify_all();
            return true;
        }
        auto old_coin = coin_;
        n_remaining_--;
        cv_.wait(lk, [&]() { return old_coin != coin_; });
        return false;
    }
};


// returns true if n is prime, otherwise returns false
// -----------------------------------------------------------------------------
static bool is_prime(int64_t n, int tid, int n_threads)
{
    // handle trivial cases
    if(global_cancel) return true;
    if (n < 2) return false;
    if (n <= 3) return true; // 2 and 3 are primes
    if (n % 2 == 0) return false; // handle multiples of 2
    if (n % 3 == 0) return false; // handle multiples of 3

    // try to divide n by every number 5 .. sqrt(n)
    //using skip method, so starting point and increments modified
    int64_t i = 5 + (6 * tid); 
    int64_t max = sqrt(n);
    while (i <= max && !global_cancel) {
        if (n % i == 0) return false;
        if (n % (i + 2) == 0) return false;
        i += (6 * n_threads);
    }
    // didn't find any divisors, so it must be a prime
    return true;
}

void task(int tid, simple_barrier & barrier, const std::vector<int64_t> & nums, int n_threads)
{   
    unsigned int i = 0; //current index in relation to nums
    bool totalresult; //the final combined result of all threads

    while(1) {
        //serial task picks one w/ barrier
        if(barrier.wait()) {
            global_cancel = false; //resets cancel flag
            if(i > 0) { //if we're not on the first loop, run the end code
                //combine per-thread results
                totalresult = true;
                for(int j = 0; j < n_threads; j++) {
                    if(!thread_results[j]) { //if one result came back false, means there's a divisor
                        totalresult = false;
                        break;
                    }
                }
                if(totalresult) result.push_back(n);
            }

            //get next number from nums
            n = nums[i]; 
            //if no numbers left, sets flag
            if(i >= nums.size()) global_finished = true;
        }
        barrier.wait();
        //end serial task

        //parallel task
        if(global_finished) break; //exits if flag is set
        else { //otherwise does the actual work
            //and records per-thread result
            thread_results[tid] = is_prime(n, tid, n_threads);
            if(!thread_results[tid])
                global_cancel = true; //cancels all other current operations
        }
        i++; //increments nums index
        //end parallel task
    }
}

// This function takes a list of numbers in nums[] and returns only numbers that
// are primes.
//
// The parameter n_threads indicates how many threads should be created to speed
// up the computation.
// -----------------------------------------------------------------------------
std::vector<int64_t>
detect_primes(const std::vector<int64_t> & nums, int n_threads)
{
    std::vector<std::thread> threads; //prepare memory for each thread
    simple_barrier barrier(n_threads); //initialize barrier
    
    bool a[n_threads];
    thread_results = a;


    for(int i = 0; i < n_threads; i++) {
        threads.emplace_back(std::thread(task, i, std::ref(barrier), nums, n_threads));
    }
    for(auto && t : threads) {
        t.join();
    }

    return result;
}
