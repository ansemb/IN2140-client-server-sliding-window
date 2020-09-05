#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include "util.h"

void print_bits(char* str, int len)
{
	char a;
  int i,j;
	for (j=0; j<len; j++){
		a = str[j];
		for (i = 0; i < 8; i++) {
				printf("%d", !!((a << i) & 0x80));
		}
	}
  printf("\n");
}

void exit_if_error(int ret, char *msg)
{
	if (ret==-1){
		perror(msg);
		exit(EXIT_FAILURE);
	}
}

bool dir_exist(char *path)
{
	DIR* dir = opendir(path);
	if (dir){
		closedir(dir);
		return true;
	}
	return false;
}

void filepaths_free(struct files_paths *files)
{
	int i;
	if (files==NULL) return;
	for (i=0; i<files->num_files; i++){
		free(files->files[i]);
	}
	free(files->files);
	free(files);
}
