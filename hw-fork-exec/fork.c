#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

#include<sys/types.h>
#include<sys/wait.h>
#include<string.h>

int main(int argc, char *argv[]) {
	int pid;

	printf("Starting program; process has pid %d\n", getpid());

	FILE *file = fopen("fork-output.txt", "w");
	fprintf(file, "BEFORE FORK\n");
	fflush(file);

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */
	fprintf(file, "SECTION A\n");
	fflush(file);

	printf("Section A;  pid %d\n", getpid());
	// sleep(30);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
		fprintf(file, "SECTION B\n");
		fflush(file);

		close(pipefd[0]);
		write(pipefd[1], "hello from Section B\n", strlen("hello from Section B\n")+1);

		printf("Section B\n");
		// sleep(30);
		// sleep(30);
		// printf("Section B done sleeping\n");

		// EXEC.C BEGIN
		char *newenviron[] = { NULL };

		printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
		// sleep(30);

		if (argc <= 1) {
			printf("No program to exec.  Exiting...\n");
			exit(0);
		}

		printf("Running exec of \"%s\"\n", argv[1]);
		dup2(fileno(file), 1);
		execve(argv[1], &argv[1], newenviron);
		printf("End of program \"%s\".\n", argv[0]);
		//EXEC.C END

		exit(0);
		
		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */
		fprintf(file, "SECTION C\n");
		fflush(file);
		printf("Section C\n");

		close(pipefd[1]);
		char c[100];
		int bytesRead = read(pipefd[0], c, 100);
		c[bytesRead] = '\0';
		printf("%s", c);

		// sleep(30);
		int status; wait(&status);
		// sleep(30);
		// printf("Section C done sleeping\n");

		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */
	fprintf(file, "SECTION D\n");
	fflush(file);

	printf("Section D\n");
	// sleep(30);

	/* END SECTION D */
}

