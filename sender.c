#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/tcp.h>
#include <signal.h>
#include <math.h>

#define BUFFER_SIZE 10000
#define SEG_SIZE 256
#define CLOSE "CLOSE"
#define ERR "ERROR"
#define ACK "ACK"
#define NACK "NACK"
static volatile int sockfd, receiver;

void intHandler() { close(sockfd); }

void exit_close(int socket) { close(socket); exit(5); }

int findInArray(char* thing, char* list[], int size, int index) {
	if (index >= 0) {
		if(strcmp(thing, list[index]) == 0) return index; 
		else return -1;
	}

	for (int i = 0; i < size; i++) if (strcmp(thing, list[i]) == 0) return i;
	return -1;
}

int send_message(char* p, char* name, int socket, int size) {
	if (size < 0) size = BUFFER_SIZE;
	char buffer[size];
	if (send(socket, p, size, 0) < 0) { perror("Send()"); exit_close(socket); }
	printf("%s was sent to receiver\n", name);
	if (recv(socket, buffer, sizeof(buffer), 0) < 0) { perror("Receive()"); exit_close(socket); }
	if (strcmp(buffer, ACK) == 0) return 1; 
	return 0;
}

int receive_int(int *num, int fd) {
    int32_t ret;
    char *data = (char*)&ret;
    int left = sizeof(ret);
    int rc;
    do {
        rc = read(fd, data, left);
        if (rc >= 0) {
			data += rc;
            left -= rc;
		}
    } while (left > 0);
    *num = ntohl(ret);
    return 0;
}

int send_int(int num, int fd) {
    int32_t conv = htonl(num);
    char *data = (char*)&conv;
    int left = sizeof(conv);
    int rc;
    do {
        rc = write(fd, data, left);
        if (rc >= 0) {
			data += rc;
            left -= rc;
		}
    } while (left > 0);
    return 0;
}

int send_int_to(int integer, char* name, int socket) {
	char buffer[BUFFER_SIZE];
	if (send_int(integer, socket) < 0) { perror("Send()"); exit_close(socket); }
	printf("%s was sent to receiver\n", name);
	if (recv(socket, buffer, sizeof(buffer), 0) < 0) { perror("Receive()"); exit_close(socket); }
	if (strcmp(buffer, ACK) == 0) {puts("Receiver ACK int"); return 1;}
	return 0;
}

void send_ack(int socket) {
	puts("Sending ACK to sender...");
	write(socket, ACK, sizeof(ACK));
}
void send_nack(int socket) {
	puts("Sending NACK to sender...");
	write(socket, NACK, sizeof(NACK));
}

int read_ack_verify(char* p, char* name, char* list[], int size, int socket, int index) {
	char buff[BUFFER_SIZE];
	printf("Waiting to receive %s from sender...\n", name);
	if (read(socket, buff, BUFFER_SIZE) < 0) {perror("Read()"); exit(5);}
	printf("Received %s for %s from sender.\n", buff, name);
	int i = findInArray(buff, list, size, index);
	if (i >= 0) {
		printf("Found %s in list relating to %s\n", buff, name);
		puts("Sending ACK to sender...");
		send(socket, ACK, sizeof(ACK), 0);
		return i;
	}
	printf("Did not find %s in list relating to %s\n", buff, name);
	puts("Sending error message to receiver...");
	send(socket, ERR, sizeof(ERR), 0);
	return i;
}

int main(int argc, char ** argv) {
	char username[BUFFER_SIZE], pwd[BUFFER_SIZE], buffer[BUFFER_SIZE];
	int username_index = -1, pwd_index = -1, client_len;
	struct sockaddr_in client, server;
	char filename[BUFFER_SIZE];
	socklen_t addr_size;
	signal(SIGINT, intHandler);
	// Args check
    if (argc != 3) { fprintf(stderr, "Usage: %s port window_size\n", argv[0]); exit(1); }
	int port = atoi(argv[1]);
	int window_size = atoi(argv[2]);
	// Stream socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){perror("Socket()");exit(2);}
	puts("Server Socket created successfully");
	// Init server address
	bzero(&server, sizeof(server));
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port); 
	server.sin_family = AF_INET; 
	client_len = sizeof(client);
	// Bind
	if (bind(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0) {perror("Bind()");exit(3);}
	puts("Binding was successful");
	// Auth data
	char* usernames[] = {"Anna","Louis","Cathie","Ken","Sam", "Bailey"};
	char* passwords[] = {"a86H6T0c","G6M7p8az","Pd82bG57","jO79bNs1","Cfw61RqV","Kuz07YLv"};
	// Main loop
	while (1) {
		if (recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&client, &client_len) < 0) { perror("recvfrom()"); exit(1);}
		printf("Received message %s from %s:%d\n", buffer, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
		if(connect(sockfd, (struct sockaddr *)&client, client_len) < 0) { perror("Connect()"); exit(0); }
		// send_ack(sockfd);
		// // Auth
		// while (1) {
		// 	if ((username_index = read_ack_verify(username, "Username", usernames, 6, sockfd, -1)) < 0) continue; 
		// 	if ((pwd_index = read_ack_verify(pwd, "Password", passwords, 6, sockfd, username_index)) >= 0) break; 
		// }
		// File name
		while (1) {
			puts("Waiting for filename...");
			if (read(sockfd, filename, BUFFER_SIZE) < 0) {perror("Read()"); exit(0);}
			printf("Received filename: %s\n", filename);
			send_ack(sockfd);
			if (access(filename, F_OK) == 0) {puts("File found and is accessible"); send_ack(sockfd); break;}
			else {puts("File not accessible"); send_nack(sockfd);};
		}
		// Send window size
		puts("Sending window size");
		send_int_to(window_size, "Window Size", receiver);
		// Transmit file
		FILE* f = fopen(filename, "rb");
		long f_len = ftell(f);
		int number_of_segments = ceil(f_len / SEG_SIZE);
		char* segments[number_of_segments];
		puts("Preparing segments...");
		for (int cur_seg = 0; cur_seg < number_of_segments; cur_seg++) {
			char* segment_buffer = (char*) malloc(SEG_SIZE * sizeof(char));
			fread(segment_buffer, SEG_SIZE, 1, f);
			strcpy(segments[cur_seg], segment_buffer);
		}
		puts("Sending segments...");
		int window_start = 0;
		int window_end = window_start + window_size;
		while(1) {
			for (int i = window_start; i < window_end; i++) {
				// send_segment(segments[i], i, sockfd);
				write(sockfd, segments[i], SEG_SIZE);
			}
		}
	}
	close(sockfd);
	exit(0);
	return 0;
}
