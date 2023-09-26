/* 046267 Computer Architecture - Winter 20/21 - HW #2 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;


// Function to extract the set bits from the address
unsigned getSet(unsigned address, int tagSize, int offsetSize)
{
    unsigned tmp = address;
    tmp = tmp >> offsetSize;
    tmp = tmp << offsetSize ;
    tmp = tmp << tagSize;
    tmp = tmp >> offsetSize;
    tmp = tmp >>tagSize;
    return tmp;
}

// Function to extract the tag bits from the address
unsigned getTag(unsigned address, int tagSize)
{
    unsigned tmp = address;
    int tmp2 = (32 - tagSize);
    tmp = tmp >> tmp2;
    return tmp;
}

class way{
public:
    unsigned address;
    unsigned tag;
    bool valid;
    bool dirty;
    way():address(0),tag(0),valid(false),dirty(false){}
};

class row{
public:
    way* ways;
    long int* LRU;
    long int waysNumber;
    row(){}
    void submitWays(long int wayNumber){
        ways = new way[wayNumber];
        LRU = new long int[wayNumber];
        for (long int i = 0; i < wayNumber; ++i) {
            LRU[i] = i;
        }
        this->waysNumber = wayNumber;
    };
    void updateLRU(long int index){
        long int tmp = LRU[index];
        LRU[index] = waysNumber -1;
        for (long int i = 0; i < waysNumber; ++i) {
            if(i!= index && LRU[i] > tmp) LRU[i]--;
        }
    }

    long int getLRU(){
        for (long int i = 0; i < waysNumber; ++i) {
            if(LRU[i] ==0) return i;
        }
        return 0;
    }

};



class cache{
public:
    row* lines;
    long int wayNumber;
    unsigned clkCycle;
    int hit;
    int access;
    unsigned tagSize,setSize,offsetSize;

    cache(int rowNumber,long int waysNumber, unsigned blksize, unsigned clk){
        lines = new row[rowNumber];
        for (int i = 0; i < rowNumber; ++i) {
            lines[i].submitWays(waysNumber);
        }
        wayNumber = waysNumber;
        clkCycle = clk;
        hit = 0;
        access = 0;
        setSize = log2(rowNumber);
        offsetSize = blksize;
        tagSize = 32- offsetSize - setSize;
    }

    // Function to find the data in the cache and update LRU
    long int findData(unsigned address, bool& dirty);

    // Function to update the cache with new data and handle replacements
    int updateData(unsigned address,unsigned& removedAddress,bool writeMode,bool& dirty);

    // Function to update the dirty bit to true for a given address in the cache
    void updateDirtyBitToTrue(unsigned address);

    // Function to mark a given address as invalid in the cache
    void markAddressUnValid(unsigned address);

};

// Function to find the data in the cache and update LRU
long int cache::findData(unsigned address, bool& dirty) {
    unsigned tag = getTag(address,tagSize);
    unsigned set = getSet(address,tagSize,offsetSize);
    for(long int i = 0; i< wayNumber; i++)
    {
        if(lines[set].ways[i].valid && lines[set].ways[i].tag == tag){
            lines[set].updateLRU(i);
            if (lines[set].ways[i].dirty == true){
                dirty = true;
            }
            return i;
        }
    }
    return -1;
}


int cache::updateData(unsigned address, unsigned& removedAddress,bool writeMode, bool& dirty)
{
    unsigned tag = getTag(address,tagSize);
    unsigned set = getSet(address,tagSize,offsetSize);

    for(long int i = 0; i< wayNumber; i++)
    {
        if(lines[set].ways[i].valid == false)
        {
            lines[set].ways[i].valid = true;
            lines[set].ways[i].tag = tag;
            if(writeMode){
                lines[set].ways[i].dirty = true;
            }else{
                dirty ? lines[set].ways[i].dirty = true : lines[set].ways[i].dirty = false;
            }
            lines[set].ways[i].address = address;
            lines[set].updateLRU(i);
            return 0; // in case we found an empty place return 0 because we didn't delete anything
        }
    }
    long int leasUsed = lines[set].getLRU();
    bool oldDirty = lines[set].ways[leasUsed].dirty;
    removedAddress = lines[set].ways[leasUsed].address;
    lines[set].ways[leasUsed].address = address;
    lines[set].ways[leasUsed].tag = tag;

    if(writeMode){
        lines[set].ways[leasUsed].dirty = true;
    }else{
        dirty ? lines[set].ways[leasUsed].dirty = true : lines[set].ways[leasUsed].dirty = false;
    }
    lines[set].ways[leasUsed].valid = true;
    lines[set].updateLRU(leasUsed);

    if (oldDirty == true){ // in case we deleted dirty data from L1, update dirty in L2
        return 3;
    }
    return -2; // in case we deleted data from L2 should delete it from L1 too
}

void cache::updateDirtyBitToTrue(unsigned int address)
{
    unsigned tag = getTag(address,tagSize);
    unsigned set = getSet(address,tagSize,offsetSize);
    for(long int i = 0; i< wayNumber; i++)
    {
        if(lines[set].ways[i].valid == true && lines[set].ways[i].tag == tag)
        {
            lines[set].ways[i].dirty = true;
            lines[set].updateLRU(i);
        }
    }

}

void cache::markAddressUnValid(unsigned int address)
{
    unsigned tag = getTag(address,tagSize);
    unsigned set = getSet(address,tagSize,offsetSize);

    for(long int i = 0; i< wayNumber; i++)
    {
        if(lines[set].ways[i].valid == true && lines[set].ways[i].tag == tag)
        {
            lines[set].ways[i].valid = false;
            lines[set].ways[i].dirty = false;
        }
    }
}



class cacheSim {
public:
    cache* L1Cache;         // Pointer to L1 cache object
    cache* L2Cache;         // Pointer to L2 cache object
    unsigned wrAlloc;       // Write allocation flag
    unsigned memCyc;        // Memory access cycles
    int totalTime;          // Total execution time
    double access;          // Total number of memory accesses

    cacheSim(int rowNumber1, long int wayNumber1,unsigned blksize1, unsigned clk1,
             int rowNumber2, long int wayNumber2,unsigned blksize2, unsigned clk2
            ,unsigned WRALLOC, unsigned memAccess):
            L1Cache(new cache(rowNumber1,wayNumber1,blksize1,clk1)),
            L2Cache(new cache(rowNumber2,wayNumber2,blksize2,clk2)),
            wrAlloc(WRALLOC), memCyc(memAccess),access(0),totalTime(0){}

    // Method for reading data from the cache
    void readData(unsigned address) {
        access++;
        L1Cache->access++;
        totalTime += L1Cache->clkCycle;
        bool tmp = false;
        if (L1Cache->findData(address, tmp) != -1) {
            L1Cache->hit++;
        } else {
            L2Cache->access++;
            totalTime += L2Cache->clkCycle;
            bool dirty = false;
            if (L2Cache->findData(address,dirty) != -1) {
                L2Cache->hit++;
                unsigned deletedAddress=1;
                int oldDirty = L1Cache->updateData(address, deletedAddress, false,dirty);
                if (oldDirty != 0 && oldDirty != -2)//dirty in L1 update in L2){}
                {
                    L2Cache->updateDirtyBitToTrue(deletedAddress);
                }
            } else {
                totalTime += memCyc;
                unsigned deletedAddress=1;
                int deleted = L2Cache->updateData(address, deletedAddress,false,tmp);
                if (deleted != 0)//if deleted from L2 delete from L1)
                {
                    L1Cache->markAddressUnValid(deletedAddress);
                }

                int oldDirty = L1Cache->updateData(address, deletedAddress,false,tmp);
                if (oldDirty != 0 && oldDirty != -2)//dirty in L1 update in L2){}
                {
                    L2Cache->updateDirtyBitToTrue(deletedAddress);
                }
            }
        }
    }

    // Method for writing data to the cache
    void writeData(unsigned address) {
        L1Cache->access++;
        access++;
        totalTime += L1Cache->clkCycle;
        unsigned L1set = getSet(address, L1Cache->tagSize, L1Cache->offsetSize);
        bool dirty = false;
        long int indexInL1 = L1Cache->findData(address, dirty);
        if (indexInL1 != -1) {
            L1Cache->hit++;
            L1Cache->lines[L1set].ways[indexInL1].valid = true;
            L1Cache->lines[L1set].updateLRU(indexInL1);
            L1Cache->lines[L1set].ways[indexInL1].dirty = true;
        } else {
            // if WA and data in L2 we should add data to L1
            L2Cache->access++;
            totalTime += L2Cache->clkCycle;
            unsigned L2set = getSet(address, L2Cache->tagSize, L2Cache->offsetSize);
            long int indexInL2 = L2Cache->findData(address,dirty);
            if (indexInL2 != -1) {
                L2Cache->lines[L2set].ways[indexInL2].dirty = true;
                L2Cache->lines[L2set].updateLRU(indexInL2);
                L2Cache->hit++;
                if (wrAlloc == 1) {
                    unsigned oldAddress=1;
                    int tmp = L1Cache->updateData(address, oldAddress,true,dirty);
                    if(tmp!= 0 && tmp != -2){
                        L2Cache->updateDirtyBitToTrue(oldAddress);
                    }
                }
            } else {
                totalTime += memCyc;
                if (wrAlloc == 1) {
                    unsigned oldAddress=1;
                    int deleted = L2Cache->updateData(address, oldAddress,true,dirty);
                    if (deleted != 0)//if deleted from L2 delete from L1)
                    {
                        L1Cache->markAddressUnValid(oldAddress);
                    }
                    int tmp = L1Cache->updateData(address, oldAddress,true,dirty);
                    if(tmp!= 0 && tmp != -2){
                        L2Cache->updateDirtyBitToTrue(oldAddress);
                    }
                }
            }
        }
    }
};





int main(int argc, char **argv) {

    if (argc < 19) {
        cerr << "Not enough arguments" << endl;
        return 0;
    }

    // Get input arguments

    // File
    // Assuming it is the first argument
    char* fileString = argv[1];
    ifstream file(fileString); //input file stream
    string line;
    if (!file || !file.good()) {
        // File doesn't exist or some other error
        cerr << "File not found" << endl;
        return 0;
    }

    unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
            L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

    for (int i = 2; i < 19; i += 2) {
        string s(argv[i]);
        if (s == "--mem-cyc") {
            MemCyc = atoi(argv[i + 1]);
        } else if (s == "--bsize") {
            BSize = atoi(argv[i + 1]);
        } else if (s == "--l1-size") {
            L1Size = atoi(argv[i + 1]);
        } else if (s == "--l2-size") {
            L2Size = atoi(argv[i + 1]);
        } else if (s == "--l1-cyc") {
            L1Cyc = atoi(argv[i + 1]);
        } else if (s == "--l2-cyc") {
            L2Cyc = atoi(argv[i + 1]);
        } else if (s == "--l1-assoc") {
            L1Assoc = atoi(argv[i + 1]);
        } else if (s == "--l2-assoc") {
            L2Assoc = atoi(argv[i + 1]);
        } else if (s == "--wr-alloc") {
            WrAlloc = atoi(argv[i + 1]);
        } else {
            cerr << "Error in arguments" << endl;
            return 0;
        }
    }


    int rowNum1 = pow(2,L1Size)/(pow(2,BSize)*pow(2,L1Assoc));
    int rowNum2 = pow(2,L2Size)/(pow(2,BSize)*pow(2,L2Assoc));
    long int waynum1 = pow(2,L1Assoc);
    long int waynum2 = pow(2,L2Assoc);

    cacheSim* simulator = new cacheSim(rowNum1,waynum1,BSize,L1Cyc
            ,rowNum2,waynum2,BSize,L2Cyc,
                                       WrAlloc,MemCyc);


    while (getline(file, line)) {

        stringstream ss(line);
        string address;
        char operation = 0; // read (R) or write (W)
        if (!(ss >> operation >> address)) {
            // Operation appears in an Invalid format
            cout << "Command Format error" << endl;
            return 0;
        }



        string cutAddress = address.substr(2); // Removing the "0x" part of the address


        unsigned long int num = 0;
        num = strtoul(cutAddress.c_str(), NULL, 16);
        if(operation == 'r')
        {
            simulator->readData(num);
        }

        if(operation == 'w')
        {
            simulator->writeData(num);
        }
    }

    double L1MissRate;
    double L2MissRate;
    double avgAccTime;

    L1MissRate = 1 - double(simulator->L1Cache->hit) / simulator->L1Cache->access;

    L2MissRate = 1 - double(simulator->L2Cache->hit) / simulator->L2Cache->access;

    avgAccTime = (simulator->totalTime/simulator->access);

    printf("L1miss=%.03f ", L1MissRate);
    printf("L2miss=%.03f ", L2MissRate);
    printf("AccTimeAvg=%.03f\n", avgAccTime);

    return 0;
}