#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <iostream>

using namespace std;

#define LEN_OFFSET 4
#define MSG_OFFSET 12
#define BUFF_SIZE 1024

enum {
    TYPE_NULL,
    TYPE_MPG,
    TYPE_MSG,
    TYPE_FIL,
};

struct Node{
    int stat = TYPE_NULL;
    bool write = false; // true if write is needed after recv_msg
    int len = 0;
    int cur = 0;
    string type;
    string buf;

    void init(){
        len = cur = 0;
        buf.clear();
        type.clear();
    }
};

int hton_str(string &s, string type){
    int len = s.size() + MSG_OFFSET;
    int tlen = len;
    string tmp(s);
    s = type;
    for (int i = 0; i != 8; ++i){
        s.push_back((char)(len & 0xff));
        len >>= 8;
    }
    s += tmp;
    return tlen;
}

void hton_msg(Node &node, string type){
    node.len = hton_str(node.buf, type);
}

int type2stat(string type){
    if (type == ".MPG")
        return TYPE_MSG;
    else if (type == ".MSG")
        return TYPE_MSG;
    else if (type == ".FIL")
        return TYPE_FIL;
    return TYPE_NULL;
}

int recv_msg(int fd, Node &node){
    // Read type
    char buf[BUFF_SIZE] = {};
    if (node.cur < LEN_OFFSET){
        int ret = recv(fd, buf, LEN_OFFSET - node.cur, MSG_DONTWAIT);
        node.type += string(buf);
        // int ret = recv(fd, node.type + node.cur, LEN_OFFSET - node->cur, MSG_DONTWAIT);
        node.cur += ret;
        if (node.cur == LEN_OFFSET)
            node.stat = type2stat(node.type);
        return (ret <= 0) ? -1 : 0;
    }
    // Read length
    else if (node.cur < MSG_OFFSET){
        int ret = recv(fd, buf, MSG_OFFSET - node.cur, MSG_DONTWAIT);
        if (ret <= 0)     // Lost connection(?)
            return -1;
        for (int i = 0; i < ret; ++i){
            int idx = node.cur + i - LEN_OFFSET;
            node.len += (int)(buf[idx]) << (8 * idx);
        }
        node.cur += ret;
        if (node.cur == MSG_OFFSET){
            // node->buf = (char *)malloc(sizeof(char) * node->len);
            // memset(node->buf, 0, sizeof(char) * node->len);
        }
        return 0;
    }
    // Read content
    else if (node.stat == TYPE_MSG){
        int ret = recv(fd, buf, node.len - node.cur, MSG_DONTWAIT);
        if (ret <= 0)    // Lost connection (?)
            return -1;
        node.buf += string(buf);
        node.cur += ret;
        return node.cur == node.len ? 1 : 0;
    }
    return -1;
}

bool send_msg(int fd, Node &node){
    int ret = send(fd, node.buf.c_str(), node.len - node.cur, MSG_DONTWAIT);
    node.cur += ret;
    return node.cur == node.len;
}