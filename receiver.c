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

#define BUFFER_SIZE 10000
#define CLOSE "CLOSE"
#define ERR "ERROR"
#define ACK "ACK"

static volatile int sockfd;

void intHandler() { close(sockfd); exit(0); }
void exit_close(int socket) { close(socket); exit(5); }
void send_ack(int socket) { puts("Sending ACK to sender..."); write(socket, ACK, sizeof(ACK));}
void get_from_user(char* p, char* name) {
	printf("Please enter a %s: ", name);
	fgets(p, BUFFER_SIZE, stdin);
	if ((strlen(p) > 0) && (p[strlen(p) - 1] == '\n')) p[strlen (p) - 1] = '\0';
}
int send_message(char* p, char* name, int socket, int size) {
	if (size < 0) size = BUFFER_SIZE;
	char buffer[size];
	socklen_t addr_size;
	if (sendto(socket, p, size, 0, (struct sockaddr*)NULL, addr_size) < 0) { perror("Send()"); exit_close(socket); }
	printf("%s was sent to sender, waiting for ack...\n", name);
	if (recv(socket, buffer, sizeof(buffer), 0) < 0) { perror("Receive()"); exit_close(socket); }
	if (strcmp(buffer, ACK) == 0) return 1; 
	return 0;
}
int getSend(char* p, char* name, int socket) {
	char buffer[BUFFER_SIZE];
	get_from_user(p, name);
	if (send_message(p, name, socket, -1)) {printf("sender acknowledged %s\n", name); return 1;}
	printf("Sender did not acknowledge %s!\n", name); 
	return 0;
}
int main(int argc, char ** argv) {
	struct sockaddr_in server;
	char buffer[BUFFER_SIZE];
	socklen_t addr_size;
	signal(SIGINT, intHandler);
	// Args check
    if (argc != 5) { printf("Usage: %s <sender_ip> <sender_port> <input_file> <output_file>\n", argv[0]); exit(1); }
	char *sender_ip = argv[1];
	int port = atoi(argv[2]);
	char *input_filename = argv[3];
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
	while (1) { if (send_message(input_filename, "Filename", sockfd, sizeof(input_filename))) break; }
    puts("Exiting...");
	close(sockfd);
    exit(0);
	return 0;
}
