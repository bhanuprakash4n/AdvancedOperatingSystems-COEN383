#include <stdio.h> 
#include <stdlib.h> 
#include <fcntl.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/time.h> 
#include <sys/ioctl.h> 
#include <string.h>
#include <time.h>
#include <stdbool.h>
 
#define BUFFER_SIZE 100000
#define READ_END     0 
#define WRITE_END    1 

int start_sec;
char* getTimeStamp()
{
    struct timeval tv;
    int sec;
    long milliseconds;
    char *time_s = (char *)malloc(40);
    gettimeofday (&tv, NULL);
    sec = (int) (tv.tv_sec); // seconds
    sec -= start_sec; // removing start seconds to get referenced seconds
    milliseconds = ((tv.tv_usec) / 1000); // micro/1000 -> milli seconds
    sprintf (time_s, "0:0:%d.%03ld", sec, milliseconds); // format 0:0:s.ms
    return time_s;
}
int main()
{
    //buffers for reading from and writing to pipe. 
    char write_msg[BUFFER_SIZE];
    char read_msg[BUFFER_SIZE]; 

    int* fd[5];
    for(int i=0; i<5 ;i++)
    {
        fd[i] = (int*)malloc(2*sizeof(int));
        // Create the pipe. 
        if (pipe(fd[i]) == -1) 
        { 
            fprintf(stderr,"pipe() failed"); 
            return 1; 
        } 
    }
    //for(int i=0; i<5 ;i++)
    //    printf("%d %d\n", fd[i][0], fd[i][1]);
    struct timeval tv;
    gettimeofday (&tv, NULL);
    // setting start_sec global variable which is used in getTimeStamp for absolute time.
    start_sec = (int) (tv.tv_sec); 

    // pid's of child processess
    pid_t *pid = (pid_t*)malloc(5*sizeof(pid_t));
    for(int i=0;i<5; i++)
    {
        pid[i] = fork();
        if(pid[i] == 0)
        {
            //close(0);
            // Close the unused READ end of the pipe.
            close(fd[i][READ_END]); 
            
            if(i != 4)
            {
                long long int message_count = 0;
                while(true)
                {
                    char *time_stamp = getTimeStamp();
                    sprintf(write_msg, "%s :Child %d message %lld\n", time_stamp, i+1, ++message_count);
                    free(time_stamp);

                    // Write to the WRITE end of the pipe. 
                    write(fd[i][WRITE_END], write_msg, strlen(write_msg)+1);

                    int sleep_time = rand()%3;
                    sleep(sleep_time);
                }
            }
            else // 5th special case
            {
                char user_input[100] = {0};
                while(true)
                {   
                    int nread;
                    ioctl(0,FIONREAD,&nread);
                    if (nread != 0)
                    {
                        nread = read(0,user_input,nread); 
                        user_input[nread] = '\0'; 
                    }
                    fgets(user_input, 100, stdin);
                    char *time_stamp = getTimeStamp();
                    sprintf(write_msg, "%s :Child %d message: %s\n", time_stamp, i+1, user_input);
                    free(time_stamp);

                    // Write to the WRITE end of the pipe. 
                    write(fd[i][WRITE_END], write_msg, strlen(write_msg)+1);
                }

            }
            exit(0);
        }
        else if(pid[i] > 0)
        {
            close(fd[i][WRITE_END]);
        }
        else
        {
            printf("fork() failed");
            return -1;
        }
    }

    time_t start_time = time(0);
    // base_fds for maintaining and cur_fds is for passing to select()
    fd_set cur_fds, base_fds;
    // setting all 0's
    FD_ZERO(&base_fds);
    for(int i=0; i<5; i++)
        FD_SET(fd[i][READ_END], &base_fds); // adding read fd to base_fds for monitoring reads.
    // text file for writing output by parent process, old file will be replaced.
    FILE *output_file_fd = fopen("output.txt", "w"); 
    if(output_file_fd == NULL)
    {
        printf("Error opening output.txt\n");
        return -1;
    }
    while(true)
    {
        time_t cur_time = time(0);
        // stop if time reaches 30 seconds.
        if(cur_time - start_time > 30)
        {
            for(int i=0; i<5; i++)
                kill(pid[i], SIGKILL); // killing each child.
            return 0;
        }

        cur_fds = base_fds;
        struct timeval timeout;
        // 2.5 sec
        timeout.tv_sec = 2;
        timeout.tv_usec = (500000);
        int rv = select(FD_SETSIZE,&cur_fds,0,0,&timeout);

        if(rv < 0)
        {
            printf("select() failed\n");
            return -1;
        } 
        else if (rv != 0)
        {
            for(int i=0; i<5; i++)
            {
                if(FD_ISSET(fd[i][READ_END], &cur_fds)) // if bit set to 1 then read is available.
                {
                    int nread;
                    // read # of bytes available on stdin and set nread arg to that value 
                    ioctl(fd[i][READ_END], FIONREAD, &nread);
                    if(nread != 0)
                    {
                        nread = read(fd[i][READ_END], read_msg, nread); // reading from pipe to read_msg buffer
                        read_msg[nread] = '\0'; // setting end of string character
                    }
                    // get and append time stamp before writing into output file
                    char *time_stamp = getTimeStamp();
                    fprintf(output_file_fd, "%s\n", read_msg);
                    free(time_stamp);
                }
            }
        }
    }
    free(pid);
    for(int i = 0; i < 5; i++) {
	    free(fd[i]);
    }
    return 0;
}
