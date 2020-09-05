#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_BUFFER 2048


#define HEADER_PAYLOAD_STATIC_SIZE (sizeof(uint16_t)*3)
#define HEADER_PAYLAOD_LEN_OFFSET 0
#define HEADER_UNIQUE_NUM_OFFSET 2
#define HEADER_FILENAME_LEN_OFFSET 4
#define HEADER_FILENAME_OFFSET 6
#define HEADER_IMAGE_OFFSET (HEADER_FILENAME_OFFSET+sizeof(char*))

#define HEADER_UDP_STATIC_SIZE (sizeof(uint32_t)+sizeof(char)*4)
#define HEADER_UDP_LEN_OFFSET 0
#define HEADER_UDP_SEQ_NUM_OFFSET 4
#define HEADER_UDP_PREV_SEQ_NUM_OFFSET 5
#define HEADER_UDP_FLAG_OFFSET 6
#define HEADER_UDP_UNUSED_OFFSET 7
#define HEADER_UDP_PAYLOAD_OFFSET 8

#define FLAG_DATA 0x1
#define FLAG_ACK 0x2
#define FLAG_TERMINATION 0x4
#define UNUSED_VALUE 0x7f


typedef struct payload_header {
	uint16_t header_len;
	uint16_t unique_num;
	uint16_t filename_len; // including final 0
	char* filename;
	char* image;
} Payload;

struct message_header_udp {
	uint32_t header_len;
	unsigned char seqnum;
	unsigned char lastrecv_seqnum;
	unsigned char flag;
	unsigned char unused;
	char *payload;
};

// payload header
Payload* set_payload_header_info(char* filename, char* image, int unique_num, int image_len);
void construct_payload_header(char* buffer, Payload *header);
void deconstruct_payload_header_static(char* buffer, Payload *header);
void deconstruct_payload_header_msg(char* buffer, Payload *header);
// void payload_header_free(struct payload_header *header);


// udp header
struct message_header_udp* set_udp_message_header_info(char* payload, int payload_len, int seq_num, int prev_seq_num, unsigned char flag);
void construct_udp_message_header(char* buffer, struct message_header_udp* header);
void deconstruct_udp_message_header_static(char* buffer, struct message_header_udp* header);
void deconstruct_udp_message_header_payload(char* buffer, struct message_header_udp* header);

#endif /* PROTOCOL_H */