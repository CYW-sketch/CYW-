#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024

/**
 * @brief 玩家棋子状态枚举
 * 
 * 定义了三种状态：空位、黑子、白子
 */
typedef enum {
    EMPTY = 0,  /**< 空位 */
    BLACK = 1,  /**< 黑子 */
    WHITE = 2   /**< 白子 */
} Stone;

int client_sockets[MAX_CLIENTS];  /**< 存储客户端套接字的数组 */
int client_count = 0;             /**< 当前连接的客户端数量 */
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; /**< 客户端列表互斥锁 */

/**
 * @brief 向除发送者外的所有客户端广播消息
 * 
 * @param message 要发送的消息内容
 * @param sender_fd 发送者的套接字描述符，不会向此客户端发送消息
 */
void broadcast_message(const char *message, int sender_fd)
{
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (client_sockets[i] != -1 && client_sockets[i] != sender_fd) 
        {
            int result = write(client_sockets[i], message, strlen(message) + 1);
            if (result <= 0) 
            {
                printf("Failed to send message to client %d\n", client_sockets[i]);
            } 
            else 
            {
                printf("Message sent to client %d: %s\n", client_sockets[i], message);
            }
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

/**
 * @brief 向指定客户端发送消息
 * 
 * @param client_fd 目标客户端的套接字描述符
 * @param message 要发送的消息内容
 */
void send_to_client(int client_fd, const char *message) {
    int result = write(client_fd, message, strlen(message) + 1);
    if (result <= 0) 
    {
        printf("Failed to send message to client %d\n", client_fd);
    } 
    else 
    {
        printf("Message sent to client %d: %s\n", client_fd, message);
    }
}

/**
 * @brief 处理客户端连接的线程函数
 * 
 * 负责处理单个客户端的所有通信，包括玩家分配、消息转发等
 * 
 * @param arg 指向客户端套接字描述符的指针
 * @return void* 总是返回NULL
 */
void *handle_client(void *arg) {
    // 直接使用传入的值，而不是解引用
    int client_fd = *(int*)arg;
    
    // 释放传入的参数内存
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int read_size;
    int player_id = 0;

    // 分配玩家ID
    pthread_mutex_lock(&client_mutex);
    if (client_count == 0) 
    {
        player_id = BLACK;
        printf("Client %d assigned as BLACK player\n", client_fd);
        send_to_client(client_fd, "PLAYER BLACK");
    } 
    else if (client_count == 1) 
    {
        player_id = WHITE;
        printf("Client %d assigned as WHITE player\n", client_fd);
        send_to_client(client_fd, "PLAYER WHITE");
        // 通知两个玩家游戏开始
        send_to_client(client_sockets[0], "START");
        send_to_client(client_fd, "START");
    } 
    else 
    {
        pthread_mutex_unlock(&client_mutex);
        printf("Too many clients, disconnecting %d\n", client_fd);
        send_to_client(client_fd, "FULL");
        close(client_fd);
        return NULL;
    }
    client_sockets[client_count] = client_fd; // 记录客户端socket
    client_count++;
    pthread_mutex_unlock(&client_mutex);

    printf("Client handler started for fd: %d (Player %s)\n", 
           client_fd, player_id == BLACK ? "BLACK" : "WHITE");

    // 主消息处理循环
    while ((read_size = read(client_fd, buffer, BUFFER_SIZE - 1)) > 0) 
    {
        buffer[read_size] = '\0';
        printf("Received from client %d: %s\n", client_fd, buffer);
        
        // 处理移动消息
        if (strncmp(buffer, "MOVE", 4) == 0) 
        {
            // 验证移动格式
            int row, col;
            if (sscanf(buffer, "MOVE %d %d", &row, &col) == 2) 
            {
                printf("Player %s moved to (%d, %d)\n", 
                       player_id == BLACK ? "BLACK" : "WHITE", row, col);
                
                // 广播移动到其他客户端
                broadcast_message(buffer, client_fd);
            } 
            else 
            {
                printf("Invalid MOVE format from client %d: %s\n", client_fd, buffer);
            }
        } 
        // 添加对 RESTART_REQUEST 消息的处理
        else if (strcmp(buffer, "RESTART_REQUEST") == 0) {
            printf("Player %d requested restart\n", client_fd);
            // 广播重新开始消息给所有客户端
            broadcast_message("RESTART", client_fd);
        } 
        else {
            // 广播其他消息
            broadcast_message(buffer, client_fd);
        }
    }

    // 从客户端列表中移除断开连接的客户端
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (client_sockets[i] == client_fd) 
        {
            client_sockets[i] = -1;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    close(client_fd);
    printf("Client disconnected (fd: %d, Player %s)\n", 
           client_fd, player_id == BLACK ? "BLACK" : "WHITE");
    return NULL;
}

/**
 * @brief 初始化并启动服务器
 * 
 * 创建TCP套接字，绑定地址和端口，监听客户端连接，并为每个连接创建处理线程
 * 
 * @param ip 服务器监听的IP地址
 * @param port 服务器监听的端口号
 */
void server_init(const char *ip, const char *port) 
{
    int server_fd;/// 服务器套接字
    struct sockaddr_in server_addr;// 服务器地址结构体
    int opt = 1;// 套接字选项，允许地址重用

    server_fd = socket(AF_INET, SOCK_STREAM, 0);// 创建TCP套接字
    if (server_fd == -1)// 创建套接字失败 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置套接字选项，允许地址重用
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))//sertsockopt失败返回 -1,SO_REUSEADDR 允许地址重用,SOL_SOCKET 选项级别
    {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));//MEMSET 函数将 server_addr 结构体的内存空间清零
    server_addr.sin_family = AF_INET;//AF_INET 表示 IPv4
    server_addr.sin_addr.s_addr = inet_addr(ip);// 将点分十进制的 IP 地址转换为网络字节序的二进制形式
    server_addr.sin_port = htons(atoi(port));// 将端口号转换为网络字节序

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)// 绑定套接字到指定地址和端口,强制转换为 struct sockaddr *
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) // 监听客户端连接，最大连接数为 MAX_CLIENTS 2
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on %s:%s\n", ip, port);

    while (1) 
    {
        struct sockaddr_in client_addr;                                                  // 客户端地址结构体
        socklen_t client_len = sizeof(client_addr);                                     // 客户端地址长度
                                                                                        // 接受客户端连接请求，返回新的套接字描述符
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);// 强制转换为 struct sockaddr * accept 返回新的套接字描述符
        if (client_fd < 0) 
        {
            perror("Accept failed");
            continue;
        }

        printf("Client connected (fd: %d)\n", client_fd);

        pthread_mutex_lock(&client_mutex);// 锁定客户端列表锁
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) // 查找空闲位置
        {
            if (client_sockets[i] == -1) // 空闲位置
            {
                client_sockets[i] = client_fd;// 保存客户端套接字
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);// 解锁客户端列表锁

        if (i == MAX_CLIENTS) // 超过最大客户端数
        {
            printf("Too many clients, connection refused\n"); // 打印错误信息
            send_to_client(client_fd, "FULL");// 发送拒绝信息
            close(client_fd);//
            continue;
        }

        // 为每个客户端创建独立的线程参数
        int *client_fd_ptr = malloc(sizeof(int));// 分配内存
        *client_fd_ptr = client_fd;// 保存客户端套接字
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd_ptr) != 0) // 创建线程
        {
            perror("Thread creation failed");
            close(client_fd);
            pthread_mutex_lock(&client_mutex);// 锁定客户端列表锁
            client_sockets[i] = -1;             // 移除客户端套接字
            pthread_mutex_unlock(&client_mutex);// 解锁客户端列表锁
            free(client_fd_ptr);
            continue;
        }
        pthread_detach(tid);// 分离线程，避免内存泄漏
    }
    
    close(server_fd);
}

/**
 * @brief 程序入口点
 * 
 * 解析命令行参数并启动五子棋服务器
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组，应包含程序名、IP地址和端口号
 * @return int 程序退出状态码
 */
int main(int argc, char *argv[]) 
{
    if (argc != 3) 
    {
        printf("Usage: %s <IP> <Port>\n", argv[0]);
        return -1;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        client_sockets[i] = -1;//创建一个数组，用于保存客户端套接字
    }

    printf("Starting Gomoku server...\n");
    server_init(argv[1], argv[2]);
    return 0;
}