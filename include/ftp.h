#pragma once

#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include "getip.h"

#define READ_BUF_SIZE 1024
#define FTP_PORT 21
#define AWAIT_CMD_RESPONSE 10000

#define LOGIN_SUCCESS "230"
#define PASV_SUCCESS "227"
#define FILE_ACCEPTED "150"

typedef struct {

    char username[255];
    char password[255];

} user_info;

typedef struct {

    char* domain;
    char* filepath;
    char* filename;

} url_info;

int ftp_server(char* url);
int parse_url(char* url, user_info* user, url_info* url_data);
int get_data_socket(user_info* user);
int get_file(url_info* url_data);
void write_cmd(int socket_fd, char* cmd);
void read_socket(int socket_fd, char* buf);
void free_resources(url_info* url_data);
