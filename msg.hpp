#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <filesystem>

using namespace std;

#define LEN_OFFSET 4
#define MSG_OFFSET 68
#define BUFF_SIZE 1024UL
#define ERR_EXIT(a){ perror(a); exit(1); }

struct Node{
    Node(int fd = 0, bool blocking = false):fd(fd),blocking(blocking){
        msg_mode = blocking ? 0 : MSG_DONTWAIT;
    }
    int fd;
    bool do_write = false; // true if write is needed after msg
    ulong len = 0;
    ulong cur = 0;
    bool blocking;
    int msg_mode;
    
    string buf;
    char c_buf[BUFF_SIZE] = {};

    // int stat = TYPE_NIL;
    string type;
    string header;

    // For file
    string filename;
    int file_fd = -1;

    void init();
    void gen_header(string, ulong);
    int recv_all(bool output);
    int recv_header();
    int send_all();
    int send_header();
    void process();
    void format_buf(string);
    bool header_ok(){
        return cur >= MSG_OFFSET;
    }
    bool msg_ok(){
        return cur == len;
    }
    void open_file(int oflag=O_RDONLY){
        if ((file_fd = open(filename.c_str(), oflag)) < 0)
            return;
        len = lseek(file_fd, 0, SEEK_END);
        lseek(file_fd, 0, SEEK_SET);
    }
    
    private:
    int __recv_all(bool);
    int __recv_msg(bool);
    int __recv_file();
    int __send_all();
    int __send_msg();
    int __send_file();
    void __clear_c_buf(){
        memset(c_buf, 0, sizeof(char) * BUFF_SIZE);
    }
};

string hton_header(string type, ulong len){ // len: message len
    string &s = type;
    len += MSG_OFFSET;
    for (int i = 0; i != MSG_OFFSET - LEN_OFFSET; ++i){
        s.push_back((char)(len & 0xff));
        len >>= 8;
    }
    return s;
}

string hton_str(string s, string type){
    auto header = hton_header(type, s.length());
    return header + s;
}

void Node::init(){
    len = cur = 0;
    buf.clear();
    type.clear();
    header.clear();
    file_fd = -1;
}

void Node::format_buf(string type){
    buf = hton_str(buf, type);
    len = buf.size();
}

void Node::gen_header(string t="", ulong tlen=-1){
    type = (t == "" ? type : t);
    len = (tlen == -1 ? len : tlen);
    header = hton_header(type, len);
    len += header.length();
}

int Node::recv_all(bool output=false){
    int ret = 0;
    if (blocking){
        while ((ret = __recv_all(output)) == 0);
        return ret;
    }
    // Get header and content
    // while (!msg_ok() || !header_ok())
    // cout << cur << ' ' << len << '!' << endl;
    if (!msg_ok() || !header_ok())
        if ((ret = __recv_all(output)) < 0)
            return -1;
    return ret;
}

int Node::recv_header(){
    // Read type
    __clear_c_buf();
    if (cur < LEN_OFFSET){
        int ret = recv(fd, c_buf, LEN_OFFSET - cur, msg_mode);
        if (ret < 0 && errno != EAGAIN)
            return -1;
        type += string(c_buf);
        cur = max(cur, cur + ret);
        return 0;
    }
    // Read length
    else if (cur < MSG_OFFSET){
        int ret = recv(fd, c_buf, MSG_OFFSET - cur, msg_mode);
        if (ret <= 0)     // Lost connection(?)
            return -1;
        for (int i = 0; i < ret; ++i){
            int idx = cur + i - LEN_OFFSET;
            len += ((ulong)((u_char)(c_buf[i]))) << (8 * idx); // So ugly
        }
        cur = max(cur + ret, cur);
        return 0;
    }
    return -1;
}

int Node::send_all(){
    int ret = 0;
    if (blocking){
        while ((ret = __send_all()) == 0);
        return ret;
    }
    // Get header and content
    // while (!msg_ok() || !header_ok())
    if (!msg_ok() || !header_ok())
        if ((ret = __send_all()) < 0)
            return -1;
    return ret;
}

int Node::__send_all(){
    int ret;
    if (!header_ok())
        if ((ret = send_header()) < 0 || !header_ok())
            return ret;
    if (type == ".NIL")
        return 1;
    else if (type == ".MSG")
        return __send_msg();
    else if (type == ".FIL")
        return __send_file();
    return cur == len ? 1 : 0;
}

int Node::send_header(){
    if (header.empty())
        gen_header();
    int ret = send(fd, header.c_str() + cur, MSG_OFFSET - cur, msg_mode);
    if (ret < 0 && errno == EAGAIN)
        return -1;
    cur = max(cur + ret, cur);
    return 0;
}

void Node::process(){
    if (type == ".MSG"){
        if (buf.substr(0, 3) == "PUT"){
            filename = buf.substr(4);
            cout << filename << '\n';
            init();
        }
        else if (buf.substr(0, 3) == "GET"){
            filename = buf.substr(4);
            init();
            do_write = true;
            type = ".FIL";
            open_file();
            if (file_fd < 0)
                type = ".NIL";
            gen_header();
        }
        else if (buf.substr(0, 2) == "LS"){
            init();
            do_write = true;
            for (const auto &entry : filesystem::directory_iterator("./"))
                buf += entry.path().filename().string() + "\n";
            gen_header(".MSG", buf.length());
        }
    }
    else if (type == ".FIL"){
        init();
        cout << "init" << endl;
    }
}

// Return 1 if finish else 0
int Node::__recv_all(bool output=false){
    int ret;
    if (!header_ok())
        if ((ret = recv_header()) < 0 || !header_ok())
            return ret;

    if (type == ".NIL")
        return 1;
    else if (type == ".MSG")
        return __recv_msg(output);
    else if (type == ".FIL")
        return __recv_file();
    return (len == cur) ? 1 : 0;
}

int Node::__recv_msg(bool output=false){
    if (cur == len) return 1;
    __clear_c_buf();
    int ret = recv(fd, c_buf, min(len - cur, BUFF_SIZE - 1), msg_mode);
    if (ret < 0 && errno != EAGAIN)    // Lost connection (?)
        return -1;
    if (output)
        printf("%s", c_buf);
    else
        buf += string(c_buf);
    cur = max(cur, cur + ret);
    return cur == len ? 1 : 0;
}

int Node::__recv_file(){
    __clear_c_buf();
    if (len == MSG_OFFSET)
        return creat(filename.c_str(), 0600), 1;
    if (file_fd < 0)
        file_fd = open(filename.c_str(), O_CREAT | O_WRONLY, 0600);
    int ret = recv(fd, c_buf, min(len - cur, BUFF_SIZE), msg_mode);
    if (ret < 0 && errno != EAGAIN)
        return -1;
    write(file_fd, c_buf, ret);
    cur = max(cur + ret, cur);
    if (cur == len)
        close(file_fd), file_fd = -1;
    cout << filename << ' ' << cur << ' ' << len << '\n';
    return cur == len ? 1 : 0;
}

int Node::__send_msg(){
    if (len == MSG_OFFSET)
        return 1;
    int ret = send(fd, buf.c_str() + cur - MSG_OFFSET, len - cur, msg_mode);
    if (ret < 0 && errno != EAGAIN)
        return -1;
    cur = max(cur + ret, cur);
    if (cur == len)
        do_write = false;
    return cur == len ? 1 : 0;
}

int Node::__send_file(){
    __clear_c_buf();
    int read_b = read(file_fd, c_buf, BUFF_SIZE);
    int send_b = send(fd, c_buf, read_b, msg_mode);
    lseek(file_fd, send_b - read_b, SEEK_CUR);
    cur += send_b;
    if (cur == len){
        do_write = false;
        close(file_fd);
        file_fd = -1;
    }
    return cur == len ? 1 : 0;
}