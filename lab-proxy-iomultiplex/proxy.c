#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXEVENTS 64
#define READ_REQUEST 1
#define SEND_REQUEST 2
#define READ_RESPONSE 3
#define SEND_RESPONSE 4

// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";
const char *http = "http://";
const char *httpport = ":80\0";
int intflag = 0;

struct client_info {
	int fd;
	int serverfd;
	char desc[1024];
	int state;
	int location;
	int requestsize;
	int nsent;
	int amountsent;
	int responsesize;
	char request[MAX_OBJECT_SIZE];
	char server_request[MAX_OBJECT_SIZE];
	char response[MAX_OBJECT_SIZE];
	char server_response[MAX_OBJECT_SIZE];
};

int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
int open_sfd(int);
void handle_new_clients(int, int);
void handle_client(struct client_info *, int);

void sig_handler1() {
	intflag = 1;
}


int main(int argc, char* argv[])
{
	int efd;
	int listensfd;
	int n;
	int port;
	struct epoll_event event;
	struct epoll_event *events;

	struct client_info *listener;
	struct client_info *active_client;

	struct sigaction sigact;

	sigact.sa_handler = sig_handler1;
	sigaction(SIGINT, &sigact, NULL);

	port = atoi(argv[1]);
	// port = 47206;

	listensfd = open_sfd(port);

	if ((efd = epoll_create1(0)) < 0) {
		fprintf(stderr, "error creating epoll fd\n");
		exit(1);
	}

	listener = malloc(sizeof(struct client_info));
	listener->fd = listensfd;
	sprintf(listener->desc, "Listen file descriptor (accepts new clients)");

	event.data.ptr = listener;
	event.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, listensfd, &event) < 0) {
		fprintf(stderr, "error adding event\n");
		exit(1);
	}

	/* Buffer where events are returned */
	events = calloc(MAXEVENTS, sizeof(struct epoll_event));

	while (1) {
		// wait for event to happen (-1 == no timeout)
		n = epoll_wait(efd, events, MAXEVENTS, 1000);

		if (n == 0) {
			// check for global flag
			if (intflag == 1) {
				break;
			}
			continue;
		} else if (n < 0) {
			// Handle Errors
			if (errno == EBADF) {
				fprintf(stderr, "efd is not a valid file descriptor\n");
				break;
			} else if (errno == EFAULT) {
				fprintf(stderr, "The memory area pointed to by events is not accessible with write permissions.\n");
				break;
			} else if (errno == EINVAL) {
				fprintf(stderr, "efd is not an epoll file descriptor, or maxevents is less than or equal to zero.\n");
				break;
			}
			continue;
		} else {
			for (int i = 0; i < n; i++) {
				// grab the data structure from the event, and cast it
				// (appropriately) to a struct client_info *.
				active_client = (struct client_info *)(events[i].data.ptr);

				printf("New event for %s\n", active_client->desc);

				if ((events[i].events & EPOLLERR) ||
						(events[i].events & EPOLLHUP) ||
						(events[i].events & EPOLLRDHUP)) {
					/* An error has occured on this fd */
					fprintf(stderr, "epoll error on %s\n", active_client->desc);
					close(active_client->fd);
					free(active_client);
					continue;
				}

				if (listensfd == active_client->fd) {
					handle_new_clients(efd, active_client->fd);
				} else {
					handle_client(active_client, efd);
				}
			}
		}
	}

	free(listener);
	free(events);

	// test_parser();
	// printf("%s\n", user_agent_hdr);
	return 0;
}

void handle_client(struct client_info * active_client, int efd) {
	fprintf(stdout, "Entering handle_client with %s\nwith state: %d\n", active_client->desc, active_client->state);
	fflush(stdout);
	if (active_client->state == READ_REQUEST) {

		char method[16], hostname[64], port[8], path[64], headers[1024], buf[MAX_OBJECT_SIZE], request[MAX_OBJECT_SIZE];
		bzero(buf, MAX_OBJECT_SIZE);
		bzero(request, MAX_OBJECT_SIZE);

		while (1) {
			int nread = recv(active_client->fd, buf, 4096, 0);
			if (nread == -1) {
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					break;
				} else {
					perror("recv from client");
				}
			} else {
				memcpy(&active_client->request[active_client->location], buf, nread);
				active_client->location += nread;
				printf("%s\n", buf);
			}
		}

		memcpy(request, active_client->request, active_client->location);

		if (parse_request(request, method, hostname, port, path, headers)) {
			printf("Testing %s\n", request);
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("HEADERS: %s\n", headers);
			int reqsize = 0;
			char httprequest[MAX_OBJECT_SIZE];
			bzero(httprequest, MAX_OBJECT_SIZE);
			if (strcmp(port, ":80\0") == 0) {
				reqsize = sprintf(httprequest, "%s %s HTTP/1.0\r\n"
				"Host: %s\r\n"
				"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0\r\n"
				"Connection: close\r\n"
				"Proxy-Connection: close\r\n\r\n"
				, method, path, &hostname[7]);
			} else {
				reqsize = sprintf(httprequest, "%s %s HTTP/1.0\r\n"
				"Host: %s%s\r\n"
				"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0\r\n"
				"Connection: close\r\n"
				"Proxy-Connection: close\r\n\r\n"
				, method, path, &hostname[7], port);
			}
			printf("%s\n", httprequest);

			memset(active_client->server_request, 0, sizeof(active_client->server_request));
			memcpy(active_client->server_request, httprequest, reqsize);
			active_client->requestsize = reqsize;

			int s;
			char portAsStr[8];
			struct addrinfo hints;
			struct addrinfo *result, *rp;

			strcpy(portAsStr, &port[1]);

			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = AF_INET;    
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = 0;
			hints.ai_protocol = 0;  
			
			s = getaddrinfo(&hostname[7], portAsStr, &hints, &result);
			if (s != 0) {
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
				exit(EXIT_FAILURE);
			}

			int serverfd;

			for (rp = result; rp != NULL; rp = rp->ai_next) {
				serverfd = socket(rp->ai_family, rp->ai_socktype,
						rp->ai_protocol);
				if (serverfd == -1)
					continue;

				if (connect(serverfd, rp->ai_addr, rp->ai_addrlen) != -1)
					break;  /* Success */

				close(serverfd);
			}

			if (fcntl(serverfd, F_SETFL, fcntl(serverfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
				fprintf(stderr, "error setting socket option\n");
				exit(1);
			}

			active_client->serverfd = serverfd;
			struct epoll_event event;

			event.data.ptr = active_client;
			event.events = EPOLLOUT | EPOLLET;
			if (epoll_ctl(efd, EPOLL_CTL_ADD, active_client->serverfd, &event) < 0) {
				fprintf(stderr, "error modifying event\n");
				exit(1);
			}

			active_client->location = 0;
			active_client->state = SEND_REQUEST;

		}
	} if (active_client->state == SEND_REQUEST) {
		printf("Entering State 2\n");
		int numsent;
		while (1) {
			if (active_client->nsent >= active_client->requestsize) {
				break;
			}
			numsent = send(active_client->serverfd, &active_client->server_request[active_client->nsent], active_client->requestsize - active_client->nsent, 0);
			if (numsent < 0) {
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					break;
				} else {
					perror("send to server");
				}
			} else {
				active_client->nsent += numsent;
			}
		}
		if (active_client->nsent >= active_client->requestsize) {
			struct epoll_event event;

			event.data.ptr = active_client;
			event.events = EPOLLIN | EPOLLET;
			if (epoll_ctl(efd, EPOLL_CTL_MOD, active_client->serverfd, &event) < 0) {
				fprintf(stderr, "error modifying event\n");
				exit(1);
			}

			active_client->state = READ_RESPONSE;
		}
	} if (active_client->state == READ_RESPONSE) {
		printf("Entering State 3\n");
		char response[MAX_OBJECT_SIZE];
		bzero(response, MAX_OBJECT_SIZE);
		int numrecvd = 0;
		while (1) {
			numrecvd = recv(active_client->serverfd, response, 4096, 0);
			if (numrecvd == 0) {
				struct epoll_event event;

				event.data.ptr = active_client;
				event.events = EPOLLOUT | EPOLLET;
				if (epoll_ctl(efd, EPOLL_CTL_MOD, active_client->fd, &event) < 0) {
					fprintf(stderr, "error modifying event\n");
					exit(1);
				}
				close(active_client->serverfd);
				active_client->state = SEND_RESPONSE;
				break;
			} else if (numrecvd < 0) {
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					break;
				} else {
					perror("send to server");
				}
			} else {
				memcpy(&active_client->server_response[active_client->responsesize], response, numrecvd);
				active_client->responsesize += numrecvd;
				// fprintf(stdout, "%s\n", response);
			}
		}
	} if (active_client->state == SEND_RESPONSE) {
		printf("Entering State 4\n");
		int numsent = 0;
		int responseleft = active_client->responsesize - active_client->amountsent;
		while (1) {
			numsent = send(active_client->fd, &active_client->server_response[active_client->amountsent], active_client->responsesize - active_client->amountsent, 0);
			if (numsent < 0) {
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					break;
				} else {
					perror("send to client");
					exit(1);
				}
			} else if (numsent >= responseleft) {
				close(active_client->fd);
				free(active_client);
				break;
			} else {
				active_client->amountsent += numsent;
			}
		}
	}
}

void handle_new_clients(int efd, int listensfd) {
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	struct client_info *new_client;
	struct epoll_event event;

	int connfd;

	clientlen = sizeof(struct sockaddr_storage);
	while (1) {
		connfd = accept(listensfd, (struct sockaddr *)&clientaddr, &clientlen);

		if (connfd < 0) {
			if (errno == EWOULDBLOCK ||
					errno == EAGAIN) {
				// no more clients ready to accept
				break;
			} else {
				perror("accept");
				exit(EXIT_FAILURE);
			}
		}

		fprintf(stdout, "New client file Descriptor: %d\n", connfd);

		// set client file descriptor non-blocking
		if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
			fprintf(stderr, "error setting socket option\n");
			exit(1);
		}

		// allocate memory for a new struct
		// client_info, and populate it with
		// info for the new client
		new_client = (struct client_info *)malloc(sizeof(struct client_info));
		new_client->fd = connfd;
		sprintf(new_client->desc, "Client with file descriptor %d", connfd);
		new_client->state = READ_REQUEST;
		new_client->location = 0;
		new_client->nsent = 0;
		new_client->location = 0;
		new_client->amountsent = 0;
		new_client->responsesize = 0;

		// register the client file descriptor
		// for incoming events using
		// edge-triggered monitoring
		event.data.ptr = new_client;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event) < 0) {
			fprintf(stderr, "error adding event\n");
			exit(1);
		}
	}
}

int open_sfd(int port) {
	struct sockaddr_in ipv4addr;
	int sfd;
	struct sockaddr *local_addr;
	socklen_t local_addr_len;

	ipv4addr.sin_family = AF_INET;
	ipv4addr.sin_addr.s_addr = INADDR_ANY; // listen on any/all IPv4 addresses
	ipv4addr.sin_port = htons(port);       // specify port explicitly, in network byte order

		// Assign local_addr and local_addr_len to ipv4addr
	local_addr = (struct sockaddr *)&ipv4addr;
	local_addr_len = sizeof(ipv4addr);

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < -1) {
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}

	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	int status = fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);
	if (status == -1) {
		perror("fcntl()");
	}

	if (bind(sfd, local_addr, local_addr_len) < 0) {
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	listen(sfd, 100);

	return sfd;
}

int all_headers_received(char *request) {
	if (strstr(request, "\r\n\r\n") == NULL) {
		return 0;
	} else {
		return 1;
	}
}

int parse_request(char *request, char *method, char *hostname, char *port, char *path, char *headers) {
	unsigned int srcLocation = 0;
	unsigned int destLocation = 0;
	bzero(method, 16);
	bzero(port, 8);
	bzero(hostname, 64);
	bzero(path, 64);
	bzero(headers, 1024);
	if (all_headers_received(request)) {
		// copy method
		while (1) {
			if (request[srcLocation] != ' ') {
				method[destLocation] = request[srcLocation];
				srcLocation++;
				destLocation++;
			} else {
				srcLocation++;
				break;
			}
		}
		method[destLocation] = '\0';
		destLocation = 7;
		// copy hostname
		srcLocation += 7;
		strcpy(hostname, http);
		while (1) {
			if (request[srcLocation] == '/') {
				strcpy(port, httpport);
				// srcLocation++;
				break;
			} else if (request[srcLocation] == ':') {
				port[0] = ':';
				unsigned int portindex = 1;
				srcLocation++;
				while(isdigit(request[srcLocation])) {
					port[portindex] = request[srcLocation];
					portindex++;
					srcLocation++;
				}
				break;
			} else {
			hostname[destLocation] = request[srcLocation];
			srcLocation++;
			destLocation++;
			}
		}
		hostname[destLocation] = '\0';
		destLocation = 0;
		// copy path
		while (1) {
			if (request[srcLocation] != ' ') {
				path[destLocation] = request[srcLocation];
				srcLocation++;
				destLocation++;
			} else {
				srcLocation++;
				break;
			}
		}
		path[destLocation] = '\0';
		// copy headers
		char * tempheaders = strstr(request, "\r\n");
		strcpy(headers, &tempheaders[2]);
		return 1;
	} else {
		return 0;
	}
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
