// -------------------------------------------------------------------------------------
// this is the only file you need to edit
// -------------------------------------------------------------------------------------
//
// (c) 2021, Pavol Federl, pfederl@ucalgary.ca
// Do not distribute this file.

#include "fatsim.h"
#include <cstdio>
#include <stack>
#include <algorithm>

// reimplement this function
std::vector<long> fat_check(const std::vector<long> & fat)
{
    //printf("size: %ld\n",fat.size());
    std::vector<long> result;
        //final result vector of the fat_check
    std::vector<std::vector<long>> adj_list;
    adj_list.resize(fat.size());
        // list of all nodes with edges pointing towards the node
    std::vector<long> end_points; 
        // indices of nodes that point to -1 that we need to check the length of

    // builds adjacency list to represent FAT graph
    for(unsigned long i = 0; i < fat.size(); i++) {
        //printf("%ld<-%ld\n", fat[i], i); //debugging
        if(fat[i] != -1) {
            adj_list[fat[i]].push_back(i); //adds node to adjacency list
        }
        else if(fat[i] == -1) {
            end_points.push_back(i); //adds to end_points list if the node points to -1
        }
    }

    std::stack<std::pair<long, long>> stack; //stack structure for the dfs
            //.first is node index, .second is current chain length
    long longest_chain, curr_chain, curr_node;
        //variables used in search

    // performs depth-first search on each end point and records the longest chain length
    for(long end : end_points) { //for each end point
        //printf("searching end point %ld:\n", end); //debugging
        longest_chain = 0; //resets longest recorded chain
        stack.emplace(end, 1); //starts stack with end point

        while(!stack.empty()) { //until we've traversed everything
            //grabs the top of the stack
            curr_node = stack.top().first;
            curr_chain = stack.top().second;
            stack.pop();

            //printf("curr_node %ld, curr_chain %ld\n", curr_node, curr_chain); //debugging

            //records chain if it's the biggest so far
            if(curr_chain > longest_chain) {
                longest_chain = curr_chain;
            }

            //adds all adjacent nodes to the stack to be checked next
            for(auto n : adj_list[curr_node]) { 
                stack.emplace(n, curr_chain+1);
            }
        }
        //printf("longest chain is %ld\n\n", longest_chain); //debugging
        result.push_back(longest_chain); //adds the chain length to results array
    }

    std::sort(result.begin(), result.end()); //sorts results in ascending order
    return result;
}