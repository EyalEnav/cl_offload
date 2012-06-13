
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>

#include <CL/cl.h>

#define MCAST_ADDR "239.192.1.100"

typedef struct thread_opts {
    int sock;
    uint16_t tcp_port;
    uint16_t udp_port;
    struct timeval udp_time;
} thread_opts_t;

static struct sockaddr_in peers[10];

static double
sub_timeval(const struct timeval *t1, const struct timeval *t2)
{
    long int long_diff = (t2->tv_sec * 1000000 + t2->tv_usec) - 
        (t1->tv_sec * 1000000 + t1->tv_usec);
    double diff = (double)long_diff/1000000;
    return diff;
}

static int
get_cl_info(char *info)
{
    int idx = 0;
    char buf[1024];
    size_t size;
    size_t items[4];

    cl_platform_id platform;
    cl_device_id device;
    cl_uint num_devices;
    cl_uint max_dim;

    clGetPlatformIDs(1, &platform, NULL);
    clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, sizeof(buf), buf, NULL);

    if (strncmp("Intel", buf, 5) == 0) {
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    }
    else {
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, &num_devices);
    }

    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(buf), buf, &size);
    idx += sprintf(info + idx, "dev_name %s\n", buf);

    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(max_dim), 
        &max_dim, NULL);
    idx += sprintf(info + idx, "max_comp_units %d\n", max_dim);

    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(max_dim), 
        &max_dim, NULL);
    idx += sprintf(info + idx, "max_work_item %d\n", max_dim);

    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_dim), 
        &max_dim, NULL);
    idx += sprintf(info + idx, "max_group_size %d\n", max_dim);

    clGetDeviceInfo(device, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(max_dim), 
        &max_dim, NULL);
    idx += sprintf(info + idx, "max_freq %d\n", max_dim);

    return idx;
}

static void *
handle_client(void *arg)
{
    int sock = (int) arg;
    char buf[1024];
    ssize_t nread;
    int total = 0;

    while (1) {
        if ((nread = recv(sock, buf, sizeof(buf), 0)) < 1) {
            fprintf(stderr, "recv failed\n");
            break;
        }
        total += nread;
    }

    printf("total %d\n", total);
    close(sock);
    pthread_exit(NULL);
}

static int
tcp_listen(const char *ip, const uint16_t port)
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
tcp_connect(const char *ip, const uint16_t port, const int count)
{
    char buf[1024];
    int sock;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr *) &addr, addr_len) < 0) {
        fprintf(stderr, "connect failed\n");
        return -1;
    }

    int i;
    for (i = 0; i < count; i++) {
        send(sock, buf, sizeof(buf), 0);
    }
    close(sock);
    return 0;
}

static void *
start_tcp_thread(void *data)
{
    thread_opts_t *opts = (thread_opts_t *) data;
    tcp_listen(NULL, opts->tcp_port);
    pthread_exit(NULL);
}

static int
get_tcp_bw(char *ip, uint16_t port, const int count)
{
    double diff;
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);
    tcp_connect(ip, port, count);
    gettimeofday(&t2, NULL);

    diff = sub_timeval(&t1, &t2);

    printf("diff time %f\n", diff);
    printf("bw %f MB\n", ((double)count)/(diff*1024));
    return 0;
}

static int
bw_test_all()
{
    int i;
    for (i = 0; i < 10; i++) {
        if (peers[i].sin_port != 0) {
            get_tcp_bw(inet_ntoa(peers[i].sin_addr), 
            ntohs(peers[i].sin_port), 10);
        }
    }
    return 0;
}

static int
create_mcast_sock(const char *udp_addr, const uint16_t port)
{
    int sock, optval = 1, optlen = sizeof(optval), result;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    struct ip_mreq mreq;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    optval = 1;
    result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
    result = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &optval, optlen);

    optval = 0;
    result = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, optlen);

    mreq.imr_multiaddr.s_addr = inet_addr(MCAST_ADDR),
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
    return sock;
}

static int
mcast_listen(thread_opts_t *opts)
{
    char buf[1024];
    char info[1024];
    struct sockaddr_in addr;
    ssize_t rcount;
    socklen_t addr_len = sizeof(addr);
    int in_size;
    int sock = opts->sock;

    in_size = get_cl_info(info) + 1;

    while (1) {
        rcount = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*) &addr,
                          &addr_len);

        if (rcount <= 0) {
            fprintf(stderr, "recvfrom failed\n");
            close(sock);
            return -1;
        }

        if (strncmp(buf, "hello", 5) == 0) {
            rcount = sendto(sock, info, in_size, 0, (struct sockaddr *) &addr,
                addr_len);
        }
        else {
            struct timeval new_time;
            gettimeofday(&new_time, NULL);
            double diff = sub_timeval(&(opts->udp_time), &new_time);
            printf("latency time %f ms\n", diff * 1000);
            printf("%s\n", buf);

            int i, found = 0;
            for (i = 0; i < 10; i++) {
                if (memcmp(&(addr.sin_addr), &(peers[i].sin_addr),
                    sizeof(addr.sin_addr)) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found == 0) {
                for (i = 0; i < 10; i++) {
                    if (peers[i].sin_port == 0) {
                        memcpy(&peers[i], &addr, sizeof(addr));
                        printf("added %s\n", inet_ntoa(addr.sin_addr));
                        break;
                    }
                }
            }
        }

    }
    return 0;
}

static int
mcast_connect(const int sock, const uint16_t port)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char buf[1024];

    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(MCAST_ADDR);

    if (sendto(sock, "hello", 6, 0, (struct sockaddr*) &addr, 
        addr_len) < 0) {
        fprintf(stderr, "sendto failed\n");
        return -1;
    }

    return 0;
}

static int
get_latency(thread_opts_t * opts)
{
    gettimeofday(&(opts->udp_time), NULL);
    mcast_connect(opts->sock, 51234);
    return 0;
}

static void *
start_mcast_thread(void *data)
{
    thread_opts_t *opts = (thread_opts_t *) data;
    mcast_listen(opts);
    pthread_exit(NULL);
}

int
main(int argc, char *argv[])
{
    thread_opts_t opts;
    opts.udp_port = 51234;
    opts.tcp_port = 51234;
    opts.sock = create_mcast_sock(NULL, opts.udp_port);

    pthread_t mcast_thread, tcp_thread;
    pthread_create(&mcast_thread, NULL, start_mcast_thread, &opts);
    pthread_create(&tcp_thread, NULL, start_tcp_thread, &opts);

    int i;
    for (i = 0; i < 10; i++) {
        peers[i].sin_port = 0;
    }

    while (1) {
        get_latency(&opts);
        bw_test_all();
        sleep(30);
    }

    pthread_join(mcast_thread, NULL);
    pthread_join(tcp_thread, NULL);
    return 0;
}

