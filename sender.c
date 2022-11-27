#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/tcp.h>
#include <sys/stat.h>
#include <signal.h>
#include <math.h>
#include <time.h>

#define FILE_END "FILE_END"
#define BUFFER_SIZE 10000
#define SEG_SIZE 256
#define CLOSE "CLOSE"
#define ERR "ERROR"
#define NACK "NACK"
#define ACK "ACK"

static volatile int sockfd, receiver;
static int packets_num, packets_num_retransmitted, ack_num, nack_num;

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
	socklen_t addr_size;
	if (write(socket, p, size) < 0) { perror("Write()"); exit_close(socket); }
	packets_num++;
	printf("%s with value %s was sent to receiver, waiting for ack...\n", name, p);
	if (recv(socket, buffer, sizeof(buffer), 0) < 0) { perror("Receive()"); exit_close(socket); }
	if (strcmp(buffer, ACK) == 0){printf("ACK received for %s\n", name); return 1;ack_num++;} 
	return 0;
}
int receive_message(char **p, char *name, int sockfd) {
	char buffer[BUFFER_SIZE];
	printf("Waiting for %s...\n", name);
	if (read(sockfd, buffer, BUFFER_SIZE) < 0) {perror("Read()"); exit(0);}
	printf("Received %s: %s\n", name, buffer);
	send_ack(sockfd);
	strcpy(p, buffer);
	return 1;
}
void send_ack(int socket) {puts("Sending ACK to sender...");write(socket, ACK, sizeof(ACK));packets_num++;}
int receive_ack(int socket, char *name) {
	char buffer[BUFFER_SIZE];
	printf("Waiting for ack for %s...\n", name);
	if (recv(socket, buffer, sizeof(buffer), 0) < 0) { perror("Receive()"); exit_close(socket); }
	if (strcmp(buffer, ACK) == 0) {printf("ACK received for %s\n", name); return 1;ack_num++;} 
	return 0;
}
void send_nack(int socket) {puts("Sending NACK to sender...");write(socket, NACK, sizeof(NACK));packets_num++;}
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
void send_segment(int sockfd, char **segments, int index) {
	char buffer[BUFFER_SIZE];
	sprintf(buffer, "%d", index);
	// Segment index
	write(sockfd, buffer, BUFFER_SIZE);
	packets_num++;
	// Segment data
	printf("Sending segment #%d\n", index);
	write(sockfd, segments[index], SEG_SIZE);
	packets_num++;
}
const char* getFileCreationTime(char *path) {
    struct stat attr;
    stat(path, &attr);
	char *buffer = malloc(BUFFER_SIZE);
    sprintf(buffer, "%s", ctime(&attr.st_mtime));
	return buffer;
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
	char *window_size = argv[2];
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
		puts("Waiting for datagrams...");
		if (recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr*)&client, &client_len) < 0) { perror("recvfrom()"); exit(1);}
		printf("Received message %s from %s:%d\n", buffer, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
		if(connect(sockfd, (struct sockaddr *)&client, client_len) < 0) { perror("Connect()"); exit(0); }
		send_ack(sockfd);
		// // Auth
		// while (1) {
		// 	if ((username_index = read_ack_verify(username, "Username", usernames, 6, sockfd, -1)) < 0) continue; 
		// 	if ((pwd_index = read_ack_verify(pwd, "Password", passwords, 6, sockfd, username_index)) >= 0) break; 
		// }
		// File name
		puts("Waiting for filename...");
		if (read(sockfd, filename, BUFFER_SIZE) < 0) {perror("Read()"); exit(0);}
		printf("Received filename: %s\n", filename);
		if (access(filename, F_OK) == 0) {puts("File found and is accessible"); send_ack(sockfd);}
		else {puts("File not accessible"); send_nack(sockfd); break;};
		// Send window size
		send_message(window_size, "Window Size", sockfd, sizeof(window_size));
		// Transmit file
		FILE* f = fopen(filename, "rb");
		fseek(f, 0L, SEEK_END);
		long f_len = ftell(f);
		fseek(f, 0L, SEEK_SET);
		printf("File len:%d\n", f_len);
		int number_of_segments = ceil(f_len / SEG_SIZE) + 1;
		// Send number of segments
		bzero(buffer, BUFFER_SIZE);
		sprintf(buffer, "%d", number_of_segments);
		send_message(buffer, "Number of segments", sockfd, BUFFER_SIZE);
		char* segments[number_of_segments];
		puts("Preparing segments...");
		for (int cur_seg = 0; cur_seg < number_of_segments; cur_seg++) {
			char* segment_buffer = (char*) malloc(SEG_SIZE * sizeof(char));
			fread(segment_buffer, SEG_SIZE, 1, f);
			segments[cur_seg] = segment_buffer;
		}
		puts("Sending segments...");
		int window_start = 0;
		int window_end = atoi(window_size) > number_of_segments ? number_of_segments : window_start + atoi(window_size);
		int current_frame;
		for (current_frame = window_start; current_frame < number_of_segments; current_frame++) {
			send_segment(sockfd, segments, current_frame);
			bzero(buffer, BUFFER_SIZE);
			read(sockfd, buffer, BUFFER_SIZE);
			if (strcmp(ACK, buffer) == 0) {ack_num++; continue;}
			else if (strcmp(NACK, buffer) == 0) {nack_num++; packets_num_retransmitted++; send_segment(sockfd, segments, current_frame);}
		}
		printf(
			"Receiver IP and Port Number: %s:%d\n"
			"Name and Size of file: %s, %ld bytes\n"
			"File Creation Date & Time: %s\n"
			"Number of Data Packets Transmitted: %d\n"
			"Number of Packets Re-transmitted: %d\n"
			"Number of Acknowledgement Received: %d\n"
			"Number of Negative Acknowledgement Received with Sequence Number: %d\n",
			inet_ntoa(client.sin_addr), ntohs(client.sin_port),
			filename, f_len,
			getFileCreationTime(filename),
			packets_num,
			packets_num_retransmitted,
			ack_num,
			nack_num
		);
		break;
	}
	puts("Exiting...");
	close(sockfd);
	exit(0);
	return 0;
}
