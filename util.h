#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

struct files_paths {
	int num_files;
	char **files;
};

void print_bits(char* str, int len);
void exit_if_error(int ret, char *msg);
void filepaths_free(struct files_paths *files);
bool dir_exist(char *path);

#endif /* UTIL_H */