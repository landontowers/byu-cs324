#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXEVENTS 64
#define MAXLINE 2048

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";
typedef struct request_info {
	int client_socket;
	int server_socket;
	int state;
	int client_bytes_read;
	int client_bytes_written;
	int server_bytes_to_write;
	int server_bytes_written;
	int server_bytes_read;
	char buf[MAX_OBJECT_SIZE];
} request_info;

int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
int open_sfd(int port);
void handle_new_clients(int epoll_fd, int socket_fd);
void handle_client(struct request_info * request);
void handle_client_READ_REQUEST(struct request_info * req);
void handle_client_SEND_REQUEST(struct request_info * req);
void handle_client_READ_RESPONSE(struct request_info * req);
void handle_client_SEND_RESPONSE(struct request_info * req);

enum State { READ_REQUEST, SEND_REQUEST, READ_RESPONSE, SEND_RESPONSE };

int main(int argc, char *argv[])
{
	int epoll_fd, socket_fd;
	struct epoll_event event, *events;

	if ((epoll_fd = epoll_create1(0)) < 0) {
		fprintf(stderr, "error creating epoll fd\n");
		exit(1);
	}

	socket_fd = open_sfd(atoi(argv[1]));

	event.events = EPOLLIN | EPOLLET;
	event.data.fd = socket_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) < 0) {
		printf("epoll_ctl error (1):  %s\n", strerror(errno)); fflush(stdout);
	}

	events = calloc(MAXEVENTS, sizeof(event));

	while(1) {
		int wait_result = epoll_wait(epoll_fd, events, 10, 1000);
		if (wait_result == 0) {
			continue;
		} else if (wait_result < 0) {
			printf("epoll_wait error:  %s\n", strerror(errno)); fflush(stdout);
			break;
		} else {
			for (int i=0; i<wait_result; i++) {
				if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (events[i].events & EPOLLRDHUP)) {
					/* An error has occured on this fd */
					fprintf (stderr, "epoll error");
					close(events[i].data.fd);
					continue;
				}

				if (0 == events[i].data.fd) {
					printf("(1) events[%d].data.fd == %d\n", i, events[i].data.fd); 
				}
				else if (socket_fd == events[i].data.fd) {
					printf("New Client\n"); 
					handle_new_clients(epoll_fd, socket_fd);
				} else {
					printf("Existing Client\n");
					handle_client(events[i].data.ptr);
				}
				fflush(stdout);
			}
		}
	}

	return 0;
}

int open_sfd(int port) {
	int epoll_fd;
	struct sockaddr_in ipv4addr;
	memset(&ipv4addr, 0, sizeof(struct sockaddr_in));
	ipv4addr.sin_family = AF_INET;
	ipv4addr.sin_addr.s_addr = INADDR_ANY;
	ipv4addr.sin_port = htons(port);

	if ((epoll_fd = socket(AF_INET, SOCK_STREAM, 0)) < -1) {
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}
	int optval = 1;
	setsockopt(epoll_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	fcntl(epoll_fd, F_SETFL, O_NONBLOCK);

	if (bind(epoll_fd, (struct sockaddr *)&ipv4addr, sizeof(struct sockaddr)) < 0) {
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	listen(epoll_fd, 100);
	printf("Listening on port %d\n", ntohs(ipv4addr.sin_port)); fflush(stdout);

	return epoll_fd;
}

void handle_new_clients(int epoll_fd, int socket_fd) {
	int new_fd;
	struct sockaddr_storage remote_addr;
	socklen_t remote_addr_len = sizeof(remote_addr);
	while (1) {
		new_fd = accept(socket_fd, (struct sockaddr *)&remote_addr, &remote_addr_len);
		if (new_fd < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			} else {
				printf("accept error: %s\n", strerror(errno)); fflush(stdout);
				exit(EXIT_FAILURE);
			}
		}

		if (fcntl(new_fd, F_SETFL, O_NONBLOCK) < 0) {
			fprintf(stderr, "error setting socket option\n");
			exit(1);
		}

		struct epoll_event event;
		event.events = EPOLLIN | EPOLLET;
		request_info req_info = {
			.client_socket = new_fd,
			.server_socket = socket_fd,
			.state = READ_REQUEST,
			.client_bytes_read = 0,
			.client_bytes_written = 0,
			.server_bytes_to_write = 0,
			.server_bytes_written = 0,
			.server_bytes_read = 0
		};
		event.data.ptr = &req_info;

		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &event) < 0) {
			printf("epoll_ctl error (2):  %s\n", strerror(errno)); fflush(stdout);
		}

		printf("New Client FD: %d\n", new_fd); fflush(stdout);
	}
}

void handle_client(struct request_info * req) {
	switch (req->state) {
		case READ_REQUEST:
			printf("READ_REQUEST\n");
			handle_client_READ_REQUEST(req);
			break;
		case SEND_REQUEST:
			printf("SEND_REQUEST\n");
			handle_client_SEND_REQUEST(req);
			break;
		case READ_RESPONSE:
			printf("READ_RESPONSE\n");
			handle_client_READ_RESPONSE(req);
			break;
		case SEND_RESPONSE:
			printf("SEND_RESPONSE\n");
			handle_client_SEND_RESPONSE(req);
			break;
		default:
			printf("Unknown state: %d\n", req->state); fflush(stdout);
	}
}

void handle_client_READ_REQUEST(struct request_info * req) {
	int n_read = recv(req->client_socket, &req->buf[req->client_bytes_read], MAXLINE, 0);

	if (n_read == 0) {
		req->state = SEND_REQUEST;
		handle_client(req);
		// close(req->client_socket);
	} else if (n_read < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			// no more data to be read
		} else {
			perror("client recv");
			close(req->client_socket);
		}
	} else {
		printf("Received %d bytes\n", n_read);
		printf("%s\n", req->buf);
		req->client_bytes_read += n_read;
	}
}

void handle_client_SEND_REQUEST(struct request_info * req) {
	int server_socket_copy = req->server_socket;
	char method[16], hostname[64], port[8], path[64], headers[1024];
	if (parse_request(req->buf, method, hostname, port, path, headers)) {
		char newRequest[MAX_OBJECT_SIZE];
		bzero(newRequest, MAX_OBJECT_SIZE);
		strcat(newRequest, method);
		strcat(newRequest, path);
		strcat(newRequest, " HTTP/1.0\r\nHost: ");
		strcat(newRequest, hostname);
		if (strcmp(port, "80")) {
			strcat(newRequest, ":");
			strcat(newRequest, port);
		}
		strcat(newRequest, "\r\nUser-Agent: ");
		strcat(newRequest, user_agent_hdr);
		strcat(newRequest, "\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n");

		req->server_bytes_to_write = strlen(newRequest);
		req->server_bytes_written = 0;
		req->server_socket = server_socket_copy;
		int n_sent;
		while (req->server_bytes_written < req->server_bytes_to_write) {
			printf("server_socket: %d\n", req->server_socket);
			printf("server_bytes_written: %d\n", req->server_bytes_written);
			printf("server_bytes_to_write: %d\n", req->server_bytes_to_write);
			n_sent = write(req->server_socket, &newRequest[req->server_bytes_written], 1);
			req->server_bytes_written += n_sent;
		}

		req->state = READ_RESPONSE;
		handle_client(req);
	} else {
		printf("REQUEST INCOMPLETE:\n%s\n\n", req->buf);
	}

}

void handle_client_READ_RESPONSE(struct request_info * req) {
	bzero(req->buf, MAX_OBJECT_SIZE);
	int n_read;
	printf("%d\n", req->server_socket);
	while ((n_read = read(req->server_socket, &req->buf[req->server_bytes_read], MAX_OBJECT_SIZE)) != 0){
		if (n_read == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		req->server_bytes_read += n_read;
	}
	handle_client(req);
}

void handle_client_SEND_RESPONSE(struct request_info * req) {

}

int all_headers_received(char *request) {
	return strstr(request, "\r\n\r\n") == NULL ? 0 : 1;
}

int parse_request(char *request, char *method, char *hostname, char *port, char *path, char *headers) {
	if (all_headers_received(request) == 0)
		return 0;

	bzero(method, 16);
	bzero(hostname, 64);
	bzero(port, 8);
	bzero(path, 64);
	bzero(headers, 1024);

	char * indexPtr = request;
	char * endPtr;
	int length;

	// METHOD
	endPtr = strstr(request, " ");
	length = endPtr - request;
	memcpy(method, request, length+1);

	// URL
	char URL[136];
	memset(URL, 0, 136);
	indexPtr = strstr(endPtr, "/")+2;
	endPtr = strstr(endPtr+1, " ");
	length = endPtr - indexPtr;
	memcpy(URL, indexPtr, length);
	
	// HOSTNAME
	if ((endPtr = strstr(URL, ":")) != NULL) {
		length = endPtr - URL;
		memcpy(hostname, URL, length);
	} else {
		if ((endPtr = strstr(URL, "/")) != NULL) {
			length = endPtr - URL;
			memcpy(hostname, URL, length);
		} else {
			printf("Error parsing hostname.\n"); fflush(stdout);
		}
	}

	// PORT
	indexPtr = strstr(URL, ":");
	if (indexPtr == NULL) {
		char * defaultPort = "80";
		memcpy(port, defaultPort, 2);
	} else {
		indexPtr++;
		endPtr = strstr(indexPtr, "/");
		length = endPtr - indexPtr;
		memcpy(port, indexPtr, length);
	}

	// PATH
	indexPtr = strstr(URL, "/");
	endPtr = &URL[strlen(URL)];
	length = endPtr - indexPtr;
	memcpy(path, indexPtr, length);

	// HEADERS
	indexPtr = strstr(request, "\r\n")+2;
	endPtr = strstr(request, "\r\n\r\n");
	length = endPtr - indexPtr;
	memcpy(headers, indexPtr, length);

	return 1;
}

void test_parser() {
	int i;
	char method[16], hostname[64], port[8], path[64], headers[1024];

       	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL
	};
	
	for (i = 0; reqs[i] != NULL; i++) {
		printf("Testing %s\n", reqs[i]);
		if (parse_request(reqs[i], method, hostname, port, path, headers)) {
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("HEADERS: %s\n", headers);
		} else {
			printf("REQUEST INCOMPLETE\n");
		}
	}
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
