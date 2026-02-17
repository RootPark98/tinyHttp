#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int set_reuseaddr(int fd){
    int opt = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        return -1;
    }

    return 0;
}

int run_server(const char *host, int port){
    (void)host;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        perror("socket");
        return 1;
    }

    if(set_reuseaddr(server_fd) < 0){
        perror("setsockopt(SO_REUSEADDR)");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("bind");
        close(server_fd);
        return 1;
    }

    if(listen(server_fd, 128) < 0){
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[tinyhttp] listening on 0.0.0.0:%d\n", port);

    for(;;){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if(client_fd < 0){
            if(errno == EINTR) continue;
            perror("accept");
            break;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        printf("[tinyhttp] client connected: %s:%d (fd=%d)\n",
           ip, ntohs(client_addr.sin_port), client_fd);

        char buf[4096];
        ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
        if(n < 0){
            perror("read");
            close(client_fd);
            continue;
        }

        buf[n] = '\0';
        printf("---- request (%zd bytes) ----\n%s\n----------------------------\n", n, buf);

        const char body[] = "hello tinyhttp\n";
        char resp[512];
        int resp_len = snprintf(
            resp, sizeof(resp),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset-utf-8\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(body), body);

        int sent = 0;
        while(sent < resp_len){
            ssize_t w = write(client_fd, resp + sent, (size_t)(resp_len - sent));
            if(w < 0){
                if(errno == EINTR) continue;
                perror("write");
                break;
            }
            sent += (int)w;
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}