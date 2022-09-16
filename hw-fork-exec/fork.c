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

	int p[2];
	pipe(p);

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

		// close(p[1]);
		// write(p[2], "hello from Section B\n", strlen("hello from Section B\n"));

		printf("Section B\n");
		// sleep(30);
		// sleep(30);
		// printf("Section B done sleeping\n");

		exit(0);
		
		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */
		fprintf(file, "SECTION C\n");
		fflush(file);

		close(p[2]);
		char *c;
		int bytesRead = read(p[1], c, 100);
		printf("bytes read: %c\n", bytesRead);
		printf("p[1]: %c\n", p[1]);
		// c[bytesRead] = '\0';
		// printf("%s", c);

		printf("Section C\n");
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

