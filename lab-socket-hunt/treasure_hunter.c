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
	unsigned char level = argv[3][0] - 48; // atoi(argv[3]); //htons(atoi(argv[3]));
	int seed = htonl(atoi(argv[4]));

	unsigned char first_request[FIRST_REQUEST_SIZE];
	memset(first_request, 0, FIRST_REQUEST_SIZE);
	memcpy(&first_request[1], &level, 1);
	memcpy(&first_request[2], &userid, 4);
	memcpy(&first_request[6], &seed, 2);

	// print_bytes(first_request, FIRST_REQUEST_SIZE);

	struct addrinfo hints;
	struct addrinfo *result;
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

	if (result == NULL) {   /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	int af;
	struct sockaddr_in ipv4addr_remote;
	struct sockaddr_in ipv4addr_local;
	struct sockaddr_in6 ipv6addr_remote;
	struct sockaddr_in6 ipv6addr_local;
	socklen_t addr_len;

	af = result->ai_family;
	sfd = socket(af, result->ai_socktype, 0);
	if (af == AF_INET) {
		ipv4addr_remote = *(struct sockaddr_in *)result->ai_addr;
		addr_len = sizeof(struct sockaddr_in);
		getsockname(sfd, (struct sockaddr *)&ipv4addr_local, &addr_len);
	} else {
		ipv6addr_remote = *(struct sockaddr_in6 *)result->ai_addr;
		addr_len = sizeof(struct sockaddr_in6);
		getsockname(sfd, (struct sockaddr *)&ipv6addr_local, &addr_len);
	}
	
	unsigned char response[RESPONSE_SIZE];
	ssize_t bytesRecieved;
	if (af == AF_INET) {
		if (sendto(sfd, first_request, FIRST_REQUEST_SIZE, 0, (struct sockaddr *) &ipv4addr_remote, addr_len) < 0) {
			perror("sendto()");
		}
		if ((bytesRecieved = recvfrom(sfd, response, RESPONSE_SIZE, 0, (struct sockaddr *) &ipv4addr_remote, &addr_len)) < 0) {
			perror("recvfrom()");
		}
	} else {
		if (sendto(sfd, first_request, FIRST_REQUEST_SIZE, 0, (struct sockaddr *) &ipv6addr_remote, addr_len) < 0) {
			perror("sendto()");
		}
		if ((bytesRecieved = recvfrom(sfd, response, RESPONSE_SIZE, 0, (struct sockaddr *) &ipv6addr_remote, &addr_len)) < 0) {
			perror("recvfrom()");
		}
	}

	freeaddrinfo(result);

	// print_bytes(response, bytesRecieved);

	unsigned char chunkLength = 0;
	unsigned char chunk[RESPONSE_SIZE];
	unsigned char opCode = 0;
	unsigned short opParam = 0;
	unsigned int nonce = 0;

	do {
		memcpy(&chunkLength, &response[0], 1);
		if (chunkLength == 0 || chunkLength >= 129){
			break;
		}
		memcpy(&chunk, &response[1], chunkLength);
		memcpy(&opCode, &response[chunkLength+1], 1);
		memcpy(&opParam, &response[chunkLength+2], 2);
		memcpy(&nonce, &response[chunkLength+4], 4);
		nonce = ntohl(nonce);
		nonce++;
		nonce = htonl(nonce);

		switch (opCode) {
			case 0:
				break;
			case 1:
				if (af == AF_INET) {
					ipv4addr_remote.sin_port = opParam;
				} else {
					ipv6addr_remote.sin6_port = opParam;
				}
				break;
			case 2:
				close(sfd);
				sfd = socket(af, SOCK_DGRAM, 0);
				if (af == AF_INET) {
					ipv4addr_local.sin_family = AF_INET; // use AF_INET (IPv4)
					ipv4addr_local.sin_port = opParam; // specific port
					ipv4addr_local.sin_addr.s_addr = 0; // any/all local addresses
					if (bind(sfd, (struct sockaddr *)&ipv4addr_local, addr_len) < 0) {
						perror("bind()");
					}
				} else {
					ipv6addr_local.sin6_family = AF_INET6; // IPv6 (AF_INET6)
					ipv6addr_local.sin6_port = opParam; // specific port
					bzero(ipv6addr_local.sin6_addr.s6_addr, 16); // any/all local addresses
					if (bind(sfd, (struct sockaddr *)&ipv6addr_local, addr_len) < 0) {
						perror("bind()");
					}
				}
				break;
			case 3:
				nonce = 0;
				opParam = ntohs(opParam);
				if (af == AF_INET) {
					struct sockaddr_in getPorts = ipv4addr_remote;
					for (int i=0; i<opParam; i++) {
					if ((bytesRecieved = recvfrom(sfd, response, RESPONSE_SIZE, 0, (struct sockaddr *) &getPorts, &addr_len)) < 0) {
						perror("recvfrom()");
					}
					nonce += ntohs(getPorts.sin_port);
				}
				} else {
					struct sockaddr_in6 getPorts = ipv6addr_remote;
					for (int i=0; i<opParam; i++) {
						if ((bytesRecieved = recvfrom(sfd, response, RESPONSE_SIZE, 0, (struct sockaddr *) &getPorts, &addr_len)) < 0) {
							perror("recvfrom()");
						}
						nonce += ntohs(getPorts.sin6_port);
					}
				}
				nonce++;
				nonce = htonl(nonce);
				break;
			case 4:
				if (af == AF_INET) {
					hints.ai_family = AF_INET6;
					sprintf(port, "%d", ntohs(opParam));
					s = getaddrinfo(server, port, &hints, &result);
					if (s != 0) {
						fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
						exit(EXIT_FAILURE);
					}
					if (result == NULL) {   /* No address succeeded */
						fprintf(stderr, "Could not connect\n");
						exit(EXIT_FAILURE);
					}
					af = result->ai_family;
					close(sfd);
					sfd = socket(af, result->ai_socktype, 0);
					ipv6addr_remote = *(struct sockaddr_in6 *)result->ai_addr;
					addr_len = sizeof(struct sockaddr_in6);
					getsockname(sfd, (struct sockaddr *)&ipv6addr_local, &addr_len);
				} else {
					hints.ai_family = AF_INET;
					sprintf(port, "%d", ntohs(opParam));
					s = getaddrinfo(server, port, &hints, &result);
					if (s != 0) {
						fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
						exit(EXIT_FAILURE);
					}
					if (result == NULL) {   /* No address succeeded */
						fprintf(stderr, "Could not connect\n");
						exit(EXIT_FAILURE);
					}
					af = result->ai_family;
					close(sfd);
					sfd = socket(af, result->ai_socktype, 0);
					ipv4addr_remote = *(struct sockaddr_in *)result->ai_addr;
					addr_len = sizeof(struct sockaddr_in);
					getsockname(sfd, (struct sockaddr *)&ipv4addr_local, &addr_len);
				}
				break;
			default:
				printf("Unknown opCode: %c\n", opCode);
				break;
		}

		memcpy(&treasure[treasureIndex], &chunk, chunkLength);
		treasureIndex += chunkLength;

		unsigned char request[REQUEST_SIZE];
		memcpy(&request, &nonce, 4);

		if (af == AF_INET) {
			if (sendto(sfd, request, REQUEST_SIZE, 0, (struct sockaddr *) &ipv4addr_remote, addr_len) < 0) {
				perror("sendto()");
			}
			if ((bytesRecieved = recvfrom(sfd, response, RESPONSE_SIZE, 0, (struct sockaddr *) &ipv4addr_remote, &addr_len)) < 0) {
				perror("recvfrom()");
			}
		} else {
			if (sendto(sfd, request, REQUEST_SIZE, 0, (struct sockaddr *) &ipv6addr_remote, addr_len) < 0) {
				perror("sendto()");
			}
			if ((bytesRecieved = recvfrom(sfd, response, RESPONSE_SIZE, 0, (struct sockaddr *) &ipv6addr_remote, &addr_len)) < 0) {
				perror("recvfrom()");
			}
		}

		//print_bytes(response, bytesRecieved);
	}
	while (chunkLength != 0);

	treasure[treasureIndex] = 0;

	printf("%s\n", treasure);

	close(sfd);
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
