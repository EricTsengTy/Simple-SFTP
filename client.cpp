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
#include <sched.h>

#include <iostream>
#include <vector>
#include "msg.hpp"

using namespace std;

#define BUFF_SIZE 1024UL
#define PORT 8787
#define ERR_EXIT(a){ perror(a); exit(1); }

/* Extract ip, port from [ip:port] */
void extract_info(char *s, char *ip, int *port){
    char *tok = strtok(s, ":");
    strcpy(ip, s);
    *port = atoi(strtok(NULL, ":"));
}

/* Parse command argument */
vector<string> parse_args(string cmd){
    int pos = 0;
    vector<string> ret;
    cmd.push_back(' ');
    while ((pos = cmd.find(" ")) != string::npos){
        if (pos != 0)
            ret.push_back(cmd.substr(0, pos));
        cmd.erase(0, pos + 1);
    }
    return ret;
}

/* Send single file */
void send_file(Node &server, string &filename){
    // Check file (may exist race condition)
    if (access(filename.c_str(), O_RDONLY) < 0){
        cout << "The " << filename << " doesn't exist." << endl;
        return;
    }
    cout << "putting " << filename << "......" << endl;

    // Send hello message
    server.init();
    server.buf = "PUT " + filename;
    server.gen_header(".MSG", server.buf.length());
    server.send_all();
    
    // Send file content
    server.init();
    server.filename = filename;
    server.open_file();
    server.gen_header(".FIL");
    server.send_all();
}

/* Send multiple files */
void send_files(Node &server, vector<string> &op){
    if (op.size() < 2){
        cout << "Command format error." << endl;
        return;
    }
    // Iterate all files
    for (int i = 1; i < op.size(); ++i)
        send_file(server, op[i]);
}

/* Receive single file */
void recv_file(Node &server, string &filename){ // Create file even when no corresponding file
    // Send hello message
    server.init();
    server.buf = "GET " + filename;
    server.gen_header(".MSG", server.buf.length());
    server.send_all();

    // Receive file content (check file access?)
    server.init();
    server.filename = filename;
    server.recv_all();

    // Log message
    if (server.type == ".NIL")
        cout << "The " << filename << " doesn't exist." << endl;
    else
        cout << "getting " << filename << "......" << endl;
}

/* Receive multiple files */
void recv_files(Node &server, vector<string> &op){
    if (op.size() < 2){
        cout << "Command format error." << endl;
        return;
    }
    // Iterate all files
    for (int i = 1; i < op.size(); ++i)
        recv_file(server, op[i]);
}

/* List remote files */
void list_remote(Node &server){
    // Send hello message
    server.init();
    server.buf = "LS  ";
    server.gen_header(".MSG", server.buf.length());
    server.send_all();

    // Receive response
    server.init();
    server.recv_all(true);
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

    // Arguments
    client_id = atoi(argv[1]);
    extract_info(argv[2], server_ip, &server_port);

    // Create directory
    char folder[256];
    sprintf(folder, "b08902040_%d_client_folder", client_id);
    if (mkdir(folder, 0700) < 0)
        // ERR_EXIT("mkdir failed\n");
        ;
    chdir(folder);

    // Avoid path traversal
    if (unshare(CLONE_NEWUSER) < 0)
        ERR_EXIT("unshare failed\n");
    if (chroot("./") < 0)
        ERR_EXIT("chroot failed\n");

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

    // Main loop
    Node server(sockfd, true);
    while (true){
        cout << "$ ";
        getline(cin, buf);
        auto op = parse_args(buf);
        if (op.empty()){

        }
        else if (op[0] == "ls")
            list_remote(server);
        else if (op[0] == "put")
            send_files(server, op);
        else if (op[0] == "get")
            recv_files(server, op);
        else
            cout << "Command not found." << endl;
    }

    close(sockfd);
    return 0;
}

