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
#include <sys/stat.h>
#include <time.h>

#define BUFFER_SIZE 10000
#define CLOSE "CLOSE"
#define SEG_SIZE 256
#define ERR "ERROR"
#define NACK "NACK"
#define ACK "ACK"

static volatile int sockfd;
static int packets_num, ack_num, nack_num;

void intHandler() { close(sockfd); exit(0); }
void exit_close(int socket) { close(socket); exit(5); }
void send_ack(int socket) { puts("Sending ACK to sender..."); write(socket, ACK, sizeof(ACK)); packets_num++;}
void send_nack(int socket) { puts("Sending NACK to sender..."); write(socket, NACK, sizeof(NACK));packets_num++;}
void get_from_user(char* p, char* name) {
	printf("Please enter a %s: ", name);
	fgets(p, BUFFER_SIZE, stdin);
	if ((strlen(p) > 0) && (p[strlen(p) - 1] == '\n')) p[strlen (p) - 1] = '\0';
}
int send_message(char* p, char* name, int socket, int size) {
	if (size < 0) size = BUFFER_SIZE;
	char buffer[size];
	socklen_t addr_size;
	if (sendto(socket, p, size, 0, (struct sockaddr*)NULL, addr_size) < 0) {perror("Send()"); exit_close(socket);packets_num++;}
	printf("%s was sent to sender, waiting for ack...\n", name);
	if (recv(socket, buffer, sizeof(buffer), 0) < 0) { perror("Receive()"); exit_close(socket); }
	if (strcmp(buffer, ACK) == 0) {printf("ACK received for %s\n", name); return 1;ack_num++;} 
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
int receive_segment(int sockfd, char **segments) {
	char buffer[BUFFER_SIZE];
	puts("Waiting for segment...");
	// Segment index
	bzero(buffer, BUFFER_SIZE);
	read(sockfd, buffer, BUFFER_SIZE);
	int index = atoi(buffer);
	// Segment data
	read(sockfd, buffer, SEG_SIZE);
	char *segment = malloc(BUFFER_SIZE);
	strcpy(segment, buffer);
	segments[index] = segment;
	return index;
}
int getSend(char* p, char* name, int socket) {
	char buffer[BUFFER_SIZE];
	get_from_user(p, name);
	if (send_message(p, name, socket, -1)) {printf("sender acknowledged %s\n", name); return 1;}
	printf("Sender did not acknowledge %s!\n", name); 
	return 0;
}
const char* getFileCreationTime(char *path) {
    struct stat attr;
    stat(path, &attr);
	char *buffer = malloc(BUFFER_SIZE);
    sprintf(buffer, "%s", ctime(&attr.st_mtime));
	return buffer;
}
int main(int argc, char ** argv) {
	srand(time(NULL));

	struct sockaddr_in server;
	char buffer[BUFFER_SIZE];
	socklen_t addr_size;
	signal(SIGINT, intHandler);
	// Args check
    if (argc != 5) { printf("Usage: %s <sender_ip> <sender_port> <input_file> <output_file>\n", argv[0]); exit(1); }
	char *sender_ip = argv[1];
	int port = atoi(argv[2]);
	char *input_filename = argv[3];
	char *output_filename = argv[4];
	// Sender address
	server.sin_addr.s_addr = inet_addr(sender_ip);
    server.sin_port = htons(port);
    server.sin_family = AF_INET;
	// Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){perror("Socket()");exit(2);} else puts("Socket created successfully");
	// Connect
	if(connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0){perror("connect()");exit(3);}
	// Auth
	send_message("AUTH", "AUTH", sockfd, sizeof("AUTH")); 
	// while (1) { char username[BUFFER_SIZE], pwd[BUFFER_SIZE]; if (getSend(username, "Username", sockfd) && getSend(pwd, "Password", sockfd)) break; }
	// Filename
	if (!send_message(input_filename, "File name", sockfd, sizeof(input_filename))) {puts("File was not found or is inaccessable."); exit(4);}
	// Window size
	bzero(buffer, BUFFER_SIZE);
	receive_message(&buffer, "Window Size", sockfd);
	int window_size = atoi(buffer);
	printf("Window size: %d\n", window_size);
	// Receive file
	bzero(buffer, BUFFER_SIZE);
	receive_message(&buffer, "Number of segments", sockfd);
	int number_of_segments = atoi(buffer);
	printf("Number of segments: %d\n", number_of_segments);
	char *segments[number_of_segments];
	// int segment_index = 0;
	int window_start, window_end;
	while (window_end < number_of_segments) {
		printf("Window end: %d\n", window_end);
		int index = receive_segment(sockfd, segments);
		// Drop 10% of the data packets
		if (rand() % 101 <= 70) {
			printf("Dropped data packet for segment #%d :(\n", index);
			// NACK
			send_nack(sockfd);
			// Index
			bzero(buffer, BUFFER_SIZE);sprintf(buffer, "%d", window_end);write(sockfd,buffer, BUFFER_SIZE);
		} else {
			printf("Received segment #%d\n", index);
			// ACK
			send_ack(sockfd);
			// Index
			bzero(buffer, BUFFER_SIZE);sprintf(buffer, "%d", window_end);write(sockfd,buffer, BUFFER_SIZE);
			if (window_end - window_start == window_size) window_start++;
			window_end++;
		}
	}
	// while (window_end < number_of_segments) {
	// 	int index = receive_segment(sockfd, segments);
	// 	// Drop 10% of the data packets and send NACK
	// 	if (rand() % 101 <= 10) {
	// 		send_nack(sockfd); 
	// 		receive_segment(sockfd, segments);
	// 	} else {
	// 		send_ack(sockfd);
	// 	}
	// }
	// Write segments to file
	FILE *fp = fopen(output_filename, "w+");
	for (int i = 0; i < number_of_segments; i++) {
		fprintf(fp, "%s", segments[i]);
	}
	FILE* f = fopen(output_filename, "rb");
	fseek(f, 0L, SEEK_END);
	long f_len = ftell(f);
	fseek(f, 0L, SEEK_SET);
	printf(
			"Name and Size of file received: %s, %ld bytes\n"
			"File Creation Date & Time: %s\n"
			"Number of Data Packets Transmitted: %d\n"
			"Number of Acknowledgement Received: %d\n"
			"Number of Negative Acknowledgement Received with Sequence Number: %d\n",
			input_filename, f_len,
			getFileCreationTime(output_filename),
			packets_num,
			ack_num,
			nack_num
		);
    puts("Exiting...");
	close(sockfd);
	fclose(fp);
    exit(0);
	return 0;
}
