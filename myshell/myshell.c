/* myshell.c 
 * Mike Gedelman
 * CS 410
 * assignment 2
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <readline/readline.h>

#define myshell_pipe(x,y,z) _myshell_pipe(x,y,z,-1)

/* remember the pid of process in the foreground so we can kill it if 
 * necessary */
pid_t fg_proc = -1;

/* returns size of null-terminated array including the null */
size_t sizeof_array(char* arr[]) {
   int i;
   for (i=0; arr[i] != NULL; i++);
   return i;
}

/*
 * wrapper for exec, handles exit statuses and such.
 * should be called inside child process.
 */
void myshell_exec(char *args[]) {
   int status;

   if (execvp(args[0], args) < 0) {
      switch (errno) {
         case ENOENT: {
                         dprintf(2, "%s: command not found\n", args[0]);
                         break;
                      }
         case EACCES: {
                         dprintf(2, "%s: permission denied\n", args[0]);
                         break;
                      }
         default: {
                     dprintf(2, "%s: unknown error\n", args[0], errno);
                     break;
                  }
      }
      exit(0);
   }
}

void print_file_err(char *path) {
   switch(errno) {
      case EACCES: {
         dprintf(2, "myshell: %s: permission denied\n", path);
         break;
      }
      case EISDIR: {
         dprintf(2, "myshell: %s is a directory\n", path);
         break;
      }
      default: {
         dprintf(2, "myshell: %s: an error occurred opening this file\n", path);
         break;
      }
   }
}

/*
 * handles '<' and '>' operators existing in argument list args,
 * including '&>', '1>', '2>'
 * 
 * sets up the file descriptors necessary - should be used *after* fork
 * returns 1 if it actually redirected anything
 *
 * puts -1 (cast to void*) in place of the '>' or '>' and the token after it
 * to be removed later; this way we don't actually pass the redirect to the
 * command.
 */
int myshell_redirects(char *args[]) {
   int i;
   int redir_flag = 0;
   for (i=0; args[i] != NULL; i++) {
      int length = strlen(args[i]);
      if (args[i][length-1] == '>' || args[i][length-1] == '<') {

         if (args[i+1] == NULL) {
            dprintf(2, "myshell: error: no filename specified after '%c'\n", 
                  args[i][length-1]);
            exit(-1);
         }

         if (args[i][length-1] == '>') {

            /* open the file for write, create or append */
            char *path = args[i+1];
            int fd = open(path, O_WRONLY|O_APPEND|O_CREAT, 
                  S_IRUSR|S_IWUSR|S_IRGRP|S_IRGRP );

            if (fd < 0) {
               print_file_err(path);
               exit(-1);
            } 

            if (length == 1) 
               dup2(fd,1);
            
            else 
              switch (args[i][length-2]) {
                 case '&': 
                    dup2(fd, 1);
                 case '2': 
                    dup2(fd, 2);  
                    break;

                 case '1':
                    dup2(fd, 1);
                    break;

                 default:
                    dprintf(2, "myshell: invalid specifier: %s\n", args[i]);
                    exit(-1);
              }
         }
         else if (args[i][length-1] == '<') {
            /* open the file for read */
            char *path = args[i+1];
            int fd = open(path, O_RDONLY);

            if (fd < 0) {
               print_file_err(path);
               exit(-1);
            } 

            dup2(fd, 0);
         }

      void* negative_one = (void*)-1;

      redir_flag = 1;
      args[i] = negative_one;
      args[i+1] = negative_one;
      i++;
      }
   }
   return redir_flag;
}

/*
 * should be called after myshell_redirects, removes tokens that are -1
 */
void myshell_collapse(char *new_args[], char *old_args[]) {
   int i;
   int new_args_ind=0;
   void* negative_one = (void*) -1;
   for (i=0; old_args[i] != NULL; i++) 
      if (old_args[i] != negative_one)
        new_args[new_args_ind++] = old_args[i];

   new_args[new_args_ind] = NULL;
}

/*
 * supports multiple pipes; calls itself recursively
 * should be called initially with fd_read = -1; later this becomes the 
 * fd for the next process to read from. see the myshell_pipe macro definition.
 */
void _myshell_pipe(char* args1[], char* args2[], int bg_flag, int fd_read) {
   int i, more_pipes_flag = 0;
   int fd[2];
   char* args3[sizeof_array(args1)];

   if (args2[0] != NULL) {
      pipe(fd);
      more_pipes_flag = 1;
      
      for(i=0; args2[i] != NULL; i++)
         if (strcmp(args2[i], "|") == 0) {
            int j;
            int args3_ind = 0;
            for (j=i+1; args2[j] != NULL; j++)
               args3[args3_ind++] = args2[j];
   
            args3[args3_ind] = NULL;
            args2[i] = NULL;
            break;
      }

      /* if we get here, no '|'s found. */
      args3[0] = NULL;
   }

   pid_t pid, wpid;
   pid = fork();
   int status;

   if (pid < 0 ) {
      dprintf(2, "mysh: pipe: an error occurred\n");
      return;
   }
   else if (pid == 0) { /* child */
      if (more_pipes_flag) {
         close(fd[0]);
         dup2(fd[1],1); /* redirect stdout to pipe */
      }
      if (fd_read > 0)
         dup2(fd_read, 0);
   
      if (myshell_redirects(args1)) {
         char *new_args[sizeof_array(args1)];
         myshell_collapse(new_args, args1);
         myshell_exec(new_args);
      }
      else 
         myshell_exec(args1);
   }
   else { /* parent */
      if (more_pipes_flag)
         close(fd[1]);
      
      if (!bg_flag) {
         wpid = waitpid(pid, &status, 0);
         fg_proc = wpid;
      }

      if (more_pipes_flag)
         _myshell_pipe(args2, args3, bg_flag, fd[0]);
   }
}

/* fork a child and then call myshell_exec */
void myshell_cmd(char* args[], int bg) {
   pid_t cpid, wpid;
   int status;

   cpid = fork();

   if (cpid < 0) {
      dprintf(2, "myshell: an error occured\n");
      return;
   }
   else if (cpid == 0) { /* child */

      if (myshell_redirects(args)) {
         char *new_args[sizeof_array(args)];
         myshell_collapse(new_args, args);
         myshell_exec(new_args);
      }
      else 
         myshell_exec(args);
   }
   else { /* parent */
      if (!bg) {
         fg_proc = cpid;

         wpid = waitpid(cpid, &status, 0);
         fg_proc = -1;
         fflush(stdout);
      }
   }
}

/*
 * verify the command's validity; tokenize; find out if it uses pipes */
void parse_cmd(char *command) {
   int i;

   if (command == NULL) {
      printf("\n");
      exit(0);
   }

   if (command[0] == '\0')
      return;

   /* split up commands by ';' first */
   for (i=0; command[i] != '\0'; i++) {
      if (command[i] == ';') {
         parse_cmd(command+i+1);
         command[i] = '\0';
         break;  
      }
   }

   int pipe_flag = 0, redir_flag = 0, bg_flag = 0;
   for (i=0; command[i] != '\0'; i++){
      switch (command[i]) {
         case '|': {
            pipe_flag = 1;
            break;
         }
         case '>':
         case '<': {
            redir_flag = 1;
            break;
         }
         case '&': {
            if ( command[i+1] == '>' )
               break;

            if ( command[i+1] != '\0') {
               printf("myshell: syntax error near '&'\n");
               return;
            }

            bg_flag = 1;
            
            command[i] = '\0';
            i--;
            break;
         }
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

   if (pipe_flag) {
      /* find pipe operator, if so pass to myshell_pipe */
      for (i=0; args[i] != NULL; i++) {
         if (strcmp(args[i], "|") == 0) {
            args[i] = NULL;

            char* args2[sizeof_array(args)];
            int j;
            int args2_ind = 0;
            for (j=i+1; args[j] != NULL; j++)
               args2[args2_ind++] = args[j];

            args2[args2_ind] = NULL;
            myshell_pipe(args, args2, bg_flag);
         }
      }
   }
   else {
      myshell_cmd(args,bg_flag);
   }
}

static void handle_sigchld(int sig, siginfo_t *siginfo, void *context) {
   int status;
   pid_t pid;
   pid = wait(&status);
}

static void handle_sigint(int sig, siginfo_t *siginfo, void *context) {
   if (fg_proc > 0)
      kill(fg_proc, SIGINT);
}

int quit_mysh(int a, int b) {
   exit(0);
}


int main() {

   /* set up signals */
   struct sigaction child_act;
   memset(&child_act,'\0', sizeof(child_act));
   child_act.sa_sigaction = handle_sigchld;

   struct sigaction int_act;
   memset(&int_act, '\0', sizeof(int_act));
   int_act.sa_sigaction = handle_sigint;


   if (sigaction(SIGCHLD, &child_act, NULL) < 0) {
      printf("sigaction error\n");
      exit(-1);
   }
   if (sigaction(SIGINT, &int_act, NULL) < 0) {
      printf("sigaction error\n");
      exit(-1);
   }

   char *line;

   while (1) {
      line = readline("myshell> ");
      parse_cmd(line);
   }
}
