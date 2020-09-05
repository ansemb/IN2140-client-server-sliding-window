#ifndef SERVAPP_H
#define SERVAPP_H

#include "protocol.h"

#define DEFAULT_OUTPUT_DIR "out.txt"

int application_start(char *imgdir, char *output_file);
void application_close();
int handle_payload(struct payload_header *paylaod);

#endif /* SERVAPP_H */