#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"


Payload *set_payload_header_info(char* filename, char* image, int unique_num, int image_len)
{
	Payload *header;
	int filename_len = (int)strlen(filename)+1;
	
	header = malloc(sizeof(Payload));
	header->header_len = (uint32_t) (filename_len+image_len+(int)HEADER_PAYLOAD_STATIC_SIZE);
	header->unique_num = (uint16_t) unique_num;
	header->filename_len = (uint16_t) filename_len;	// filename length + null byte
	header->filename = filename;
	header->image = image;
	return header;
}

void construct_payload_header(char* buffer, Payload *header)
{
	size_t image_offset, image_len;
	// int image_offset = HEADER_FILENAME_OFFSET+header->filename_len;
	image_offset = HEADER_PAYLOAD_STATIC_SIZE+header->filename_len;
	image_len = header->header_len-(header->filename_len+HEADER_PAYLOAD_STATIC_SIZE);

	memcpy(&(buffer[HEADER_PAYLAOD_LEN_OFFSET]), &(header->header_len), sizeof(uint16_t));
	memcpy(&(buffer[HEADER_UNIQUE_NUM_OFFSET]), &(header->unique_num), sizeof(uint16_t));
	memcpy(&(buffer[HEADER_FILENAME_LEN_OFFSET]), &(header->filename_len), sizeof(uint16_t));
	memcpy(&(buffer[HEADER_FILENAME_OFFSET]), header->filename, (header->filename_len)*sizeof(char));
	memcpy(&(buffer[image_offset]), header->image, image_len *sizeof(char));
}

void deconstruct_payload_header_static(char* buffer, Payload* header)
{
	memcpy(&(header->header_len), &(buffer[HEADER_PAYLAOD_LEN_OFFSET]), sizeof(uint16_t));
	memcpy(&(header->unique_num), &(buffer[HEADER_UNIQUE_NUM_OFFSET]), sizeof(uint16_t));
	memcpy(&(header->filename_len), &(buffer[HEADER_FILENAME_LEN_OFFSET]), sizeof(uint16_t));
}

void deconstruct_payload_header_msg(char* buffer, Payload* header)
{
	size_t image_offset, image_len;
	image_offset = HEADER_FILENAME_OFFSET+header->filename_len;
	image_len = (header->header_len -(HEADER_PAYLOAD_STATIC_SIZE+header->filename_len));

	header->filename = malloc((header->filename_len)*sizeof(char));
	header->image = malloc(image_len*sizeof(char));

	memcpy(header->filename, &(buffer[HEADER_FILENAME_OFFSET]), (header->filename_len)*sizeof(char));
	memcpy(header->image, &(buffer[image_offset]), image_len*sizeof(char));
}

struct message_header_udp* set_udp_message_header_info(char* payload, int payload_len, int seq_num, int prev_seq_num, unsigned char flag)
{
	struct message_header_udp *header;
  
  header = (struct message_header_udp*) malloc(sizeof(struct message_header_udp));
  header->header_len = (uint32_t) ((int)HEADER_UDP_STATIC_SIZE+payload_len);
  header->seqnum = (unsigned char)seq_num;
  header->lastrecv_seqnum = (unsigned char)prev_seq_num;
  header->flag = flag;
  header->unused = UNUSED_VALUE;
  header->payload = payload;

  return header;
}

void construct_udp_message_header(char* buffer, struct message_header_udp *header)
{
	/*
	* buffer[0][1][2][3] => header->header_len
	* buffer[4] => header->seq_number
	* buffer[5] => header->seq_number_prev
	* buffer[6] => header->flag
	* buffer[7] => header->unused
	* [8]+ => header->payload
	*/
	memcpy(&(buffer[HEADER_UDP_LEN_OFFSET]), &(header->header_len), sizeof(uint32_t));
	memcpy(&(buffer[HEADER_UDP_SEQ_NUM_OFFSET]), &(header->seqnum), sizeof(char));
	memcpy(&(buffer[HEADER_UDP_PREV_SEQ_NUM_OFFSET]), &(header->lastrecv_seqnum), sizeof(char));
	memcpy(&(buffer[HEADER_UDP_FLAG_OFFSET]), &(header->flag), sizeof(char));
	memcpy(&(buffer[HEADER_UDP_UNUSED_OFFSET]), &(header->unused), sizeof(char));
	memcpy(&(buffer[HEADER_UDP_PAYLOAD_OFFSET]), header->payload, ((header->header_len)-HEADER_UDP_STATIC_SIZE)*sizeof(char));
}

void deconstruct_udp_message_header_static(char* buffer, struct message_header_udp *header)
{
	memcpy(&(header->header_len), &(buffer[HEADER_UDP_LEN_OFFSET]), sizeof(uint32_t));
	memcpy(&(header->seqnum), &(buffer[HEADER_UDP_SEQ_NUM_OFFSET]), sizeof(char));
	memcpy(&(header->lastrecv_seqnum), &(buffer[HEADER_UDP_PREV_SEQ_NUM_OFFSET]), sizeof(char));
	memcpy(&(header->flag), &(buffer[HEADER_UDP_FLAG_OFFSET]), sizeof(char));
	memcpy(&(header->unused), &(buffer[HEADER_UDP_UNUSED_OFFSET]), sizeof(char));
}

void deconstruct_udp_message_header_payload(char* buffer, struct message_header_udp *header)
{
	size_t payload_len = header->header_len - HEADER_UDP_STATIC_SIZE;
	
	// allocate memory to payload
	header->payload = malloc(payload_len);
	memcpy(header->payload, &(buffer[HEADER_UDP_PAYLOAD_OFFSET]), payload_len*sizeof(char));
}
