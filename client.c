#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <libgen.h>
#include <sys/select.h>


#include "util.h"
#include "send_packet.h"
#include "protocol.h"

#define HELP_MSG "Usage: ./client <hostname> <port> <filename> <drop-percentage>\n"\
									"  <hostname> The hostname or ip-address of the machine where the server application runs.\n"\
									"  <port> The port number at which the client application should send the packages. Value between 1024 and 65535.\n"\
									"  <filename> A filename that contains a list of image filenames.\n"\
									"  <drop-percentage> A drop percentage, between 0-100. This should be in the range 0 to 20.\n"
#define FILE_NOT_EXIST_ERROR_MSG "File %s does not exist.\n"
#define FILE_ERROR_MSG "Input file corrupted: %s. A file in the list does not exist.\n"

#define LOCALHOST "127.0.0.1"
#define WINDOW_SIZE 7
#define TIMEOUT 5 // seconds


typedef struct node {
	int seqnum;
	int buffer_len;
	char *buffer;
	struct node *next;
} pkg_frame;

// PACKAGES
void pkg_frame_free(pkg_frame *pkg);
void send_termination_package(int sock, struct sockaddr_in *servaddrptr, int seqnum, int lastrecv_seqnum);
void sendpkgframe(int sock, struct sockaddr_in *servaddrptr, pkg_frame *pkg);
void sendallpkgs(int sock, struct sockaddr_in *servaddrptr, pkg_frame *head);
pkg_frame* create_pkg(char *buffer, char *filepath, int seqnum, int lastrecv_seqnum, int unique_num);

// NETWORKING
int wait_for_ack(int sock, struct sockaddr_in *servaddrptr, struct timeval *timeout, struct message_header_udp* header);
int send_images_gobackn(int sock, struct sockaddr_in *servaddrptr, struct files_paths *imgfiles);
struct sockaddr_in *create_sockaddr(char *target, int port);
int create_socket(struct sockaddr_in *server_addrptr);
extern in_addr_t inet_addr_from_ip_or_hostname(char *target);

// FILES
int read_file_to_memory(char *filepath, char **out);
void filepaths_free(struct files_paths *files);
struct files_paths* read_in_files(char *fname, int num_files);
int eval_input_file(char *fname);


int main(int argc, char *argv[])
{
	int port, num_files, sock;
	float drop_percentage;
	char *hostname, *filename;
	struct files_paths *imgfiles;
	struct sockaddr_in *servaddrptr;

	// require 4 input arguments
	if (argc != 5){
		printf(HELP_MSG);
		return EXIT_FAILURE;
	}

	hostname = argv[1];
	filename = argv[3];
	port = atoi(argv[2]);
	drop_percentage = (float)atof(argv[4])/100;

	if (drop_percentage < 0 || drop_percentage > 100){
		printf(HELP_MSG);
		return EXIT_FAILURE;
	}

	printf("setting output dir\n");
	// evaluate the given input file
	if ((num_files = eval_input_file(filename)) == -1){
		if (access(filename, F_OK) == -1)
			printf(FILE_NOT_EXIST_ERROR_MSG, filename);
		else
			printf(FILE_ERROR_MSG, filename);
		return EXIT_FAILURE;
	}

	printf("setting drop percentage\n");
	// set drop percentage
	set_loss_probability(drop_percentage);

	imgfiles = read_in_files(filename, num_files);

	servaddrptr = create_sockaddr(hostname, port);
	sock = create_socket(servaddrptr);

	send_images_gobackn(sock, servaddrptr, imgfiles);

	close(sock);
	free(servaddrptr);
	filepaths_free(imgfiles);

	return EXIT_SUCCESS;
}



int wait_for_ack(int sock, struct sockaddr_in *servaddrptr, struct timeval *timeout, struct message_header_udp* header)
{
	fd_set readfds;
	int  numbytes, ret;
	char recvbuf[HEADER_UDP_STATIC_SIZE];
	socklen_t addrlen = sizeof(*servaddrptr);

	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);

	ret = select(sock+1, &readfds, NULL, NULL, timeout);
	exit_if_error(ret, "select");
	// timeout
	if (ret==0){
		printf("timeout.\n");
		return ret;
	} 
	
	numbytes = (int)recvfrom(sock, (char *)recvbuf,
															MAX_BUFFER, MSG_WAITALL,
															(struct sockaddr *) servaddrptr, &addrlen);
	exit_if_error(numbytes, "recvfrom"); // TODO: free memory on exit
	
	memset(header, 0, sizeof(struct message_header_udp));
	deconstruct_udp_message_header_static(recvbuf, header);

	return ret;
}

bool recv_seqnum_invalid(unsigned char recv_seqnum, unsigned char exp_seqnum){
	return recv_seqnum < exp_seqnum || (recv_seqnum == 255 && exp_seqnum==0);
}

int send_images_gobackn(int sock, struct sockaddr_in *servaddrptr, struct files_paths *imgfiles)
{
	char buffer[MAX_BUFFER];
	int seqlower, sequpper, lastrecv_seqnum, unique_num, ackseqnum_last, move_window, ret;
	struct message_header_udp *ack = malloc(sizeof(struct message_header_udp));
	pkg_frame *head=NULL, *tail, *pkg;
	struct timeval timeout;

	seqlower=0, sequpper=0, lastrecv_seqnum=0;
	unique_num=rand(), ackseqnum_last=(imgfiles->num_files)-1;
	move_window = WINDOW_SIZE;

	printf("WHILE LOOP: send_images_gobackn...\n");
	while (true){
		memset(buffer, 0, sizeof(char)*MAX_BUFFER);
		memset(ack, 0, sizeof(struct message_header_udp));
		// reset timer
		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = 0;

		// create frame and move upper bound
		while(0 < move_window){
			if (sequpper > ackseqnum_last) break;

			pkg = create_pkg(buffer, imgfiles->files[sequpper], sequpper, lastrecv_seqnum, unique_num++);
			if (head==NULL){
				head = pkg;
				tail = pkg;
			}
			else {
				tail->next = pkg;
				tail = pkg;
			}
			printf("sending package, seqnum %d\n", sequpper);
			sendpkgframe(sock, servaddrptr, pkg);
			move_window --, sequpper++;
		}

		do { // wait for ack
			printf("waiting for ack...\n");
			ret = wait_for_ack(sock, servaddrptr, &timeout, ack);
			
			// timeout
			if (ret == 0) {
				// reset timer before sending current window frame
				timeout.tv_sec = TIMEOUT;
				timeout.tv_usec = 0;
				printf("sending all packages in window frame.\n\n");
				sendallpkgs(sock, servaddrptr, head); // send all 
				continue;
			}
			printf("ack recieved. seqnum; recv: %d, exp: %d\n", ack->seqnum, seqlower);
		} while (recv_seqnum_invalid(ack->seqnum, seqlower));
		
		// last package recieved ?
		if ((lastrecv_seqnum = ack->seqnum) == ackseqnum_last) break;

		// move lower bound of window frame upwards
		move_window = (lastrecv_seqnum - seqlower) +1;
		for (int i=0; i< move_window; i++){
			pkg = head;
			head = head->next;
			pkg_frame_free(pkg);
			seqlower++;
		}
	}
	
	send_termination_package(sock, servaddrptr, ackseqnum_last+1, lastrecv_seqnum);

	do { // package cleanup
		pkg = head;
		head = pkg->next;
		pkg_frame_free(pkg);
	} while (head != NULL);

	free(ack);
	
	return 0;
}


/**************/
/*  PACKAGES  */
/**************/

void pkg_frame_free(pkg_frame *pkg)
{
	if(pkg==NULL) return;
	free(pkg->buffer);
	free(pkg);
}

void send_termination_package(int sock, struct sockaddr_in *servaddrptr, int seqnum, int lastrecv_seqnum)
{
	char buffer[HEADER_UDP_STATIC_SIZE];
	struct message_header_udp *header = 
				set_udp_message_header_info("", 0, seqnum, lastrecv_seqnum, FLAG_TERMINATION);
	construct_udp_message_header(buffer, header);
	send_packet(sock, buffer, header->header_len, 
						MSG_CONFIRM, (struct sockaddr *) servaddrptr,
						sizeof(*servaddrptr));
	free(header);
}

void sendpkgframe(int sock, struct sockaddr_in *servaddrptr, pkg_frame *pkg)
{
		send_packet(sock, pkg->buffer, pkg->buffer_len, 
						MSG_CONFIRM, (struct sockaddr *) servaddrptr,
						sizeof(*servaddrptr));
}

void sendallpkgs(int sock, struct sockaddr_in *servaddrptr, pkg_frame *head)
{
	pkg_frame *pkg = head;
	while (pkg){
		sendpkgframe(sock, servaddrptr, pkg);
		pkg = pkg->next;
	}
}

pkg_frame* create_pkg(char *buffer, char *filepath, int seqnum, int lastrecv_seqnum, int unique_num)
{
	char *payload_buf, *node_buf;
	int imglen;
	pkg_frame *pkg;
	char *img;

	printf("creating package, seqnum %d\n", seqnum);
	
	imglen = read_file_to_memory(filepath, &img);
	exit_if_error(imglen, "get_file_data");
	
	// construct payload buffer
	Payload *payload = 
			set_payload_header_info(basename(filepath), img, unique_num, imglen);

	payload_buf = calloc(sizeof(char), payload->header_len);
	construct_payload_header(payload_buf, payload);

	// construct udp buffer
	struct message_header_udp *header = 
			set_udp_message_header_info(payload_buf, payload->header_len, 
																	seqnum, lastrecv_seqnum, FLAG_DATA);
	construct_udp_message_header(buffer, header);

	// create node
	node_buf = malloc((header->header_len)*sizeof(char));
	memcpy(node_buf, buffer, (header->header_len)*sizeof(char));

	// node = calloc(1, sizeof(node_t));
	pkg = malloc(sizeof(pkg_frame));
	pkg->buffer_len = header->header_len;
	pkg->seqnum=seqnum;
	pkg->next = NULL;
	pkg->buffer = node_buf;

	free(payload_buf);
	free(payload);
	free(header);
	free(img);
	printf("finished creating package %d.\n", seqnum);
	return pkg;
}


/****************/
/*  NETWORKING  */
/****************/

struct sockaddr_in *create_sockaddr(char *target, int port)
{
	struct sockaddr_in *addrptr;
	addrptr = calloc(1, sizeof(struct sockaddr_in));

	// use ipv4 (AF_INET)
	addrptr->sin_family = AF_INET;
	addrptr->sin_port = htons((uint16_t)port);
	addrptr->sin_addr.s_addr = inet_addr_from_ip_or_hostname(target);
	exit_if_error((int)(addrptr->sin_addr.s_addr), "inet_addr_from_ip_or_hostname");

	return addrptr;
}

int create_socket(struct sockaddr_in *server_addrptr)
{
	int ret, client_socket;
	struct sockaddr_in server_addr;
	server_addr = *server_addrptr;

	// creating socket ipv4, SOCK_DGRAM (UDP)
	client_socket = socket(AF_INET, SOCK_DGRAM, 0);
	exit_if_error(client_socket, "socket");

	// open connection on socket
	ret = connect(client_socket, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
	exit_if_error(ret, "connect");

	return client_socket;
}

extern in_addr_t inet_addr_from_ip_or_hostname(char *target)
{
	struct hostent *hp;
	in_addr_t in_addr;

	in_addr = inet_addr(target);
	if ((int)(in_addr) != -1) return in_addr;

	hp = gethostbyname(target);
	if (!hp) return (in_addr_t)-1;

	// copy first address in list to 'to'
	memcpy(&(in_addr), hp->h_addr_list[0], (size_t)hp->h_length);
	return in_addr;
}


/***********/
/*  FILES  */
/***********/

int read_file_to_memory(char *filepath, char **out)
{
	long len;
	FILE *file = fopen(filepath, "rb");

	if (file){
		fseek(file, 0, SEEK_END);
		len = ftell(file);
		fseek(file, 0, SEEK_SET);

		*out = malloc(len);
		if (*out){
			fread(*out, 1, len, file);
		}
		fclose(file);
	}

	if (!*out) return -1;
	
	return (int)len;
}

struct files_paths* read_in_files(char *fname, int num_files)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int i=0;
	size_t linelen;
	struct files_paths* img_files;
	char **image_files;
	
	img_files = (struct files_paths*) malloc(sizeof(struct files_paths));
	image_files = malloc(num_files*sizeof(char *));

	fp = fopen(fname, "r");
	if (fp == NULL) {
		free(image_files);
		return NULL;
	}

	while ((read = getline(&line, &len-1, fp)) != -1) {
		strtok(line, "\n");
		if (access(line, F_OK) == -1){
			free(image_files);		
			fclose(fp);
			if (line) free(line);
			return NULL;
		}

		linelen = strlen(line);
		image_files[i] = calloc(linelen+1, sizeof(char));
		strcpy(image_files[i++], line);
	}

	img_files->num_files = num_files;
	img_files->files = image_files;
	
	fclose(fp);
	if (line) free(line);
	return img_files;
}

int eval_input_file(char *fname)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int num_lines=0;

	fp = fopen(fname, "r");
	if (fp == NULL) return -1;
	
	while ((read = getline(&line, &len-1, fp)) != -1) {
		strtok(line, "\n");
		if (access(line, F_OK) == -1){ return -1; }
		num_lines++;
	}

	// close pointer, and free space
	fclose(fp);
	if (line) free(line);
	return num_lines;
}
