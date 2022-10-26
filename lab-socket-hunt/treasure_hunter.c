// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823690496 // 12345
#define REQUEST_SIZE 8

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	const int userid = htonl(USERID);
	char* server = argv[1];
	int port = htons(atoi(argv[2]));
	int level = htons(atoi(argv[3]));
	int seed = htons(atoi(argv[4]));

	printf("server: %s\n", server);
	printf("port: %d\n", port);
	printf("level: %d\n", level);
	printf("seed: %d\n", seed);

	unsigned char request[REQUEST_SIZE];
	memset(request, 0, REQUEST_SIZE);
	memcpy(&request[1], &level, 1);
	memcpy(&request[2], &userid, 4);
	memcpy(&request[6], &seed, 2);

	// print_bytes(request, REQUEST_SIZE);

	
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
