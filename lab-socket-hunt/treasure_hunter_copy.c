// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823701285
#define BUFSIZE 8
#define RESPONSE_SIZE 256
#define MAX_CHUNK_SIZE 127
#define MAX_TREASURE_SIZE 1024

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	struct addrinfo hints;
	struct addrinfo *result;
	int socketfd, s;
	unsigned char buf[BUFSIZE];
	unsigned char response_buffer[RESPONSE_SIZE];
	unsigned char treasure[MAX_TREASURE_SIZE];
	unsigned int treasure_length = 0;
	int af;
	char * server;
	// int port;
	int level;
	int zero = 0;
	unsigned short seed;
	unsigned char chunk_length, op_code;
	unsigned char chunk[MAX_CHUNK_SIZE];
	unsigned short op_param;
	unsigned int nonce;
	socklen_t server_addr_len;
	struct sockaddr_in ipv4addr_remote;
	struct sockaddr_in ipv4addr_local;
	struct sockaddr_in6 ipv6addr_remote;
	struct sockaddr_in6 ipv6addr_local;
	unsigned int portSum = 0;

	af = AF_INET;

	server = argv[1];
	// port = atoi(argv[2]);
	char* portAsStr = argv[2];
	level = atoi(argv[3]);
	seed = atoi(argv[4]);
	unsigned int userid = htonl(USERID);
	unsigned short seed_nbo = htons(seed);

	// printf("server: %s, port: %d, level: %d, seed: %d", server, port, level, seed);

	bzero(buf, BUFSIZE);
	bzero(response_buffer, RESPONSE_SIZE);
	bzero(chunk, MAX_CHUNK_SIZE);
	bzero(treasure, MAX_TREASURE_SIZE);

	memcpy(&buf[0], &zero, 1);
	memcpy(&buf[1], &level, 1);
	memcpy(&buf[2], &userid, 4);
	memcpy(&buf[6], &seed_nbo, 2);

	// print_bytes(buf, 8);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = af;    /* Allow IPv4, IPv6, or both, depending on
				    what was specified on the command line. */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;  /* Any protocol */

	s = getaddrinfo(server, portAsStr, &hints, &result);
	if (s != 0) {
		perror("getaddrinfo");
	}

	socketfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	server_addr_len = result->ai_addrlen;
	ipv4addr_remote = *(struct sockaddr_in *)result->ai_addr;

	freeaddrinfo(result);   
	sendto(socketfd, buf, BUFSIZE, 0, result->ai_addr, server_addr_len);
	recvfrom(socketfd, response_buffer, RESPONSE_SIZE, 0, NULL, NULL);	

	while(1) {
		chunk_length = response_buffer[0];

		if (chunk_length == 0) {
			break;
		}

		memcpy(&chunk[0], &response_buffer[1], chunk_length);
		op_code = response_buffer[chunk_length + 1];
		memcpy(&op_param, &response_buffer[chunk_length + 2], 2);
		memcpy(&nonce, &response_buffer[chunk_length + 4], 4);
		chunk[chunk_length + 1] = '\0';
		memcpy(&treasure[treasure_length], &chunk[0], chunk_length + 1);
		treasure_length += chunk_length;
		unsigned short op_param_host_order = ntohs(op_param);
		unsigned int nonce_host_order = ntohl(nonce);

		// printf("chunk length: %d, op code: %x, op param: %x, nonce: %x", chunk_length, op_code, op_param_host_order, nonce_host_order);
		// print_bytes(chunk, chunk_length);

		unsigned char nonce_buf[4];
		bzero(nonce_buf, 4);
		bzero(response_buffer, RESPONSE_SIZE);
		bzero(chunk, MAX_CHUNK_SIZE);
		nonce = htonl(nonce_host_order + 1);
		memcpy(&nonce_buf[0], &nonce, 4);
		if (op_code == 0) {
			sendto(socketfd, nonce_buf, 4, 0, result->ai_addr, server_addr_len);
			recvfrom(socketfd, response_buffer, RESPONSE_SIZE, 0, NULL, NULL);
		} else if (op_code == 1) {
			sprintf(portAsStr, "%d", op_param_host_order);
			s = getaddrinfo(server, portAsStr, &hints, &result);
			if (s != 0) {
				perror("getaddrinfo");
			}
			sendto(socketfd, nonce_buf, 4, 0, result->ai_addr, server_addr_len);
			recvfrom(socketfd, response_buffer, RESPONSE_SIZE, 0, NULL, NULL);
		} else if (op_code == 2) {
			s = getaddrinfo(server, portAsStr, &hints, &result);
			if (s != 0) {
				perror("getaddrinfo");
			}

			socketfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
			server_addr_len = result->ai_addrlen;
			ipv4addr_local.sin_family = af; // use AF_INET (IPv4)
			ipv4addr_local.sin_port = htons(op_param_host_order); // specific port
			ipv4addr_local.sin_addr.s_addr = 0; // any/all local addresses
			if (bind(socketfd, (struct sockaddr *)&ipv4addr_local, sizeof(struct sockaddr_in)) < 0) {
				perror("bind()");
			}

			sendto(socketfd, nonce_buf, 4, 0, result->ai_addr, server_addr_len);
			recvfrom(socketfd, response_buffer, RESPONSE_SIZE, 0, NULL, NULL);
		} else if (op_code == 3) {
			struct sockaddr_in op3_addr;
			socklen_t op3_addr_len;
			op3_addr_len = sizeof(struct sockaddr_in);
			unsigned char buf3[1];
			for (int i = 0; i < op_param_host_order; i++) {
				if (recvfrom(socketfd, buf3, 0, 0, (struct sockaddr *) &op3_addr, &op3_addr_len) < 0) {
					perror("recvfrom");
				};
				portSum += ntohs(op3_addr.sin_port);
				// printf("port: %d\n", ntohs(op3_addr.sin_port));
			}
			// printf("port sum: %d\n", portSum);
			nonce = htonl(portSum + 1);
			portSum = 0;
			bzero(nonce_buf, 4);
			memcpy(&nonce_buf[0], &nonce, 4);
			// print_bytes(nonce_buf, 4);
			sendto(socketfd, nonce_buf, 4, 0, result->ai_addr, server_addr_len);
			recvfrom(socketfd, response_buffer, RESPONSE_SIZE, 0, NULL, NULL);
		} else if (op_code == 4) {
			if (af == AF_INET) {
				af = AF_INET6;
			} else {
				af = AF_INET;
			}

			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = af;    /* Allow IPv4, IPv6, or both, depending on
				    					what was specified on the command line. */
			hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
			hints.ai_flags = 0;
			hints.ai_protocol = 0;  /* Any protocol */
			sprintf(portAsStr, "%d", op_param_host_order);
			s = getaddrinfo(server, portAsStr, &hints, &result);
			if (s != 0) {
				perror("getaddrinfo");
			}

			socketfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
			server_addr_len = result->ai_addrlen;


		}


		// print_bytes(response_buffer, RESPONSE_SIZE);
	}

	if (write(STDOUT_FILENO, treasure, treasure_length) < 0) {
		perror("write");
	}
	printf("\n");

	exit(EXIT_SUCCESS);
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
