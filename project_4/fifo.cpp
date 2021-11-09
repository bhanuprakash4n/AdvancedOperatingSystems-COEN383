#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <random>
#include <algorithm>
using namespace std;

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
int remainingFreePages = 100;
processNode* processHead;
int timeStamp = 0;
//cant use queue, need to use vector so we can remove from middle of list
vector<int> fifoCache;
float hits = 0;
float misses = 0;
float swaps = 0;
vector<int> memMap;



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


//allocates a page to the process
//removes the current head of the free pages adds that as the head of the pages a process owns 
//tail of processPageHead is always the first process inserted into the processPageHead list
//MUST HAVE FREE PAGES
void allocatePage( pageNode** processPageHead, int id, int pageId )
{
    //cout << "FREE PAGES BEFORE STARTING ALLOCATION" << endl;
    // for ( pageNode* x = freePageHead; x != NULL; x = x->next)
    // {
    //     cout << x->frameId << endl;
    // }
    pageNode* tempHead = freePageHead;
    //shows that the process 
    memMap[ freePageHead->frameId ] = id;
    if(freePageHead == NULL)
        cout << "NULL" <<endl;
    //if there is only one free page
    if ( tempHead->next == NULL)
    {    
        tempHead = new pageNode;
        tempHead->frameId = freePageHead->frameId;
        freePageHead = NULL;
    }
    else{
        freePageHead = freePageHead->next;
    }
    pageNode* hi = *processPageHead;
    // cout << "PAGE LIST BEFORE ALLOCATION" << endl;
    // for ( pageNode* x = hi; x != NULL; x = x->next)
    // {
    //     cout << x->frameId << endl;
    // }
    //keeping track of the page order
    fifoCache.push_back( tempHead->frameId );
    cout << endl;
    
    cout << "PAGE " << tempHead->frameId << " ALLOCATED TO " << id << endl;
    tempHead->processPageId = pageId;
    tempHead->usingProcessId = id;
    tempHead->next = *processPageHead;
    // cout << "PAGE LIST AFTER ALLOCATION" << endl;
    // for ( pageNode* x = tempHead; x != NULL; x = x->next)
    // {
    //     cout << x->frameId << endl;
    // }
    //tempHead becomes the new head of the process's pages
    *processPageHead = tempHead;
    remainingFreePages--; 
    // cout << "FREE PAGES AFTER ALLOCATION" << endl;
    // for ( pageNode* x = freePageHead; x != NULL; x = x->next)
    // {
    //     cout << x->frameId << endl;
    // }
}

//this should only be called when there are no free pages currently in the freePageHead
void findAndRemovePage()
{
    //iterate through each process
    for( processNode* curProcess = processHead; curProcess != NULL; curProcess = curProcess->next )
    {
        //cout << "LOOKING FOR" << fifoCache.front() << endl;
        pageNode* head = curProcess->pageHead;
        pageNode* tail = head;
        if( head == NULL )
        {
            continue;
        }
        else if( head->next == NULL )
        {
            if( head->frameId == fifoCache.front() )
            {
                cout << "ONE NODE" << endl;
                cout << "DELETING THIS: " << fifoCache.front() << endl;
                //this means the page list is only 1 page long
                //make freePageHead  a copy of the head
                freePageHead = new pageNode;
                freePageHead->frameId = head->frameId;
                freePageHead->usingProcessId = head->usingProcessId;
                cout << "One Page: ";
                cout << "\tPage "<< freePageHead->frameId;
                cout << "\tEvicted from Process " << freePageHead->usingProcessId << endl;  
                freePageHead->usingProcessId = -1;
                freePageHead->processPageId = -1;
                fifoCache.erase( fifoCache.begin() );
                remainingFreePages++;
                //does this work or do I need to check
                curProcess->pageHead = NULL;

                return;
            }
        }
        else
        {
            tail = head->next;
            //while we are not at the tail
            while( tail->next != NULL)
            {
                tail = tail->next;
                head = head->next;
            }
            if (tail->frameId == fifoCache.front())
            {
                cout << "MULTIPLE NODES" << endl;
                cout << "DELETING THIS: " << fifoCache.front() << endl;
                head->next = NULL;
                freePageHead = tail;
                //do this work lol
                cout << "Multiple Pages Owned: ";
                cout << "\tPage "<< freePageHead->frameId;
                cout << "\tEvicted from Process " << freePageHead->usingProcessId << endl; 
                fifoCache.erase( fifoCache.begin() );
                remainingFreePages++;
                return;
            }
        }
    }
}

//makes a copy of the pages a process has, and then appends it to the free page list
//works perfectly from what I can tell
void removePagesFromProcess( pageNode** processPageHead )
{
    pageNode* tempHead = new pageNode;
    pageNode* tempTail = tempHead;
    //making a copy of every page in the process so I can then allocate
    for ( pageNode* cur = *processPageHead; cur != NULL; cur = cur->next )
    {
        //also need to remove each frame from the cache
        fifoCache.erase( remove( fifoCache.begin(), fifoCache.end(), cur->frameId ), fifoCache.end());
        memMap[ cur->frameId ] = -1;
        tempTail->frameId = cur->frameId;
        tempTail->next = cur->next;
        tempTail->processPageId = -1;
        tempTail->usingProcessId = -1;
        remainingFreePages++;
        tempTail = tempTail->next;
    }
    pageNode* temp = freePageHead;
    
    // cout << " REMOVING PAGES FROM FINISHED PROCESS CURRNET FREE" << endl;
    // for ( pageNode* x = freePageHead; x != NULL; x = x->next)
    // {
    //     cout << x->frameId << endl;
    // }
    // cout << "WHAT WE APPENDING " << endl;
    // for ( pageNode* x = tempHead; x != NULL; x = x->next)
    // {
    //     cout << x->frameId << endl;
    // }
    if ( temp == NULL )
    {
        freePageHead = tempHead;
    }
    else
    {
        //iterate until at the tail
        while ( temp->next != NULL )
            temp = temp->next;
        temp->next = tempHead;
        //cout << "has at least one node" << endl;
    }
    // cout << "CHECKING END STATUS" << endl;
    // for ( pageNode* x = freePageHead; x != NULL; x = x->next)
    // {
    //     cout << x->frameId << endl;
    // }
    *processPageHead = NULL;
}

void printMemMap()
{
    for( int i = 0; i < memMap.size() ; i++ )
    {
        if ( i % 10 == 0 && i != 0 )
            cout << endl;
        if ( memMap[ i ] == - 1 )
            cout << "|.|";
        else
            cout << "|" << memMap[ i ] <<  "|";
        
    }
    cout << endl;
}
void fifo( int ref )
{
    
    //processNode* process = ( processNode* ) curProcess;
    //lock on every time update
    //1000 msec in a second, 60,000 msec in a minute
    //updates every 100 msec, so 60,000/100 = 6000 updates
    //1 update = 100 msec
    while( timeStamp < 600 )
    {
        for ( processNode* cur = processHead; cur != NULL; cur = cur->next)
        {
            //if a process hasn't started yet, we add it to the list, and allocate pages
            //each second we will check for a new process to run
            if ( timeStamp % 10 == 0 )
            {
                //cout << "one second" << endl;
                //adding process to the queue if there are enough remaining pages
                if ( cur->startTime == -1 && remainingFreePages >= 4 && cur->arrivalTime <= timeStamp / 10 )
                {
                    cur->startTime = timeStamp / 10;
                    cout << "Time: " << cur->startTime << ".0";
                    cout << "\tProcess ID: " << cur->processId;
                    cout << "\tEntered ";
                    cout << "\tSize: " << cur->memorySize;
                    cout << "\tService Time: " << cur->serviceTime << endl;
                    //also need to print the memory map here, not sure what that will look like yet
                    cur->startTime = timeStamp;
                    allocatePage( &cur->pageHead, cur->processId, 0 );
                    //need this for random page function
                    cur->previousPageId = 0;
                    swaps++;
                    printMemMap();
                    //after allocating page, we consider this the 0th iteration, so we dont want to make a memory reference
                    continue;
                }
                
                //decrementing service time every second
                if( cur->serviceTime > 0 && cur->startTime != -1 )
                {
                    cur->serviceTime--;
                    if ( cur->serviceTime == 0)
                    {
                        cout << "Time: " << timeStamp / 10 << ".0";
                        cout << "\tProcess ID: " << cur->processId;
                        cout << "\thas finished " << endl;
                        //removeALLPAGES;
                        printMemMap();
                        removePagesFromProcess( &cur->pageHead );
                        //set process as finished, take all of the pages away and put it back to free list
                        cur->finished = true;
                    }
                }
            }
            //check if the process is running
            if( cur->finished == true)
            {
                continue;
            }
            else if ( cur->startTime >= 0 )
            {
                //cout << "memory access" << endl;
                //make a memory access 
                int pageId = getRandomPage( cur->previousPageId, cur->memorySize );
                //need to check if  pageId exists in the process
                bool inMemory = false;
                for( pageNode* pageTracker = cur->pageHead; pageTracker != NULL; pageTracker = pageTracker->next)
                {
                    if ( pageTracker->processPageId == pageId )
                        inMemory = true;
                }
                //cout << "after check in memory" << endl;
                //if the page was in memory, i.e. a process has it, we dont have to do anything
                if ( inMemory == true )
                {
                    cur->previousPageId = pageId;
                    cout << "Time: " << timeStamp / 10 << "." << timeStamp % 10;
                    cout << "\tProcess ID: " << cur->processId;
                    cout << "\tReferenced Page in Memory:  " << pageId;
                    cout << "\tNo Page Evicted " << endl; 
                    hits++;
                }
                else
                {
                    misses++;
                    cout << "Time: " << timeStamp / 10 << "." << timeStamp % 10;
                    cout << "\tProcess ID: " << cur->processId;
                    cout << "\tReferenced Page Not in Memory:  " << pageId;
                    //if page not in memory, check if there are any free pages
                    //this do not get called because many free pages
                    if ( remainingFreePages == 0 )
                    {
                        //no free pages, so we do FIFO
                        //something wrong with this function
                        cout << endl;
                        cout << "OUT OF FREE PAGES" << endl;
                        findAndRemovePage();
                        //cout << freePageHead->frameId << endl;
                        allocatePage( &cur->pageHead, cur->processId, pageId );
                    }
                    else
                    {
                        //there are free pages, so freely allocate page
                        //cout << "PREALLOCATE WITH FREE" << endl;
                        allocatePage( &cur->pageHead, cur->processId, pageId );
                        //cout << "HERE" << endl;
                        cout << "\tFree Page Added to Process " << endl;
                    }
                }
                ref --;
                if ( ref == 0 )
                {
                    //after 100 page references we finish
                    return;
                }
            }
            else
            {
                //end the for loop when we get to a process that isnt running or finished
                //if no memory references at this time, we will push back a -1 to print as a hole later
                break;
            }     
        }
        timeStamp++;
    }
    return;
}

int main()
{
    for (int i = 0; i < 5; i ++ )
    {
        memMap.clear();
        processHead = generateJobsAndSort( 150 );
        freePageHead = generateFreePageList( 100 );
        fifoCache.clear();
        timeStamp = 0;
        fifo( -1);
    }
    float hitRate = hits/ ( hits + misses);
    float averageRef = ( hits + misses ) / 5;
    float avgSwaps = swaps / 5;
    
    int references = 100;
    memMap.clear();
    processHead = generateJobsAndSort( 150 );
    freePageHead = generateFreePageList( 100 );
    fifoCache.clear();
    timeStamp = 0;
    fifo( references );
    printMemMap();
    cout << "Hit Rate over 5 runs: " << hitRate << endl;
    cout << "Average Number of Page References: " << averageRef << endl;
    cout << "Average Number of Processes Swapped In: " << avgSwaps << endl;
    

    return 0;
}