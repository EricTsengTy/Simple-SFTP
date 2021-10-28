#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "msg.hpp"

#include <iostream>

using namespace std;

#define BUFF_SIZE 1024
#define PORT 8787
#define ERR_EXIT(a){ perror(a); exit(1); }

void extract_info(char *s, char *ip, int *port){
    char *tok = strtok(s, ":");
    strcpy(ip, s);
    *port = atoi(strtok(NULL, ":"));
}

int main(int argc , char *argv[]){
    int sockfd, read_byte, client_id, server_port;
    char server_ip[32];
    struct sockaddr_in addr;
    string buf;
    char c_buf[BUFF_SIZE] = {};

    if (argc < 2){
        printf("argument error\n");
        return 0;
    }

    client_id = atoi(argv[1]);
    extract_info(argv[2], server_ip, &server_port);

    // Create directory
    char folder[256];
    sprintf(folder, "b08902040_%d_client_folder", client_id);
    if (mkdir(folder, 0700) < 0)
        ERR_EXIT("mkdir failed\n");
    chdir(folder);

    // Get socket file descriptor
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        ERR_EXIT("socket failed\n");
    }

    // Set server address
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server_ip);  // inet_aton() may be better
    addr.sin_port = htons(server_port);

    // Connect to the server
    if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        ERR_EXIT("connect failed\n");
    }

    while (true){
        cout << "$ ";
        cin >> buf;
        int len = hton_str(buf, ".MSG");
        if (send(sockfd, buf.c_str(), len, 0) < 0)
            ERR_EXIT("send failed\n");
        if (recv(sockfd, c_buf, sizeof(c_buf) - 1, 0) < 0)
            ERR_EXIT("receive failed\n");
    }
   
    // Receive message from server
    // if((read_byte = read(sockfd, buffer, sizeof(buffer) - 1)) < 0){
    //     ERR_EXIT("receive failed\n");
    // }
    // printf("Received %d bytes from the server\n", read_byte);
    // printf("%s\n", buffer);
    close(sockfd);
    return 0;
}

