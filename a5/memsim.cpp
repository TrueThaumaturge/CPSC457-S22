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
    std::unordered_map<long, std::vector<PartitionRef>> tagged_blocks; //deallocation optimization, references to all blocks owned by process
    std::set<PartitionRef, scmp> free_blocks; //allocation optimization, references to all free blocks

    //variables
    int64_t page_size;

    //constructor
    Simulator(int64_t page_size) {
        this->page_size = page_size;
        result.n_pages_requested = 0;
        result.max_free_partition_size = 0;
        result.max_free_partition_address = 0;
    }

    /*
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
        //printf("partition results: addr=%ld, size=%ld, tag=%d\n", largest->addr, largest->size, largest->tag);
        return largest;
    }
    */

    void allocate(int tag, int size) {
        //printf("allocating %d blocks for process %d\n", size, tag);
        //special case when partition is completely empty (first allocation)
        if(all_blocks.empty()) {
            int pages_req = 1 + ((size - 1) / page_size); //# of pages requested
            int blocks_req = pages_req * page_size; //# of blocks requested, may be more
            result.n_pages_requested += pages_req; //records pages requested

            all_blocks.emplace_back(-1, blocks_req, 0); //allocates enough free space
            free_blocks.insert(all_blocks.begin()); //adds new partition to free blocks list
        }

        //PartitionRef partition = worst_fit_search(size); //finds worst-fit partition (aka biggest)
        PartitionRef partition;
        if(free_blocks.empty())
            partition = all_blocks.end(); //sets to end, indicating no free space
        else
            partition = *(free_blocks.begin()); //sets to worst-fit free partition
        
        //if the biggest fit isn't enough or there is no free space at all
        if(size > partition->size || partition == all_blocks.end()) {
            //printf("asking for more space - ");
            int blocks_req; //# of blocks requested, may be more
            int pages_req; //# of pages requested

            //if the end is free, we increase its size
            if(all_blocks.back().tag == -1) {
                //printf("end free\n");
                //sets the final partition to be the new best
                partition = all_blocks.end();
                --partition;
                free_blocks.erase(partition); //modifying - we need to remove and re-add partition to set
                pages_req = 1 + ((size - all_blocks.back().size - 1) / page_size); //integer division rounded up
                blocks_req = pages_req * page_size; //blocks requested may be more than needed
                all_blocks.back().size += blocks_req; //adds request to last partition
                free_blocks.insert(partition);
            }
            //otherwise we make a new free partition
            else {
                //printf("making new partition\n");
                pages_req = 1 + ((size - 1) / page_size); //integer division rounded up
                blocks_req = pages_req * page_size; //blocks requested may be more than needed
                all_blocks.emplace_back(-1, blocks_req, (all_blocks.back().addr + all_blocks.back().size)); //creates new partition at end of partition list
                free_blocks.insert(std::prev(all_blocks.end())); //adds new partition to free blocks list
                //sets the final partition to be the new best
                partition = all_blocks.end();
                --partition;
            }
            result.n_pages_requested += pages_req; //records pages requested
        }

        //if the best partition is a perfect fit, we simply change the tag
        if(size == partition->size) {
            //printf("perfect fit\n");
            free_blocks.erase(partition); //removes from free blocks set
            partition->tag = tag;
            tagged_blocks[tag].push_back(partition); //adds reference to tagged blocks hashmap
        }
        //otherwise we need to create a new partition with the empty space after 
        else if(size < partition->size) {
            //printf("filling part of partition\n");
            //inserts new partition before that represents the filled space
            all_blocks.insert(partition, Partition(tag, size, partition->addr));
            //updates current partition - the empty part
            // -> [tag] -> {[-1]} ->
            //    prev    partition
            free_blocks.erase(partition); //modifying the free partition, we'll add it back after
            partition->addr += size;
            partition->size -= size;

            //merges node after
            if(partition != all_blocks.end()) { //if next will be within bounds
                if(std::next(partition)->tag == -1) {
                    partition->size += std::next(partition)->size; //merges sizes
                    free_blocks.erase(std::next(partition)); //removes merged partition from free set
                    all_blocks.erase(std::next(partition)); //erases node from linked list
                }
            }
            tagged_blocks[tag].push_back(std::prev(partition)); //adds reference to newly created partition to tagged hashmap
            free_blocks.insert(partition); //adds the free partition back
        }
    }

    void deallocate(int tag) {
        //printf("deallocating process %d\n", tag);
        PartitionRef prev; //previous partition
        PartitionRef next;

        //for each partition of the tag
        for(PartitionRef p : tagged_blocks[tag]) {
            prev = std::prev(p);
            next = std::next(p);
            //if the tags match
            if(p->tag == tag) {
                p->tag = -1; //marks as free

                //merges previous node
                if(p != all_blocks.begin()) { //if prev will be within bounds
                    if(prev->tag == -1) {
                        p->size += prev->size; //merges sizes
                        p->addr = prev->addr; //updates address
                        free_blocks.erase(prev); //removes from free blocks set
                        all_blocks.erase(prev); //erases node from linked list
                    }
                }
                //merges node after
                if(next != all_blocks.end()) { //if next will be within bounds
                    if(next->tag == -1) {
                        p->size += next->size; //merges sizes
                        free_blocks.erase(next); //removes from free blocks set
                        all_blocks.erase(next); //erases the node from linked list
                    }
                }
                free_blocks.insert(p); //block is now free so we insert it in the free blocks
            }
        }
        tagged_blocks[tag].clear(); //clears all elements from tagged_blocks since they've been deallocated now
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
        //printf("CONSISTENCY CHECK:\n");
        PartitionRef p;
        // make sure the sum of all partition sizes in your linked list is
        // the same as number of page requests * page_size
        int64_t size_sum = 0;
        for(p = all_blocks.begin(); p != all_blocks.end(); ++p) {
            size_sum += p->size;
            //printf("partition: addr=%ld, size=%ld, tag=%d\n", p->addr, p->size, p->tag); //debug
        }
        if(size_sum == result.n_pages_requested * page_size) {
            //printf("SUM CHECK PASSED: partition size equals page requests\n");
        }
        else {
            printf("!!!SUM CHECK FAILED: size_sum = %ld, requests * size = %ld\n", size_sum, (result.n_pages_requested * page_size));
            exit(-1);
        }

        // make sure your addresses are correct
        int64_t failed_addr = -1;
        for(p = all_blocks.begin(); p != all_blocks.end(); ++p) {
            if(p == all_blocks.begin()) { //at start, checks if it's 0
                if(p->addr != 0) {
                    failed_addr = 0;
                    break;
                }
            }
            else { //otherwise, checks if address is correct
                if(p->addr != std::prev(p)->addr + std::prev(p)->size) {
                    failed_addr = p->addr;
                    break;
                }
            }
        }
        if(failed_addr == -1) {
            //printf("ADDRESS CHECK PASSED: addresses correct\n");
        }
        else {
            printf("!!!ADDRESS CHECK FAILED: first failed address at %ld\n", failed_addr);
            exit(-1);
        }


        // make sure the number of all partitions in your tag data structure +
        // number of partitions in your free blocks is the same as the size
        // of the linked list
        /*
        int count1 = 0;
        for(auto p : tagged_blocks) {
            for(auto n : p.second) {
                count1++;
            }
        }
        for(auto n : free_blocks) {
            count1++;
        }
        int count2 = 0;
        for(auto n : all_blocks) {
            count2++;
        }
        if(count1 == count2) {
            //printf("COUNT CHECK PASSED: free + tagged partitions = total partitions\n");
        }
        else {
            printf("!!!COUNT CHECK FAILED: free + tagged partitions != total partitions (%d != %d)\n", count1, count2);
            exit(-1);
        }
        */

        // make sure that every free partition is in free blocks

        // make sure that every partition in free_blocks is actually free

        // make sure that none of the partition sizes or addresses are < 1
        bool pass = true;
        for(p = all_blocks.begin(); p != all_blocks.end(); ++p) {
            if(p->size < 1 || p->addr < 0) {
                pass = false;
                break;
            }
        }
        if(pass) {
            //printf("MEMBER CHECK PASSED: no invalid size/address\n");
        }
        else {
            printf("!!!MEMBER CHECK FAILED: negative address or negative/zero size found.\n");
            exit(-1);
        }

        //printf("CONSISTENCY CHECK END\n\n");
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