// -------------------------------------------------------------------------------------
// this is the only file you need to edit
// -------------------------------------------------------------------------------------
//
// (c) 2022, Pavol Federl, pfederl@ucalgary.ca
// Do not distribute this file.

#include "memsim.h"
#include <cassert>
#include <iostream>
#include <list>
#include <set>
#include <unordered_map>
#include <iterator>

//represents a partition in the list of partitions
struct Partition {
    int tag; //which process currently occupies it, -1 means its empty
    int64_t size, addr;
    Partition(int tag, int64_t size, int64_t addr) {
        this->tag = tag;
        this->size = size;
        this->addr = addr;
    }
};
typedef std::list<Partition>::iterator PartitionRef;

//defines how we sort our list
struct scmp {
    bool operator()(const PartitionRef & c1, const PartitionRef & c2) const {
        if(c1->size == c2->size) //if theyre the same size
            return c1->addr < c2->addr; //return the first address
        else
            return c1->size > c2->size; //otherwise return the largest size
    }
};

struct Simulator {

    //results recorded
    MemSimResult result;
        //n_pages_requested
        //max_free_partition_size
        //max_free_partition_address
    
    //data structures
    std::list<Partition> all_blocks; //simple doubly linked list
    //std::unordered_map<long, std::vector<PartitionRef>> tagged_blocks; //deallocation optimization, references to all blocks owned by process
    //std::set<PartitionRef, scmp> free_blocks; //allocation optimization, references to all free blocks

    //variables
    int64_t page_size;

    //constructor
    Simulator(int64_t page_size) {
        this->page_size = page_size;
        result.n_pages_requested = 0;
        result.max_free_partition_size = 0;
        result.max_free_partition_address = 0;
    }

    PartitionRef worst_fit_search(int size) {
        PartitionRef largest = all_blocks.begin(); //current largest partition
        PartitionRef p = all_blocks.begin(); //iterator

        //for all partitions
        for(p = all_blocks.begin(); p != all_blocks.end(); ++p) {
            if(largest->tag != -1)
                largest = p;
            else if(p->tag == -1 && p->size > largest->size) //if the partition is empty and larger than the previously largest
                largest = p; //update the largest
        }

        return largest;
    }

    void allocate(int tag, int size) {
        //printf("allocating %d blocks for process %d\n", size, tag);
        //special case when partition is completely empty (first allocation)
        if(all_blocks.empty()) {
            int pages_req = 1 + ((size - 1) / page_size); //# of pages requested
            int blocks_req = pages_req * page_size; //# of blocks requested, may be more
            all_blocks.emplace_back(-1, blocks_req, 0);
            result.n_pages_requested += pages_req; //records pages requested
        }

        PartitionRef partition = worst_fit_search(size); //finds worst-fit partition (aka biggest)
        //printf("partition results: addr=%ld, size=%ld, tag=%d\n", partition->addr, partition->size, partition->tag);
        
        //if the biggest fit isn't enough or there wasn't a valid search result
        if(size > partition->size || partition->tag != -1) {
            int blocks_req; //# of blocks requested, may be more
            int pages_req; //# of pages requested
            //if the end is free, we increase its size

            if(all_blocks.back().tag == -1) {
                pages_req = 1 + ((size - all_blocks.back().size - 1) / page_size); //integer division rounded up
                blocks_req = pages_req * page_size; //blocks requested may be more than needed
                all_blocks.back().size += blocks_req; //adds request to last partition
            }
            //otherwise we make a new free partition
            else {
                pages_req = 1 + ((size - 1) / page_size); //integer division rounded up
                blocks_req = pages_req * page_size; //blocks requested may be more than needed
                all_blocks.emplace_back(-1, blocks_req, (all_blocks.back().addr + all_blocks.back().size)); //creates new partition at end of partition list
            }
            result.n_pages_requested += pages_req; //records pages requested
            partition = all_blocks.end(); //sets the final partition to be the new best
            --partition;
        }

        //if the best partition is a perfect fit, we simply change the tag
        if(size == partition->size) {
            partition->tag = tag;
        }
        //otherwise we need to create a new partition with the empty space after 
        else if(size < partition->size) {
            //inserts new partition before that represents the filled space
            all_blocks.insert(partition, Partition(tag, size, partition->addr));
            //updates current partition to be the empty part
            partition->addr += size;
            partition->size -= size;
            partition->tag = -1;

            //merges node after
            if(partition != all_blocks.end()) { //if next will be within bounds
                if(std::next(partition)->tag == -1) {
                    partition->size += std::next(partition)->size; //merges sizes
                    all_blocks.erase(std::next(partition)); //erases the node
                }
            }
        }
    }

    void deallocate(int tag) {
        //printf("deallocating process %d\n", tag);
        if(all_blocks.empty())  return; //no work to do

        PartitionRef p = all_blocks.begin(); //current partition
        PartitionRef prev; //previous partition
        PartitionRef next = std::next(p); //next partition

        while(p != all_blocks.end()) {
            //if the tags match
            if(p->tag == tag) {
                p->tag = -1; //marks as free

                //merges previous node
                if(p != all_blocks.begin()) { //if prev will be within bounds
                    if(prev->tag == -1) {
                        p->size += prev->size; //merges sizes
                        p->addr = prev->addr; //updates address
                        all_blocks.erase(prev); //erases the node
                    }
                }
                //merges node after
                if(next != all_blocks.end()) { //if next will be within bounds
                    if(next->tag == -1) {
                        p->size += next->size; //merges sizes
                        all_blocks.erase(next); //erases the node
                    }
                }
            }
            //increments the 3 pointers
            p++;
            prev = std::prev(p);
            next = std::next(p);
        }
    }

    MemSimResult getStats() {
        PartitionRef p;
        if(!all_blocks.empty()) {
            for(p = all_blocks.begin(); p != all_blocks.end(); ++p) {
                if(p->tag == -1 && result.max_free_partition_size < p->size) {
                    result.max_free_partition_size = p->size;
                    result.max_free_partition_address = p->addr;
                }
            }
        }
        return result;
    }
    void check_consistency() {
        // make sure the sum of all partition sizes in your linked list is
        // the same as number of page requests * page_size
        printf("checking consistency\n");
        PartitionRef p;
        for(p = all_blocks.begin(); p != all_blocks.end(); ++p) {
            printf("addr=%ld, size=%ld, tag=%d\n", p->addr, p->size, p->tag);
        }
        printf("check ended\n\n");

        // make sure your addresses are correct

        // make sure the number of all partitions in your tag data structure +
        // number of partitions in your free blocks is the same as the size
        // of the linked list

        // make sure that every free partition is in free blocks

        // make sure that every partition in free_blocks is actually free

        // make sure that none of the partition sizes or addresses are < 1
    }
};

// re-implement the following function
// ===================================
// parameters:
//        page_size: integer in range [1..1,000,000]
//        requests: array of requests
MemSimResult mem_sim(int64_t page_size, const std::vector<Request> & requests)
{
    Simulator sim(page_size);
    for (const auto & req : requests) {
        if (req.tag < 0) {
            sim.deallocate(-req.tag);
        } else {
            sim.allocate(req.tag, req.size);
        }
        //sim.check_consistency();
    }
    return sim.getStats();
}