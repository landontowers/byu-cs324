#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE 500

int main(int argc, char *argv[]) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s, j;
	size_t len;
	ssize_t nread;
	char buf[BUF_SIZE];
	int hostindex;
	int af;

	if (argc < 3 ||
		((strcmp(argv[1], "-4") == 0 || strcmp(argv[1], "-6") == 0) &&
			argc < 4)) {
		fprintf(stderr, "Usage: %s [ -4 | -6 ] host port msg...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Use only IPv4 (AF_INET) if -4 is specified;
	 * Use only IPv6 (AF_INET6) if -6 is specified;
	 * Try both if neither is specified. */
	af = AF_UNSPEC;
	if (strcmp(argv[1], "-4") == 0 ||
			strcmp(argv[1], "-6") == 0) {
		if (strcmp(argv[1], "-6") == 0) {
			af = AF_INET6;
		} else { // (strcmp(argv[1], "-4") == 0) {
			af = AF_INET;
		}
		hostindex = 2;
	} else {
		hostindex = 1;
	}

	/* Obtain address(es) matching host/port */

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = af;    /* Allow IPv4, IPv6, or both, depending on
				    what was specified on the command line. */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;  /* Any protocol */

	/* SECTION A - pre-socket setup; getaddrinfo() */

	s = getaddrinfo(argv[hostindex], argv[hostindex + 1], &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect(2).
	   If socket(2) (or connect(2)) fails, we (close the socket
	   and) try the next address. */

	/* SECTION B - pre-socket setup; getaddrinfo() */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;  /* Success */

		close(sfd);
	}

	if (rp == NULL) {   /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);   /* No longer needed */

	/* SECTION C - interact with server; send, receive, print messages */

	char buffer[BUF_SIZE];
	int bytesRead;
	int totalBytesRead = 0;
	while ((bytesRead = fread(&buffer[totalBytesRead], 1, BUF_SIZE, stdin)) != '\0') {
		totalBytesRead+=bytesRead;
	}

	int offset = 0;
	int bytesSent;
	while (offset < totalBytesRead) {
		bytesSent = write(sfd, buffer+offset, 5);
		offset += bytesSent;
		//printf("%d\n", bytesSent);
	}

	char buffer2[16384];
	totalBytesRead = 0;
	while ((bytesRead = read(sfd, buffer2+totalBytesRead, BUF_SIZE)) != 0){
		if (bytesRead == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		totalBytesRead+=bytesRead;
	}

	// printf("\n%d\n", totalBytesRead);
	// for (int i=0; i<totalBytesRead; i++) {
	// 	printf("%c", (char) buffer2[i]);
	// }
	// printf("\n"); fflush(stdout);

	offset = 0;
	while (offset < totalBytesRead) {
		// bytesSent = write(1, buffer2+offset, 5);
		bytesSent = fwrite(buffer2+offset, 1, totalBytesRead, stdout);
		offset += bytesSent; 
	}


	//printf("\n%d\n", totalBytesRead);
	// for (int i=0; i<totalBytesRead; i++) {
	// 	printf("%c", (char) buffer[i]);
	// }
	// printf("\n"); fflush(stdout);

	// /* Send remaining command-line arguments as separate
	//    datagrams, and read responses from server */
	// for (j = hostindex + 2; j < argc; j++) {
	// 	len = strlen(argv[j]) + 1;
	// 	/* +1 for terminating null byte */

	// 	if (len + 1 > BUF_SIZE) {
	// 		fprintf(stderr,
	// 				"Ignoring long message in argument %d\n", j);
	// 		continue;
	// 	}

	// 	if (write(sfd, argv[j], len) != len) {
	// 		fprintf(stderr, "partial/failed write\n");
	// 		exit(EXIT_FAILURE);
	// 	}

	// 	// nread = read(sfd, buf, BUF_SIZE);
	// 	// if (nread == -1) {
	// 	// 	perror("read");
	// 	// 	exit(EXIT_FAILURE);
	// 	// }

	// 	// printf("Received %zd bytes: %s\n", nread, buf);

	// }

	exit(EXIT_SUCCESS);
}
