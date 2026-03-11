#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#define PORT "8080"
#define MAX_EVENTS 1000
#define BUF_SIZE 1000
#define BACKLOG 100

typedef struct {
    int fd;
    int handshake_done;
    char name[100];
} Client;

void send_text_frame(int client_fd, const char *msg){
    
    unsigned char frame[BUF_SIZE];
    size_t len = strlen(msg);
    frame[0] = 0x81;
    if (len <= 125) {
        frame[1] = len;
        memcpy(&frame[2], msg, len);
        send(client_fd, frame, len + 2, 0);
    }
    else if (len <= 65535) {
        frame[1] = 126;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        memcpy(&frame[4], msg, len);
        send(client_fd, frame, len + 4, 0);
    }
}

int make_socket_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void generate_websocket_accept_key(const char *client_key, char *accept_key)
{
    const char *magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];
    unsigned char sha1[SHA_DIGEST_LENGTH];
    snprintf(combined, sizeof(combined), "%s%s", client_key, magic_string);
    SHA1((unsigned char *)combined, strlen(combined), sha1);
    EVP_EncodeBlock((unsigned char *)accept_key, sha1, SHA_DIGEST_LENGTH);
}

void accept_connections(int sockfd, int epfd){
    struct epoll_event ev;
    while (1) {

        int client_fd = accept(sockfd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else {
                perror("accept");
                break;
            }
        }
        make_socket_non_blocking(client_fd);
        printf("New client connected: %d\n\n", client_fd);
        Client *c = malloc(sizeof(Client));
        memset(c, 0, sizeof(Client));
        c->fd = client_fd;
        c->handshake_done = 0;
        ev.events = EPOLLIN;
        ev.data.ptr = c;
        epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
    }
}

void handle_disconnect(Client *c, int epfd){

    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    free(c);
}

void handle_handshake(Client *c, unsigned char *buf){

    if (strstr((char *)buf,"Connection: Upgrade") && strstr((char *)buf,"Upgrade: websocket")){
        char *key_ptr = strstr((char *)buf,"Sec-WebSocket-Key:");
        if (!key_ptr) return;
        key_ptr += 19;
        printf("HANDSHAKE REQUEST \n");
        char client_key[256] = {0};
        sscanf(key_ptr, "%255[^\r\n]", client_key);
        char accept_key[128];
        generate_websocket_accept_key(client_key, accept_key);
        char resp[512];
        snprintf(resp, sizeof(resp),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "\r\n",
            accept_key);
        send(c->fd, resp, strlen(resp), 0);
        printf("Handshake completed: %d\n\n", c->fd);
        c->handshake_done = 1;
        char msg[] = "{\"type\":\"connected\"}";
        send_text_frame(c->fd, msg);
        return;
    }
}

void handle_websocket_frame(Client *c, unsigned char *buf, int n, int epfd){

    unsigned char byte1 = buf[0];
    unsigned char byte2 = buf[1];
    int opcode = byte1 & 0x0F;
    int mask = (byte2 >> 7) & 1;
    unsigned long payload_len = byte2 & 0x7F;
    int mask_offset;
    if (payload_len == 126) {
        payload_len = (buf[2] << 8) | buf[3];
        mask_offset = 4;
    }
    else {
        mask_offset = 2;
    }
    if (opcode == 0x8 && mask == 1) {
        handle_disconnect(c, epfd);
        return;
    }
    if(opcode == 0x1 && mask == 1){
        unsigned char masking_key[4];
        memcpy(masking_key, &buf[mask_offset], 4);
        unsigned char *payload = &buf[mask_offset + 4];
        char decoded[100];
        for (int i = 0; i < payload_len; i++) {
            decoded[i] = payload[i] ^ masking_key[i % 4];
        }
        decoded[payload_len] = '\0';
        strcpy(c->name, decoded);
        printf("Client %d name: %s\n", c->fd, c->name);
    }
}

void handle_client_event(Client *c, int epfd)
{
    unsigned char buf[BUF_SIZE];
    int n = recv(c->fd, buf, BUF_SIZE, 0);
    if (n <= 0) {
        handle_disconnect(c, epfd);
        return;
    }
    if (!c->handshake_done) {
        handle_handshake(c, buf);
    } 
    else {
        handle_websocket_frame(c, buf, n, epfd);
    }
}

int main(void)
{
    int sockfd, epfd, rv, yes = 1;
    struct addrinfo hints, *res, *p;
    struct epoll_event ev, events[MAX_EVENTS];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
       fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    for (p = res; p != NULL; p = p->ai_next) {

        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }
    if (p == NULL) {
        printf("Failed to bind\n");
        return 1;
    }
    listen(sockfd, BACKLOG);
    make_socket_non_blocking(sockfd);
    epfd = epoll_create1(0);
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
    printf("Chat Server running on port %s\n", PORT);
    while (1) {

        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == sockfd) {
                accept_connections(sockfd, epfd);
            }
            else {

                Client *c = events[i].data.ptr;
                handle_client_event(c, epfd);
            }
        }
    }
    close(sockfd);
    freeaddrinfo(res);
    return 0;
}