/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXCMDS      20   /* max commands on a command line */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
char sbuf[MAXLINE];         /* for composing sprintf messages */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Execute the shell's read/eval loop */
    while (1) {

        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (e.g., "quit")
 * then execute it immediately.
 *
 * Otherwise, build a pipeline of commands
 * run the job in the context of the child.  Have the parent wait for child process to complete and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    FILE *debug = fopen("debug.txt", "w+");

    char* argv[MAXARGS];
    memset(argv, 0, sizeof(argv));
    parseline(cmdline,argv);

    builtin_cmd(argv);

    int cmds[MAXCMDS];
    int stdin_redir[MAXCMDS];
    int stdout_redir[MAXCMDS];
    memset(cmds, 0, sizeof(cmds));
    memset(stdin_redir, 0, sizeof(stdin_redir));
    memset(stdout_redir, 0, sizeof(stdout_redir));
    int totalCmds = parseargs(argv, cmds, stdin_redir, stdout_redir);

    fprintf(debug, "argv: ");
    for (int i=0; i<MAXARGS; i++) {
        fprintf(debug, "%s - ", argv[i]);
    }
    fprintf(debug, "\n\ncmds: ");
    for (int i=0; i<MAXCMDS; i++) {
        fprintf(debug, "%d - ", cmds[i]);
    }
    fprintf(debug, "\n\nstdin_redir: ");
    for (int i=0; i<MAXCMDS; i++) {
        fprintf(debug, "%d - ", stdin_redir[i]);
    }
    fprintf(debug, "\n\nstdout_redir: ");
    for (int i=0; i<MAXCMDS; i++) {
        fprintf(debug, "%d - ", stdout_redir[i]);
    }

    if (totalCmds == 0) {
        // No commands!
    }
    else if (totalCmds == 1) {
        int childPid, c = 0;
        if ((childPid = fork()) == 0) {
            if (argv[cmds[c]]) {
                if (stdin_redir[c] > 1) {
                    FILE *file = fopen(argv[stdin_redir[c]], "r");
                    if (dup2(fileno(file), 0) < 0) {
                        fprintf(debug, "dup2 in stdin_redir failed");
                    }
                    close(fileno(file));
                }
                if (stdout_redir[c] > 1) {
                    FILE *file = fopen(argv[stdout_redir[c]], "w");
                    if (dup2(fileno(file), 1) < 0) {
                        fprintf(debug, "dup2 in stdout_redir failed");
                    }
                    close(fileno(file));
                }
            }

            if (execve(argv[c], argv, environ) < 0) {
                fprintf(debug, "%s: Command not found.\n", argv[c]);
            }

            exit(0);
        }
        else {
            if (setpgid(childPid, childPid) < 0) {
                fprintf(debug, "setpgid failed");
            }

            int status;
            if (waitpid(childPid, &status, 0) < 0) {
                fprintf(debug, "waitpid failed");
            }
        }
    } else {
        int pids[MAXCMDS], childCount = 0;
        for (int cmdCounter=0; cmdCounter < totalCmds-1; cmdCounter+=2) {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                fprintf(debug, "Pipe Failed");
            }

            if ((pids[childCount] = fork()) == 0) {
                if (argv[cmds[cmdCounter]]) {
                    if (stdin_redir[cmdCounter] > 1) {
                        FILE *file = fopen(argv[stdin_redir[cmdCounter]], "r");
                        if (dup2(fileno(file), 0) < 0) {
                            fprintf(debug, "dup2 in stdin_redir failed");
                        }
                        close(fileno(file));
                    }
                    if (stdout_redir[cmdCounter] > 1) {
                        FILE *file = fopen(argv[stdout_redir[cmdCounter]], "w");
                        if (dup2(fileno(file), 1) < 0) {
                            fprintf(debug, "dup2 in stdout_redir failed");
                        }
                        close(fileno(file));
                    }

                    if (dup2(pipefd[1], 1) < 0) {
                        fprintf(debug, "dup2 pipe write redirect failed");
                    }
                    close(pipefd[1]);
                    // if (dup2(pipefd[0], 0) < 0) {
                    //     fprintf(debug, "dup2 pipe read redirect failed");
                    // }
                    // close(pipefd[0]);
                }

                if (execve(argv[cmdCounter], argv, environ) < 0) {
                    fprintf(debug, "%s: Command not found.\n", argv[cmdCounter]);
                }

                exit(0);
            }
            else {
                if (setpgid(pids[childCount], pids[0]) < 0) {
                    fprintf(debug, "setpgid failed");
                }

                close(pipefd[1]);
            }
            childCount++;

            if ((pids[childCount] = fork()) == 0) {
                fprintf(debug, "\nFLAG\n");

                if (argv[cmds[cmdCounter+1]]) {
                    if (stdin_redir[cmdCounter+1] > 1) {
                        FILE *file = fopen(argv[stdin_redir[cmdCounter+1]], "r");
                        if (dup2(fileno(file), 0) < 0) {
                            fprintf(debug, "dup2 in stdin_redir failed");
                        }
                        close(fileno(file));
                    }
                    if (stdout_redir[cmdCounter+1] > 1) {
                        FILE *file = fopen(argv[stdout_redir[cmdCounter+1]], "w");
                        if (dup2(fileno(file), 1) < 0) {
                            fprintf(debug, "dup2 in stdout_redir failed");
                        }
                        close(fileno(file));
                    }

                    if (dup2(pipefd[0], 0) < 0) {
                        fprintf(debug, "dup2 pipe read redirect failed");
                    }
                    close(pipefd[0]);
                }

                fprintf(debug, "\n\nargv2: ");
                for (int i=cmds[cmdCounter+1]; i<MAXARGS; i++) {
                    fprintf(debug, "%s - ", argv[i]);
                }

                if (execve(argv[cmds[cmdCounter+1]], &argv[cmds[cmdCounter+1]], environ) < 0) {
                    fprintf(debug, "%s: Command not found.\n", argv[cmdCounter+1]);
                }

                exit(0);
            }
            else {
                if (setpgid(pids[childCount], pids[0]) < 0) {
                    fprintf(debug, "setpgid failed");
                }

                close(pipefd[1]);
                close(pipefd[0]);
            }
            childCount++;
            // /bin/cat test.txt | /bin/grep notlandontowers
        }

        int status;
        for (int i=0; i<childCount; i++) {
            if (waitpid(pids[i], &status, 0) < 0) {
                fprintf(debug, "waitpid failed");
            }
        }
    }

    fclose(debug);
    return;
}

/* 
 * parseargs - Parse the arguments to identify pipelined commands
 * 
 * Walk through each of the arguments to find each pipelined command.  If the
 * argument was | (pipe), then the next argument starts the new command on the
 * pipeline.  If the argument was < or >, then the next argument is the file
 * from/to which stdin or stdout should be redirected, respectively.  After it
 * runs, the arrays for cmds, stdin_redir, and stdout_redir all have the same
 * number of items---which is the number of commands in the pipeline.  The cmds
 * array is populated with the indexes of argv corresponding to the start of
 * each command sequence in the pipeline.  For each slot in cmds, there is a
 * corresponding slot in stdin_redir and stdout_redir.  If the slot has a -1,
 * then there is no redirection; if it is >= 0, then the value corresponds to
 * the index in argv that holds the filename associated with the redirection.
 * 
 */
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir) 
{
    int argindex = 0;    /* the index of the current argument in the current cmd */
    int cmdindex = 0;    /* the index of the current cmd */

    if (!argv[argindex]) {
        return 0;
    }

    cmds[cmdindex] = argindex;
    stdin_redir[cmdindex] = -1;
    stdout_redir[cmdindex] = -1;
    argindex++;
    while (argv[argindex]) {
        if (strcmp(argv[argindex], "<") == 0) {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex]) { /* if we have reached the end, then break */
                break;
	        }
            stdin_redir[cmdindex] = argindex;
        } else if (strcmp(argv[argindex], ">") == 0) {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex]) { /* if we have reached the end, then break */
                break;
            }
            stdout_redir[cmdindex] = argindex;
        } else if (strcmp(argv[argindex], "|") == 0) {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex]) { /* if we have reached the end, then break */
                break;
            }
            cmdindex++;
            cmds[cmdindex] = argindex;
            stdin_redir[cmdindex] = -1;
            stdout_redir[cmdindex] = -1;
        }
        argindex++;
    }

    return cmdindex + 1;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    if(strcmp(argv[0],"quit") == 0)  
        exit(0);
    return 0;     /* not a builtin command */
}

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

