/* myshell.c 
 * Mike Gedelman
 * CS 410
 * assignment 2
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <readline/readline.h>

#define myshell_pipe(x,y) _myshell_pipe(x,y,-1)

/* returns size of null-terminated array including the null */
size_t sizeof_array(char* arr[]) {
   int i;
   for (i=0; arr[i] != NULL; i++);
   return i;
}
/*
 * wrapper for exec, handles exit statuses and such
 * should be called inside child process.
 */
void myshell_exec(char *args[]) {
   int status;

   if (execvp(args[0], args) < 0) {
      switch (errno) {
         case ENOENT: {
                         printf("%s: command not found\n", args[0]);
                         break;
                      }
         case EACCES: {
                         printf("%s: permission denied\n", args[0]);
                         break;
                      }
         default: {
                     printf("%s: unknown error\n", args[0], errno);
                     break;
                  }

      }
      exit(0);
   }
}

/*
 * handles '<' and '>' operators existing in argument list args
 * sets up the file descriptors necessary so should be used after fork
 *
void myshell_redirects(char *args) {
   for (i=0; args[i] != NULL; i++) {
      int length = strlen(args[i]);
      if (args[i][length-1] == ">") {
        switch (args[i][length-2]) {
           case '&': 
           case '2': {
              
              break;
           }
           case '1':
           case ' ': {

              break;
           }
           case '<': {

              break;
           
            
        }
      }
   }

}*/

void _myshell_pipe(char* args1[], char* args2[], int fd_read) {
   int i, more_pipes_flag = 0;
   int fd[2];
   char* args3[sizeof_array(args1)];
   
   if (args2[0] == NULL) {
      pipe(fd);
   }
   else { /* check if we have more pipe arguments in arg2 */
      for(i=0; args2[i] != NULL; i++)
         if (strcmp(args2[i], "|") == 0) {
            more_pipes_flag = 1;

            char* args3[sizeof_array(args2)];
            int j;
            int args3_ind = 0;
            for (j=i+1; args2[j] != NULL; j++)
               args3[args3_ind++] = args2[j];
   
          args3[args3_ind] = NULL;
          break;
      }

      /* if we get here, no '|'s found. */
      args3[0] = NULL;
   }

   pid_t pid, wpid;
   pid = fork();
   int status;

   if (pid < 0 ) {
      printf("mysh: pipe: an error occurred\n");
      return;
   }
   else if (pid == 0) { /* child */
      close(fd[0]);
      dup2(1,fd[1]); /* redirect stdout to pipe */
      
      myshell_exec(args1);
   }
   else { /* parent */
      close(fd[1]);
      wpid = waitpid(pid, &status, 0);
      
      _myshell_pipe(args2, args3, fd[0]);
   }
   
}

/*
 * copies all values from source[0] (inc) to source[end_index] (inc)
 *
void sub_array(int[] dest, int[] source, int end_index) {
   int i;
   for (i=0; i<=end_index; i++)
      dest[i] = source[i];

   dest[i] = NULL;
}*/

/* fork a child and then call myshell_exec */
void myshell_cmd(char* args[]) {
   pid_t cpid, wpid;
   int status;


   cpid = fork();

   if (cpid < 0) {
      printf("myshell: an error occured\n");
      return;
   }
   else if (cpid == 0) { /* child */
      /* process '>' and '<' here */
      myshell_exec(args);
   }
   else { /* parent */
      wpid = waitpid(cpid, &status, 0);
      fflush(stdout);
   }

}


void parse_cmd(char *command) {
   int i;
   for (i=0; command[i] != '\0'; i++) {
      if (command[i] == ';') {
         parse_cmd(command+i+1);
         command[i] = '\0';
         break;  
      }
   }

   char *args[50]; /* support up to 50 args */
   int argindex;
   args[0] = strtok(command, " ");
   for (argindex=1; (args[argindex] = strtok(NULL, " ")) != NULL; 
         argindex++);

   if (strcmp(args[0], "exit") == 0) {
      exit(0);
   }

   /* check for pipe operator, if so pass to myshell_pipe */
   for (i=0; args[i] != NULL; i++) {
      if (strcmp(args[i], "|") == 0) {
         args[i] = NULL;
         
         char* args2[sizeof_array(args)];
         int j;
         int args2_ind = 0;
         for (j=i+1; args[j] != NULL; j++)
            args2[args2_ind++] = args[j];

         args2[args2_ind] = NULL;
         myshell_pipe(args, args2);
       }
   }
         
   myshell_cmd(args);
}



int main() {
   char *line;

   while (1) {
      line = readline("myshell> ");
      parse_cmd(line);

   }
}
