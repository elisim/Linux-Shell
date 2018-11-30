	#include <stdio.h> 
	#include <sys/types.h>
	#include <unistd.h>
	#include <sys/wait.h>
	#include <linux/limits.h>
	#include <stdlib.h>
	#include <string.h> 
    #include <sys/stat.h>
    #include <fcntl.h>

	#include "../include/LineParser.h"

	#define CAPACITY 20 
	#define MAX_COMMAND_LENGTH 128
	#define STDIN 0
	#define STDOUT 1

	typedef struct my_link my_link; 
	struct my_link {
	   	char *name ;
	    char *value;
	    my_link *next ;  
	};

	static void printDirectory () ;  /*print current working directory - Task0-1*/ 
	static void execute (cmdLine *pCmdLine) ; /* receives a parsed line and invokes the command */
	static void changeDirectory (char *path) ; /* execute cd command. TODO c */ 
	static void printHistory () ; /* prints the list of all the command lines typed, in an increasing chronological order TODO d*/ 
	static void addToHistory (char *command) ; /* adds the command typed to history list TODO d*/ 	
	static void initCommands () ; /* initialize command list */ 
	static void freeCommands () ; /* free the memory of history list */

	/* part2 */
	static void addToEnvironment (char *x, char *y) ; /* creates an environment variable with name x and value y */
	static void printEnvironment();
	static void free_environment(); 
	static void remove_var (char *x) ; /* deletes the variable x from the environment */
	static void check_internal_variables (cmdLine *pCmdLine) ; /* check for $ in command arguments */
	static char* findValue (char *name) ; /* find the value associated with name */
	static my_link* makeLink (char *name, char *value) ;

	/* lab6-task4 */
	static int **createPipes(int nPipes) ;
	static void releasePipes(int **pipes, int nPipes) ;
	static int *leftPipe(int **pipes, cmdLine *pCmdLine) ;
	static int *rightPipe(int **pipes, cmdLine *pCmdLine) ; 
	static int getSize (cmdLine *cmdl) ;

	int debug = 0 ; /* -d flag */ 

	char** commands ; /* list of all the command lines typed. TODO d */
	int numOfCommands = 0 ; /* num of cuurent commands typed*/ 
	my_link *environment_variables = NULL ;  /* linked list of (name, value) string pairs */

	int main (int argc, char** argv) {
		if (argc > 1) debug = 1 ; /* check for -d */ 

		if (debug) printf ("DEBUG: command list capacity: %d\n", CAPACITY) ;
		initCommands () ;
		printDirectory() ;

		while (1) {
			int status = 0; 
			char input [2048] ;

			fgets (input, 2048, stdin) ; /* read a line from the user */
			if (strcmp (input, "quit\n") == 0) /* End the infinite loop of the shell if the command "quit" is entered */
				break ; 

			/* TODO e */ 
			if (input [0] == '!') {
				int index = atoi (input+1) ; /* take the index number */
				if (index >=0 && index<numOfCommands) {
					strcpy (input, commands[index]) ; 
				}
				else {
					fprintf(stderr, "ERROR: wrong command. You typed only %d commands.\n", numOfCommands);
					continue ; 
				}
			}

			addToHistory (input) ;
			cmdLine* cmdl = parseCmdLines (input) ;
			check_internal_variables (cmdl) ;

			char* command = (cmdl->arguments)[0] ; /* arg[0] is the command */

			int **pipes = NULL ;
			int size_cmdl = getSize (cmdl) ;

			/* select command */ 
			if (strcmp (command, "set") == 0) addToEnvironment ((cmdl->arguments)[1], (cmdl->arguments)[2]) ; 
			else if (strcmp(command, "env") == 0) printEnvironment () ;
			else if (strcmp (command, "delete") == 0) remove_var ((cmdl->arguments)[1]) ;
			else if (strcmp(command, "cd") == 0) changeDirectory ((cmdl->arguments)[1]) ; /* argv[1] is the path */
			else if (strcmp (command, "history") == 0) printHistory () ;		
			else {
				/* START task 4b */
				
				int *leftP = NULL ; 
				int *rightP = NULL ;

				if (size_cmdl > 1) 
					pipes = createPipes (size_cmdl-1) ;

				cmdLine *curr = cmdl ;
				while (curr != NULL) {

					if (pipes != NULL) { /* pipes are given */
						leftP = leftPipe (pipes, curr) ;
						rightP = rightPipe (pipes, curr) ;
					}

					pid_t pid = fork () ;

					if (debug) fprintf (stderr, "DEBUG: fork returns pid: %d\n", (int) pid) ;

					if (pid < 0)
						perror ("Cannot forking. WTF?") ; 

					else if (pid == 0) { /* I am the child */
						char const *input = curr->inputRedirect;
						char const *output = curr->outputRedirect;
						if (input != NULL){
					          close (STDIN);
					          open (input, O_RDONLY);  
					    }
					    if (output != NULL){
					          close (STDOUT);
					          open (output, O_WRONLY | O_CREAT);
					    }

					   
						if (leftP != NULL) {
							int reader = leftP[0] ;
							close (STDIN);
       						dup(reader);
        					close(reader);
					    }

						if (rightP != NULL) {
							int writer = rightP[1] ;
							close (STDOUT);
       						dup(writer);
        					close(writer);
					    }
				
						execute (curr) ; /* execute the command */ 
						
					}

				   else { /* I am the parent */
				   		if (curr->blocking == 1) waitpid (-1, &status, 0) ; /* TODO b */
						if (leftP != NULL) 
							close (leftP[0]) ;
						if (rightP != NULL) 
        					close(rightP[1]);	  
				   }

				   curr = curr->next ; 
				}
			} /* else command */

			releasePipes (pipes, size_cmdl-1) ;
			freeCmdLines(cmdl); 	
		} /* while (True) */

		free_environment (environment_variables) ;
		freeCommands (commands) ; 
		printf("Parent process terminating... \n");
		return 0 ;
	}  /* main */

	static void execute (cmdLine *pCmdLine) {
		int ret ; /* debug purpose */
		char *command = (pCmdLine->arguments)[0] ; /*arg[0] is the command*/
		if (debug) fprintf (stderr, "DEBUG: Executing command: %s\n", command) ;
		ret = execvp (command, pCmdLine->arguments);

		/* check error */
		if (ret == -1) {
			fprintf(stderr, "execvp ERROR\n") ;
			freeCommands (commands) ; 
			freeCmdLines (pCmdLine) ;
			if (environment_variables) free_environment (environment_variables) ;
			_exit(1) ;
		}
	}

	static void printDirectory () {
		char cwd[PATH_MAX];
		getcwd (cwd, PATH_MAX); 
		printf( "> My working directory is: %s\n", cwd);
	}

	static void changeDirectory (char* path) {
		int ret ; /* debug purpose */
		if (strcmp (path, "~") == 0) 
			path = getenv ("HOME") ;
		ret = chdir (path) ; 
		/* check error */
		if (ret == -1) fprintf(stderr, "chdir ERROR\n") ;
		printDirectory () ; /* test cd in action */
	}

	static void printHistory () {
		int i ;
		for (i=0; i<numOfCommands; ++i) 
			printf ("%d %s", i, commands[i]) ;
	}

	static void addToHistory (char *command) {
		if (numOfCommands < CAPACITY-1) { 
			strcpy (commands[numOfCommands], command) ;
			numOfCommands ++ ; 
		}
		else {
			fprintf(stderr, "out of bounds index. You typed more than %d commands. exiting...\n", CAPACITY) ;
			exit (1) ; 
		}

		if (debug) fprintf (stderr, "DEBUG: numOfCommands: %d\n", numOfCommands) ;
	}

	static void initCommands () {
		/* initialize commands array */
		commands = malloc (CAPACITY * sizeof(int)) ; 
		int i=0 ;
		for (; i<CAPACITY; ++i) 
			commands[i] = malloc (MAX_COMMAND_LENGTH * sizeof (char)) ; 
	}

	static void freeCommands () {
		int i=0 ;
		for (; i<CAPACITY; ++i)
			free (commands[i]) ;
		free (commands) ;
	}

	static void addToEnvironment (char *x, char *y) {
	    my_link *toAdd = makeLink (x, y) ;
	    if (environment_variables == NULL)
	    	environment_variables = toAdd ;
	    else {
	    	toAdd->next = environment_variables ;
	    	environment_variables = toAdd ;
	    }
	}

	static my_link* makeLink (char *name, char *value) {
		my_link *ans = (my_link*) malloc(sizeof(my_link)) ;
		ans->name = malloc(strlen(name)+1);
	    ans->value = malloc(strlen(value)+1);
	    strcpy(ans->name, name);
	    strcpy(ans->value, value);
	    ans->next = NULL ;
	    return ans ;
	}

	static void printEnvironment () {
		my_link* curr = environment_variables ;
		if (curr == NULL) printf("no environment variables\n");
		while (curr != NULL) {
			printf ("name: %s\nvalue: %s\n", curr->name, curr->value) ;
			printf ("-------------\n") ;
			curr = curr->next ;
		}
	}

	static void check_internal_variables (cmdLine *pCmdLine) {
		int argc = pCmdLine->argCount;
	    int i=0 ;
	    for (; i<argc; ++i){
	        char *arg = pCmdLine->arguments[i];
	        if (arg[0] == '$'){ /* if the argument is internal variable */
	            char *var = arg+1;
	            char *value = findValue(var);
	            if (value == NULL)/* variable does not exsit */
	                fprintf(stderr, "ERROR: variable '%s' does not even exist\n", var);
	            else 
	                replaceCmdArg(pCmdLine, i, value);   
	        }
	    }
	}

	static char* findValue (char *name) {
		my_link *curr = environment_variables ;
		if (curr == NULL) return NULL;

		while (curr != NULL && strcmp(curr->name, name) != 0)  /* till the var did not found */
			curr = curr->next ;
		if (curr == NULL) return NULL ;
		
		return curr->value ;
	}

	static void remove_var (char *x) {
		my_link *curr = environment_variables ;
		if (curr == NULL) {
			printf("no environment variables\n"); 
			return ;
		}

		my_link *prev = NULL ;
		if (strcmp(curr->name, x) == 0) { /* first link */
			environment_variables = curr->next ; 
			free (curr->name) ;
			free (curr->value) ;
			free (curr) ;
			printf ("variable %s has been deleted\n", x) ;
		}
		else {
			while (curr != NULL && strcmp(curr->name, x) != 0)  {/* till the var did not found */
				prev = curr ; 
				curr = curr->next ;
			}
			if (curr == NULL) 
				fprintf(stderr, "ERROR: can't delete an environment variable that does not exist\n");
			else {
				prev->next = curr->next ; 
				free (curr->name) ;
				free (curr->value) ;
				free (curr) ;
				printf ("variable %s has been deleted\n", x) ;
			}
		}
	}

	static void free_environment() {
		my_link *curr = environment_variables ;
		my_link *temp = NULL ;
		while (curr != NULL) {
			free (curr->name) ;
			free (curr->value) ;
			temp = curr ;
			curr = curr->next ;
			free (temp) ;
		}
	}

	static int **createPipes(int nPipes) {
		int **pipes = malloc (sizeof (int) * nPipes) ;
		int i = 0 ;
		for (; i<nPipes; ++i) {
			int *pipefd = malloc (2*sizeof(int)) ;/* create pipefd[2] - read and writer end */
			if (pipe(pipefd) == -1) {
				perror ("ERROR: pipe") ;
				exit(EXIT_FAILURE) ;
			}
			pipes[i] = pipefd ;
		}
		return pipes ;
	}

	static void releasePipes(int **pipes, int nPipes) {
		int i = 0 ;
		for (; i<nPipes; ++i) 
			free (pipes[i]) ;
		free (pipes) ;
	}

	static int *leftPipe(int **pipes, cmdLine *pCmdLine) {
		int *pipe = NULL ; /* answer */
		int idx = pCmdLine->idx ; 
		if (idx > 0)
			pipe = pipes[idx-1];
		return pipe ;
	}

	static int *rightPipe(int **pipes, cmdLine *pCmdLine) {
		int *pipe = NULL ; /* answer */
		int idx = pCmdLine->idx ; 
		if (pCmdLine->next != NULL)
			pipe = pipes[idx] ;
		return pipe ;
	}

	static int getSize (cmdLine *cmdl) {
		int ans = 1 ;
		cmdLine* curr = cmdl ;
		while (curr->next != NULL) {
			++ans ;
			curr = curr->next ;  
		}
		return ans ;
	}