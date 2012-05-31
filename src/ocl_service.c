
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>

static void *
handle_client(void *arg)
{
    printf("starting thread id %d\n", (int)pthread_self());
    int sock = (int) arg;
    char buf[1024];
    ssize_t nread;

    while (1) {
        if ((nread = recv(sock, buf, sizeof(buf), 0)) < 1) {
            fprintf(stderr, "recv failed\n");
            break;
        }

        printf("recv %s\n", buf);

        if (send(sock, buf, nread, 0) != nread) {
            fprintf(stderr, "send failed\n");
            break;
        }
    }
    close(sock);
    pthread_exit(NULL);
}

/*
 This is adapted from Definitive Guide to Linux Network Programming
*/

static int
tcp_listen(char *ip, uint16_t port)
{
    int sock, cli_sock, opt;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    pthread_t thread_id;

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "setsockopt failed\n");
        return -1;
    }

    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *) &addr, addr_len) < 0) {
        fprintf(stderr, "bind failed\n");
        close(sock);
        return -1;
    }

    if (listen(sock, 10) < 0) {
        fprintf(stderr, "listen failed\n");
        close(sock);
        return -1;
    }

    while (1) {
        cli_sock = accept(sock, (struct sockaddr *) &addr, &addr_len);
        if (cli_sock < 0) {
            fprintf(stderr, "accept failed\n");
            break;
        }

        if (pthread_create(&thread_id, NULL, handle_client, 
            (void *) cli_sock) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            close(cli_sock);
            break;
        }

        pthread_detach(thread_id);
    }
    return 0;
}

static int
udp_listen(char *udp_addr, uint16_t port)
{
    int sock, optval = 1, optlen = sizeof(optval), result;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    struct ip_mreq mreq;
    char buf[1024];
    ssize_t rcount;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    optval = 1;
    result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
    result = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &optval, optlen);

    optval = 0;
    result = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, optlen);

    mreq.imr_multiaddr.s_addr = inet_addr("239.192.1.100"),
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    result = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    if (result < 0) {
        fprintf(stderr, "setsockopt membership failed\n");
        close(sock);
        return -1;
    }

    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*) &addr, addr_len) < 0) {
        fprintf(stderr, "bind failed\n");
        close(sock);
        return -1;
    }

    while (1) {
        rcount = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*) &addr,
                          &addr_len);

        printf("recv %s\n", buf);

    }
    return 0;
}


int
main(int argc, char *argv[])
{
    udp_listen("127.0.0.1", 51234);
    return 0;
}

