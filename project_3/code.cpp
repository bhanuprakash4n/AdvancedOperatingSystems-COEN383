#include <stdio.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <algorithm>

using namespace std;

//The concert class contains 2 2D arrays. The "seats" array is used to tell
//which seller sold the given seat, and the other 2D array "customerSeats"
//is used to tell us which customer number is in the given seat. When we 
//create a Concert object, we initialize all the values of both 2D arrays 
//to -1 and set the number of seats remaning to 100 (since all the seats
//of the 100 seat concert are empty).
class Concert 
{
    public:
    int seats[ 10 ][ 10 ];
    int seatsRemaining;
    //used for printing
    int customerSeats[ 10 ][ 10 ];

    Concert()
    {
        for ( int i = 0; i < 10; i++ )
        {
            for( int j = 0; j < 10; j++ )
            {
                seats[ i ][ j ] = -1; 
                customerSeats[ i ][ j ] = -1;
            }
        }
        seatsRemaining = 100;
    }
};

//The Customer class contains the randomized arrival and service time of the customer
//as well as the response and turnaround time (assuming the customer is assigned a seat).
//The startTime is when the customer started being assigned a seat and the endTime is used
//to calculate the throughput of a given seller (since we have to know when a given seller
//finished selling their last customer in line).
//The inQueue variable is used to describe whether a given customer has been served or not
//or is still in a seller queue waiting to be sold a ticket to the concert. The finished
//boolean is used to tell which sellers have been sold a ticket and is used to calculate
//the metrics for each of the given seller types. Finally, the customerID is used to
//differentiate each customer waiting in each of the sellers' queues.
class Customer {
    public:
    int arrivalTime;
    int serviceTime;
    float turnaroundTime;
    float responseTime;
    int startTime;
    int endTime;
    bool inQueue;
    bool finished;
    int customerId;

    Customer()
    {
        arrivalTime = rand() % 60; //set arrivalTime to be a random integer between 0 and 59
        inQueue = false; //since the customer does not start in a seller's queue, set it as false
        responseTime = -1;
        turnaroundTime = -1;
        finished = false; ///since the customer has not been sold a ticket, set "finished" as false
        startTime = -1;
        endTime = -1;
    }
};

//In order to sort customers in each of the seller's queues, we need
//a function in order to compare their arrival times with each other
//so we know who to serve first.
bool customerCompare( Customer* x, Customer* y )
{
    return x->arrivalTime < y->arrivalTime;
}

//A seller object has a type (High, Medium or Low) which is a single
//character value (H, M, or L). Then we have each seller's queue, which
//is a vector object that holds multiple "customer" objects inside. 
//Each seller is given an ID number, which is a single digit value between
//0 and 6.
class Seller {
    public:
    char sellerType;
    vector< Customer* > customerQueue;
    int sellerId;
    Seller( char sType, int sellId, int customers )
    {
        sellerType = sType;
        sellerId = sellId;
        for ( int i = 0; i < customers; i ++ )
        {
            customerQueue.push_back( new Customer() );
            //initialize service time according to the seller type
            if ( sellerId == 0 )
                customerQueue[ i ]->serviceTime = rand() % 2 + 1; //For the high seller, the service time is 1 or 2 min.
            else if ( sellerId < 4 )
                customerQueue[ i ]->serviceTime = rand() % 3 + 2; //For the medium seller, the service time is 2-4 min.
            else
                customerQueue[ i ]->serviceTime = rand() % 4 + 4; //For the medium seller, the service time is 4-7 min.
        }
        //Sort a seller's queue by arrival time (so customers that arrive the earliest are at the front of the queue)
        sort( customerQueue.begin(), customerQueue.end(), customerCompare );
        //Set a given customer's idea as an integer that is one more than the previous customer's ID number.
        for ( int i = 0; i < customers; i++ )
        {
            customerQueue[ i ]->customerId = i + 1;
        }
    }
};

//Global variables used with the mutex and condition locks.
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t concertMutex= PTHREAD_MUTEX_INITIALIZER;

//Here we initialize a concert object.
Concert concert;
int current_time = -1;

//This function handles printing out the 10x10 matrix of concert seats.
//If a concert seat is empty, the function prints out a dash line, otherwise
//the function prints out the type of seller (H,M,L) and the seller's ID number
//followed by the customer's ID number.
void printConcert()
{
    cout << "CONCERT SEATING CHART" << endl;
    for ( int i = 0; i < 10; i++ )
    {
        for ( int j = 0; j < 10; j++ )
        {
            if ( concert.seats[ i ][ j ] == -1)
                cout << " || - ||";
            else if ( concert.seats[ i ][ j ] == 0 )
            {                
                cout << " |H";
                cout << concert.seats[ i ][ j ];
                if ( concert.customerSeats[ i ][ j ] < 10 )
                {
                    cout << "0";
                }
                cout << concert.customerSeats[ i ][ j ] << "| ";
                
            }
            else if ( concert.seats[ i ][ j ] < 4 )
            {
                cout << " |M";
                cout << concert.seats[ i ][ j ];
                if ( concert.customerSeats[ i ][ j ] < 10 )
                {
                    cout << "0";
                }
                cout << concert.customerSeats[ i ][ j ] << "| ";
            }
            else
            {
                cout << " |L";
                cout << concert.seats[ i ][ j ] - 3;
                if ( concert.customerSeats[ i ][ j ] < 10 )
                {
                    cout << "0";
                }
                cout << concert.customerSeats[ i ][ j ] << "| ";
            }
        }
        cout << endl;
    }
}

//This is a helper function for the findAndAllocateSeat function. 
//This function checks all the seats in a given row of the concert, and 
//if it finds an empty seat in a given row, it returns that seat index.
int findSeatInRow ( int row )
{
    int index = -1;
    for ( int i = 0; i < 10 ; i ++ )
    {
        if ( concert.seats[ row ][ i ] == -1 )
        {
            index = i;
            return index;
        }
    }
    return index;
}

//This function finds a free seat for a given seller. This function
//should only be called when a thread has the mutex lock.
//If a free seat was found, we allocate it for the given customer
//and return true. Since each seller has to start at a different row
//we used an array of integers for each seller type, and we move 
//through each of the rows in order until we find a free seat in
//a given row.
bool findAndAllocateSeat( int id, int custId )
{
    //For high sellers (with ID = 0).
    if ( id == 0 )
    {
        int rowOrder[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        //Look through all 10 rows of the concert in the above order.
        for ( int i = 0; i < 10; i++ )
        {
            int row = rowOrder[ i ];
            //Find a seat in the given row, if not, then return false
            int seat = findSeatInRow( row );
            if( seat != -1 )
            {
                concert.seats[ row ][ seat ] = id; //This tells us which seller sold the seat to the customer.
                concert.customerSeats[ row ][ seat ] = custId; //This tells us which customer in a seller's queue was sold the seat.
                concert.seatsRemaining--; //Since a seat is no longer free, we decremnet seatsRemaning by 1 to keep track whether the concert is full or not.
                return true;
            }
        }
    }
    else if ( id < 4 )
    {
        int rowOrder[] = { 4, 5, 3, 6, 2, 7, 1, 8, 0, 9 };
        for ( int i = 0; i < 10; i++ )
        {
            int row = rowOrder[ i ];
            int seat = findSeatInRow( row );
            if( seat != -1 )
            {
                concert.seats[ row ][ seat ] = id;
                concert.customerSeats[ row ][ seat ] = custId;
                concert.seatsRemaining--;
                return true;
            }
        }
    }
    else {
        int rowOrder[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
        for ( int i = 0; i < 10; i++ )
        {
            int row = rowOrder[ i ];
            int seat = findSeatInRow( row );
            if( seat != -1 )
            {
                concert.seats[ row ][ seat ] = id;
                concert.customerSeats[ row ][ seat ] = custId;
                concert.seatsRemaining--;
                return true;
            }
        }
    }
    return false;
}

//This is the function that each of the seller threads execute. 
//to serve one time slice (1 minute)
void *sell( void *arg )
{
    //Convert the argument of the "sell" function to a pointer to a "seller" object.
    Seller *seller = (Seller *) arg;
    bool servicing = false;
    //check if there are currently customers in the event queue
    while ( current_time < 60 || servicing )
    {
        pthread_mutex_lock( &concertMutex);
        pthread_cond_wait(&cond, &concertMutex);

        //When a customer arrives at a given seller's queue, we print out the current time
        //as well as the seller's ID number along with the customer's ID number.
        for ( int i = 0; i < seller->customerQueue.size(); i++ )
        {
            if ( seller->customerQueue[ i ]->arrivalTime <= current_time && !seller->customerQueue[ i ]->inQueue && 
                !seller->customerQueue[ i ]->finished )
            {
                if ( seller->sellerId > 4 )
                    cout << current_time <<  ":\tCustomer " << seller->customerQueue[ i ]->customerId << " has arrived at the end of seller " 
                        << seller->sellerType << seller->sellerId - 3 << "'s queue" << endl;
                else
                    cout << current_time <<  ":\tCustomer " << seller->customerQueue[ i ]->customerId << " has arrived at the end of seller " 
                        << seller->sellerType << seller->sellerId << "'s queue" << endl;
                seller->customerQueue[ i ]->inQueue = true;
            }
        }

        //This for loop is where we service all of the customers of a given seller.
        for ( int i = 0; i < seller->customerQueue.size(); i++ )
        {
            //If the customer is not in the queue, but has finished being served, we have to go to the next customer in line.
            if ( !seller->customerQueue[ i ]->inQueue && seller->customerQueue[ i ]->finished )
            {
                continue;
            }
            else if ( seller->customerQueue[ i ]->inQueue )
            {
                //If service time is 0, this means that the customer has completed the process of 
                //getting a seat, and now may be assigned a seat.
                if ( seller->customerQueue[ i ]->serviceTime == 0 )
                {
                    //If we have found a seat for the customer, we print out the current time, the seller's ID
                    //number as well as the customer's ID number. Then we calculate the turnaround time 
                    //and set the booleans finished to true (since we have found a seat for the customer)
                    //and the boolean inQueue to false (since we are done serving the customer, they are no longer
                    //in the seller's queue). Then we print the concert seating chart to show the addition we have made.
                    if ( findAndAllocateSeat( seller->sellerId, seller->customerQueue[ i ]->customerId ) )
                    {
                        if ( seller->sellerId >= 4 )
                            cout << current_time <<  ":\tA seat was sold by seller " << seller-> sellerType << seller->sellerId - 3;
                        else
                            cout << current_time <<  ":\tA seat was sold by seller " << seller->sellerType << seller->sellerId;
                        cout << " to customer " << seller->customerQueue[ i ]->customerId << endl;
                        //turnaround time for when the customer gets the seat
                        seller->customerQueue[ i ]->turnaroundTime = current_time - seller->customerQueue[ i ]->arrivalTime;
                        seller->customerQueue[ i ]->finished = true;
                        seller->customerQueue[ i ]->inQueue = false;
                        seller->customerQueue[ i ]->endTime = current_time; 
                        servicing = false;
                        printConcert();
                    }

                    //Otherwise, if there are no empty seats, this means that the concert is full and we need to turn 
                    //everyone still waiting in a seller's queue away. Print out all the customers that were turned 
                    //away from a given seller.
                    else
                    {
                        //Also set end time here so we can keep track of the final throughput for each seller type.
                        if ( seller->sellerId >= 4 )
                            for(int j = i; j < seller->customerQueue.size(); j++)
                            {
                                cout << current_time <<  ":\tThe conert is sold out, " << "customer " << seller->customerQueue[ j ]->customerId << " of seller " << seller-> sellerType << seller->sellerId -3 << " was rejected " << endl;
                            }
                        else
                            for(int j = i; j < seller->customerQueue.size(); j++)
                            {
                                cout << current_time <<  ":\tThe conert is sold out, " << "customer " << seller->customerQueue[ j ]->customerId << " of seller " << seller-> sellerType << seller->sellerId << " was rejected " << endl;
                            }
                        seller->customerQueue[ i ]->endTime = current_time; 
                        pthread_mutex_unlock(&concertMutex);
                        return NULL;
                    }
                }
                else
                {
                    //If this is the first time a customer is serviced, we need to set the 
                    //response time and the start time for that given customer. Now the 
                    //customer can be served in the future.
                    if ( seller->customerQueue[ i ]->responseTime == -1 )
                    {
                        seller->customerQueue[ i ]->responseTime = current_time - 
                            seller->customerQueue[ i ]->arrivalTime;
                        seller->customerQueue[ i ]->startTime = current_time;
                        servicing = true;
                    }
                    //If the customer has already started being service by a seller, then 
                    //we decrease the seller's service time by 1 to act as though time 
                    //has passed for one minute.
                    seller->customerQueue[ i ]->serviceTime--;
                }
                break;
            }
        }
        pthread_mutex_unlock(&concertMutex);
    }
    return NULL; // thread exits
    
}

//This function wakes up all of the seller threads for them to start servicing
//customers.
void wakeup_all_seller_threads()
{
    pthread_mutex_lock(&concertMutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&concertMutex);
}

//This function prints all the statistical information on a per seller type 
//basis.
void printMetrics( vector< Seller* > sellers)
{
    float avgTurn = 0;
    float turn = 0;
    float resp = 0;
    float avgResp = 0;
    float avgThroughput = 0;
    float endTime = 0;
    float customers = 0;
    float totalCustomersServed = 0;
    float totalCustomers = 0;
    for( int i = 0; i < sellers.size(); i ++ )
    {
        customers = 0;
        turn = 0;
        resp = 0;
        for ( int j = 0; j < sellers[ i ]->customerQueue.size(); j++ )
        {
            if ( sellers[ i ]->customerQueue[ j ]->finished )
            {
                turn += sellers[ i ]->customerQueue[ j ]->turnaroundTime;
                resp += sellers[ i ]->customerQueue[ j ]->responseTime;
                customers++;
                //endtime is set this way since customerQueue is organized by arrivalTime
                //therefore the last finished customer has the largest endtime value
                endTime = sellers[ i ]->customerQueue[ j ]->endTime;
                totalCustomersServed++;
            }
            totalCustomers++;
        }
        avgTurn += turn / customers;
        avgResp += resp / customers;
        avgThroughput += customers / endTime;
        if( i == 0)
        {
            cout << "Metrics for seller type " << sellers[i]->sellerType << endl;
            cout << "Average Turnaround Time: " << avgTurn << endl;
            cout << "Average Response Time: " << avgResp << endl;
            cout << "Average Throughput: " << avgThroughput << endl;
            cout << "Customers Served: " << totalCustomersServed << endl;
            cout << "Total Customers: " << totalCustomers << endl;
            cout << "Average Customers Turned Away: " << totalCustomers - totalCustomersServed << endl;
            cout << endl;
            avgTurn = 0;
            avgResp = 0;
            avgThroughput = 0;
            totalCustomersServed = 0;
            totalCustomers = 0;   
        }
        else if( i == 3)
        {
            cout << "Metrics for seller type " << sellers[i]->sellerType << endl;
            cout << "Average Turnaround Time: " << avgTurn / 3 << endl;
            cout << "Average Response Time: " << avgResp / 3 << endl;
            cout << "Average Throughput: " << avgThroughput / 3 << endl;
            cout << "Customers Served: " << totalCustomersServed << endl;
            cout << "Total Customers: " << totalCustomers << endl;
            cout << "Average Customers Turned Away: " << ( totalCustomers - totalCustomersServed ) / 3 << endl;
            cout << endl;
            avgTurn = 0;
            avgResp = 0;
            avgThroughput = 0;
            totalCustomersServed = 0;
            totalCustomers = 0;
        }
        else if( i == 9 )
        {
            cout << "Metrics for seller type " << sellers[i]->sellerType << endl;
            cout << "Average Turnaround Time: " << avgTurn / 6 << endl;
            cout << "Average Response Time: " << avgResp / 6 << endl;
            cout << "Average Throughput: " << avgThroughput / 6 << endl;
            cout << "Customers Served: " << totalCustomersServed << endl;
            cout << "Total Customers: " << totalCustomers << endl;
            cout << "Average Customers Turned Away: " << ( totalCustomers - totalCustomersServed ) / 6 << endl;
            cout << endl;
            avgTurn = 0;
            avgResp = 0;
            avgThroughput = 0;
            totalCustomersServed = 0;
            totalCustomers = 0;
        } 
    }

}

int main( int argc, char* argv[] )
{
    int i;
    pthread_t tids[10];
    char seller_type;
    int customers = atoi( argv[ 1 ] );   
    // Create necessary data structures for the simulator.
    // Create customer list for each seller ticket queue based on the
    // N value within an hour and have them in the seller queue.
    // Create 10 threads representing the 10 sellers.
    vector< Seller* > sellers;
    seller_type = 'H';
    sellers.push_back (  new Seller( seller_type, 0, customers ) );
    pthread_create( &tids[0], NULL, sell, sellers[0] );
    seller_type = 'M';
    for (i = 1; i < 4; i++)
    {
        usleep( 1000 );
        sellers.push_back( new Seller( seller_type, i, customers ) );
        pthread_create(&tids[i], NULL, sell , sellers[i] );
    }
    seller_type = 'L';
    for (i = 4; i < 10; i++)
    {
        usleep( 1000 );
        sellers.push_back( new Seller( seller_type, i, customers ) );
        pthread_create(&tids[i], NULL, sell, sellers[i] );
    }
    // wakeup all seller threads
    //concert only goes to time 60, but loop goes longer in case customers are in progress of being serviced
    for ( current_time = -1; current_time < 120; current_time++ )
    {
        usleep( 5000 );
        wakeup_all_seller_threads();
    }
    //wait for all seller threads to finish their work.
    for (i = 0 ; i < 10 ; i++)
    {
        pthread_join( tids[i], NULL );
    }
    cout << endl;
    printConcert();
    cout << endl;
    printMetrics( sellers );    
    return 0;
}