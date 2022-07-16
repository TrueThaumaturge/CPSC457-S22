#include "getDirStats.h"
#include "digester.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <algorithm>
#include <set>
#include <cstring>

std::unordered_map<std::string,int> words_hist;
std::unordered_map<std::string,int> types_hist;
std::unordered_map<std::string,std::vector<std::string>> hash_hist;
constexpr int MAX_WORD_SIZE = 1024;

int
get_file_size(const std::string & path) {
    struct stat stats;
    if(stat(path.c_str(), &stats) == -1) { //stores stuff in stats structure
        printf("ERROR OBTAINING STATS");
        exit(1);
    }
    else 
        return (int)stats.st_size;
}

//CODE SNIPPET MODIFIED FROM https://gitlab.com/cpsc457/public/popen-example/
std::string
get_file_type(const std::string & path) {
    std::string cmd = "file -b " + path;
    FILE * fp = popen( cmd.c_str(), "r");
    if( fp == nullptr) {
        printf("popen() failed, quitting\n");
        exit(-1);
    }

    std::string ftype;
    char buff[4096];
    char * res = fgets(buff, sizeof(buff), fp);
    pclose(fp);
    // try to parse the buffer
    if( res != nullptr) {
        // find the end of the first field ('\0', or '\n' or ',')
        int eol = 0;
        while(buff[eol] != ',' && buff[eol] != '\n' && buff[eol] != 0) eol ++;
        // terminate the string
        buff[eol] = 0;
        // remember the type
        ftype = buff;
    } else {
        // file(1) did not produce any output... no idea why, so let's
        // just skip the file, but make the file type something bizare
        ftype = "file(1) failed, not sure why";
    }
    return ftype;
}

//CODE SNIPPET MODIFIED FROM https://gitlab.com/cpsc457/public/word-histogram
// returns the next word from stdin
//     - word is a sequence of characters [a-zA-Z]
//     - the word is converted into lower case
//     - words are separated by any non alphabetic characters
//     - return empty string on EOF
//    example:
//     "     Hello..    World:) !" --> ["hello", "world", ""]
std::string
next_word(FILE* fp)
{
    std::string result;
    while(1) {
        int c = fgetc(fp); //MODIFIED LINE - reads char by char from file pointer
        if(c == EOF) break;
        c = tolower(c);
        if(! isalpha(c)) {
            if(result.size() == 0)
            continue;
            else
            break;
        }
        else {
            if(result.size() >= MAX_WORD_SIZE) {
            printf("input exceeded %d word size, aborting...\n", MAX_WORD_SIZE);
            exit(-1);
            }
            result.push_back(c);
        }
    }
    return result;
}

static bool is_dir(const std::string & path)
{
    struct stat buff;
    if (0 != stat(path.c_str(), &buff)) return false;
    return S_ISDIR(buff.st_mode);
}


// getDirStats() computes stats about directory a directory
//         root_dir = name of the directory to examine
//         n = how many top words/filet types/groups to report

Results getDirStats(const std::string & root_dir, int n)
{
    Results res;
    //initializes values
    res.all_files_size = 0;
    res.n_files = 0;
    res.n_dirs = 0;
    res.largest_file_path = ""; //defaults to empty string if no files
    res.largest_file_size = -1; //defaults to -1 if no files

    if (! is_dir(root_dir)) return res; // if input directory isn't correct, returns empty results

    // FOLLOWING SNIPPET MODIFIED FROM
    // https://gitlab.com/cpsc457/public/find-empty-directories/-/blob/master/myfind.cpp
    // SNIPPET Recursively examines a directory, finding and printing all files
    std::vector<std::string> stack;
    stack.push_back(root_dir.c_str()); //adds base directory to stack to start
    while (! stack.empty()) {
        auto dirname = stack.back();
        stack.pop_back();

        printf("%s\n", dirname.c_str());
        if(is_dir(dirname)) { //if the current name is a directory and not a file
            if(strcmp(root_dir.c_str(), dirname.c_str()) != 0) //explicit!
                res.n_dirs++; //increments directory counter
        }
        else //if the current name is a file, we need to obtain all needed info
        {
            
            res.n_files++; //increments file counter

            FILE* fp;
            fp = fopen(dirname.c_str(), "r"); //opens file for reading

            if(fp != NULL) { //if the file pointer's valid, let's read it
                //reads all words in file and stores in words_hist
                while(1) {
                    auto w = next_word(fp);
                    if(w.size() == 0) //if there are no more words, break the loop
                        break;
                    else if(w.size() >= 3) //if word is 3 chars or greater (requirement), adds it to the histogram
                        words_hist[w] ++;
                }
                fclose(fp); //closes the file when we're done
            }
            else {
                printf("ERROR: Couldn't read file\n"); //shouldn't happen
            }

            //check file size - add to total sum, check if largest
            int bytes = get_file_size(dirname);
            res.all_files_size += bytes; //adds size to total sum
            if(bytes > res.largest_file_size) { //if it's larger, we record it
                res.largest_file_size = bytes;
                res.largest_file_path = dirname;
            }

            //use popen file utility to obtain file type
            std::string ftype = get_file_type(dirname);
            types_hist[ftype]++; //adds a count to the file type hashmap

            //calculate hash
            std::string hash = sha256_from_file(dirname);
            if(hash_hist.count(hash)) { //if the hash is already in the histogram
                hash_hist[hash].push_back(dirname); //add the file name to the list of duplicates
            }
            else { //otherwise this is the first occurence
                std::vector<std::string> filevect;
                filevect.push_back(dirname); //make and put a name in a vector
                hash_hist[hash] = filevect; //and put the vector in the hashmap
            }
            
        }

        //opens directory stream and uses it to navigate to other files/directories within the current directory
        DIR * dir = opendir(dirname.c_str());
        if (dir) {
            while (1) {
                dirent * de = readdir(dir);
                if (! de) break;
                std::string name = de->d_name;
                if (name == "." || name == "..") continue;
                std::string path = dirname + "/" + de->d_name;
                stack.push_back(path);
            }
            closedir(dir);
        }
    }

    //We have gathered all the information, now we need to process it
    //most common words and types need to be sorted and cropped

    // first we place the words and counts into array (with count
    // negative to reverse the sort)
    std::vector<std::pair<int,std::string>> arr1;
    for(auto & h : words_hist)
        arr1.emplace_back(-h.second, h.first);
    // if we have more than N entries, we'll sort partially, since
    // we only need the first N to be sorted
    if(arr1.size() > size_t(n)) {
        std::partial_sort(arr1.begin(), arr1.begin() + n, arr1.end());
        // drop all entries after the first n
        arr1.resize(n);
    } else {
        std::sort(arr1.begin(), arr1.end());
    }
    //now we insert these into the results vector
    for(auto & p : arr1) {
        res.most_common_words.emplace_back(p.second, -p.first);
    }

    std::vector<std::pair<int,std::string>> arr2;
    for(auto & h : types_hist)
        arr2.emplace_back(-h.second, h.first);
    // if we have more than N entries, we'll sort partially, since
    // we only need the first N to be sorted
    if(arr2.size() > size_t(n)) {
        std::partial_sort(arr2.begin(), arr2.begin() + n, arr2.end());
        // drop all entries after the first n
        arr2.resize(n);
    } else {
        std::sort(arr2.begin(), arr2.end());
    }
    //now we insert these into the results vector
    for(auto & p : arr2) {
        res.most_common_types.emplace_back(p.second, -p.first);
    }
    
    //duplicates - uses second approach
    std::set<std::pair<int,std::vector<std::string>>> mmq; //min/max queue
    for(auto & h : hash_hist) {
        if(h.second.size() > 1) //if duplicates were found
            mmq.emplace(-(h.second.size()), h.second); //puts in a pair that includes the # of duplicates and the array of file names
        if(mmq.size() > size_t(n)) //if the queue is too big, dump the lowest
            mmq.erase(std::prev(mmq.end()));
    } 
    for(auto & v : mmq) { //pulls just the string vectors and puts them in the duplicate files
        res.duplicate_files.push_back(v.second);
    }
    
    //everything done!!!
    return res;
}