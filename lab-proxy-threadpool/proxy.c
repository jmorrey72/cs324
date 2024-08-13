#include "sbuf.h"
#include "sbuf.c"
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

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define BUF_SIZE 102400
#define NTHREADS  8
#define SBUFSIZE  5

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";
const char *http = "http://";
const char *httpport = ":80\0";

int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
int open_sfd(int);
void handle_client(int);
void *thread_function(void *vargp);

sbuf_t sbuf;

int main(int argc, char *argv[])
{
	// test_parser();
	int sfd, i, connfd;
	// int socketfd;
	// struct sockaddr_storage remote_addr;
	// socklen_t remote_addr_len;
	// int *connfdp;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;
	// remote_addr_len = sizeof(struct sockaddr_storage);
	sfd = open_sfd(atoi(argv[1]));
	// while(1) {
	// 	socketfd = accept(sfd, (struct sockaddr *) &remote_addr, &remote_addr_len);
	// 	handle_client(socketfd);
	// }

	sbuf_init(&sbuf, SBUFSIZE); 
	for (i = 0; i < NTHREADS; i++)  /* Create worker threads */
		pthread_create(&tid, NULL, thread_function, NULL);

	while (1) {
		clientlen = sizeof(struct sockaddr_storage);
		// connfdp = malloc(sizeof(int)); //line:conc:echoservert:beginmalloc
		connfd = accept(sfd, (struct sockaddr *) &clientaddr, &clientlen); //line:conc:echoservert:endmalloc
		// pthread_create(&tid, NULL, thread_function, connfdp);
		sbuf_insert(&sbuf, connfd);
	}

	printf("%s\n", user_agent_hdr);
	return 0;
}

void *thread_function(void *vargp) {  
	// int connfd = *((int *)vargp);
	pthread_detach(pthread_self()); //line:conc:echoservert:detach
	// free(vargp);                    //line:conc:echoservert:free
	// handle_client(connfd);
	while (1) { 
		int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ //line:conc:pre:removeconnfd
		handle_client(connfd);                /* Service client */
		// close(connfd);
	}
	// close(connfd);
	return NULL;
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

	if (bind(sfd, local_addr, local_addr_len) < 0) {
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	listen(sfd, 100);

	return sfd;
}

void handle_client(int socketfd) {
	char method[16], hostname[64], port[8], path[64], headers[1024], request[BUF_SIZE], buf[BUF_SIZE];
	int location = 0;
	bzero(request, BUF_SIZE);
	bzero(buf, BUF_SIZE);

	while (1) {
		int nread = recv(socketfd, buf, 2048, 0);
		if (nread == -1) {
			continue;   /* Ignore failed request */
		} else {
			memcpy(&request[location], buf, nread);
			location += nread;
		}

		if (parse_request(request, method, hostname, port, path, headers)) {
			// close(socketfd);
			break;
		}
	}
	printf("Testing %s\n", request);
	printf("METHOD: %s\n", method);
	printf("HOSTNAME: %s\n", hostname);
	printf("PORT: %s\n", port);
	printf("HEADERS: %s\n", headers);

	// Start creating server request

	char httprequest[BUF_SIZE];
	bzero(httprequest, BUF_SIZE);
	if (strcmp(port, ":80\0") == 0) {
		sprintf(httprequest, "%s %s HTTP/1.0\r\n"
		"Host: %s\r\n"
		"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0\r\n"
		"Connection: close\r\n"
		"Proxy-Connection: close\r\n\r\n"
		, method, path, &hostname[7]);
	} else {
		sprintf(httprequest, "%s %s HTTP/1.0\r\n"
		"Host: %s%s\r\n"
		"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0\r\n"
		"Connection: close\r\n"
		"Proxy-Connection: close\r\n\r\n"
		, method, path, &hostname[7], port);
	}
	printf("%s\n", httprequest);

	// Start server communication

	int s;
	char portAsStr[8];
	char response[BUF_SIZE];
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	bzero(response, BUF_SIZE);

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

	int sockfd;

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sockfd == -1)
			continue;

		if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;  /* Success */

		close(sockfd);
	}

	int nsent = 0;
	int requestlocation = 0;

	nsent = send(sockfd, &httprequest[requestlocation], strlen(httprequest), 0);
		
	int numrecvd = 0;
	int response_location = 0;
	while (1) {
		numrecvd = recv(sockfd, &response[response_location], 124, 0);
		if (numrecvd == 0) {
			close(sockfd);	
			break;
		} else if (numrecvd < 0) {
			perror("recv");
		} else {
			response_location += numrecvd;
		}
	}

		nsent = send(socketfd, response, response_location, 0);
		if (nsent < 0) {
			perror("send");
		}

	close(socketfd);

	printf("Printing server response:\n");
	printf("%s", response);

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
