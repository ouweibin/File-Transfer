
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : filetransfer.c
*   Last Modified : 2019-11-06 21:01
*   Describe      :
*
*******************************************************/

#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FT_VERSION      "0.1"
#define SOCK_ERROR      -1
#define SEND_BUF_SIZE   10000
#define RECV_BUF_SIZE   10000
#define DEFAULT_PORT    9527
#define DEFAULT_ADDR    "127.0.0.1"

#if _DEBUG
#define TRIM(x) strrchr(x,'/')? strrchr(x,'/')+1 : x
#define DEBUG(format, ...) \
    printf("FILE: %s, LINE: %d: "format"\n", TRIM(__FILE__), __LINE__, ##__VA_ARGS__)
#else  
#define DEBUG(format, ...)
#endif

#define INFO(format, ...) \
    printf("> "format"\n", ##__VA_ARGS__)

#define ERR_EXIT(x) \
    do { \
        perror(x); \
        exit(EXIT_FAILURE); \
    } while(0)

const char* lable = "|/-\\";

int connectForSend(void) {
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd == SOCK_ERROR)
        ERR_EXIT("socket error");

    int optval = 1;
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof serveraddr);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(DEFAULT_PORT);

    if(bind(serverfd, (struct sockaddr*)&serveraddr, sizeof serveraddr) == SOCK_ERROR)
        ERR_EXIT("bind error");

    if(listen(serverfd, SOMAXCONN) == SOCK_ERROR)
        ERR_EXIT("listen error");

    INFO("wait for connect...");

    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof clientaddr;
    int clientfd = accept(serverfd, (struct sockaddr*)&clientaddr, &addrlen);
    if(clientfd == SOCK_ERROR)
        ERR_EXIT("accept error");

    return clientfd;
}

int connectForRecv(const char* ipaddr, short port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == SOCK_ERROR)
        ERR_EXIT("socket error");

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof serveraddr);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(ipaddr == NULL? DEFAULT_ADDR : ipaddr);
    serveraddr.sin_port = htons(port == 0? DEFAULT_PORT : port);

    if(connect(sockfd, (struct sockaddr*)&serveraddr, sizeof serveraddr) == SOCK_ERROR)
        ERR_EXIT("connect error");

    INFO("connect success...");
   
    return sockfd;
}

void closeConnect(int sockfd) {
    if(close(sockfd) == SOCK_ERROR)
        ERR_EXIT("close error");
}


int sendBuffer(int sockfd, const void* buf, int n) {
    int left = n;
    int res = 0;
    int offset = 0;

    while(left > 0) {
        res = send(sockfd, (const char*)buf + offset, left, 0);
        if(res == SOCK_ERROR) {
            ERR_EXIT("send error");
        }

        offset += res;
        left -= res;
    }
    return offset;
}

void sendFile(int sockfd, const char* path, int passwd) {
    char filename[NAME_MAX];
    if(strrchr(path, '/') == NULL) {
        sprintf(filename, "%s", path);
    }
    else {
        sprintf(filename, "%s", strrchr(path, '/')+1);
    }

    FILE* fp = fopen(path, "rb");
    if(fp == NULL)
        ERR_EXIT("fopen error");

    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);    // 不大于2G 
    fseek(fp, 0, SEEK_SET);

    int filesizeKB = filesize / 1024;
    if(filesizeKB == 0) {
        INFO("[%s] is being sent... (size: %d B)", filename, filesize);
    }
    else {
        INFO("[%s] is being sent... (size: %d KB)", filename, filesizeKB);
    }

    if(send(sockfd, filename, NAME_MAX, 0) == SOCK_ERROR)
        ERR_EXIT("send error");
    if(send(sockfd, &filesize, sizeof filesize, 0) == SOCK_ERROR)
        ERR_EXIT("send error");

    if(passwd != -1)
        srand(passwd);

    char buf[SEND_BUF_SIZE];
    int count = 0;
    int res = 0;
    char bar[52] = { 0 };
    int k = 0;
    while((res = fread(buf, 1, SEND_BUF_SIZE, fp))) {
        if(passwd != -1) {
            for(int i = 0; i != res; ++i)
                buf[i] = buf[i] ^ rand();
        }

        count += sendBuffer(sockfd, buf, res);

        // 进度条
        if(filesizeKB > 256) {  // 大于256KB
            k = (count / 1024 * 100) / filesizeKB;
            bar[k/2] = '#';
            printf("[%-51s][%d%%][%c]\r", bar, k, lable[k%4]);
            fflush(stdout);
        }
    }
    printf("\n");

    if(count == filesize) {
        INFO("send success");
    }
    else {
        INFO("send failed");
    }

    fclose(fp);
}

int readBuffer(int sockfd, void* buf, int n) {
    int left = n;
    int res = 0;
    int offset = 0;

    while(left > 0) {
        res = recv(sockfd, (char*)buf + offset, left, 0);
        if(res == SOCK_ERROR) {
            perror("recv error");
            return SOCK_ERROR;
        }

        if(res == 0)
            return offset;  // 必须

        offset += res;
        left -= res;
    }
    return offset;
}

void recvFile(int sockfd, const char* path, int passwd) {
    char filename[NAME_MAX];
    if(recv(sockfd, filename, NAME_MAX, 0) == SOCK_ERROR)
        ERR_EXIT("recv error");
    int filesize;
    if(recv(sockfd, &filesize, sizeof filesize, 0) == SOCK_ERROR)
        ERR_EXIT("recv error");

    char filepath[PATH_MAX];
    if(path == NULL) {
        sprintf(filepath, "%s", filename);
    }
    else {
        sprintf(filepath, "%s/%s", path, filename);
    }

    FILE* fp = fopen(filepath, "wb");
    if(fp == NULL)
        ERR_EXIT("fopen error");

    int filesizeKB = filesize / 1024;
    if(filesizeKB == 0) {    // 小于1KB就以B为单位
        INFO("[%s] is being received... (size: %d B)", filename, filesize);
    }
    else {
        INFO("[%s] is being received... (size: %d KB)", filename, filesizeKB);
    }

    if(passwd != -1)
        srand(passwd);

    char buf[RECV_BUF_SIZE];
    int count = 0;
    int res = 0;
    char bar[52] = { 0 };
    int k = 0;
    while((res = readBuffer(sockfd, buf, RECV_BUF_SIZE)) != SOCK_ERROR) {
        if(res == 0)
            break;

        if(passwd != -1) {
            for(int i = 0; i != res; ++i)
                buf[i] = buf[i] ^ rand();
        }
        
        count += fwrite(buf, 1, res, fp);

        // 进度条
        if(filesizeKB > 256) {  // 大于256KB
            k = (count / 1024 * 100) / filesizeKB;
            bar[k/2] = '#';
            printf("[%-51s][%d%%][%c]\r", bar, k, lable[k%4]);
            fflush(stdout);
        }
    }
    printf("\n");

    if(res == SOCK_ERROR) {
        exit(EXIT_FAILURE);
    }

//    fseek(fp, 0, SEEK_END);
//    int check_filesize = ftell(fp);

    if(count == filesize) {
        INFO("download finish");
    }
    else {
        INFO("download failed");
    }

    fclose(fp);
}


void usage(void) {
    printf("\nft v%s (Usage)\n"
           "send : ft -s -l <filepath> [-P <password>]\n"
           "recv : ft -r [-l <filepath>] [-h <ipaddr>] [-p <port>] [-P <password>]\n\n",
           FT_VERSION);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        usage();
    }
    
    // 解析参数
    int opt = -1;
    int check_send = 0;
    int check_recv = 0;
    int passwd = -1;
    char* ipaddr = NULL;
    char* filepath = NULL;
    short port = 0;

    while((opt = getopt(argc, argv, "srl:h:p:P:")) != -1) {
        switch(opt) {
            case 's':
                check_send = 1;
                break;
            case 'r':
                check_recv = 1;
                ipaddr = optarg;
                break;
            case 'l':
                filepath = optarg;
                break;
            case 'h':
                ipaddr = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'P':
                passwd = atoi(optarg);
                break;
            default:
                usage();
        }
    }

    if(check_send) {
        if(filepath == NULL)
            usage();
        int sockfd = connectForSend();
        sendFile(sockfd, filepath, passwd);
        closeConnect(sockfd);
    }
    else if(check_recv) {
        int sockfd = connectForRecv(ipaddr, port);
        recvFile(sockfd, filepath, passwd);
        closeConnect(sockfd);
    }
    else {
        usage();
    }

    return 0;
}


