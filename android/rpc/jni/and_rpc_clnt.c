
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include <CL/cl.h>

#include "tpl.h"
#include "and_rpc_clnt.h"

#define MCAST_ADDR "239.192.1.100"

#define GET_PLAT_ID 1
#define GET_DEV_ID 2

int tcp_sock = -1;
char _buf[1024];

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
tcp_connect(struct sockaddr_in *addr, const int count)
{
    char buf[1024];
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

    diff = ((double)count)/(diff*1024);
    printf("bw %f MB\n", diff);
    return diff;
}


static int
discover_services(char *server)
{
    char buf[1024];
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

static int
tpl_rpc_call(tpl_node *stn, tpl_node *rtn)
{
    size_t nread = -1, sz = -1;

    tpl_pack(stn, 0);
    tpl_dump(stn, TPL_GETSIZE, &sz);

    if (sz < 1 || sz > 1020) {
        fprintf(stderr, "error dump size %d\n", sz);
        tpl_free(stn);
        return -1;
    }

    if (tpl_dump(stn, TPL_MEM|TPL_PREALLOCD, _buf + 4, 
        sizeof(_buf) - 4) < 0) {
        fprintf(stderr, "dump failed\n");
        tpl_free(stn);
        return -1;
    }

    if (send(tcp_sock, _buf, sz + 4, 0) < 1) {
        fprintf(stderr, "send failed\n");
        tpl_free(stn);
        close(tcp_sock);
        return -1;
    }

    if ((nread = recv(tcp_sock, _buf, sizeof(_buf), 0)) < 1) {
        fprintf(stderr, "recv failed\n");
        tpl_free(stn);
        close(tcp_sock);
        return -1;
    }

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

cl_int clGetPlatformIDs (cl_uint num_entries,
                         cl_platform_id *platforms,
                         cl_uint *num_platforms)
{
    int result;
    _buf[0] = GET_PLAT_ID;
    tpl_node *stn, *rtn;

    stn = tpl_map("i", &num_entries);
    rtn = tpl_map("iI", &result, platforms);

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
    int result;
    _buf[0] = GET_DEV_ID;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iii", &platform, &device_type, &num_entries);
    rtn = tpl_map("iI", &result, devices);

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
    cl_context context;
    _buf[0] = CREATE_CTX;
    tpl_node *stn, *rtn;

    stn = tpl_map("iI", &num_devices, devices);
    rtn = tpl_map("I", &context);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return context;
}

cl_command_queue clCreateCommandQueue (cl_context context,
                                   cl_device_id device,
                                   cl_command_queue_properties properties,
                                   cl_int *errcode_ret)
{
    cl_command_queue queue;
    _buf[0] = CREATE_CQUEUE;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIi", &context, &device, &properties);
    rtn = tpl_map("I", &queue);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return queue;
}

cl_program clCreateProgramWithSource (cl_context context,
                                      cl_uint count,
                                      const char **strings,
                                      const size_t *lengths,
                                      cl_int *errcode_ret)
{
    cl_program program;
    _buf[0] = CREATE_PROG_WS;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iis", &context, &count, strings);
    rtn = tpl_map("I", &program);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return program;
}

cl_int clBuildProgram (cl_program program,
                       cl_uint num_devices,
                       const cl_device_id *device_list,
                       const char *options,
                       void (CL_CALLBACK *pfn_notify)(cl_program program,
                           void *user_data),
                       void *user_data)
{
    int result;
    _buf[0] = BUILD_PROG;
    tpl_node *stn, *rtn;

    stn = tpl_map("IiI", &program, &num_devices, device_list);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_kernel clCreateKernel (cl_program program,
                          const char *kernel_name,
                          cl_int *errcode_ret)
{
    cl_kernel kernel;
    _buf[0] = CREATE_KERN;
    tpl_node *stn, *rtn;

    stn = tpl_map("Is", &program, &kernel_name);
    rtn = tpl_map("I", &kernel);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return kernel;
}

cl_mem clCreateBuffer (cl_context context,
                      cl_mem_flags flags,
                      size_t size,
                      void *host_ptr,
                      cl_int *errcode_ret)
{
    cl_mem buffer;
    _buf[0] = CREATE_KERN;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iii", &context, &flags, &size);
    rtn = tpl_map("I", &buffer);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    if (errcode_ret != NULL) {
        *errcode_ret = CL_SUCCESS;
    }

    return buffer;
}

cl_int clSetKernelArg (cl_kernel kernel,
                       cl_uint arg_index,
                       size_t arg_size,
                       const void *arg_value)
{
    int result;
    _buf[0] = SET_KERN_ARG;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iic#", &kernel, &arg_index, arg_value, arg_size);
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
    int result;
    _buf[0] = ENQ_NDR_KERN;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiii", &command_queue, &kernel, &work_dim, 
        global_work_size, &num_events_in_wait_list);
    rtn = tpl_map("i", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);

    return result;
}

cl_int clFinish (cl_command_queue command_queue)
{
    int result;
    _buf[0] = FINISH;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &command_queue);
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
    void *buf = malloc(cb);
    _buf[0] = FINISH;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiiiii", &command_queue, &buffer, &blocking_map, 
        &map_flags, &offset, &cb, &num_events_in_wait_list);
    rtn = tpl_map("c#", &result);

    tpl_rpc_call(stn, rtn);
    tpl_deserialize(stn, rtn);



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
    return 0;
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
    return 0;
}

cl_int clReleaseMemObject (cl_mem memobj)
{
    return 0;
}

cl_int clReleaseProgram (cl_program program)
{
    return 0;
}

cl_int clReleaseKernel (cl_kernel kernel)
{
    return 0;
}

cl_int clReleaseCommandQueue (cl_command_queue command_queue)
{
    return 0;
}

cl_int clReleaseContext (cl_context context)
{
    return 0;
}

int 
init_rpc()
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(51234);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    tcp_connect(&addr, 0);

    return 0;
}
