/*!******************************************************************************************
   File Name           : writer.c
   Author Name         : Ananth Deshpande
                         Professional Masters in Embedded Systems
                         Fall 2021, UCB.
   Author email id     : ande9392@colorado.edu
   Compiler            : gcc and aarch64-none-linux-gnu-gcc 
   Date                : 18th January 2022

   references:

   https://stackoverflow.com/questions/23092040/how-to-open-a-file-which-overwrite-existing-content/23092113
  

*******************************************************************************************/


/*header files*/
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

/*macros*/
#define NUMBER_OF_EXPECTED_ARGS 3

/*point of entry -- main function*/
int main(int argc, char * argv[])
{
   int fd;
   char * writeFile;
   char * writeStr;
   int writeStrLen;
   int retVal = 0;
   
   /*initialise syslog daemon*/
   openlog(NULL, 0, LOG_USER);
   
   /*error check for incorrect number of arguments*/
   if(NUMBER_OF_EXPECTED_ARGS != argc)
   {
      syslog(LOG_INFO, "Please provide two arguments to writer utility as mentioned below\n");
      syslog(LOG_INFO, "1st argument must be a file path\n");
      syslog(LOG_INFO, "2nd argument must be a string to be written to the file \n");
      syslog(LOG_ERR, "Invalid number of arguments %d\n", argc);
      exit(1);
   }
   
   /*to copy command line arguments to local variables for future usage*/
   writeFile   = argv[1];
   writeStr    = argv[2];
   writeStrLen = strlen(argv[2]);
   
   /*open/create a new file in the provided path*/
   fd = open(writeFile, O_RDWR | O_CREAT | O_TRUNC, 0644);
   
   /*error check for open system call*/
   if(-1 == fd)
   {
      syslog(LOG_ERR, "file open/creation failed with error number %d!!!\n", errno);
      exit(1);      
   }
   
   /*write the provided string to the created file*/
   retVal = write( fd, writeStr, writeStrLen);
   
   /*error check for write system call*/
   if(-1 == retVal)
   {
      syslog(LOG_ERR, "write to file failed with error number %d\n", errno);
   }
   /*handle partial write*/
   else if(writeStrLen != retVal)
   {
      syslog(LOG_ERR, "writing partially to the file %d\n", errno);
   }
   else
   {
      /*log message on successful writing of file*/
      syslog(LOG_DEBUG, "Writing %s to %s\n", writeStr, writeFile);
   }
   
   /*clean up - to close the file*/
   if(-1 == close(fd))
   {
      syslog(LOG_ERR, "closing of file failed with error number %d\n", errno);
   }
}

/**************************************end of file**********************************************/
