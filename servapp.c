#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>


#include "util.h"
#include "pgmread.h"
#include "servapp.h"

#define DEFAULT_IMG_NAME "UNKNOWN\0"

struct Image* image_from_file(char *filepath);
char* get_localimgname(struct Image *img);
struct files_paths* read_in_files_in_dir(char *dir);

FILE *Output_file = NULL;
struct files_paths *localfiles = NULL;



int application_start(char *imgdir, char *output_file)
{
	Output_file = fopen(output_file, "w");
	if (!Output_file) return -1;

	if (!dir_exist(imgdir)) {
		application_close();
		return -1;
	}
	localfiles = read_in_files_in_dir(imgdir);
	
	return 0;
}

void application_close()
{
	if (Output_file) fclose(Output_file);
	filepaths_free(localfiles);
}

struct files_paths* read_in_files_in_dir(char *dir)
{
	DIR *dfd;
	struct dirent *entry;
	int i, dirlen, len, numfiles;
	struct files_paths* filepaths;
	char **paths;

	dfd = opendir(dir);
	if (!dfd){
		perror("opendir");
		exit(EXIT_FAILURE);
	}

	numfiles = 0;
	while ((entry = readdir(dfd)) != NULL) {
    if (entry->d_type == DT_REG) numfiles++;
	}

	filepaths = (struct files_paths*) malloc(sizeof(struct files_paths));
	paths = malloc(numfiles*sizeof(char *));

	// reset directory stream
	rewinddir(dfd);

	i=0, dirlen = strlen(dir);
	while ((entry = readdir(dfd)) != NULL) {
    if (entry->d_type != DT_REG) continue;
		
		len = dirlen+strlen(entry->d_name)+2; // len of dirpath, name of file, '/' and \0
		paths[i] = malloc(len*sizeof(char));

		sprintf(paths[i++], "%s/%s", dir, entry->d_name);
	}

	filepaths->files = paths;
	filepaths->num_files = numfiles;

	closedir(dfd);
	return filepaths;
}

/* Checks if image exist in local folder by comparing imgs.
 * returns the filename if true, else NULL.
 */
char* get_localimgname(struct Image *recvimg)
{
	// struct DIR *dfd;
	int i;
	struct Image *localimg;
	
	char *imgname = NULL, *curpath;
	
	for (i=0; i<localfiles->num_files; i++){
		curpath = localfiles->files[i];
		localimg = image_from_file(curpath);

		if (localimg==NULL) continue;

		if (Image_compare(recvimg, localimg)==1){
			imgname = basename(curpath);
			Image_free(localimg);
			break;
		}
		Image_free(localimg);
	}

	return imgname;
}

void append_to_output_file(char *recv_imgname, char *local_imgname)
{
	char *line;
	int linelen;

	linelen = strlen(recv_imgname)+strlen(local_imgname)+3; // add to strlen 3; space, \n, and \0
	line = calloc(linelen, sizeof(char));
	sprintf(line , "%s %s\n", recv_imgname, local_imgname);

	fwrite(line, sizeof(char), strlen(line), Output_file);

	free(line);
}

int handle_payload(Payload *payload)
{
	char *imgname;
	struct Image *recvimg;
	
	printf("image name: %s\n", payload->filename);
	recvimg = Image_create(payload->image);

	imgname = get_localimgname(recvimg);

	if (imgname==NULL){	// imgfile does not exist locally
		printf("no equal image found on locally\n");
		append_to_output_file(payload->filename, DEFAULT_IMG_NAME);
	}
	else {
		printf("equal image was found locally. local name: %s\n", imgname);
		append_to_output_file(payload->filename, imgname);
		// free(imgname);
	}

	Image_free(recvimg);
	return 0;
}

struct Image* image_from_file(char *filepath)
{
	struct Image *img;
	char *buffer;
	long len;
	FILE *file = fopen(filepath, "rb");

	if (file){
		fseek(file, 0, SEEK_END);
		len = ftell(file);
		fseek(file, 0, SEEK_SET);

		buffer = malloc(len);
		if (buffer){
			fread(buffer, 1, len, file);
		}
		fclose(file);
	}

	if (!buffer) return NULL;
	
	img = Image_create(buffer);
	free(buffer);
	return img;
}