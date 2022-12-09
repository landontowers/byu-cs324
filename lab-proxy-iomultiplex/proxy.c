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
void handle_client(struct request_info * request, int epoll_fd);
void handle_client_READ_REQUEST(struct request_info * req, int epoll_fd);
void handle_client_SEND_REQUEST(struct request_info * req, int epoll_fd);
void handle_client_READ_RESPONSE(struct request_info * req, int epoll_fd);
void handle_client_SEND_RESPONSE(struct request_info * req, int epoll_fd);
void print_request_info(struct request_info * req);

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
		int wait_result = epoll_wait(epoll_fd, events, MAXEVENTS, 1000);
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
					// printf("New Client\n"); 
					handle_new_clients(epoll_fd, socket_fd);
				} else {
					// printf("Existing Client\n");
					handle_client(events[i].data.ptr, epoll_fd);
				}
				fflush(stdout);
			}
		}
	}

	return 0;
}

int open_sfd(int port) {
	int socket_fd;
	struct sockaddr_in ipv4addr;
	memset(&ipv4addr, 0, sizeof(struct sockaddr_in));
	ipv4addr.sin_family = AF_INET;
	ipv4addr.sin_addr.s_addr = INADDR_ANY;
	ipv4addr.sin_port = htons(port);

	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < -1) {
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}
	int optval = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) | O_NONBLOCK);

	if (bind(socket_fd, (struct sockaddr *)&ipv4addr, sizeof(struct sockaddr)) < 0) {
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	listen(socket_fd, 100);
	printf("Listening on port %d\n", ntohs(ipv4addr.sin_port)); fflush(stdout);

	return socket_fd;
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

		if (fcntl(new_fd, F_SETFL, fcntl(new_fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
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

void handle_client(struct request_info * req, int epoll_fd) {
	switch (req->state) {
		case READ_REQUEST:
			printf("READ_REQUEST\n");
			handle_client_READ_REQUEST(req, epoll_fd);
			break;
		case SEND_REQUEST:
			printf("SEND_REQUEST\n");
			handle_client_SEND_REQUEST(req, epoll_fd);
			break;
		case READ_RESPONSE:
			printf("READ_RESPONSE\n");
			handle_client_READ_RESPONSE(req, epoll_fd);
			break;
		case SEND_RESPONSE:
			printf("SEND_RESPONSE\n");
			handle_client_SEND_RESPONSE(req, epoll_fd);
			break;
		default:
			printf("Unknown state: %d\n", req->state); fflush(stdout);
	}
}

void handle_client_READ_REQUEST(struct request_info * req, int epoll_fd) {
	int n_read;
	int client_socket_backup = req->client_socket;

	while ((n_read = recv(req->client_socket, &req->buf[req->client_bytes_read], MAXLINE, 0)) > 0) {
		printf("Received %d bytes\n", n_read);
		req->client_bytes_read += n_read;
	}
	if (n_read < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			// no more data to be read
			printf("Try again later...\n");
		} else {
			perror("client recv");
			close(req->client_socket);
		}
	} else {
		char method[16], hostname[64], port[8], path[64], headers[1024], newRequest[MAX_OBJECT_SIZE];
		if (parse_request(req->buf, method, hostname, port, path, headers)) {
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
		} else {
			printf("REQUEST INCOMPLETE:\n%s\n\n", req->buf);
			exit(EXIT_FAILURE);
		}
		sprintf(req->buf, "%s", newRequest);

		struct addrinfo hints;
		struct addrinfo *result, *rp;
		int s, sfd2;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if ((s=getaddrinfo(hostname, port, &hints, &result)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
			exit(EXIT_FAILURE);
		}

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			sfd2 = socket(AF_INET, SOCK_STREAM, 0);
			if (sfd2 == -1)
				continue;
			if (connect(sfd2, rp->ai_addr, rp->ai_addrlen) != -1)
				break;
			close(sfd2);
		}

		if (rp == NULL) {   /* No address succeeded */
			fprintf(stderr, "Could not connect\n");
			exit(EXIT_FAILURE);
		}

		freeaddrinfo(result);

		fcntl(sfd2, F_SETFL, fcntl(sfd2, F_GETFL, 0) | O_NONBLOCK);

		req->server_bytes_to_write = (int) strlen(newRequest);
		req->state = SEND_REQUEST;
		req->server_socket = sfd2;
		req->server_bytes_written = 0;
		req->client_socket = client_socket_backup;

		struct epoll_event event;
		event.events = EPOLLOUT | EPOLLET;
		event.data.fd = sfd2;
		event.data.ptr = req;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sfd2, &event) < 0) {
			printf("epoll_ctl error (1):  %s\n", strerror(errno)); fflush(stdout);
		}
	}
}

void handle_client_SEND_REQUEST(struct request_info * req, int epoll_fd) {
	int n_sent;
	while (req->server_bytes_written < req->server_bytes_to_write) {
		if ((n_sent = write(req->server_socket, &req->buf[req->server_bytes_written], 1)) < 0) {
			perror("write");
			exit(EXIT_FAILURE);
		}
		req->server_bytes_written += n_sent;
	}
	req->state = READ_RESPONSE;
	req->server_bytes_read = 0;
	struct epoll_event event;
	event.events = EPOLLIN | EPOLLET;
	event.data.fd = req->server_socket;
	event.data.ptr = req;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, req->server_socket, &event) < 0) {
		printf("epoll_ctl error (1):  %s\n", strerror(errno)); fflush(stdout);
	}
	// handle_client(req, epoll_fd);
}

void handle_client_READ_RESPONSE(struct request_info * req, int epoll_fd) {
	int server_socket = req->server_socket;
	int client_socket = req->client_socket;
	int server_bytes_read = req->server_bytes_read;
	int state = req->state;

	int n_read;
	while ((n_read = read(server_socket, &req->buf[server_bytes_read], MAX_OBJECT_SIZE)) != 0){
		if (n_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			} else {
				perror("read");
				exit(EXIT_FAILURE);
			}
		}
		server_bytes_read += n_read;
		req->server_socket = server_socket;
		req->client_socket = client_socket;
		req->server_bytes_read = server_bytes_read;
		req->state = state;
		// print_request_info(req);
	}
	printf("server_bytes_read: %d\n", server_bytes_read);
	if (n_read == 0) {
		// print_request_info(req);
		req->server_socket = server_socket;
		req->client_socket = client_socket;
		req->state = SEND_RESPONSE;
		struct epoll_event event;
		event.events = EPOLLOUT | EPOLLET;
		event.data.fd = req->client_socket;
		event.data.ptr = req;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, req->client_socket, &event) < 0) {
			printf("epoll_ctl error (1):  %s\n", strerror(errno)); fflush(stdout);
		}
	}
}

void handle_client_SEND_RESPONSE(struct request_info * req, int epoll_fd) {
	int client_socket = req->client_socket;
	print_request_info(req);
	int to_send = strlen(req->buf)+1, n_sent;
	printf("to_send: %d\n", to_send);
	req->client_bytes_written = 0;
	while (req->client_bytes_written < to_send) {
		printf("client socket: %d\n", client_socket);
		printf("written: %d\n",req->client_bytes_written);
		if ((n_sent = write(client_socket, &req->buf[req->client_bytes_written], 1)) < 0) {
			perror("write");
			exit(EXIT_FAILURE);
		}
		req->client_bytes_written += n_sent;
	}
	printf("client_bytes_written: %d\n", req->client_bytes_written);
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

void print_request_info(struct request_info * req) {
	printf("\n");
	printf("CLIENT_SOCKET: %d\n", req->client_socket);
	printf("SERVER_SOCKET: %d\n", req->server_socket);
	printf("STATE: %d\n", req->state);
	printf("CLIENT_BYTES_READ: %d\n", req->client_bytes_read);
	printf("CLIENT_BYTES_WRITTEN: %d\n", req->client_bytes_written);
	printf("SERVER_BYTES_TO_WRITE: %d\n", req->server_bytes_to_write);
	printf("SERVER_BYTES_WRITTEN: %d\n", req->server_bytes_written);
	printf("SERVER_BYTES_READ: %d\n", req->server_bytes_read);
	printf("BUF:\n%s\n", req->buf);
	fflush(stdout);
}