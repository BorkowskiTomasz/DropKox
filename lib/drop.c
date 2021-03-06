#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <poll.h>
#include <sys/stat.h>
#include "drop.h"

/** Receive file from socket */
int receive_file_from_socket(int socket, const char *file_path, struct file_info info)
{
	deal_with_old_file(file_path);

	printf("Receiving file: \t %s\n", file_path);
	int received = 0;

	int buff[BUFF_SIZE];
	int size = 1;
	int output = open(file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IRGRP  );

	printf("\tFile size %d \n", (int) info.file_size );
	int buff_size;
	while (received  < (int) info.file_size) {
		buff_size = (info.file_size - received) > BUFF_SIZE ? BUFF_SIZE : info.file_size - received;
		size = recv(socket, buff, buff_size, 0);
		received += size;
		printf("\t\t---- Receiving: %d%%\n", (int) received * 100 / (int) info.file_size);
		write(output, buff, size);
	}

	close(output);
	return 0;
}


int deal_with_old_file(const char* file_path)
{	
	/* Checking if file exists*/
	if(access(file_path, F_OK))
		return 0;

	char *tmp = calloc(strlen(file_path) + MAX_BACKUPS * strlen(OLD_FILE), sizeof(char));
	strcpy(tmp, file_path);
	strcat(tmp, OLD_FILE);

	int i = 0;
	while (!access(tmp, F_OK) && i < MAX_BACKUPS)
	{
		strcat(tmp, OLD_FILE);
		i++;
	}
	
	/* To much copies */
	if (i == MAX_BACKUPS) {
		memset(tmp + strlen(tmp) - strlen(OLD_FILE), 0, strlen(OLD_FILE));
		if (remove(tmp) < 0)
			error("cannot remove this file");
	}

	char *old_name = calloc(strlen(tmp), sizeof(char));
	strncpy(old_name, tmp, strlen(tmp) - strlen(OLD_FILE));
	
	while (strcmp(tmp, file_path)) {
		if (rename(old_name, tmp) < 0)
			error("rename");

		strcpy(tmp, old_name);
		memset(old_name, 0, strlen(old_name));
		strncpy(old_name, tmp, strlen(tmp) - strlen(OLD_FILE));
	}

	free(tmp);
	free(old_name);
	return 1;
}


/** Sends file to socket
	Returns 1 if ok else -1 */
int send_file_to_socket(int socket, const char* path, const char* main_path, const struct stat *info)
{

	/* Remove start dir from path */
	char *normalized_name = calloc(strlen(path) - strlen(main_path), sizeof(char));
	strncpy(normalized_name, path + strlen(main_path), strlen(path) - strlen(main_path));
	printf("Sending file: \t %s \n", normalized_name);

	struct message message;
	message.type = NEW_FILE;

	strcpy(message.file.file_name, normalized_name);
	message.file.file_size = info -> st_size;
	message.file.permissions = info -> st_mode;

	int to_send = open(path, O_RDONLY);
    if (to_send == -1) {
   		printf("Cannot open file to send!\n");
   		return -1;
    }

	if (send(socket, &message, sizeof(message), 0) < 0)
		perror("send file info");

	int size;
	off_t offset = 0; 
	size = sendfile (socket, to_send, &offset, info -> st_size);
    if (size == -1) {
    	error("Can't send file! :(\n");
    	return -1;
    }
    if (size != message.file.file_size){
    	error("Not whole file was sent!\n");
    	return -1;
    }
  
	free(normalized_name);
	return 1;
}

int send_dir_to_socket(int socket, const char* path, const char* main_path, const struct stat *info)
{
		/* Remove start dir from path */
	char *normalized_name = calloc(strlen(path) - strlen(main_path), sizeof(char));
	strncpy(normalized_name, path + strlen(main_path), strlen(path) - strlen(main_path));

	printf("Sending dir: \t %s\n", normalized_name);
	
	struct message message;
	message.type = NEW_DIR;

	strcpy(message.file.file_name, normalized_name);
	message.file.file_size = info -> st_size;
	message.file.permissions = info -> st_mode;
	
	if (send(socket, &message, sizeof(message), 0) < 0)
		perror("send dir");

	
	free(normalized_name);
	return 0;
}

int receive_dir_from_socket(int socket, const char *path, const struct file_info info)
{
	printf("Receiving dir: \t %s\n", path);

	if(!access(path, F_OK))
		return 0;

	if (mkdir(path, info.permissions) < 0)
		error("mkdir");

	printf("Received dir:  \t %s\n", path);
	return 0;
}

int confirm_received(int socket)
{
	struct message message;

	message.type = CONF;

	if (send(socket, 
					&message, 
					sizeof(message), 
					0) < 0)
				error("sendmsg");	

	return 0;
}

int create_backup_dir(char *path)
{
	if(!access(path, F_OK))
		return 0;

	int size = strlen(path);
	char *buff = calloc(size, sizeof(char));
	char c = '/';
	int i = 0;

	int perm = S_IRGRP | S_IRWXU | S_IROTH | S_IWOTH; 

	while(i < size) {
		
		while(i < size && c != path[i]) {
			buff[i] = path[i];
			i++;
		}

		buff[i] = path[i];
		++i;

		if (strcmp(buff, "./") && strcmp(buff, "."))
			if (access(path, F_OK) && mkdir(buff, perm)) {
				perror("mkdir");
				return -1;
			}
	}

	free(buff);
	return 0;
}

int error(char *error_info)
{
	perror(error_info);
	exit(-1);
}