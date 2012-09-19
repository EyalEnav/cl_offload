
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>

#include <CL/cl.h>

#include "tpl.h"
#include "and_rpc_clnt.h"
#include "and_rpc.h"

#define BUFSIZE 1024

int tcp_sock = -1;
char _buf[BUFSIZE];
char *_big_buf;

typedef long long int int64;

static struct timeval TIMEOUT = { 25, 0 };

static double
sub_timeval(const struct timeval *t1, const struct timeval *t2)
{
    long int long_diff = (t2->tv_sec * 1000000 + t2->tv_usec) - 
        (t1->tv_sec * 1000000 + t1->tv_usec);
    double diff = (double)long_diff/1000000;
    return diff;
}

static int
create_sock(const char *udp_addr, const uint16_t port)
{
    int sock, optval = 1, optlen = sizeof(optval);
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);

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
mcast_connect(const int sock, const uint16_t port)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char buf[BUFSIZE];

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
tcp_connect(struct sockaddr_in *addr, const int count)
{
    char buf[BUFSIZE];
    int sock;
    socklen_t addr_len = sizeof(*addr);

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fprintf(stderr, "socket failed\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *) addr, addr_len) < 0) {
        fprintf(stderr, "connect failed\n");
        return -1;
    }

    tcp_sock = sock;

    return 0;
}

static double
get_tcp_bw(struct sockaddr_in *addr, const int count)
{
    double diff;
    struct timeval t1, t2;

    gettimeofday(&t1, NULL);
    tcp_connect(addr, count);
    gettimeofday(&t2, NULL);

    diff = sub_timeval(&t1, &t2);

    printf("diff time %f\n", diff);

    diff = ((double)count)/(diff*BUFSIZE);
    printf("bw %f MB\n", diff);
    return diff;
}


static int
discover_services(char *server)
{
    char buf[BUFSIZE];
    struct sockaddr_in addr;
    ssize_t rcount;
    socklen_t addr_len = sizeof(addr);
    int sock = create_sock(NULL, 5555);

    mcast_connect(sock, 51234);
    rcount = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*) &addr,
                      &addr_len);

    printf("recvd >> %s\n", buf);

    double diff = get_tcp_bw(&addr, 10);

    if (diff > 10) {
        inet_ntop(AF_INET, &(addr.sin_addr), server, 50);
        printf("server %s\n", server);
        printf("BW is greated than 10 MB, running over RPC\n");
        return 0;
    }
    else { 
        printf("BW is less than 10 MB, running locally\n");
        return -1;
    }
}

int init_rpc2()
{
    char server[50];
    if (discover_services(server) == -1) {
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// RPC Segment
//////////////////////////////////////////////////////////////////////////////

static int
tpl_rpc_call(tpl_node *stn, tpl_node *rtn)
{
    char *sbuf = NULL;
    size_t nsend, nread, sz = -1;

    tpl_pack(stn, 0);
    tpl_dump(stn, TPL_GETSIZE, &sz);
    sz += 4;

    printf("sz %d\n", sz);

    if (sz < 1) {
        fprintf(stderr, "error dump size %d\n", sz);
        tpl_free(stn);
        return -1;
    }
    else if (sz > 1020) {
        printf("doing malloc\n");
        sbuf = malloc(sz);
        memcpy(sbuf, _buf, 4);
    }
    else {
        sbuf = _buf;
    }

    tpl_dump(stn, TPL_MEM|TPL_PREALLOCD, sbuf + 4, sz);

    int idx = sz, i = 0;
    while (1) {
        if (idx > 1024) {
            if ((nsend = send(tcp_sock, sbuf + i, 1024, 0)) < 1) {
                fprintf(stderr, "send failed\n");
                tpl_free(stn);
                close(tcp_sock);
                return -1;
            }
            idx -= nsend;
            i += nsend;
            printf("idx %d\n", idx);
        }
        if (idx < 1024) {
            if ((nsend = send(tcp_sock, sbuf + i, idx, 0)) < 1) {
                fprintf(stderr, "send failed\n");
                tpl_free(stn);
                close(tcp_sock);
                return -1;
            }
            printf("nsend %d\n", nsend);
            break;
        }

    }

    if ((nread = recv(tcp_sock, _buf, sizeof(_buf), 0)) < 1) {
        fprintf(stderr, "recv failed\n");
        tpl_free(stn);
        close(tcp_sock);
        return -1;
    }

    printf("nread %d\n", nread);

    if (_buf[1] > 1 || _buf[1] == -1) {
        fprintf(stderr, "big buffer size %d\n", _buf[1]);
        int nnread, buf_size, j;
        if (_buf[1] == -1) {
            buf_size = 1024 * (_buf[2] * 256 + _buf[3]);
            j = _buf[2] * 256 + _buf[3];
        }
        else {
            buf_size = 1024 * _buf[1];
            j = _buf[1];
        }
        _big_buf = malloc(buf_size);
        memcpy(_big_buf, _buf, nread);

        int i = 1, x = 0;
        while ( i < j) {
            if ((nnread = recv(tcp_sock, _buf, 1024, 
                0)) < 1) {
                fprintf(stderr, "recv large buffer failed\n");
                close(tcp_sock);
            }
            memcpy(_big_buf + (1024 * i), _buf, nnread);
            i++;
            nread += nnread;
            x += 1024 - nnread;
            if (x >= 1024) {
                i--;
                x -= 1024;
            }
            printf("loop nnread %d i %d j %d\n", nnread, i, j);
        }
        printf("nnread %d\n", nnread);
    }

    if (sbuf != _buf) {
        free(sbuf);
    }

    printf("total %d\n", nread);
    return nread;
}

static int
tpl_deserialize(tpl_node *stn, tpl_node *rtn)
{
    if (tpl_load(rtn, TPL_MEM|TPL_EXCESS_OK, _buf + 4, 
        sizeof(_buf) -4) < 0) {
        fprintf(stderr, "load failed\n");
        tpl_free(stn);
        tpl_free(rtn);
        return -1;
    }

    tpl_unpack(rtn, 0);
    tpl_free(stn);
    tpl_free(rtn);

    return 0;
}

static int
tpl_deserialize_array(tpl_node *stn, tpl_node *rtn, int cb, char *c, char *buf)
{
    if (cb > 1020) {
        fprintf(stderr, "big tpl_load %d\n", cb);
        if (tpl_load(rtn, TPL_MEM|TPL_EXCESS_OK, _big_buf + 4, 
            cb - 4) < 0) {
            fprintf(stderr, "load failed\n");
            tpl_free(stn);
            tpl_free(rtn);
            return 0;
        }
    }
    else {
        if (tpl_load(rtn, TPL_MEM|TPL_EXCESS_OK, _buf + 4, 
            sizeof(_buf) -4) < 0) {
            fprintf(stderr, "load failed\n");
            tpl_free(stn);
            tpl_free(rtn);
            return 0;
        }
    }

    tpl_unpack(rtn, 0);
    int i = 0;
    while (tpl_unpack(rtn, 1) > 0) {
        buf[i] = *c;
        i++;
    }


    tpl_free(stn);
    tpl_free(rtn);

    if (cb > 1020) {
        free(_big_buf);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// OCL Segment
//////////////////////////////////////////////////////////////////////////////

cl_int clGetPlatformIDs (cl_uint num_entries,
                         cl_platform_id *platforms,
                         cl_uint *num_platforms)
{
    printf("doing getplatformid\n");
    int result;
    int64 *platforms_l = malloc(sizeof(int64));
    *platforms = platforms_l;

    _buf[0] = GET_PLAT_ID;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("i", &num_entries);
    rtn = tpl_map("iI", &result, platforms_l);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return 0;
}

cl_int clGetDeviceIDs (cl_platform_id platform,
                       cl_device_type device_type,
                       cl_uint num_entries,
                       cl_device_id *devices,
                       cl_uint *num_devices)
{
    printf("doing getdeviceid\n");
    int result;
    int64 *devices_l = malloc(sizeof(int64));
    *devices = devices_l;
    _buf[0] = GET_DEV_ID;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iii", platform, &device_type, &num_entries);
    rtn = tpl_map("iI", &result, devices_l);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return 0;
}

cl_context clCreateContext (const cl_context_properties *properties,
                            cl_uint num_devices,
                            const cl_device_id *devices,
                            void (CL_CALLBACK *pfn_notify)(const char *errinfo,
                                const void *private_info, size_t cb, 
                                void *user_data),
                            void *user_data,
                            cl_int *errcode_ret)
{
    printf("doing createcontext\n");
    int64 *context_l = malloc(sizeof(int64));
    _buf[0] = CREATE_CTX;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("iI", &num_devices, *devices);
    rtn = tpl_map("I", context_l);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return context_l;
}

cl_command_queue clCreateCommandQueue (cl_context context,
                                   cl_device_id device,
                                   cl_command_queue_properties properties,
                                   cl_int *errcode_ret)
{
    printf("doing createcommandqueue\n");
    int64 *queue_l = malloc(sizeof(int64));
    _buf[0] = CREATE_CQUEUE;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIi", context, device, &properties);
    rtn = tpl_map("I", queue_l);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    printf("QUEUE_L %p\n", *queue_l);

    return queue_l;
}

cl_program clCreateProgramWithSource (cl_context context,
                                      cl_uint count,
                                      const char **strings,
                                      const size_t *lengths,
                                      cl_int *errcode_ret)
{
    printf("doing createprogramwithsource\n");
    int64 *program_l = malloc(sizeof(int64));
    _buf[0] = CREATE_PROG_WS;
    _buf[1] = strlen(*strings)/1024 + 1;
    tpl_node *stn, *rtn;

    printf("strlen %d\n", strlen(*strings));

    stn = tpl_map("Iis", context, &count, strings);
    rtn = tpl_map("I", program_l);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return program_l;
}

cl_int clBuildProgram (cl_program program,
                       cl_uint num_devices,
                       const cl_device_id *device_list,
                       const char *options,
                       void (CL_CALLBACK *pfn_notify)(cl_program program,
                           void *user_data),
                       void *user_data)
{
    printf("doing build program\n");
    int64 *ptr = NULL;
    int result;
    _buf[0] = BUILD_PROG;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    if (device_list == NULL) {
        stn = tpl_map("IiI", program, &num_devices, &ptr);
    }
    else {
        stn = tpl_map("IiI", program, &num_devices, *device_list);
    }

    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_kernel clCreateKernel (cl_program program,
                          const char *kernel_name,
                          cl_int *errcode_ret)
{
    printf("doing createkernel\n");
    int64 *kernel_l = malloc(sizeof(int64));
    _buf[0] = CREATE_KERN;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("Is", program, &kernel_name);
    rtn = tpl_map("I", kernel_l);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return kernel_l;
}

cl_mem clCreateBuffer (cl_context context,
                      cl_mem_flags flags,
                      size_t size,
                      void *host_ptr,
                      cl_int *errcode_ret)
{
    printf("doing createbuffer\n");
    int64 *buffer_l = malloc(sizeof(int64));
    _buf[0] = CREATE_BUF;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iii", context, &flags, &size);
    rtn = tpl_map("I", buffer_l);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return buffer_l;
}

cl_int clSetKernelArg (cl_kernel kernel,
                       cl_uint arg_index,
                       size_t arg_size,
                       const void *arg_value)
{
    printf("doing setkernelarg\n");
    int64 *ptr = NULL;
    int result;
    _buf[0] = SET_KERN_ARG;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    if (arg_value == NULL) {
        stn = tpl_map("IiiI", kernel, &arg_index, &arg_size, &ptr);
    }
    else if (arg_size == 1234) {
        stn = tpl_map("IiiI", kernel, &arg_index, &arg_size, arg_value);
    }
    else {
        int64 *tmp = (int64 *)arg_value;
        arg_size = 8;
        stn = tpl_map("IiiI", kernel, &arg_index, &arg_size, *tmp);
    }

    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}


cl_int clEnqueueNDRangeKernel (cl_command_queue command_queue,
                               cl_kernel kernel,
                               cl_uint work_dim,
                               const size_t *global_work_offset,
                               const size_t *global_work_size,
                               const size_t *local_work_size,
                               cl_uint num_events_in_wait_list,
                               const cl_event *event_wait_list,
                               cl_event *event)
{
    printf("doing enqndrange\n");
    int result;
    _buf[0] = ENQ_NDR_KERN;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiii", command_queue, kernel, &work_dim, 
        global_work_size, &num_events_in_wait_list);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_int clFinish (cl_command_queue command_queue)
{
    printf("doing finish\n\n");
    int result;
    _buf[0] = FINISH;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", command_queue);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

void * clEnqueueMapBuffer (cl_command_queue command_queue,
                           cl_mem buffer,
                           cl_bool blocking_map,
                           cl_map_flags map_flags,
                           size_t offset,
                           size_t cb,
                           cl_uint num_events_in_wait_list,
                           const cl_event *event_wait_list,
                           cl_event *event,
                           cl_int *errcode_ret)
{
    printf("doing enqmapbuffer\n\n");
    char *buf = malloc(cb);
    char c;
    int result;
    _buf[0] = ENQ_MAP_BUF;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiiiii", command_queue, buffer, &blocking_map, 
        &map_flags, &offset, &cb, &num_events_in_wait_list);
    rtn = tpl_map("iA(c)", &result, &c);

    cb = tpl_rpc_call(stn, rtn);
    tpl_deserialize_array(stn, rtn, cb, &c, buf);

    return buf;
}

cl_int clEnqueueReadBuffer (cl_command_queue command_queue,
                            cl_mem buffer,
                            cl_bool blocking_read,
                            size_t offset,
                            size_t cb,
                            void *ptr,
                            cl_uint num_events_in_wait_list,
                            const cl_event *event_wait_list,
                            cl_event *event)
{
    printf("doing read buffer\n");
    char *buf = ptr;
    char c;
    int result;
    _buf[0] = ENQ_READ_BUF;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiiii", command_queue, buffer, &blocking_read, 
        &offset, &cb, &num_events_in_wait_list);
    rtn = tpl_map("iA(c)", &result, &c);

    cb = tpl_rpc_call(stn, rtn);
    tpl_deserialize_array(stn, rtn, cb, &c, buf);

    return result;
}

cl_int clEnqueueWriteBuffer (cl_command_queue command_queue,
                            cl_mem buffer,
                            cl_bool blocking_write,
                            size_t offset,
                            size_t cb,
                            const void *ptr,
                            cl_uint num_events_in_wait_list,
                            const cl_event *event_wait_list,
                            cl_event *event)
{
    printf("doing write buffer %d\n", cb);
    int no_bufs = cb/1024 + 1;
    const char *buf = ptr;
    char c;
    int result;
    _buf[0] = ENQ_WRITE_BUF;
    if (no_bufs > 127) {
        _buf[1] = -1;
        _buf[2] = no_bufs / 256;
        _buf[3] = no_bufs % 256;
    }
    else {
        _buf[1] = cb/1024 + 1;
    }
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiiiiA(c)", command_queue, buffer, &blocking_write, 
        &offset, &cb, &num_events_in_wait_list, &c);
    rtn = tpl_map("i", &result);

    int i;
    for (i = 0; i < cb; i++) {
        c = buf[i];
        tpl_pack(stn, 1);
    }

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_int clReleaseMemObject (cl_mem memobj)
{
    int result;
    _buf[0] = RELEASE_MEM;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", memobj);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_int clReleaseProgram (cl_program program)
{
    int result;
    _buf[0] = RELEASE_PROG;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", program);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_int clReleaseKernel (cl_kernel kernel)
{
    int result;
    _buf[0] = RELEASE_KERN;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", kernel);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_int clReleaseCommandQueue (cl_command_queue command_queue)
{
    int result;
    _buf[0] = RELEASE_CQUEUE;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", command_queue);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_int clReleaseContext (cl_context context)
{
    int result;
    _buf[0] = RELEASE_CTX;
    _buf[1] = 1;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", context);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

int 
init_rpc()
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(51234);
    addr.sin_addr.s_addr = inet_addr("10.244.18.145");

    tcp_connect(&addr, 0);

    return 0;
}
