#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <filesystem>
#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;

#define LEN_OFFSET 4
#define MSG_OFFSET 68
#define BUFF_SIZE 1024UL
#define ERR_EXIT(a){ perror(a); exit(1); }

struct Socket{
    Socket(int fd = 0, bool blocking = false):fd(fd),blocking(blocking){
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

    // For mpg
    Mat video_img;
    VideoCapture cap;
    int width, height;
    int imgSize;

    void init();
    void gen_header(string, ulong);
    int recv_all(bool output);
    int recv_header();
    int send_all();
    int send_header();
    void process();
    void play();
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
    
    int __recv_all(bool);
    int __recv_msg(bool);
    int __recv_file();
    int __recv_mpg();
    int __send_all();
    int __send_msg();
    int __send_file();
    int __send_mpg();
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

void Socket::init(){
    len = cur = 0;
    buf.clear();
    type.clear();
    header.clear();
    file_fd = -1;
}

// void Socket::format_buf(string type){
//     buf = hton_str(buf, type);
//     len = buf.size();
// }

void Socket::gen_header(string t="", ulong tlen=-1){
    type = (t == "" ? type : t);
    len = (tlen == -1 ? len : tlen);
    header = hton_header(type, len);
    len += header.length();
}

int Socket::recv_all(bool output=false){
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

int Socket::recv_header(){
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

int Socket::send_all(){
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

int Socket::__send_all(){
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
    else if (type == ".MPG")
        return __send_mpg();
    return cur == len ? 1 : 0;
}

int Socket::send_header(){
    if (header.empty())
        gen_header();
    int ret = send(fd, header.c_str() + cur, MSG_OFFSET - cur, msg_mode);
    if (ret < 0 && errno == EAGAIN)
        return -1;
    cur = max(cur + ret, cur);
    return 0;
}

void Socket::process(){
    if (type == ".MSG"){
        if (buf.substr(0, 3) == "PUT"){
            filename = buf.substr(4);
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
        else if (buf.substr(0, 4) == "ASK "){
            string filename = buf.substr(4);
            init();
            do_write = true;
            if (access(filename.c_str(), O_RDONLY) < 0){
                gen_header(".NIL", 0);
                return;
            }
            cap = VideoCapture(filename);
            width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
            height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
            video_img = Mat::zeros(height, width, CV_8UC3);
            imgSize = video_img.total() * video_img.elemSize();
            buf = "RES " + to_string(width) + "x" + to_string(height);
            gen_header(".MSG", buf.length());
        }
        else if (buf.substr(0, 4) == "PLAY"){
            init();
            do_write = true;
            cap >> video_img;
            gen_header(".MPG", imgSize);
        }
        else if (buf.substr(0, 4) == "EXIT"){
            init();
        	cap.release();
        }
    }
    else if (type == ".FIL"){
        init();
    }
}

/* Play mpg (only support blocking mode) */
void Socket::play(){
    video_img = Mat::zeros(height, width, CV_8UC3);

    // Debug log
    cerr << "Video resolution: " << height << 'x' << width << endl;

    // Ensure memory is continuos (for efficiency issue)
    if (!video_img.isContinuous())
        video_img = video_img.clone();
    
    imgSize = video_img.total() * video_img.elemSize();

    // First Hello message
    init();
    buf = "PLAY";
    gen_header(".MSG", buf.length());
    send_all();

    // Main loop
    while (true){
        // Get frame
        // uchar *iptr = video_img.data;
        // recv(fd, iptr, imgSize, 0);
        init();
        recv_all();

        // Show frame
        imshow("Video", video_img);

        // Continue or break
        char c = (char)waitKey(33.3333);
        init();
        buf = (c == 27) ? "EXIT" : "PLAY";
        gen_header(".MSG", buf.length());
        send_all();
        if (c == 27)
            break;
    }
	destroyAllWindows();
}

// Return 1 if finish else 0
int Socket::__recv_all(bool output=false){
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
    else if (type == ".MPG")
        return __recv_mpg();
    return (len == cur) ? 1 : 0;
}

int Socket::__recv_msg(bool output=false){
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

int Socket::__recv_file(){
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
    return cur == len ? 1 : 0;
}

int Socket::__recv_mpg(){
    if (cur == len) return 1; // impossible?
    int ret = recv(fd, video_img.data + cur - MSG_OFFSET, len - cur, msg_mode);
    if (ret < 0 && errno != EAGAIN)
        return -1;
    cur = max(cur, cur + ret);
    return cur == len ? 1 : 0;
}

int Socket::__send_msg(){
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

int Socket::__send_file(){
    __clear_c_buf();
    int read_b = read(file_fd, c_buf, BUFF_SIZE);
    int send_b = send(fd, c_buf, read_b, msg_mode);
    lseek(file_fd, (send_b < 0) ? -read_b : send_b - read_b, SEEK_CUR);
    if (send_b < 0 && errno != EAGAIN)
        return -1;
    cur = max(cur + send_b, cur);
    if (cur == len){
        do_write = false;
        close(file_fd);
        file_fd = -1;
    }
    return cur == len ? 1 : 0;
}

int Socket::__send_mpg(){
    if (len == MSG_OFFSET)  // I think it's impossible
        return 1;
    int ret = send(fd, video_img.data + cur - MSG_OFFSET, len - cur, msg_mode);
    if (ret < 0 && errno != EAGAIN)
        return -1;
    cur = max(cur + ret, cur);
    if (cur == len)
        do_write = false;
    return cur == len ? 1 : 0;
}