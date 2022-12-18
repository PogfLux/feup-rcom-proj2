#include "../include/ftp.h"

char read_buf[READ_BUF_SIZE];
int ctrl_socket_fd;
int data_socket_fd;

int ftp_server(char* url) {

    user_info user;
    url_info url_data;
    struct sockaddr_in server;

    if (parse_url(url, &user, &url_data) == -1) {
        return -1;
    }

    printf("Downloading %s from %s...\n", url_data.filename, url_data.domain);

    struct hostent* addr = get_ip(url_data.domain);

    bzero((unsigned char *) &server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) addr->h_addr)));
    server.sin_port = htons(FTP_PORT); 

    if ((ctrl_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error opening control socket\n");
        exit(-1);
    }
    
    if (connect(ctrl_socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        fprintf(stderr, "Error connecting to server\n");
        exit(-1);
    }

    printf("Opened connection to server.\n");

    if (get_data_socket(&user) == -1) return -1;
    if (get_file(&url_data) == -1) return -1;

    if (close(ctrl_socket_fd) < 0) {
        fprintf(stderr, "Error closing control socket\n");
        exit(-1);
    }

    if (close(data_socket_fd) < 0) {
        fprintf(stderr, "Error closing control socket\n");
        exit(-1);
    }

    free_resources(&url_data);

    return 0;

}

int parse_url(char* url, user_info* user, url_info* url_data) {

    char* token;
    char* filepath;

    if (strstr(url, "://") != NULL) url += 6;
    if (strstr(url, "@") == NULL) {
        
        strcpy(user->username, "anonymous");
        strcpy(user->password, "password");

        token = strtok(url, "/");
        filepath = token + strlen(token) + 1;

        url_data->domain = strdup(token);
        url_data->filepath = strdup(filepath);

        if (strchr(filepath, '/') == NULL) url_data->filename = strdup(filepath);
        else url_data->filename = strdup(strrchr(filepath, '/') + 1);

        return 0;

    } else {

        token = strtok(url, ":");
        if (token == NULL || strcmp(token, url)) {
            fprintf(stderr, "Error parsing username\n");
            return -1;
        }

        strcpy(user->username, token);

        token = strtok(NULL, "@");
        if (token == NULL){
            fprintf(stderr, "Error parsing password\n");
            return -1;
        }

        strcpy(user->password, token);

        char* full_url = malloc(strlen(token + strlen(token) + 1));
        strcpy(full_url, token + strlen(token) + 1);

        token = strtok((token + strlen(token) + 1), "/");
        filepath = token + strlen(token) + 1;
    
        url_data->domain = strdup(token);
        url_data->filepath = strdup(filepath);
        
        if (strchr(filepath, '/') == NULL) url_data->filename = strdup(filepath);
        else url_data->filename = strdup(strrchr(filepath, '/') + 1);

        free(full_url);
        return 0;

    }

    return -1;

}

int get_data_socket(user_info* user) {

    char user_cmd[261];
    char code[4]; code[3] = '\0';

    sprintf(user_cmd, "user %s\n", user->username);
    write_cmd(ctrl_socket_fd, user_cmd);

    sprintf(user_cmd, "pass %s\n", user->password);
    write_cmd(ctrl_socket_fd, user_cmd);

    memcpy(code, read_buf, 3);
    if (strcmp(code, LOGIN_SUCCESS) != 0) {
        fprintf(stderr, "Failed to login.\n");
        return -1;
    }

    printf("Login successful.\n");
    sprintf(user_cmd, "pasv\n");
    write_cmd(ctrl_socket_fd, user_cmd);

    memcpy(code, read_buf, 3);
    if (strcmp(code, PASV_SUCCESS) != 0) {
        fprintf(stderr, "Error entering passive mode in server\n");
        return -1;
    }

    printf("\nEntered passive mode, getting data socket...\n");

    unsigned char data_socket_info[6];
    
    char* read_buf_cpy = strdup(read_buf);
    char* token = strchr(read_buf_cpy, '(');
    token++;

    token = strtok(token, ",");
    for (int i = 0; i < 6; i++) {
        data_socket_info[i] = atoi(token); 
        token = strtok(NULL, ",");
    }

    free(read_buf_cpy);

    char data_socket_ip[18];
    sprintf(data_socket_ip, "%d.%d.%d.%d", data_socket_info[0], data_socket_info[1], data_socket_info[2], data_socket_info[3]);

    int data_socket_port = data_socket_info[4] * 256 + data_socket_info[5];
    
    struct sockaddr_in data;

    bzero((unsigned char *) &data, sizeof(data));
    data.sin_family = AF_INET;
    data.sin_addr.s_addr = inet_addr(data_socket_ip);
    data.sin_port = htons(data_socket_port); 

    if ((data_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error opening data socket\n");
        return(-1);
    }

    if (connect(data_socket_fd, (struct sockaddr *) &data, sizeof(data)) < 0) {
        fprintf(stderr, "Error connecting to data socket\n");
        return(-1);
    }

    printf("Opened data socket.\n");

    return 0;
}

int get_file(url_info* url_data) {

    char user_cmd[261];
    char code[4]; code[3] = '\0';
    
    sprintf(user_cmd, "retr %s\n", url_data->filepath);
    write_cmd(ctrl_socket_fd, user_cmd);

    memcpy(code, read_buf, 3);

    if (strcmp(code, FILE_ACCEPTED) != 0) {
        fprintf(stderr, "Failed to open file in FTP server, check for filename errors.\n");
        return -1;
    }

    FILE* file = fopen(url_data->filename, "w+");
    if (file == NULL) {
        fprintf(stderr, "Error creating local file.\n");
        return -1;
    }

    printf("Downloading file...\n");

    size_t bytes_read = recv(data_socket_fd, read_buf, READ_BUF_SIZE, 0);
    if (bytes_read <= 0) {
        fprintf(stderr, "Failed to read from data socket\n");
        return -1;
    }

    fwrite(read_buf, sizeof(unsigned char), bytes_read, file);
    
    while (bytes_read != 0) {

        bytes_read = recv(data_socket_fd, read_buf, READ_BUF_SIZE, 0);

        if (bytes_read < 0) {
            fprintf(stderr, "Failed to read from data socket\n");
            return -1;
        }

        fwrite(read_buf, sizeof(unsigned char), bytes_read, file);

    }

    printf("File download successful, leaving...\n");
    fclose(file);

    return 0;
}

void write_cmd(int socket_fd, char* cmd) {

    if (send(socket_fd, cmd, strlen(cmd), 0) <= 0) {
        fprintf(stderr, "Error writing to control socket\n");
        close(socket_fd);
        exit(-1);
    }

    memset(read_buf, 0, READ_BUF_SIZE);
    usleep(AWAIT_CMD_RESPONSE);
    read_socket(socket_fd, read_buf);

}

void read_socket(int socket_fd, char* buf) {

    size_t bytes_read = recv(socket_fd, buf, READ_BUF_SIZE, 0);
    if (bytes_read <= 0) {
        fprintf(stderr, "Failed to read from socket\n");
        return;
    }

    while (bytes_read == READ_BUF_SIZE) {
        if (bytes_read <= 0) {
            fprintf(stderr, "Failed to read from socket\n");
            return;
        }

        bytes_read = recv(socket_fd, buf, READ_BUF_SIZE, 0);
    }

}

void free_resources(url_info* url_data) {

    free(url_data->domain);
    free(url_data->filename);
    free(url_data->filepath);

}
