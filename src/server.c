// src/server.c
#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int set_reuseaddr(int fd) {
  int opt = 1;
  return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

static void send_response_and_close(int client_fd, const char *resp, int resp_len) {
  int sent = 0;
  while (sent < resp_len) {
    ssize_t w = write(client_fd, resp + sent, (size_t)(resp_len - sent));
    if (w < 0) {
      if (errno == EINTR) continue;
      perror("write");
      break;
    }
    sent += (int)w;
  }
  close(client_fd);
}

int run_server(const char *host, int port) {
  (void)host;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  if (set_reuseaddr(server_fd) < 0) {
    perror("setsockopt(SO_REUSEADDR)");
    close(server_fd);
    return 1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 128) < 0) {
    perror("listen");
    close(server_fd);
    return 1;
  }

  printf("[tinyhttp] listening on 0.0.0.0:%d\n", port);

  for (;;) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }

    // (선택) 클라이언트 주소 로그
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    printf("[tinyhttp] client: %s:%d (fd=%d)\n", ip, ntohs(client_addr.sin_port), client_fd);

    // 요청 읽기 (Phase 2에서는 최소 1회만)
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if(n == 0){
      close(client_fd);
      continue;
    }
    if (n < 0) {
      perror("read");
      close(client_fd);
      continue;
    }
    buf[n] = '\0';

    // 요청 라인 파싱
    char method[16] = {0};
    char path[1024] = {0};
    char version[16] = {0};

    if (sscanf(buf, "%15s %1023s %15s", method, path, version) < 2) {
      const char *body = "bad request\n";
      char resp[512];
      int resp_len = snprintf(resp, sizeof(resp),
          "HTTP/1.1 400 Bad Request\r\n"
          "Content-Type: text/plain; charset=utf-8\r\n"
          "Content-Length: %zu\r\n"
          "Connection: close\r\n"
          "\r\n"
          "%s",
          strlen(body), body);

      send_response_and_close(client_fd, resp, resp_len);
      continue;
    }

    // (10분 업데이트) GET만 허용
    if (strcmp(method, "GET") != 0) {
      const char *body = "method not allowed\n";
      char resp[512];
      int resp_len = snprintf(resp, sizeof(resp),
          "HTTP/1.1 405 Method Not Allowed\r\n"
          "Allow: GET\r\n"
          "Content-Type: text/plain; charset=utf-8\r\n"
          "Content-Length: %zu\r\n"
          "Connection: close\r\n"
          "\r\n"
          "%s",
          strlen(body), body);

      send_response_and_close(client_fd, resp, resp_len);
      continue;
    }

    // 라우팅
    int status = 200;
    const char *status_text = "OK";
    const char *body = NULL;

    if (strcmp(path, "/") == 0) {
      body = "hello tinyhttp\n";
    } else if (strcmp(path, "/health") == 0) {
      body = "ok\n";
    } else {
      status = 404;
      status_text = "Not Found";
      body = "not found\n";
    }

    char resp[512];
    int resp_len = snprintf(resp, sizeof(resp),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status, status_text, strlen(body), body);

    send_response_and_close(client_fd, resp, resp_len);
  }

  close(server_fd);
  return 0;
}