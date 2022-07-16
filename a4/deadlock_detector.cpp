// this is the ONLY file you should edit and submit to D2L

#include "deadlock_detector.h"
#include "common.h"

/// this is the function you need to (re)implement
///
/// parameter edges[] contains a list of request- and assignment- edges
///   example of a request edge, process "p1" resource "r1"
///     "p1 -> r1"
///   example of an assignment edge, process "XYz" resource "XYz"
///     "XYz <- XYz"
///
/// You need to process edges[] one edge at a time, and run a deadlock
/// detection after each edge. As soon as you detect a deadlock, your function
/// needs to stop processing edges and return an instance of Result structure
/// with edge_index set to the index that caused the deadlock, and dl_procs set
/// to contain with names of processes that are in the deadlock.
///
/// To indicate no deadlock was detected after processing all edges, you must
/// return Result with edge_index = -1 and empty dl_procs[].
///

#define MAX_SIZE 30000 //max number of input edges

//main body of the program
class Graph 
{
    public:
        std::vector<int> adj_list[2*MAX_SIZE];  //list of all nodes with edges pointing towards the node
                                                //we don't want values initialized so we use array

        std::vector<int> out_counts;            //list of all nodes and the number of outgoing edges
                                                //we want all values initalized to 0 so we use vector

        std::string processes[2*MAX_SIZE];      //list of processes (aka names on left of input)
                                                //we don't want values initalized so we use array

        Word2Int word2int; //helper class

        Graph(int s) {
            out_counts.resize(s);
        }

        void add_edge(const std::vector<std::string> & edge);
        std::vector<std::string> toposort();
};

//runs a topological sort algorithm to detect any cycles
std::vector<std::string> Graph::toposort() {
    std::vector<int> out = out_counts;
    std::vector<int> zeroes;

    unsigned int i = 0;
    //populates zeroes vector with indexes of nodes
    for (auto o : out) {
        if (o == 0) //if no nodes are pointing at the current node it can run
            zeroes.push_back(i);
        i++;
    }

    //main algorithm loop
    while (!zeroes.empty()) {
        //grabs a number from the zeroes vector
        int n = zeroes.back();
        zeroes.pop_back();

        for (auto n2 : adj_list[n]) { //removes the edges from each node pointing at the freed node (they get this resource now)
            out[n2]--;
            if (!out[n2]) //adds any nodes that are now zeroes to the vector
                zeroes.push_back(n2);
        }
    }

    //compiling the results
    std::vector<std::string> dl_procs;
    i = 0;
    for (auto o : out) {
        if (o > 0 && processes[i].back() == '.') { //if there's an edge waiting on the current node and it's a process
            processes[i].pop_back(); //remove the mark
            dl_procs.push_back(processes[i]); //and adds it to the array 
        }
        i++;
    }
    return dl_procs;
}

//reads the split input line and adds the info to the graph
void Graph::add_edge(const std::vector<std::string> & edge) {
    //edge[0] specifies process name
    //edge[1] specifies assignment vs request edge
    //edge[2] specifies resource name
    
    std::string p_name = edge[0]+".", r_name = edge[2]; //node names
    unsigned int p_index = word2int.get(p_name), r_index = word2int.get(r_name); //node indexes

    if(processes[p_index].empty())
        processes[p_index] = p_name;
    if(processes[r_index].empty())
        processes[r_index] = r_name;

    if(edge[1].compare("->")) { //request edge - P -> R
        adj_list[p_index].push_back(r_index); //adds resource pointing at process
        out_counts[r_index]++; //increases the out-count at the resource
    }
    else if(edge[1].compare("<-")) { //assignment edge - P <- R
        adj_list[r_index].push_back(p_index); //adds process pointing at resource
        out_counts[p_index]++; //increases the out-count at the process
    }
}

//main class that calls the other classes
Result detect_deadlock(const std::vector<std::string> & edges)
{
    //initialize classes
    Result result;
    Graph graph(2*edges.size());
    result.edge_index = -1; //-1 by default

    for(unsigned int i = 0; i < edges.size(); i++) {
        graph.add_edge(split(edges[i]));
        result.dl_procs = graph.toposort();

        if(!result.dl_procs.empty()) { //dl_procs isn't empty, there's a deadlock
            result.edge_index = i; //override the index
            break;
        }
    }

    return result;
}