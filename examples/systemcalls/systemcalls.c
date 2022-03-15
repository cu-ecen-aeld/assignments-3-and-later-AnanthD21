#include "systemcalls.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success 
 *   or false() if it returned a failure
*/
   int retVal = 0;

   /*trigger the system call*/
   retVal = system(cmd);
   
   /*if a child process couldnt be created*/
   if(-1 == retVal)
   {
      return false;
   }
   
   /*if the child process exited successfully but the command execution failed*/
   if(WIFEXITED(retVal) && WEXITSTATUS(retVal) != 0)
   {
      return false;
   }

   return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *   
*/
   pid_t pid = 0;
   int status = 0;
   pid_t childPid = 0;
	
   pid = fork();
	
   /*fork system call failed*/
   if(-1 == pid)
   {
      return false;
   }
   /*implies we are in child process*/
   else if( 0 == pid)
   {
      execv(command[0], command);
      
      /*below line gets executed only if execv fails*/
      exit(EXIT_FAILURE);
   }

   /*inside parent process*/
   childPid = waitpid(pid, &status, 0);

   /*error check for waitpid*/	
   if(-1 == childPid)
   {
      return false;
   }
   /*check the exit status of child process*/
   else if(true == WIFEXITED(status))
   {
      /*check the status of command executed in child*/
      if(0 != WEXITSTATUS(status))
      {
         return false;
      }	
	
      va_end(args);
      return true;
   }

   return false;
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *   
*/
    int fd = 0;
    
    if(NULL != outputfile)
    {
       fd = open(outputfile, O_RDWR|O_CREAT, 0644);
    
       if( fd < 0)
       {
          printf("file open failed \n");
          return false;
       }
   }

   pid_t pid = 0;
   int status = 0;
   pid_t childPid = 0;
	
   pid = fork();
	
   /*fork system call failed*/
   if(-1 == pid)
   {
      return false;
   }
   /*implies we are in child process*/
   else if( 0 == pid)
   {
      /*redirect the stdout to outputfile*/
      if(dup2(fd, STDOUT_FILENO) < 0)
      {
         return false;
      }
      
      /*close the file descriptor*/
      close(fd);
      
      execv(command[0], command);
      
      /*below line gets executed only if execv fails*/
      exit(EXIT_FAILURE);
   }

   /*closing file in parent process*/
   close(fd);
   
   /*inside parent process*/
   childPid = waitpid(pid, &status, 0);
	
   /*error check for waitpid*/
   if(-1 == childPid)
   {
      return false;
   }
   else if(true == WIFEXITED(status))
   {
      if(0 != WEXITSTATUS(status))
      {
         return false;
      }	
	
      va_end(args);
      return true;
   }

   return false; 

}
