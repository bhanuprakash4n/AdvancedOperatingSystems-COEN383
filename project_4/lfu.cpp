#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <random>
#include <algorithm>
#include <climits>

using namespace std;

#define NUM_PAGES 100
#define NUM_JOBS 150

struct pageNode {
    //frame ID is physical memory frame ID, so we can keep track of where pages are going
    int frameId;
    //processPageId/virtualPageID is per process from 0 to memorySize
    int processPageId;
    // usingProcessId is the processId of the process using this page
    int usingProcessId;
    pageNode* next;

    pageNode() : frameId(-1),
                 processPageId(-1),
                 usingProcessId(-1),
                 next(NULL){}
};

struct processNode {
    int memorySize;
    int serviceTime;
    int processId;
    int arrivalTime;
    int startTime;
    int previousPageId;
    bool finished;
    processNode* next;
    pageNode* pageHead;

    processNode() : arrivalTime((rand() % 60)),
                    startTime(-1),
                    next(NULL),
                    pageHead(NULL),
                    finished(false),
                    previousPageId(-1){}
};

//Global Variables
pageNode* freePageHead;
// int remainingFreePages = 100;
int remainingFreePages = 2;
processNode* processHead;
int timeStamp = 0;
//cant use queue, need to use vector so we can remove from middle of list
vector<int> fifoCache;
float hits = 0;
float misses = 0;
float swaps = 0;
vector<int> memMap;
// LFU and MFU
vector<int> pageCounter(NUM_PAGES);

//will use for FIFO
processNode* mergeSort(processNode* head)
{
    //base condition
    if(!head || !(head->next)) return head;

    //find mid of linked list
    processNode* slowPtr = head;
    processNode* fastPtr = head;
    while(fastPtr->next && fastPtr->next->next)
    {
        slowPtr = slowPtr->next;
        fastPtr = fastPtr->next->next;
    }

    //returns head pointer of sorted linked list
    processNode* head2 = mergeSort(slowPtr->next);
    slowPtr->next = NULL;
    processNode* head1 = mergeSort(head);

    processNode* tempHead = new processNode();
    head = tempHead;
    //merging two sorted linked lists
    while(head1 && head2)
    {
        if(head1->arrivalTime <= head2->arrivalTime)
        {
            head->next = head1;
            head1 = head1->next;
        }
        else
        {
            head->next = head2;
            head2 = head2->next;
        }
        head = head->next;
    }
    if(head1)
        head->next = head1;
    else if(head2)
        head->next = head2;
    head = tempHead->next;
    free(tempHead);
    return tempHead->next;
}

//will use for FIFO
processNode* generateJobsAndSort( int jobs )
{
    int mem[ 4 ] = { 5, 11, 17, 31 };
    int service[ 5 ] = { 1, 2, 3, 4, 5 };

    processNode* newProcessNode;
    processNode* head = NULL;
    //create unsorted process list 
    for( int i = 0; i < jobs; i++ )
    {
        newProcessNode = new processNode();
        newProcessNode->processId = i;
        // evenly distributing memory size, service time to all the created processes
        newProcessNode->memorySize = mem[ i%4 ];
        newProcessNode->serviceTime = service[ i%5 ];
        if(!head) head = newProcessNode;
        else{
            newProcessNode->next = head;
            head = newProcessNode;
        }
    }
    return mergeSort(head);
}

//will use for FIFO
pageNode* generateFreePageList( int pages )
{
    pageNode* newPageNode;
    pageNode* head = NULL;
    pageNode* tail = head;
    for ( int i = 0; i < pages; i++ )
    {
        //iniatilize memMap to 
        memMap.push_back( -1 );
        newPageNode = new pageNode();
        newPageNode->frameId = i;
        if(!head)
        { 
            head = newPageNode;
            tail = head;
        }
        else{
            tail->next = newPageNode;
            tail = tail->next;
        }
    }
    remainingFreePages = pages;
    return head;
}

//returns process id of the next page we want to reference
int getRandomPage(int prevPage, int memorySize)
{
    // resultant random page using "locality of reference" algorithm.
    int randomPage;
    int randNum = rand() % 10;
    int deltaI;
    //values from 0-6
    if ( randNum < 7 )
    {
        //values from 0 - 2
        deltaI = rand() % 3;
        //values from -1 to 1
        deltaI = deltaI - 1;
        //need to wraparound to the memorySize - 1
        if ( prevPage + deltaI == - 1 )
        {
            randomPage = memorySize - 1;
        }
        else
        {
            //dont need to wraparound
            randomPage = prevPage + deltaI;
        }
    }
    else
    {
        //if randNum 7-9
        //random page should be from 0 <= randomPage <= prevPage - 2 or
        //random page should be prevPage + 2 <= randomPage <= memorySize - 1
        // rand() % ( max - min + 1 ) + min;
        // max and min inclusivie
        int binaryRand = rand() % 2;
        if( binaryRand == 1 )
        {
            //need to check if prevPage -2 + 1 <= 0
            //if it is, we can't use rand(), so get a new random number
            if ( ( prevPage - 2 + 1 )<= 0 )
            {
                getRandomPage( prevPage, memorySize );
            }
            else
            {
                //values from 0 to prevPage - 2
                randomPage = ( rand() % ( prevPage - 2 + 1) );
            }
        }
        else{
            //check if rand value is valid
            if( ((memorySize - 1) - (prevPage + 2)) + 1 <= 0 )
            {
                getRandomPage( prevPage, memorySize );
            }
            else
            {
            //values from prevPage + 2 to memorySize-1 inclusive
            randomPage = ( rand()%((memorySize - 1) - (prevPage + 2) + 1)) + prevPage + 2;
            }
        }
    }
    return randomPage;
}

// allocatePage()
void allocatePage(pageNode** processPageHead, int id, int pageId) {
    // args are &cur->pageHead, cur->processId, 0
    pageNode* tempHead = freePageHead;
    memMap[freePageHead->frameId] = id;
    if(freePageHead == NULL) {
        cout << "NULL" << endl;
    }
    if(tempHead->next == NULL) {
        tempHead = new pageNode;
        tempHead->frameId = freePageHead->frameId;
        freePageHead = NULL;
    }
    else {
        freePageHead = freePageHead->next;
    }
    pageCounter[tempHead->frameId]++;
    cout << endl;

    cout << "PAGE " << tempHead->frameId << " ALLOCATED TO " << id << endl;
    tempHead->processPageId = pageId;
    tempHead->usingProcessId = id;
    tempHead->next = *processPageHead;

    *processPageHead = tempHead;
    remainingFreePages--;
}

void findAndRemovePage() {
    int smallest_ref = INT_MAX;
    int lfu;
    // cout << "ARRAY" << endl;
    for(int i = 0; i < NUM_PAGES; i++) {
        // cout << "page " << i << " count: " << pageCounter[i] << endl; 
        if(pageCounter[i] < smallest_ref) {
            smallest_ref = pageCounter[i];
            lfu = i;
        }
    }

    cout << "Page " << lfu << " Evicted from Process " << memMap[lfu] << endl;
    // cout << "PAGE TO REMOVE IS " << lfu << endl;
    // cout << "IN PROCESSS " << memMap[lfu] << endl;

    for(processNode* curProcess = processHead; curProcess != NULL; curProcess = curProcess->next) {
        // cout << "Process " << curProcess->processId << endl;
        if(curProcess->pageHead == NULL) {
            continue;
        }
        if(curProcess->processId != memMap[lfu]) {
            continue;
        }
        // cout << "PAGE LINKED LIST B4 REMOVAL" << endl;
        // for(pageNode* x = curProcess->pageHead; x != NULL; x = x->next) {
        //     cout << "PAGE " << x->frameId << endl;
        // }
        // check if linked list is of size 1
        if(curProcess->pageHead->next == NULL) {
            // check if the node has frameId with value lfu
            if(curProcess->pageHead->frameId == lfu) {
                curProcess->pageHead = NULL;
                freePageHead = new pageNode;
                freePageHead->frameId = lfu;
                freePageHead->usingProcessId = -1;
                freePageHead->processPageId = -1;
                remainingFreePages++;
                pageCounter[lfu] = 0;
                return;
                // PROBABLY HAVE TO DO OTHER THINGS HERE AS WELL
            }
        }
        else {
            // check if head of linked list has frameId with value lfu
            if(curProcess->pageHead->frameId == lfu) {
                curProcess->pageHead = curProcess->pageHead->next;
                freePageHead = new pageNode;
                freePageHead->frameId = lfu;
                freePageHead->usingProcessId = -1;
                freePageHead->processPageId = -1;
                remainingFreePages++;
                pageCounter[lfu] = 0;
                return;
                // PROBABLY HAVE TO DO OTHER THINGS HERE
            }
            else {
                // iterate through the linked list and see if there is a node with frameId = lfu
                for(pageNode* curPage = curProcess->pageHead; curPage != NULL; curPage = curPage->next) {
                    if(curPage->next->frameId == lfu) {
                        curPage->next = curPage->next->next;
                        freePageHead = new pageNode;
                        freePageHead->frameId = lfu;
                        freePageHead->usingProcessId = -1;
                        freePageHead->processPageId = -1;
                        remainingFreePages++;
                        pageCounter[lfu] = 0;
                        return;
                        // PROBABLY HAVE TO DO OTHER THINGS HERE
                    }
                }
            }
        }
    }
}

void removePagesFromProcess(pageNode** processPageHead) {
    pageNode* tempHead = new pageNode;
    pageNode* tempTail = tempHead;

    for(pageNode* cur = *processPageHead; cur != NULL; cur = cur->next) {
        memMap[cur->frameId] = -1;
        pageCounter[cur->frameId] = 0;
        tempTail->frameId = cur->frameId;
        tempTail->next = cur->next;
        tempTail->processPageId = -1;
        tempTail->usingProcessId = -1;
        remainingFreePages++;
        tempTail = tempTail->next;
    }
    pageNode* temp = freePageHead;

    if(temp == NULL) {
        freePageHead = tempHead;
    }
    else {
        while(temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = tempHead;
    }

    *processPageHead = NULL;
}

void printMemMap() {
    for(int i = 0; i < memMap.size(); i++) {
        if(i%10 == 0 && i != 0) {
            cout << endl;
        }
        if(memMap[i] == -1) {
            cout << "|.|";
        }
        else {
            cout << "|" << memMap[i] << "|";
        }
    }
    cout << endl;
}


void lfu(int ref) {
    while(timeStamp < 600) {
        for(processNode* cur = processHead; cur != NULL; cur = cur->next) {
            if(timeStamp % 10 == 0) {
                if(cur->startTime == -1 && remainingFreePages >= 4 && cur->arrivalTime <= timeStamp / 10) {
                    cur->startTime = timeStamp / 10;
                    cout << "Time: " << cur->startTime << ".0";
                    cout << "\tProcess ID: " << cur->processId;
                    cout << "\tEntered ";
                    cout << "\tSize: " << cur->memorySize;
                    cout << "\tService Time: " << cur->serviceTime << endl;
                    cur->startTime = timeStamp;
                    allocatePage(&cur->pageHead, cur->processId, 0);
                    // return;
                    cur->previousPageId = 0;
                    swaps++;
                    printMemMap();
                    continue;
                }
                if(cur->serviceTime > 0 && cur->startTime != -1) {
                    cur->serviceTime--;
                    if(cur->serviceTime == 0) {
                        cout << "Time: " << timeStamp / 10 << ".0";
                        cout << "\tProcess ID: " << cur->processId;
                        cout << "\thas finished " << endl;
                        // printMemMap();
                        // cout << "PAGE LINKED LIST B4 REMOVAL" << endl;
                        // for(pageNode* x = cur->pageHead; x != NULL; x = x->next) {
                        //     cout << "PAGE " << x->frameId << endl;
                        // }
                        removePagesFromProcess(&cur->pageHead);
                        printMemMap();
                        // cout << "PAGE LINKED LIST AFTER REMOVAL" << endl;
                        // for(pageNode* x = cur->pageHead; x != NULL; x = x->next) {
                        //     cout << "PAGE " << x->frameId << endl;
                        // }
                        cur->finished = true;
                    }
                }
            }
            if(cur->finished == true) {
                continue;
            }
            else if(cur->startTime >= 0) {
                int pageId = getRandomPage(cur->previousPageId, cur->memorySize);
                bool inMemory = false;
                for(pageNode* pageTracker = cur->pageHead; pageTracker != NULL; pageTracker = pageTracker->next) {
                    // cout << "processPageId: " << pageTracker->processPageId << "\tpageId: " << pageId << endl;
                    if(pageTracker->processPageId == pageId) {
                        inMemory = true;
                    }
                }
                if(inMemory == true) {
                    cur->previousPageId = pageId;
                    cout << "Time: " << timeStamp / 10 << "." << timeStamp % 10;
                    cout << "\tProcess ID: " << cur->processId;
                    cout << "\tReferenced Page in Memory: " << pageId;
                    cout << "\tNo Page Evicted " << endl;
                    hits++;
                }
                else {
                    misses++;
                    cout << "Time: " << timeStamp / 10 << "." << timeStamp % 10;
                    cout << "\tProcess ID: " << cur->processId;
                    cout << "\tReferenced Page Not in Memory: " << pageId;
                    if(remainingFreePages == 0) {
                        cout << endl;
                        cout << "OUT OF FREE PAGES" << endl;
                        // cout << "B4 PAGE REMOVAL" << endl;
                        // for(pageNode* x = cur->pageHead; x != NULL; x = x->next) {
                        //     cout << "PAGE " << x->frameId << endl;
                        // }
                        findAndRemovePage();
                        // cout << "AFTER PAGE REMOVAL" << endl;
                        // for(pageNode* x = cur->pageHead; x != NULL; x = x->next) {
                        //     cout << "PAGE " << x->frameId << endl;
                        // }
                        allocatePage(&cur->pageHead, cur->processId, pageId);
                    }
                    else{
                        allocatePage(&cur->pageHead, cur->processId, pageId);
                        cout << "Free Page Added to Process " << endl;
                        printMemMap();
                    }
                }
                ref--;
                if(ref == 0) {
                    return;
                }
            }
            else {
                break;
            }
        }
        timeStamp++;
    }
    return;
}

int main() {
    // processHead = generateJobsAndSort(NUM_JOBS);
    // freePageHead = generateFreePageList(NUM_PAGES);

    // // for(processNode* x = processHead; x != NULL; x = x->next) {
    // //     cout << x->processId << endl;
    // //     cout << "Mem size: " << x->memorySize << " ";
    // //     cout << "Service time: " << x->serviceTime << " ";
    // //     cout << "Arrival time: " << x->arrivalTime << endl;
    // // }

    // // cout << endl;   

    // // for(pageNode* x = freePageHead; x != NULL; x = x->next) {
    // //     cout << x->frameId << endl;
    // //     cout << "processPageId: " << x->processPageId << " ";
    // //     cout << "usingProcessId: " << x->usingProcessId << endl;
    // // }

    // // cout << endl;
    // // cout << "Simulation" << endl;
    // // cout << endl;

    // lfu(-1);

    for(int i = 0; i < 5; i++) {
        memMap.clear();
        fill(pageCounter.begin(), pageCounter.end(), 0);
        processHead = generateJobsAndSort(NUM_JOBS);
        freePageHead = generateFreePageList(NUM_PAGES);
        timeStamp = 0;
        lfu(-1);
    }

    float hitRate = hits / (hits+ misses);
    float averageRef = (hits + misses) / 5;
    float avgSwaps = swaps / 5;

    int references = 100;
    memMap.clear();
    processHead = generateJobsAndSort(NUM_JOBS);
    freePageHead = generateFreePageList(NUM_PAGES);
    fill(pageCounter.begin(), pageCounter.end(), 0);
    timeStamp = 0;
    lfu(references);
    // lfu(-1);
    printMemMap();

    cout << "Hit Rate over 5 runs: " << hitRate << endl;
    cout << "Average Number of Page References: " << averageRef << endl;
    cout << "Average Number of Processes Swapped In: " << avgSwaps << endl;

    return 0;
}