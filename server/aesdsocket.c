/*!******************************************************************************************
   File Name           : aesdsocket.c
   Author Name         : Ananth Deshpande
                         Professional Masters in Embedded Systems
                         Fall 2021, UCB.
   Author email id     : ande9392@colorado.edu
   Compiler            : gcc and aarch64-none-linux-gnu-gcc 
   Date                : 20th february 2022
   references:
   https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
  
*******************************************************************************************/
/*header files*/
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/queue.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>

/*macros*/
#define FILE "/var/tmp/aesdsocketdata"
#define PORT "9000"
#define BUF_SIZE 1000
#define MAX_CONNECTIONS 10

/*global variables*/
static int servfd = -1;
static struct addrinfo  *updated_addr;
static pthread_mutex_t mutex_buffer = PTHREAD_MUTEX_INITIALIZER;
bool signal_recv = false;

typedef struct 
{
   int clientfd;
   pthread_t thread_id; 
   pthread_mutex_t* mutex;
   bool isComplete;
} thread_data; 

struct slist_data_s
{
   thread_data   params;
   SLIST_ENTRY(slist_data_s) entries;
};

typedef struct slist_data_s slist_data_t;


/*****************************************************************
 * To clean all the pending resources
 *
 * Parameters:
 *   none
 * 
 * Returns:
 *   none
 ****************************************************************/
void cleanup()
{

    if(servfd > -1)
    {
        shutdown(servfd, SHUT_RDWR); 

        close(servfd);
    }

    pthread_mutex_destroy(&mutex_buffer);

    closelog();
}

/*****************************************************************
 * handle SIGINT and SIGTERM signals
 *
 * Parameters:
 *   none
 * 
 * Returns:
 *   none
 ****************************************************************/
static void signal_handler(int sig)
{
    syslog(LOG_INFO, "Signal Caught %d\n\r", sig);
    signal_recv = true;
    
    if(sig == SIGINT)
    {
       cleanup();
    }
    else if(sig == SIGTERM)
    {
       cleanup();
    }
}

/*****************************************************************
 * implements the timer functionality requirements of assignment
 * wherein timestamp is added to FILE every 10 seconds.
 * In order to accomplish 10 seconds of delay clock_nanosleep is
 * employed
 *
 * Parameters:
 *   args --> provides arguments to thread, in this case not utilised
 * 
 * Returns:
 *   void * --> returns void pointer
 ****************************************************************/
void *timer_thread(void *args)
{
   size_t bufLen;
   time_t rawtime;
   struct tm *localTime;
   struct timespec request = {0, 0};
   int timeInSecs = 10; //Timer Interval

   while (!signal_recv)
   {
      /*obtain the current monotonic time*/    
      if (clock_gettime(CLOCK_MONOTONIC, &request))
      {
         syslog(LOG_ERR, "Error: failed to get monotonic time, [%s]\n", strerror(errno));
         continue;
      }
      
      /*update requested time to +1 second from previous obtained monotonic time*/
      request.tv_sec += 1; 
      request.tv_nsec += 1000000;
    
      /*precisely sleep for 1 second*/
      if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &request, NULL) != 0)
      {
         if (errno == EINTR)
         {
            break; 
         }
      }

      /*ensure we sleep 10 seconds before adding timestamp to FILE*/
      if ((--timeInSecs) <= 0)
      {
         char buffer[100] = {0};
         
         time(&rawtime);        
         localTime = localtime(&rawtime); 
         
         /*timestamp in accordance to RFC2822 format*/
         bufLen = strftime(buffer, 100, "timestamp:%a, %d %b %Y %T %z\n", localTime);

         /*to open the file and write to it according to RFC 2822 spec*/
         int fd = open(FILE, O_RDWR | O_APPEND, 0644);
        
         if (fd < 0)
         {
            syslog(LOG_ERR, "failed to open a file:%d\n!!!", errno);
         }

         /*lock using mutex, since the file is getting updated from multiple threads*/
         int retVal = pthread_mutex_lock(&mutex_buffer);
        
         if(retVal)
         {
            syslog(LOG_ERR, "Error in locking the mutex");
            close(fd);
         }
        
         /*seek to the end of file, since we want to append the timestamp at the end*/
         lseek(fd, 0, SEEK_END); 

         int noOfBytesWritten = write(fd, buffer, bufLen);
        
         syslog(LOG_INFO, "Timestamp %s written to file\n", buffer);
   
         if (noOfBytesWritten < 0)
         {
            syslog(LOG_ERR, "Write of timestamp failed errno %d",errno);
         }
        
         retVal = pthread_mutex_unlock(&mutex_buffer);
        
         if(retVal)
         {
            syslog(LOG_ERR, "Error in unlocking the mutex\n\r");
            close(fd);
         }
        
         close(fd);
         
         /*update the timeInSecs variable for next cycle*/
         timeInSecs = 10;
      } 
  }

  pthread_exit(NULL);
}

/*****************************************************************
 * This function is an entry point of thread which handles 4 tasks:
 * 1. read i.e. recv packets from client
 * 2. write the packets to FILE until '\n'
 * 3. read from the FILE
 * 4. write i.e send the data read previously to client
 *
 * Parameters:
 *   thread_params --> the thread parameters provided
 * 
 * Returns:
 *   void * --> returns void pointer
 ****************************************************************/
void* handlePacketsOfEachClient(void* thread_params)
{  
    char buf[BUF_SIZE];

    thread_data* tdParams = (thread_data*)thread_params;

    int noOfBytesRead = 0;

    while(1)
    {
       /*to read packets from client*/
       noOfBytesRead = read(tdParams->clientfd, buf, (BUF_SIZE));
        
       if (noOfBytesRead < 0) 
       {
           syslog(LOG_ERR, "reading from socket errno=%d\n", errno);
           tdParams->isComplete = true;
           pthread_exit(NULL);
       }


       if (noOfBytesRead == 0)
       {
           continue;
       }

       /*
        if a newline is found, it implies we have recived the complete packet
        so we proceed to next steps
        */
       if (strchr(buf, '\n')) 
       {  
          break; 
       } 
    }

    /*open the file*/
    int fd = open(FILE, O_RDWR | O_APPEND, 0644);
    
    if (fd < 0)
    {
       syslog(LOG_ERR, "failed to open a file:%d\n", errno);
    } 
    
    int retVal = pthread_mutex_lock(tdParams->mutex);
    
    if(retVal)
    {
        syslog(LOG_ERR, "Error in locking the mutex\n\r");
        tdParams -> isComplete = true;
        pthread_exit(NULL);
    }

    /*write the packet content read to the FILE*/
    int writeByteCount = write(fd, buf, noOfBytesRead);
    
    if(writeByteCount < 0)
    {
        syslog(LOG_ERR, "Writing to file error no: %d\n\r", errno);
        
        tdParams -> isComplete = true;
        close(fd);
        pthread_exit(NULL);
    }

    retVal = pthread_mutex_unlock(tdParams->mutex);
    
    if(retVal)
    {
        syslog(LOG_ERR, "Error in unlocking the mutex\n\r");
        tdParams -> isComplete = true;
        pthread_exit(NULL);
    }

    close(fd);

    int read_offset = 0;

    while(1) 
    {
        /*open the file*/
        int fd = open(FILE, O_RDWR | O_APPEND, 0644);
        
        if(fd < 0)
        {
            syslog(LOG_ERR, "failed to open a file:%d\n", errno);
            continue; 
        }

        /*move offset back to start since we have used write() previously*/
        lseek(fd, read_offset, SEEK_SET);


        int retVal = pthread_mutex_lock(tdParams->mutex);
        
        if(retVal)
        {
            syslog(LOG_ERR, "Error in locking the mutex\n\r");
            tdParams -> isComplete = true;
            pthread_exit(NULL);
        }
        
        /*read the contents of FILE to buffer buf*/
        int noOfBytesRead = read(fd, buf, BUF_SIZE);

        retVal = pthread_mutex_unlock(tdParams->mutex);   
        
        if(retVal)
        {
            syslog(LOG_ERR, "Error in locking the mutex\n\r");
            tdParams -> isComplete = true;
            pthread_exit(NULL);
        }
        
        close(fd);
        
        if(noOfBytesRead < 0)
        {
            syslog(LOG_ERR, "failed to read from file:%d\n", errno);
            continue;
        }

        if(noOfBytesRead == 0)
        {
            break;
        }
        
        /*write the buf contents read from FILE above to the client*/
        int writeByteCount = write(tdParams->clientfd, buf, noOfBytesRead);

        if(writeByteCount < 0)
        {
            syslog(LOG_ERR, "failed to write on client fd:%d\n", errno);
            continue;
        }

        read_offset += writeByteCount;
    }

    tdParams -> isComplete = true;
    pthread_exit(NULL);
}

/*point of entry*/
int main(int argc, char **argv) 
{

   openlog(NULL, 0, LOG_USER);

   /*register signal handlers*/
   if (signal (SIGINT, signal_handler) == SIG_ERR) 
   {
      syslog(LOG_ERR, "Cannot handle SIGINT!\n");
      cleanup();
   }

   if (signal (SIGTERM, signal_handler) == SIG_ERR) 
   {
      syslog(LOG_ERR, "Cannot handle SIGTERM!\n");
      cleanup();
   }

   bool createDeamon = false;
   
   /*check if need to createDeamon*/
   if (argc == 2) 
   {
      if (!strcmp(argv[1], "-d")) 
      {
         createDeamon = true;
      } 

      else 
      {
         syslog(LOG_ERR, "incorrect argument:%s", argv[1]);
         return (-1);
      }
   }
   
   /*create a FILE*/
   int write_fd = creat(FILE, 0766);

    if(write_fd < 0)
    {
       syslog(LOG_ERR, "aesdsocketdata file could not be created, error number %d", errno);
       cleanup();
       exit(1);
    }
    close(write_fd);
    
    /*Initialize the linked list*/
    slist_data_t *listPtr = NULL;
    
    SLIST_HEAD(slisthead, slist_data_s) head;
    SLIST_INIT(&head);

    struct addrinfo addr_hints;
    memset(&addr_hints, 0, sizeof(addr_hints));

    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_flags = AI_PASSIVE;

    /*fill the updated_addr with necessary socket configurations*/
    int res = getaddrinfo(NULL, (PORT), &addr_hints, &updated_addr);
     
    if (res != 0) 
    {
        syslog(LOG_ERR, "getaddrinfo() error %s\n", gai_strerror(res));
        cleanup();
        return -1;
    }

    /* create socket for communication */
    servfd = socket(updated_addr->ai_family, SOCK_STREAM, 0);

    if (servfd < 0) 
    {
        syslog(LOG_ERR, "Socket creation failed:%d\n", errno);
        cleanup();
        return -1;
    }
    
    /*setsockopt with SO_REUSEADDR to avoid address already in use error*/
    if (setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) 
    {
        syslog(LOG_ERR, "setsockopt failed with errono:%d\n", errno);
        cleanup();
        return -1;
    }

    /*bind the socket*/
    if (bind(servfd, updated_addr->ai_addr, updated_addr->ai_addrlen) < 0)
    {
        syslog(LOG_ERR, "failed to bind:%d\n", errno);;
        cleanup();
        return -1;
    }

    freeaddrinfo(updated_addr);

    /*listen for connections*/
    if (listen(servfd, MAX_CONNECTIONS)) 
    {
       syslog(LOG_ERR, "ERROR:failed to listen %d\n", errno);
       cleanup();
        return -1;
    }

    printf("waiting for client connections\n\r");

    /*create a daemon from this process*/
    if (createDeamon == true) 
    {  
        int retVal = daemon(0,0);

        if(retVal < 0)
        {
           syslog(LOG_ERR,"Failed to create daemon");
           cleanup();
           return -1;
        } 
    }

    /*in order to handle timestamp requirement, spawn a timer thread*/
    pthread_t timer_thread_id; 
    pthread_create(&timer_thread_id, NULL, timer_thread, NULL);

    /*accept multiple connections from clients, and create a new thread for each client*/
    while(!(signal_recv)) 
    {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);

        /*accept the connection*/
        int clientfd = accept(servfd, (struct sockaddr*)&clientAddr, &clientAddrSize);

        if(signal_recv)
        {
            break;
        }

        if(clientfd < 0)
        {
            syslog(LOG_ERR, "failed to accept the connection: %s", strerror(errno));
            cleanup();
            return -1;
        }
  
        /*to obtain the ip address of the client*/
        char client_info[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), client_info, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Connection Accepted %s \n\r",client_info);
  
        /*In order to accomplish the clean up resources, we use linked list to store
          thread related parameters, wherein each node contains parameters of one thread*/
        listPtr = (slist_data_t *) malloc(sizeof(slist_data_t));
        
        /*insert a node into  linked list*/
        SLIST_INSERT_HEAD(&head, listPtr, entries);

        listPtr->params.clientfd     = clientfd;
        listPtr->params.mutex        = &mutex_buffer;
        listPtr->params.isComplete   = false;
        
        /*create a thread and pass the necessary arguments*/
        pthread_create(&(listPtr->params.thread_id), NULL, handlePacketsOfEachClient, (void*)&listPtr->params);

        /*
           check each node of linked list and if the thread 
           has finished execution then clean up all the 
           involved resources
        */
        SLIST_FOREACH(listPtr, &head, entries)
        {     
            if(listPtr->params.isComplete == true)
            {
                /*join the thread*/
                pthread_join(listPtr->params.thread_id,NULL);
                
                /*shutdown and close the client file descriptor */
                shutdown(listPtr->params.clientfd, SHUT_RDWR);
                
                close(listPtr->params.clientfd);
                
                syslog(LOG_INFO, "Join spawned thread:%d\n\r",(int)listPtr->params.thread_id); 
            }
        }
    }

    /*join the timer thread*/
    pthread_join(timer_thread_id, NULL);

    /*clean up pending nodes if any*/
    while (!SLIST_EMPTY(&head))
    {
        listPtr = SLIST_FIRST(&head);
        
        pthread_cancel(listPtr->params.thread_id);
        
        syslog(LOG_INFO, "Thread is killed:%d\n\r", (int) listPtr->params.thread_id);
        
        SLIST_REMOVE_HEAD(&head, entries);
        
        free(listPtr); 
        
        listPtr = NULL;
    }

    /*finally delete the FILE*/
    if (access(FILE, F_OK) == 0) 
    {
       remove(FILE);
    }
    
    /*and also the socket file descriptor*/
    cleanup();

    exit(0);
}

/*******************************************end of file*******************************************/

