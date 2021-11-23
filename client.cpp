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
void send_file(Socket &server, string &filename){
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
void send_files(Socket &server, vector<string> &op){
    if (op.size() < 2){
        cout << "Command format error." << endl;
        return;
    }
    // Iterate all files
    for (int i = 1; i < op.size(); ++i)
        send_file(server, op[i]);
}

/* Receive single file */
void recv_file(Socket &server, string &filename){ // Create file even when no corresponding file
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
void recv_files(Socket &server, vector<string> &op){
    if (op.size() < 2){
        cout << "Command format error." << endl;
        return;
    }
    // Iterate all files
    for (int i = 1; i < op.size(); ++i)
        recv_file(server, op[i]);
}

/* List remote files */
void list_remote(Socket &server, vector<string> &op){
    // Format error
    if (op.size() >= 2){
        cout << "Command format error." << endl;
        return;
    }

    // Send hello message
    server.init();
    server.buf = "LS  ";
    server.gen_header(".MSG", server.buf.length());
    server.send_all();

    // Receive response
    server.init();
    server.recv_all(true);
}

/* Play remote mpg */
void play_mpg(Socket &server, vector<string> &op){
    // Command error
    auto ext = op[1].substr(max(0, int(op[1].size()) - 4));
    if (ext != ".mpg" && ext != ".MPG")
        return cout << "The " << op[1] << " is not a mpg file." << endl, void();

    // Send hello message - ask resolution
    server.init();
    server.buf = "ASK " + op[1];
    server.gen_header(".MSG", server.buf.length());
    server.send_all();

    // Get resolution
    server.init();
    server.recv_all();
    if (server.type == ".NIL") // not exist
        return cout << "The " << op[1] << " doesn't exist." << endl, void();

    int pos_x = server.buf.find('x');
    server.width = stoi(server.buf.substr(4, pos_x));
    server.height = stoi(server.buf.substr(pos_x + 1));
    cerr << server.width << 'x' << server.height << endl; // Debug

    // Play mpg
    server.init();
    server.play();
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
    Socket server(sockfd, true);
    while (true){
        cout << "$ ";
        getline(cin, buf);
        if (cin.eof())
            break;
        auto op = parse_args(buf);
        if (op.empty());
            // Do nothing
        else if (op[0] == "ls")
            list_remote(server, op);
        else if (op[0] == "put")
            send_files(server, op);
        else if (op[0] == "get")
            recv_files(server, op);
        else if (op[0] == "play")
            play_mpg(server, op);
        else
            cout << "Command not found." << endl;
    }

    close(sockfd);
    return 0;
}

