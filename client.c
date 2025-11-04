// 包含标准输入输出库，用于printf等函数
#include <stdio.h>
// 包含标准库函数，如exit等
#include <stdlib.h>
// 包含字符串处理函数，如memset, strerror等
#include <string.h>
// 包含Unix标准函数，如close等
#include <unistd.h>
// 包含线程处理函数（虽然当前代码未使用）
#include <pthread.h>
// 包含网络编程相关函数和结构体
#include <arpa/inet.h>

// 声明外部变量sockfd，表示套接字文件描述符
// 这个变量在其他文件中定义和使用
extern int sockfd;

// 声明外部函数place_stone_on_canvas_remote
// 该函数在其他文件中定义，用于在画布上放置棋子
extern void place_stone_on_canvas_remote(void *, int, int, int);

/**
 * 启动客户端连接函数
 * 该函数创建TCP套接字并连接到指定的服务器IP和端口
 * @param ip 服务器IP地址字符串
 * @param port 服务器端口号字符串
 * @return 成功返回0，失败返回-1
 */
int start_client(const char *ip, const char *port)
{
    // 检查输入参数是否为空
    if (ip == NULL || port == NULL) {
        printf("IP or port is NULL\n");
        return -1;
    }

    printf("Creating socket...\n");
    // 初始化客户端连接
    struct sockaddr_in server_addr;
    // 创建TCP套接字，AF_INET表示IPv4，SOCK_STREAM表示TCP流套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // 检查套接字创建是否成功
    if (sockfd < 0) {
        perror("Socket creation failed");  // 打印错误信息
        return -1;
    }

    printf("Setting up server address...\n");
    // 初始化服务器地址结构体为0
    memset(&server_addr, 0, sizeof(server_addr));
    // 设置地址族为IPv4
    server_addr.sin_family = AF_INET;
    // 设置端口号，并转换为网络字节序
    server_addr.sin_port = htons(atoi(port));
    // 设置服务器IP地址，并转换为网络字节序
    server_addr.sin_addr.s_addr = inet_addr(ip);

    printf("Connecting to server %s:%s...\n", ip, port);
    // 连接到服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");  // 连接失败，打印错误信息
        close(sockfd);  // 关闭套接字
        sockfd = -1;  // 重置套接字描述符
        return -1;
    } 
    printf("Connected to server successfully\n");

    return 0;  // 连接成功，返回0
}

/**
 * 客户端反初始化函数
 * 关闭客户端套接字连接并重置套接字描述符
 */
void client_uninit() {
    // 检查套接字是否有效
    if (sockfd != -1) {
        close(sockfd);  // 关闭套接字连接
        sockfd = -1;    // 重置套接字描述符为无效值
    }
}
