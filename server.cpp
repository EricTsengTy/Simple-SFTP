#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#include <iostream>
#include <vector>
#include "msg.hpp"

using namespace std;

#define BUFF_SIZE 1024
#define MAX_CLIENT 16384
#define PORT 8787
#define ERR_EXIT(a){ perror(a); exit(1); }

/* Return 1 if write is needed */
int process_msg(int fd, Node &node){
    node.buf = "hi";
    // strcpy(node->buf, "hi");
    hton_msg(node, ".MSG");
    // node->cur = 0;
    node.cur = 0;
    return 1;
}

int main(int argc, char *argv[]){
    int server_sockfd, client_sockfd, write_byte;
    int server_port;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    string buffer(2 * BUFF_SIZE, ' ');
    
    if (argc < 2){
        printf("argument error\n");
        return 0;
    }
    server_port = atoi(argv[1]);

    // Create directory
    string folder = "./b08902040_server_folder";
    if (mkdir(folder.c_str(), 0700) < 0)
        ERR_EXIT("mkdir failed\n");
    chdir(folder.c_str());

    // Get socket file descriptor
    if((server_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        ERR_EXIT("socket failed\n")
    }

    // Set server address information
    bzero(&server_addr, sizeof(server_addr)); // erase the data
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(server_port);
    
    // Bind the server file descriptor to the server address
    if(bind(server_sockfd, (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0){
        ERR_EXIT("bind failed\n");
    }
        
    // Listen on the server file descriptor
    if(listen(server_sockfd , 3) < 0){      // Is 3 enough ???
        ERR_EXIT("listen failed\n");
    }
    
    // Setup fds
    fd_set read_fds, read_fds_sl;
    fd_set write_fds, write_fds_sl;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    int max_nfds = server_sockfd;
    FD_SET(server_sockfd, &write_fds);
    FD_SET(server_sockfd, &read_fds);

    // Handle client msgs
    vector<Node> clients(MAX_CLIENT);

    while (true){
        read_fds_sl = read_fds;
        write_fds_sl = write_fds;
        if (select(max_nfds + 1, &read_fds_sl, &write_fds_sl, NULL, NULL) < 0)
            ERR_EXIT("select failed\n");

        for (int fd = 0; fd <= max_nfds; ++fd){
            if (FD_ISSET(fd, &read_fds_sl)){
                printf("read_set %d\n", fd);
                if (fd == server_sockfd){
                    int new_fd = accept(server_sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len);
                    if (new_fd < 0)
                        ERR_EXIT("accept failed\n");

                    FD_SET(new_fd, &read_fds);
                    max_nfds = (new_fd > max_nfds) ? new_fd : max_nfds;
                }
                else{
                    int ret = recv_msg(fd, clients[fd]);
                    if (ret < 0){                           // Lost connection (?)
                        FD_CLR(fd, &read_fds);
                        clients[fd].init();
                    }
                    if (ret > 0 && process_msg(fd, clients[fd])){
                        FD_CLR(fd, &read_fds);
                        FD_SET(fd, &write_fds);
                    }
                }
            }
            else if (FD_ISSET(fd, &write_fds_sl)){
                printf("write_set %d\n", fd);
                if (send_msg(fd, clients[fd])){
                    clients[fd].init();
                    FD_CLR(fd, &write_fds);
                    FD_SET(fd, &read_fds);
                }
            }
        }
    }

    // Note
    // 1. How to detemine when to client sockfd

    // Accept the client and get client file descriptor
    // if((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0){
    //     ERR_EXIT("accept failed\n");
    // }
    
    // sprintf(buffer, "%s\n", message);
    // if((write_byte = send(client_sockfd, buffer, strlen(buffer), 0)) < 0){
    //     ERR_EXIT("write failed\n");
    // }

    // close(client_sockfd);
    // close(server_sockfd);
    return 0;
}
