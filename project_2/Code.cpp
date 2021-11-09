// Include standard librarie and queue
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <queue>
// #include <bits/stdc++.h>
using namespace std;

//Global Variables
char next_process_name = 'A';
class performance_metrics
{
    public:
    float response_time;
    float turnaround_time;
    float waiting_time;
    performance_metrics()
    {
        response_time = 0;
        turnaround_time = 0;
        waiting_time = 0;
    }
};

class process
{
    public:
    float remaining_time;
    float arrival_time;
    float service_time;
    int priority;
    int job_start_time;
    bool job_started;       // Ryan added this
    char process_name;
    performance_metrics metrics;
    process()
    {
        arrival_time = (rand() % 100);// will return numbetween 0 and 99
        service_time = (rand() % 11);//will return num between 0 and 10
        remaining_time = service_time;
        job_started = false;
        if (service_time == 0)
            service_time += 1;// service_time = 1 .. 10
        priority = (rand() % 5);
        if (priority == 0)
            priority += 1;// priority between 1 .. 4
        process_name = next_process_name;
        next_process_name++;
    }
};

bool processComparator(process p1, process p2)
{
    return (p1.arrival_time < p2.arrival_time);
}

void createProcessList(vector<process> &process_list, int extra_processes = 0)
{
    process_list.clear();
    next_process_name = 'A';

    int processes_count = min(10 + extra_processes, 26); //generating 10 by default, adding extra processes if requested with the limit of maximum 26 processes.

    cout << "\npopulating new processes..." << endl;

    for(int i=0; i<processes_count; ++i)
    {
        process p;
        process_list.push_back(p);
    }
    sort(process_list.begin(), process_list.end(), processComparator);
}

void printProcessList(vector<process> &process_list)
{
    cout << "Processes Name  " << "Arrival Time   " << " Expected Run Time   " << "Priority" << endl;
    for(int process_index = 0; process_index<process_list.size(); ++process_index)
        cout << process_list[process_index].process_name << "\t\t"
             << process_list[process_index].arrival_time << "\t\t"
             << process_list[process_index].service_time << "\t\t    "
             << process_list[process_index].priority << endl;
}

void printTimeChart(vector<char> &time_chart)
{
    cout << "Time Chart : [ ";
    for(int chart_index = 0; chart_index<time_chart.size(); ++chart_index)
        cout << time_chart[chart_index];
    cout << " ]" << endl;
}

void calculateAndPrintPerformanceMetrics( vector<vector<process> > &workloads)
{
    // average metrics for all the workloads.
    float avg_turnaround_time = 0.0;
    float avg_response_time = 0.0;
    float avg_waiting_time = 0.0;

    for(int workload_index = 0; workload_index < workloads.size(); ++workload_index)
    {
        // for current workload
        int total_turnaround_time = 0;
        int total_response_time = 0;
        int total_waiting_time = 0;

        int processes_completed = 0;
        vector<process> &process_list = workloads[workload_index];
        for(int process_index = 0; process_index<process_list.size(); ++process_index)
        {
            if(process_list[process_index].metrics.turnaround_time == 0)
                continue;
            ++processes_completed;
            total_turnaround_time += process_list[process_index].metrics.turnaround_time;       
            total_waiting_time += process_list[process_index].metrics.waiting_time;
            total_response_time += process_list[process_index].metrics.response_time;
        }

        cout << "Performance metrics for workload " <<  workload_index+1 << " :" << endl;
        cout << "\tAverage Turnaround Time : " << (1.0*total_turnaround_time)/processes_completed << endl;
        avg_turnaround_time += (1.0*total_turnaround_time)/processes_completed;
        cout << "\tAverage Waiting Time : " << (1.0*total_waiting_time)/processes_completed << endl;
        avg_waiting_time += (1.0*total_waiting_time)/processes_completed;
        cout << "\tAverage Response Time : " << (1.0*total_response_time)/processes_completed << endl;
        avg_response_time += (1.0*total_response_time)/processes_completed;
    }
    cout << "Average Performance metrics for all workload :" << endl;
    cout << "Average Turnaround Time : " << avg_turnaround_time/workloads.size() << endl;
    cout << "Average Waiting Time : " << avg_waiting_time/workloads.size() << endl;
    cout << "Average Response Time : " << avg_response_time/workloads.size() << endl;
}

bool firstComeFirstServe(vector<process> &process_list, vector<char> &time_chart)
{
    int process_index = 0;
    float remaining_service_time = 0;
    int current_cpu_consecutive_idle_time = 0;
    int max_cpu_consecutive_idle_time = 0;

    for(int quanta=0; quanta<100 || remaining_service_time>0; ++quanta)
    {
        if(remaining_service_time == 0 && process_list[process_index].arrival_time <= quanta)
        {
            remaining_service_time = process_list[process_index].service_time;
            //updating performance metrics
            process_list[process_index].metrics.response_time = quanta - process_list[process_index].arrival_time;
        }

        if(remaining_service_time > 0)
        {
            time_chart.push_back(process_list[process_index].process_name);
            --remaining_service_time;
            if(remaining_service_time == 0)
            {
                ++process_index;
                // updating performance metrics
                process_list[process_index].metrics.turnaround_time = quanta+1 - process_list[process_index].arrival_time;
                process_list[process_index].metrics.waiting_time = process_list[process_index].metrics.turnaround_time - process_list[process_index].service_time;
            }
            current_cpu_consecutive_idle_time = 0;
        }
        else
        {
            time_chart.push_back('-');
            ++current_cpu_consecutive_idle_time;
            max_cpu_consecutive_idle_time = max(max_cpu_consecutive_idle_time, current_cpu_consecutive_idle_time);
        }
    }
    return max_cpu_consecutive_idle_time < 2 ? true : false;
}

// Round Robin Scheduling Algorithm Implementation.
bool runRoundRobin(vector<process> &process_list, vector<char> &time_chart, vector<vector<process> > &workloads){
    int timeQuanta = 0;
    int process_index = 0;
    int current_cpu_consecutive_idle_time = 0;
    int max_cpu_consecutive_idle_time = 0;
    
    // Initialize queue populated with sorted process list.
    queue<process> requestQueue;
    vector<process> metric_list;
    // Run round robin until all processes are completed or the quanta is greater than or equal 100.
    while(timeQuanta < 100 || !requestQueue.empty()){ 
        
        while (process_index < process_list.size() && process_list[process_index].arrival_time <= timeQuanta && timeQuanta < 100){
            requestQueue.push(process_list[process_index]);
            (requestQueue.front()).metrics.response_time = timeQuanta - (requestQueue.front()).arrival_time;
            process_index++;
        }

        // If there are still processes, run the processes and update the values.
        if(!requestQueue.empty()){
            if(!(requestQueue.front()).job_started){
                (requestQueue.front()).job_start_time = timeQuanta;
                (requestQueue.front()).job_started = true;
                
            }
        }
        
        // Reduce the service time by 1 quanta
        if(!requestQueue.empty()){
            (requestQueue.front()).remaining_time -= 1;
            time_chart.push_back(requestQueue.front().process_name);
        
        
            // Check if the process needs to be placed at the back of the queue.
            if((requestQueue.front()).remaining_time > 0){ 
                // If it hasn't been completed, push the process back into the queue.            
                process tmp = requestQueue.front();
                requestQueue.pop();
                requestQueue.push(tmp);

                // Reset CPU consecutive idle time.
                current_cpu_consecutive_idle_time = 0;
            }
            else{
                // Update the turnaround and waiting time.
                cout << (requestQueue.front().job_start_time) << endl;
                (requestQueue.front()).metrics.turnaround_time = timeQuanta - (requestQueue.front().job_start_time);
                (requestQueue.front()).metrics.waiting_time = (requestQueue.front()).metrics.turnaround_time - (requestQueue.front()).service_time;
                metric_list.push_back(requestQueue.front());
                requestQueue.pop();
            }
        }
        else{
            time_chart.push_back('-');
            ++current_cpu_consecutive_idle_time;
            max_cpu_consecutive_idle_time = max(max_cpu_consecutive_idle_time, current_cpu_consecutive_idle_time);
        }
        timeQuanta++;
    }
    if(max_cpu_consecutive_idle_time < 2){
        workloads.push_back(metric_list);
    }

    // Returns whether cpu consecutive idle time exceeds 2.
    return max_cpu_consecutive_idle_time < 2 ? true : false;
}


int main()
{
    cout << "Choose One of the Process Scheduling Algorithm:" << endl;
    cout << "1. First-come first-served (FCFS) [non-preemptive]" << endl;
    cout << "2. Shortest job first (SJF) [non-preemptive]" << endl;
    cout << "3. Shortest remaining time (SRT) [preemptive]" << endl;
    cout << "4. Round robin (RR) [preemptive]" << endl;
    cout << "5. Highest priority first (HPF) [preemptive]" << endl;
    cout << "6. Highest priority first (HPF) [non-preemptive]" << endl;

    int algorithm_selected;
    cin >> algorithm_selected;

    vector<process> process_list;
    vector<vector<process> > workloads;
    int unsuccessful_count = 0;
    int successful_count = 0;
    bool successful = false;

    vector<char> time_chart;

    switch(algorithm_selected)
    {
        case 1: while(successful_count<5)
                {
                    createProcessList(process_list, unsuccessful_count*2);
                    time_chart.clear();
                    successful = firstComeFirstServe(process_list, time_chart);                   
                    
                    if(!successful)
                    {
                        cout << "Simulation failed because CPU is idle for more than 2 consecutive quanta." << endl;
                        printTimeChart(time_chart);
                        ++unsuccessful_count;
                    }
                    else
                    {
                        ++successful_count;
                        cout << "Simulation successful." << endl;
                        printProcessList(process_list);
                        printTimeChart(time_chart);
                        workloads.push_back(process_list);
                    }
                }
                calculateAndPrintPerformanceMetrics(workloads);
                break;
            case 4: while(successful_count<5)
                {
                    createProcessList(process_list, unsuccessful_count*2);
                    time_chart.clear();
                    successful = runRoundRobin(process_list, time_chart, workloads);                   
                    
                    if(!successful)
                    {
                        cout << "Simulation failed because CPU is idle for more than 2 consecutive quanta." << endl;
                        printTimeChart(time_chart);
                        ++unsuccessful_count;
                    }
                    else
                    {
                        ++successful_count;
                        cout << "Simulation successful." << endl;
                        printProcessList(process_list);
                        printTimeChart(time_chart);
                        // workloads.push_back(process_list);
                    }
                }
                calculateAndPrintPerformanceMetrics(workloads);
                break;
        default : cout << "Invalid Input" << endl;
    }
    
    return 0;
}