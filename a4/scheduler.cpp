// this is the only file you should modify and submit for grading
// NOTE: THIS ASSIGNMENT WAS RUSHED AND PRETTY UNFINISHED AND WILL NOT WORK PROPERLY.

#include "scheduler.h"
#include "common.h"
#include <queue>

#define MAX_PROC 30 //max number of processes

// this is the function you should implement
//
// runs Round-Robin scheduling simulator
// input:
//   quantum = time slice
//   max_seq_len = maximum length of the reported executing sequence
//   processes[] = list of process with populated IDs, arrival_times, and bursts
// output:
//   seq[] - will contain the execution sequence but trimmed to max_seq_len size
//         - idle CPU will be denoted by -1
//         - other entries will be from processes[].id
//         - sequence will be compressed, i.e. no repeated consecutive numbers
//   processes[]
//         - adjust finish_time and start_time for each process
//         - do not adjust other fields
//

void simulate_rr(
    int64_t quantum,
        //length of time slice
    int64_t max_seq_len,
        //the maximum size of the seq vector
    std::vector<Process> & processes,
        //description of process id, arrival time, burst time, start time, finish time
        //start time and finish time need to be filled by us
    std::vector<int> & seq
        //condensed execution sequence order that we need to create
) {
    seq.clear(); //empties sequence vector in case its populated
    if(processes.empty()) { //if there's no processes, we have no simulation to perform
        return;
    }
    
    int64_t curr_time = 0; 
        //current simulation time
    int64_t remaining_slice; 
        //remaining time in current time slice
    int cpu = -1; 
        //currently running process
    std::queue<int> rq, jq;
        //ready queue is order of jobs to be executed
        //job queue is order of jobs waiting on their start time
    int64_t remaining_burst[MAX_PROC]; 
        //remaining time

    //populates job queue and remaining bursts
    for(auto p : processes) {
        jq.push(p.id); //adds to job queue
        remaining_burst[p.id] = p.burst; //adds its burst time
    }

    //initialization - skips time to the start of the first job to run
    if(processes[jq.front()].arrival_time == 0) //if it starts idle, adds to sequence
        seq.push_back(0); //cpu starts idle

    remaining_slice = quantum; //initializes remaining_slice (goes to 0)

    //main simulation loop
    while(1) {
        //exit condition
        //optimization 4: if we're executing the last process, we can finish it and end the simulation
        if(jq.empty() && rq.empty()) {
            //on edge cases we have to record the sequence here
            if(cpu != seq.back() && (int64_t)seq.size() < max_seq_len) {
                seq.push_back(cpu);
            }
            processes[cpu].finish_time = curr_time + remaining_burst[cpu]; //records final finish time
            break;
        }
        //

        //finish process
        //if process in cpu is done
        if(remaining_burst[cpu] == 0) {
            processes[cpu].finish_time = curr_time; //marks finish time
            cpu = -1; //sets cpu to idle
            remaining_slice = quantum; //reset time slice
            continue; //goes to start of loop
        } 
        //

        //context switch
        //if cpu is idle or remaining slice is done and rq isn't empty
        if((cpu == -1 || remaining_slice == 0) && !rq.empty()) {
            remaining_slice = quantum; //resets time slice when switching
            if(cpu != -1) {
                rq.push(cpu); //pushes any job on cpu back into rq
            }
            cpu = rq.front(); //moves front of rq to cpu
            rq.pop(); //removes from rq
            
            //if this is the first time the program's started, record its time
            if(processes[cpu].burst == remaining_burst[cpu]) {
                processes[cpu].start_time = curr_time;
            }
            
            //OPTIMIZATIONS
            //optimization 4: if we're executing the last process, we can finish it and end the simulation
            if(jq.empty() && rq.empty()) {
                //on edge cases we have to record the sequence here
                if(cpu != seq.back() && (int64_t)seq.size() < max_seq_len) {
                    seq.push_back(cpu);
                }
                processes[cpu].finish_time = curr_time + remaining_burst[cpu]; //records final finish time
                break;
            }

            //optimization 2: if it can finish within the time slice we can skip ahead a bit
            else if(cpu != -1 && remaining_burst[cpu] <= quantum) { //if work needed fits within the slice
                //records sequence
                if(cpu != seq.back() && (int64_t)seq.size() < max_seq_len) {
                    seq.push_back(cpu);
                }
                curr_time += remaining_burst[cpu]; //does the rest of the work
                remaining_burst[cpu] = 0;
            }

            //optimization 3: we can immediately execute a full time slice as long as we check for any arrivals before we put it back in rq
            else  { //if the ready queue still isn't empty - a waste to do this optimization if its just gonna come back
                //records sequence
                if(cpu != seq.back() && (int64_t)seq.size() < max_seq_len) {
                    seq.push_back(cpu);
                }
                curr_time += quantum; //advance time
                remaining_burst[cpu] -= quantum; //do the work

                //adds any arrivals before putting process back into rq
                while(!jq.empty()) {
                    //this is < instead of <= because of the given order required
                    if(processes[jq.front()].arrival_time < curr_time) {
                        rq.push(jq.front()); //adds to rq
                        jq.pop(); //removes from jq
                    }
                    else break;
                }

                //context switch
                rq.push(cpu);
                cpu = rq.front();
                rq.pop();
                //if this is the first time the program's started, record its time
                if(processes[cpu].burst == remaining_burst[cpu]) {
                    processes[cpu].start_time = curr_time;
                }
            }
            //END OPTIMIZATIONS

            continue;
        }
        //

        //job queue -> ready queue
        //if a new process has arrived
        if(!jq.empty()) {
            if(processes[jq.front()].arrival_time <= curr_time) {
                rq.push(jq.front()); //adds to rq
                jq.pop(); //removes from jq
                continue;
            }
        }
        //

        //records sequence
        if(cpu != seq.back() && (int64_t)seq.size() < max_seq_len) {
            seq.push_back(cpu);
        }
        //

        //move time forward and either work or stay idle. many cases optimize how much time we can skip
        if(rq.empty()) {
            //optimization #5: if RQ is empty and CPU is idle skip to arrival of next process
            if(cpu == -1) {
                if(!jq.empty()) { //dont think its possible for jq to be empty here but it's a paranoia check
                    curr_time = processes[jq.front()].arrival_time;
                    continue;
                }
            }
        }

        //executes one burst if not idle
        //should only reach here if none of the optimizations worked - still missing 1 implementation!
        //printf("t=%ld\n", curr_time);
        if(cpu != -1) {
            remaining_burst[cpu]--; //reduces burst
            remaining_slice--; //reduces time slice
            //if the rq is empty and the slice is zero
            if(remaining_slice == 0 && rq.empty()) {
                remaining_slice = quantum; //reset slice
            }
        }
        curr_time++;
        
        //
    }

    return;
}