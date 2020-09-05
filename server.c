#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#include "util.h"
#include "servapp.h"
#include "protocol.h"

#define HELP_MSG "Usage: ./server <port> <directory> <file>\n"\
									"  <port> The port number at which the server application should recieve packages. Value between 1024 and 65535.\n"\
									"  <directory> A directory containing image-files.\n"\
									"  <file> Filename for printing matches.\n"

int create_server_socket(int port);
void run_server(int sock);
void handle_connection(int sock, struct sockaddr_in *clientaddr, char *buffer, struct message_header_udp *header);
void send_ack(int sock, struct sockaddr_in *clientaddr, int seqnum);





int main(int argc, char *argv[])
{
	int port, server_socket;
	char *dir, *output_file;

	// require 3 input arguments
	if (argc != 4){
		printf(HELP_MSG);
		return EXIT_FAILURE;
	}

	port = atoi(argv[1]);
	dir = argv[2];
	output_file = argv[3];

	if (application_start(dir, output_file) == -1){
		printf("Invalid img directory or output file. dir: %s, outputfile: %s\n", dir, output_file);
		return EXIT_FAILURE;
	}

	server_socket = create_server_socket(port);
	run_server(server_socket);

	close(server_socket);

	application_close();

	return EXIT_SUCCESS;
}

void send_ack(int sock, struct sockaddr_in *clientaddr, int seqnum)
{
	char buffer[HEADER_UDP_STATIC_SIZE];
	struct message_header_udp *header;

	header = set_udp_message_header_info("", 0, seqnum, seqnum-1, FLAG_ACK);
	construct_udp_message_header(buffer, header);

	sendto(sock, buffer, header->header_len, 
					MSG_CONFIRM, (struct sockaddr *) clientaddr,
					sizeof(*clientaddr));
	free(header);
}

void handle_connection(int sock, struct sockaddr_in *clientaddr, char *buffer, struct message_header_udp *header)
{
	int numbytes;
	socklen_t addrlen = sizeof(*clientaddr);
	
	memset(clientaddr, 0, sizeof(*clientaddr));

	// Receive message from client
	numbytes = (int)recvfrom(sock, (char *)buffer,
														MAX_BUFFER, MSG_WAITALL,
														(struct sockaddr *) clientaddr, &addrlen);
	exit_if_error(numbytes, "recvfrom");

	deconstruct_udp_message_header_static(buffer, header);
}

int create_server_socket(int port)
{
	int ret;
	int sock;
	struct sockaddr_in server_addr;

	// creating socket ipv4
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	exit_if_error(sock, "socket");

	// use ipv4 (AF_INET)
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons((uint16_t)port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	// connect to to host
	ret = bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
	exit_if_error(ret, "bind");

	return sock;
}

void run_server(int sock)
{
	char buffer[MAX_BUFFER];
	int exp_seqnum=0, ret;
	struct sockaddr_in *clientaddr;
	struct message_header_udp *header;
	Payload *payload;
	
	// allocate memory 
	clientaddr = malloc(sizeof(struct sockaddr_in));
	header = malloc(sizeof(struct message_header_udp));
	payload = malloc(sizeof(Payload));
	
	printf("Starting server...\n");
	while (true) {
		// reset memory each time
		memset(header, 0, sizeof(struct message_header_udp));
		memset(buffer, 0, sizeof(char)*MAX_BUFFER);

		printf("\n\nwaiting for incoming connection...\n");
		handle_connection(sock, clientaddr, buffer, header);
		
		if (header->seqnum != exp_seqnum){
			printf("incoming sequence num not equal expected. got: %d, expected: %d\n", header->seqnum, exp_seqnum);
			send_ack(sock, clientaddr, exp_seqnum-1);
			continue;
		}
		// is this a termination package?
		if (header->flag==FLAG_TERMINATION){
			printf("termination package recieved.\n");
			send_ack(sock, clientaddr, exp_seqnum);
			break;
		}

		printf("recieved package with seqnum: %d\n", header->seqnum);
		memset(payload, 0, sizeof(Payload));

		// deconstruct payload
		deconstruct_udp_message_header_payload(buffer, header);
		deconstruct_payload_header_static(header->payload, payload);
		deconstruct_payload_header_msg(header->payload, payload);
		
		
		printf("sending payload to application layer for processing...\n");
		// let application layer process payload
		ret = handle_payload(payload);
		exit_if_error(ret, "handle_payload");
		
		// send ack after processing
		send_ack(sock, clientaddr, exp_seqnum++);

		// free current used memory
		free(header->payload);
		free(payload->filename);
		free(payload->image);
	}

	// free used memory
	free(clientaddr);
	free(payload);
	free(header);
	
	printf("Terminating server...\n");
}

