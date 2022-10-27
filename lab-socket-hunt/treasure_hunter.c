// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823690496 // 12345
#define FIRST_REQUEST_SIZE 8
#define REQUEST_SIZE 4
#define RESPONSE_SIZE 256
#define TREASURE_SIZE 1024

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	unsigned char treasure[TREASURE_SIZE];
	int treasureIndex = 0;
	
	const int userid = htonl(USERID);
	char* server = argv[1];
	char* port = argv[2];
	int level = htons(atoi(argv[3]));
	int seed = htons(atoi(argv[4]));

	unsigned char first_request[FIRST_REQUEST_SIZE];
	memset(first_request, 0, FIRST_REQUEST_SIZE);
	memcpy(&first_request[1], &level, 1);
	memcpy(&first_request[2], &userid, 4);
	memcpy(&first_request[6], &seed, 2);

	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;    /* Allow IPv4, IPv6, or both, depending on
				    what was specified on the command line. */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;  /* Any protocol */

	s = getaddrinfo(server, port, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(AF_INET, SOCK_DGRAM, 0);
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

	freeaddrinfo(result);

	unsigned char response[RESPONSE_SIZE];

	send(sfd, first_request, FIRST_REQUEST_SIZE, 0);
	ssize_t bytesRecieved = recv(sfd, response, RESPONSE_SIZE, 0);

	// print_bytes(response, bytesRecieved);

	unsigned char chunkLength;
	unsigned char chunk[RESPONSE_SIZE];
	int opCode;
	unsigned short opParam;
	unsigned int nonce;

	do {
		memcpy(&chunkLength, &response[0], 1);
		memcpy(&chunk, &response[1], chunkLength);
		memcpy(&opCode, &response[chunkLength+1], 1);
		memcpy(&opParam, &response[chunkLength+2], 2);
		memcpy(&nonce, &response[chunkLength+4], 4);
		nonce = ntohl(nonce);
		++nonce;
		nonce = htonl(nonce);

		memcpy(&treasure[treasureIndex], &chunk, chunkLength);
		treasureIndex += chunkLength;

		unsigned char request[REQUEST_SIZE];
		memcpy(&request, &nonce, 4);

		send(sfd, request, REQUEST_SIZE, 0);
		bytesRecieved = recv(sfd, response, RESPONSE_SIZE, 0);
	}
	while (chunkLength != 0);
	treasure[treasureIndex] = 0;

	printf("%s\n", treasure);
}

void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}
