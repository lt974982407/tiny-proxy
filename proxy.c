#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <mysql/mysql.h>
#include <time.h>

#define BUF_SIZE 4096 * 4

struct thread_para
{
    char *ip;
    int fd;
};

struct thread_para_2
{
    int fd1;
    int fd2;
    char *rip;
    char *dip;
    char *hostname;
    char *protocol;
};

void add_data(char *s)
{
    MYSQL sql;
    MYSQL_RES *res;
    MYSQL_FIELD *field;
    MYSQL_ROW row;
    mysql_init(&sql);
    if (mysql_real_connect(&sql, "localhost", "test01", "test", "proxy", 0, NULL, 0)) //连接数据库
    {
        printf("connect success for database\n");
    }
    else
    {
        printf("connect failed for database\n");
    }

    mysql_query(&sql, s); //将返回的数据包信息写入数据库

    mysql_close(&sql);
}

void forword_1(void *data)
{
    pthread_detach(pthread_self());
    struct thread_para_2 fd;
    fd = *(struct thread_para_2 *)data;
    free(data);

    char buffer1[2048];
    memset(buffer1, 0, sizeof(buffer1));

    time_t t = time(0);
    char nowtime[100];

    char buf[BUF_SIZE];
    bzero(buf, BUF_SIZE);
    int bytes = 0;
    while ((bytes = read(fd.fd1, buf, BUF_SIZE)) > 0)
    {
        send(fd.fd2, buf, bytes, MSG_NOSIGNAL);
        strftime(nowtime, 100, "%Y-%m-%d %H:%M:%S", localtime(&t)); //format date     and time.
        sprintf(buffer1, "INSERT INTO log (rip ,dip ,hostname ,protocol ,time ,size) VALUES ('%s','%s','%s','%s','%s',%d)", fd.rip, fd.dip, fd.hostname, fd.protocol, nowtime, bytes);
        add_data(buffer1);
    }
    close(fd.fd1);
    close(fd.fd2);
}

void out2in(void *data)
{
    pthread_detach(pthread_self());

    struct thread_para recv_para;
    recv_para = *(struct thread_para *)data;
    free(data);

    int fd_acc = recv_para.fd;

    char buffer1[2048];
    memset(buffer1, 0, sizeof(buffer1));

    char protocol[100];
    memset(protocol, 0, sizeof(protocol));
    strcpy(protocol, "http");

    time_t t = time(0);
    char nowtime[100];

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    int sever_port = 80;
    int fd_sever = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd_sever < 0)
    {
        printf("Socket to out created failed! \n");
        close(fd_acc);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.146.129");
    server_addr.sin_port = htons(sever_port);

    if (connect(fd_sever, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("remote connect failed!_out \n");
        close(fd_sever);
        return;
    }

    char buf[BUF_SIZE];
    bzero(buf, BUF_SIZE);

    struct thread_para_2 *para_th = (struct thread_para_2 *)malloc(sizeof(struct thread_para_2));
    para_th->fd1 = fd_sever;
    para_th->fd2 = fd_acc;
    para_th->dip = recv_para.ip;
    para_th->rip = "192.168.146.129";
    para_th->hostname = " ";
    para_th->protocol = protocol;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, (void *)forword_1, (void *)para_th);
    int read_res = 0;
    while ((read_res = read(fd_acc, buf, BUF_SIZE)) > 0)
    {
        //printf("222\n");
        send(fd_sever, buf, read_res, MSG_NOSIGNAL);
        strftime(nowtime, 100, "%Y-%m-%d %H:%M:%S", localtime(&t)); //format date     and time.
        sprintf(buffer1, "INSERT INTO log (rip ,dip ,hostname ,protocol ,time ,size) VALUES ('%s','%s','%s','%s','%s',%d)", recv_para.ip, "192.168.146.129", " ", protocol, nowtime, read_res);
        add_data(buffer1);
    }

    close(fd_acc);
    // close(fd_sever);
    printf("thread end!\n");
}

void in2out(void *data)
{
    pthread_detach(pthread_self());

    printf("new thread! \n");
    struct thread_para recv_para;
    recv_para = *(struct thread_para *)data;
    free(data);
    int fd_acc = recv_para.fd;

    char buf[BUF_SIZE], hostname[256], ip_addr[64];
    bzero(buf, BUF_SIZE);
    bzero(hostname, 256);
    bzero(ip_addr, 64);

    int read_res = read(fd_acc, buf, BUF_SIZE);

    while (read_res <= 0)
    {
        printf("read fail!\n");
        return;
    }

    char buffer1[2048];
    memset(buffer1, 0, sizeof(buffer1));

    char protocol[100];
    memset(protocol, 0, sizeof(protocol));

    time_t t = time(0);
    char nowtime[100];

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    int sever_port = 443;
    int fd_sever = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_sever < 0)
    {
        printf("Socket to out created failed! \n");
        close(fd_acc);
        return;
    }
    char *p = strstr(buf, "host: ");
    if (!p)
    {
        p = strstr(buf, "Host: ");
    }
    printf("%u\n", p);
    if (p != NULL)
    {
        printf("http!\n");
        int i, j;

        for (i = (p - buf) + 6, j = 0; i < read_res; i++, j++)
        {
            if (buf[i] == '\r')
            {
                hostname[j] = '\0';
                break;
            }
            else
                hostname[j] = buf[i];
        }

        char *tmp = strstr(hostname, ":");
        if (tmp)
        {
            sever_port = atoi(tmp + 1);
            *tmp = '\0';
        }
        printf("ho:%s\n", hostname);
        struct hostent *hptr;
        hptr = gethostbyname(hostname);
        if (!hptr)
        {
            printf("hostname error!");
            close(fd_acc);
            return;
        }

        if (sever_port == 80)
        {
            strcpy(protocol, "http");
        }
        else if (sever_port == 443)
        {
            strcpy(protocol, "https");
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr = *((struct in_addr *)hptr->h_addr);
        server_addr.sin_port = htons(sever_port);

        printf("ipaddress:\n");
        printf("%s %u\n", (char *)inet_ntoa(server_addr.sin_addr), sever_port);

        strcpy(ip_addr, (char *)inet_ntoa(server_addr.sin_addr));

        if (connect(fd_sever, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            printf("remote connect failed! \n");
            close(fd_sever);
            return;
        }
        char buf2[50] = "HTTP/1.1 200 Connection Established\r\n\r\n";
        send(fd_acc, buf2, read_res, MSG_NOSIGNAL);
    }
    else if (buf[0] == '\x05')
    {
        strcpy(protocol, "tcp");

        send(fd_acc, "\x05\0", 2, MSG_NOSIGNAL);
        read_res = read(fd_acc, buf, BUF_SIZE);
        if (buf[1] == '\x01')
        {
            printf("con!\n");
            if (buf[3] == '\x01')
            {
                printf("ip!\n");
                sprintf(ip_addr, "%d.%d.%d.%d", (unsigned char)buf[4], (unsigned char)buf[5], (unsigned char)buf[6], (unsigned char)buf[7]);
                sever_port = (int)((unsigned char)buf[8]) * 256 + (int)((unsigned char)buf[9]);
                printf("%s\n", ip_addr);
                printf("%d\n", sever_port);
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr.s_addr = inet_addr(ip_addr);
                server_addr.sin_port = htons(sever_port);
                if (connect(fd_sever, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
                {
                    printf("remote connect failed! \n");
                    close(fd_sever);
                    close(fd_acc);
                    return;
                }
                buf[1] = '\x00';
                send(fd_acc, buf, 10, MSG_NOSIGNAL);
            }
            else if (buf[3] == '\x03')
            {
                printf("host!\n");
                int length = (int)((unsigned char)buf[4]);
                strcpy(hostname, buf + 5);
                hostname[length] = '\0';
                sever_port = (int)((unsigned char)buf[read_res - 2]) * 256 + (int)((unsigned char)buf[read_res - 1]);
                printf("%s\n", hostname);
                struct hostent *hptr;
                hptr = gethostbyname(hostname);
                if (!hptr)
                {
                    printf("hostname error!");
                    close(fd_acc);
                    return;
                }
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr = *((struct in_addr *)hptr->h_addr);
                server_addr.sin_port = htons(sever_port);

                strcpy(ip_addr, (char *)inet_ntoa(server_addr.sin_addr));
                if (connect(fd_sever, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
                {
                    printf("remote connect failed! \n");
                    close(fd_sever);
                    close(fd_acc);
                    return;
                }
                buf[1] = '\x00';
                send(fd_acc, buf, 10, MSG_NOSIGNAL);
            }
        }
        
    }

    struct thread_para_2 *para_th = (struct thread_para_2 *)malloc(sizeof(struct thread_para_2));
    para_th->fd1 = fd_sever;
    para_th->fd2 = fd_acc;
    para_th->dip = recv_para.ip;
    para_th->rip = ip_addr;
    para_th->hostname = hostname;
    para_th->protocol = protocol;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, (void *)forword_1, (void *)para_th);
    while ((read_res = read(fd_acc, buf, BUF_SIZE)) > 0)
    {
        //printf("222\n");
        send(fd_sever, buf, read_res, MSG_NOSIGNAL);
        strftime(nowtime, 100, "%Y-%m-%d %H:%M:%S", localtime(&t)); //format date     and time.
        sprintf(buffer1, "INSERT INTO log (rip ,dip ,hostname ,protocol ,time ,size) VALUES ('%s','%s','%s','%s','%s',%d)", recv_para.ip, ip_addr, hostname, protocol, nowtime, read_res);
        add_data(buffer1);
    }

    close(fd_acc);
    // close(fd_sever);
    printf("thread end!\n");
}

void beginwall()
{
    pid_t pid;
    if ((pid = fork()) == 0)
    {
        int fd_in = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in cl_addr, server_addr;
        pthread_t thread_id;

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(7045);

        if (fd_in < 0)
        {
            printf("Socket created failed! \n");
            return;
        }

        if (bind(fd_in, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            printf("Socket bind failed! \n");
            return;
        }

        if (listen(fd_in, 20) < 0)
        {
            printf("Socket listen failed! \n");
            return;
        }
        printf("listen...\n");

        while (1)
        {
            socklen_t sock_size = sizeof(struct sockaddr_in);
            int fd_acc = accept(fd_in, (struct sockaddr *)&cl_addr, &sock_size);
            if (fd_acc < 0)
            {
                printf("Socket accept failed! \n");
                continue;
            }
            struct thread_para *in_para;
            in_para = (struct thread_para *)malloc(sizeof(struct thread_para));
            in_para->ip = (char *)inet_ntoa(cl_addr.sin_addr);
            in_para->fd = fd_acc;
            pthread_create(&thread_id, NULL, (void *)in2out, (void *)in_para);
        }
    }
    else
    {
        int fd_in = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in cl_addr, server_addr;
        pthread_t thread_id;

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(8004);

        if (fd_in < 0)
        {
            printf("Socket created failed! \n");
            return;
        }

        if (bind(fd_in, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            printf("Socket bind failed! \n");
            return;
        }

        if (listen(fd_in, 20) < 0)
        {
            printf("Socket listen failed! \n");
            return;
        }
        printf("listen...\n");

        while (1)
        {
            socklen_t sock_size = sizeof(struct sockaddr_in);
            int fd_acc = accept(fd_in, (struct sockaddr *)&cl_addr, &sock_size);
            if (fd_acc < 0)
            {
                printf("Socket accept failed! \n");
                continue;
            }
            struct thread_para *in_para;
            in_para = (struct thread_para *)malloc(sizeof(struct thread_para));
            in_para->ip = (char *)inet_ntoa(cl_addr.sin_addr);
            in_para->fd = fd_acc;
            pthread_create(&thread_id, NULL, (void *)out2in, (void *)in_para);
        }
    }
}

int main()
{
    beginwall();
}