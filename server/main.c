#include "../lib/drop.h"

void configure_data();
void disconnect_clients();

/* Rememeber to create this*/
char backup_main_path[100] = "./data/";
char confing_file_path[100] = "./config";

time_t last_client_push;
int is_working = 1;
int server_socket;
int delay_time = 10;
int port = PORT; 
int current_socket;
time_t current_time;
fd_set set;
int fd_hwm;

int main(int argc, char *argv[])
{	
	/*Prepare to exit */
	signal(SIGINT, exit_signal_handler);
	
	// TODO: AteXIT fun!
	atexit(before_exit);

	/* Reading configuration */
	configure_data();

	/* Creating, binding and listening on socket */
	struct sockaddr_in sa_server;

	sa_server.sin_family = AF_INET;
	sa_server.sin_addr.s_addr = INADDR_ANY;
	sa_server.sin_port = htons(port);
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(server_socket, (struct sockaddr *) &sa_server, sizeof(sa_server)) < 0)
		error("inet bind");
	if(listen(server_socket, SOMAXCONN) < 0)
		error("listen inet");

	/* FD set */
	fd_set read_set;
	FD_ZERO(&set);
	FD_SET(server_socket, &set);
	fd_hwm = server_socket;

	struct message message;
	int new_client;
	while(is_working) {
		read_set = set;
		if (select(fd_hwm + 1, &read_set, NULL, NULL, NULL) < 0)
			error("select");

		for (int fd = 0; fd <= fd_hwm; fd++) {
			if(FD_ISSET(fd, &read_set)) {
				/* If this is new connection to accept*/
				if (fd == server_socket) {
					new_client = accept(fd, NULL, 0);
					FD_SET(new_client, &set);
					fd_hwm = new_client > fd_hwm ? new_client : fd_hwm;
				}
				/* If this client is ready to read from him */
				else {
					recv(fd, &message, sizeof(message), 0);
					char *file_name = message.file.file_name;
					char *normalized_name = calloc(strlen(file_name) + strlen(backup_main_path),
						 sizeof(char));
					strcpy(normalized_name, backup_main_path);
					strcat(normalized_name, file_name);
					
					if (message.type == NEW_FILE) {	
						time(&last_client_push);
						receive_file_from_socket(fd, normalized_name, message.file);
						confirm_received(fd);
					
					} else if(message.type == NEW_DIR) {
						time(&last_client_push);
						receive_dir_from_socket(fd, normalized_name, message.file);
						confirm_received(fd);

					} else if(message.type == PULL_REQUEST) {
						current_socket = fd;
						current_time = message.last_update_time;
						if (difftime(last_client_push, current_time) > FILE_DIFF);
							ftw(backup_main_path, check_updates, 16);

					} else if(message.type == DISCONNECT) {
						FD_CLR(fd, &set);
					} 
					free(normalized_name);
					message.type = E;
				}
			}
		}
	}
}

/* Reads congration file and sets variables */
void configure_data()
{
	printf("\n== Starting configuration ==\n\n");
	FILE *config_file = fopen(confing_file_path, "r");
	if (config_file == NULL)
		error("fopen config file");

	char *line;
	size_t len = 0;
	ssize_t read;
	char what[256];
	char value[256];

	while ((read = getline(&line, &len, config_file)) > 0){
		char *eq = strstr(line, "=");
		if (eq == NULL)
			continue;

		*eq = 0;
		*(line + read -1) = 0;
		strcpy(what, line);
		strcpy(value, eq + 1);
		match_val(what, value);
	}

	fclose(config_file);
	FILE *server_data = fopen("server.data", "r+");
	if (server_data != NULL) {
		while ((read = getline(&line, &len, server_data)) > 0) {
			char *eq = strstr(line, "=");
			if (eq == NULL)
				continue;

			*eq = 0;
			*(line + read -1) = 0;
			strcpy(what, line);
			strcpy(value, eq + 1);
			match_val(what, value);
		}
		
		fclose(server_data);
	}

	printf("\n== Configuration finished ==\n\n");
}

/**
	Find variable with property "what" and set this variable "val"
*/
void match_val(char *what, char* val)
{	
	printf("Setting %s with %s\n", what, val);
	if (!strcmp(what, "backup start path")) {
		strcpy(backup_main_path, val);
		if(create_backup_dir(val))
			error("bad start path");
	}
	else if (!strcmp(what, "delay time"))
		delay_time = atoi(val);
	else if (!strcmp(what, "port"))
		port = atoi(val);
}

int check_updates(const char *path, const struct stat *info, int type)
{	
	if(strcmp(path, ".") == 0 
		|| strcmp(path, "..") == 0 
		|| strcmp(path, backup_main_path) == 0)
			return 0;	

	if(strstr(path, OLD_FILE) != NULL)
		return 0;

	/* Check and send file if it was modified after last update */
	if(type == FTW_F && difftime(info -> st_mtime, current_time) > FILE_DIFF) {		
		if(send_file_to_socket(current_socket, path, backup_main_path, info) < 0) {
			error("Error sending file!");
		}
	} else if(type == FTW_D && difftime(info -> st_mtime, current_time) > FILE_DIFF) {
		if(send_dir_to_socket(current_socket, path, backup_main_path, info) < 0) {
			error("Error sending file!");
		}
	}

	return 0;	
}

void disconnect_clients()
{		
	struct message message;
	message.type = DISCONNECT;

	for (int fd = 0; fd <= fd_hwm; fd++) 
		if(FD_ISSET(fd, &set))
			if (send(fd, &message, sizeof(message),	0) < 0)
				error("sendmsg");		

}

void before_exit()
{	
	shutdown(server_socket, 2);
}

void exit_signal_handler(int signum)
{
	printf("\n\nExiting\n");
	exit(EXIT_SUCCESS);
}



