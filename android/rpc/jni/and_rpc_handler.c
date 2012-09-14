
#include <stdio.h>
#include <CL/cl.h>

#include "tpl.h"

//#include "and_rpc_handler.h"

#define GET_PLAT_ID 1
#define GET_PLAT_INFO 2
#define GET_DEV_ID 3
#define CREATE_CTX 4
#define CREATE_CQUEUE 5
#define CREATE_PROG_WS 6
#define BUILD_PROG 7
#define CREATE_KERN 8
#define CREATE_BUF 9
#define SET_KERN_ARG 10
#define ENQ_NDR_KERN 11
#define FINISH 12
#define ENQ_MAP_BUF 13
#define ENQ_READ_BUF 14
#define ENQ_WRITE_BUF 15
#define RELEASE_MEM 16
#define RELEASE_PROG 17
#define RELEASE_KERN 18

typedef struct rpc_state {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_mem buffer;
} rpc_state_t;

rpc_state_t st;

static int
do_get_plat_id(char *buf, int size)
{
    tpl_node *stn, *rtn;
    int result, num_entries, sz;
    cl_platform_id platform;

    stn = tpl_map("i", &num_entries);
    rtn = tpl_map("iI", &result, &platform);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    result = clGetPlatformIDs(num_entries, &platform, NULL);
    printf("result %d platform %p\n", result, platform);
    st.platform = platform;

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_get_dev_id(char *buf, int size)
{
    tpl_node *stn, *rtn;
    int result, sz;
    cl_platform_id platform;
    cl_device_type device_type;
    int num_entries;
    cl_device_id devices;

    stn = tpl_map("Iii", &platform, &device_type, &num_entries);
    rtn = tpl_map("iI", &result, &devices);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    result = clGetDeviceIDs(platform, device_type, num_entries, &devices, 
        NULL);
    printf("result %d device %p\n", result, devices);
    st.device = devices;

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_create_ctx(char *buf, int size)
{
    int sz;
    cl_device_id devices;
    int num_devices;
    cl_context context;
    tpl_node *stn, *rtn;

    stn = tpl_map("iI", &num_devices, &devices);
    rtn = tpl_map("I", &context);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    context = clCreateContext(NULL, num_devices, &devices, NULL, NULL, NULL);
    printf("context %p\n", context);
    st.context = context;

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_create_cqueue(char *buf, int size)
{
    int sz;
    cl_context context;
    cl_device_id device;
    cl_command_queue_properties properties;
    cl_command_queue queue;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIi", &context, &device, &properties);
    rtn = tpl_map("I", &queue);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    queue = clCreateCommandQueue(context, device, properties, NULL);
    printf("queue %p\n", queue);
    st.queue = queue;

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_create_prog_ws(char *buf, int size)
{
    int sz;
    cl_context context;
    int count;
    const char *strings;
    cl_program program;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iis", &context, &count, &strings);
    rtn = tpl_map("I", &program);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    program = clCreateProgramWithSource(context, count, &strings, NULL, NULL);
    printf("program %p\n", program);
    st.program = program;

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_build_prog(char *buf, int size)
{
    int result, sz;
    cl_program program;
    int num_devices;
    const cl_device_id device_list;
    tpl_node *stn, *rtn;

    stn = tpl_map("IiI", &program, &num_devices, &device_list);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    result = clBuildProgram(program, num_devices, &device_list,
        NULL, NULL, NULL);
    printf("build_prog result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_create_kern(char *buf, int size)
{
    int sz;
    cl_program program;
    const char *kernel_name;
    cl_kernel kernel;
    tpl_node *stn, *rtn;

    stn = tpl_map("Is", &program, &kernel_name);
    rtn = tpl_map("I", &kernel);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    kernel = clCreateKernel(program, kernel_name, NULL);
    printf("kernel %p\n", kernel);
    st.kernel = kernel;

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_create_buf(char *buf, int size)
{
    int sz;
    cl_context context;
    cl_mem_flags flags;
    int len;
    cl_mem buffer;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iii", &context, &flags, &len);
    rtn = tpl_map("I", &buffer);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    buffer = clCreateBuffer(context, flags, len, NULL, NULL);
    printf("buffer %p\n", buffer);
    st.buffer = buffer;

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_set_kern_arg(char *buf, int size)
{
    int result, sz;
    cl_kernel kernel;
    int arg_index;
    int arg_size;
    const void *arg_value;
    tpl_node *stn, *rtn;

    stn = tpl_map("IiiI", &kernel, &arg_index, &arg_size, &arg_value);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    result = clSetKernelArg(kernel, arg_index, arg_size, &arg_value);
    printf("set_kern result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_enq_ndr_kern(char *buf, int size)
{
    int result, sz;
    cl_command_queue command_queue;
    cl_kernel kernel;
    int work_dim;
    const int global_work_size;
    int num_events_in_wait_list;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiii", &command_queue, &kernel, &work_dim, 
        &global_work_size, &num_events_in_wait_list);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    result = clEnqueueNDRangeKernel(command_queue, kernel, work_dim, NULL,
        &global_work_size, NULL, num_events_in_wait_list, NULL, NULL);

    printf("enq_ndr result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_finish(char *buf, int size)
{
    int result, sz;
    cl_command_queue command_queue;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &command_queue);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    result = clFinish(command_queue);
    printf("finish result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}

static int
do_enq_map_buf(char *buf, int size)
{
    int sz;
    char c;
    char *buff;
    cl_command_queue command_queue;
    cl_mem buffer;
    cl_bool blocking_map;
    cl_map_flags map_flags;
    int offset;
    int cb;
    int num_events_in_wait_list;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiiiii", &command_queue, &buffer, &blocking_map, 
        &map_flags, &offset, &cb, &num_events_in_wait_list);
    rtn = tpl_map("A(c)", &c);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    buff = (char *) clEnqueueMapBuffer( command_queue, buffer, blocking_map, map_flags,
                                          offset, cb, num_events_in_wait_list,
                                          NULL, NULL, NULL );

    int i;
    for (i = 0; i < cb; i++) {
        c = buff[i];
        tpl_pack(rtn, 1); 
    }

    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    return sz;
}


int
process_request(char *buf, ssize_t nread, int size)
{
    int meth = buf[0];
    char *nbuf = buf + 4;
    int len = size - 4;
    int sz = 0;

    switch (meth) {

    case GET_PLAT_ID:
        sz = do_get_plat_id(nbuf, len);
        break;

    case GET_DEV_ID:
        sz = do_get_dev_id(nbuf, len);
        break;

    case CREATE_CTX:
        sz = do_create_ctx(nbuf, len);
        break;

    case CREATE_CQUEUE:
        sz = do_create_cqueue(nbuf, len);
        break;

    case CREATE_PROG_WS:
        sz = do_create_prog_ws(nbuf, len);
        break;

    case BUILD_PROG:
        sz = do_build_prog(nbuf, len);
        break;

    case CREATE_KERN:
        sz = do_create_kern(nbuf, len);
        break;

    case CREATE_BUF:
        sz = do_create_buf(nbuf, len);
        break;

    case SET_KERN_ARG:
        sz = do_set_kern_arg(nbuf, len);
        break;

    case ENQ_NDR_KERN:
        sz = do_enq_ndr_kern(nbuf, len);
        break;

    case FINISH:
        sz = do_finish(nbuf, len);
        break;

    case ENQ_MAP_BUF:
        sz = do_enq_map_buf(nbuf, len);
        break;

    default:
        break;

    }
    printf("size %d\n", sz);
    return sz + 4;
}


