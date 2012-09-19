
#include <stdio.h>
#include <string.h>
#include <CL/cl.h>

#include "tpl.h"
#include "and_rpc.h"

static int
do_get_plat_id(char **buf, int size)
{
    tpl_node *stn, *rtn;
    int result, num_entries, sz;
    cl_platform_id platform;

    stn = tpl_map("i", &num_entries);
    rtn = tpl_map("iI", &result, &platform);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    result = clGetPlatformIDs(num_entries, &platform, NULL);
    printf("result %d platform %p\n", result, platform);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_get_dev_id(char **buf, int size)
{
    tpl_node *stn, *rtn;
    int result, sz;
    cl_platform_id platform;
    cl_device_type device_type;
    int num_entries;
    cl_device_id devices;

    stn = tpl_map("Iii", &platform, &device_type, &num_entries);
    rtn = tpl_map("iI", &result, &devices);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    result = clGetDeviceIDs(platform, device_type, num_entries, &devices, 
        NULL);
    printf("result %d device %p\n", result, devices);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_create_ctx(char **buf, int size)
{
    int sz;
    cl_device_id devices;
    int num_devices;
    cl_context context;
    tpl_node *stn, *rtn;

    stn = tpl_map("iI", &num_devices, &devices);
    rtn = tpl_map("I", &context);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    context = clCreateContext(NULL, num_devices, &devices, NULL, NULL, NULL);
    printf("context %p\n", context);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_create_cqueue(char **buf, int size)
{
    int sz;
    cl_context context;
    cl_device_id device;
    cl_command_queue_properties properties;
    cl_command_queue queue;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIi", &context, &device, &properties);
    rtn = tpl_map("I", &queue);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    queue = clCreateCommandQueue(context, device, properties, NULL);
    printf("queue %p\n", queue);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_create_prog_ws(char **buf, int size)
{
    int sz;
    cl_context context;
    int count;
    const char *strings;
    cl_program program;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iis", &context, &count, &strings);
    rtn = tpl_map("I", &program);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    program = clCreateProgramWithSource(context, count, &strings, NULL, NULL);
    printf("program %p\n", program);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    (*buf)[1] = 1;

    return sz;
}

static int
do_build_prog(char **buf, int size)
{
    int result, sz;
    cl_program program;
    int num_devices;
    const cl_device_id device_list;
    tpl_node *stn, *rtn;

    stn = tpl_map("IiI", &program, &num_devices, &device_list);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    printf("prog %p num_dev %d\n", program, num_devices);
    if (device_list == NULL) {
        printf("null found\n");
        result = clBuildProgram(program, num_devices, NULL,
            NULL, NULL, NULL);
    }
    else {
        result = clBuildProgram(program, num_devices, &device_list,
            NULL, NULL, NULL);
    }
    printf("build_prog result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_create_kern(char **buf, int size)
{
    int sz;
    cl_program program;
    const char *kernel_name;
    cl_kernel kernel;
    tpl_node *stn, *rtn;

    stn = tpl_map("Is", &program, &kernel_name);
    rtn = tpl_map("I", &kernel);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    kernel = clCreateKernel(program, kernel_name, NULL);
    printf("kernel %p\n", kernel);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_create_buf(char **buf, int size)
{
    int sz;
    cl_context context;
    cl_mem_flags flags;
    int len;
    cl_mem buffer;
    tpl_node *stn, *rtn;

    stn = tpl_map("Iii", &context, &flags, &len);
    rtn = tpl_map("I", &buffer);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    buffer = clCreateBuffer(context, flags, len, NULL, NULL);
    printf("buffer %p\n", buffer);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_set_kern_arg(char **buf, int size)
{
    int result, sz;
    cl_kernel kernel;
    int arg_index;
    int arg_size;
    const void *arg_value;
    tpl_node *stn, *rtn;

    stn = tpl_map("IiiI", &kernel, &arg_index, &arg_size, &arg_value);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    printf("kernel %p arg_idx %d arg_size %d arg_val %p\n", 
            kernel, arg_index, arg_size, arg_value);

    if (arg_value == NULL) {
        result = clSetKernelArg(kernel, arg_index, arg_size, NULL);
    }
    else if (arg_size == 1234) {
        int *tmp = malloc(sizeof(int));
        *tmp = arg_value;
        printf("arg_val %d\n", *tmp);
        arg_size = sizeof(int);
        result = clSetKernelArg(kernel, arg_index, arg_size, tmp);
    }
    else {
        result = clSetKernelArg(kernel, arg_index, arg_size, &arg_value);
    }
    printf("set_kern result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_enq_ndr_kern(char **buf, int size)
{
    int result, sz;
    cl_command_queue command_queue;
    cl_kernel kernel;
    int work_dim;
    int global_work_size;
    int num_events_in_wait_list;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiii", &command_queue, &kernel, &work_dim, 
        &global_work_size, &num_events_in_wait_list);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    int tmp = global_work_size;
    result = clEnqueueNDRangeKernel(command_queue, kernel, work_dim, NULL,
        &tmp, NULL, 0, NULL, NULL);

    printf("enq_ndr result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_finish(char **buf, int size)
{
    int result, sz;
    cl_command_queue command_queue;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &command_queue);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    printf("queue %p\n", command_queue);

    result = clFinish(command_queue);
    printf("finish result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_enq_map_buf(char **buf, int size)
{
    int sz, result = 0;
    char c;
    char *buff;
    char *tmp_buf;
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
    rtn = tpl_map("iA(c)", &result, &c);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    int tmp = 512;
    buff = (char *) clEnqueueMapBuffer( command_queue, buffer, blocking_map, map_flags,
                                        offset, cb, num_events_in_wait_list, 
                                        NULL, NULL, NULL);

    tpl_pack(rtn, 0);
    int i;
    for (i = 0; i < cb; i++) {
        c = buff[i];
        tpl_pack(rtn, 1);
    }

    tpl_dump(rtn, TPL_GETSIZE, &sz);

    if (sz > 1020) {
        tmp_buf = malloc(sz + 4);
        memcpy(tmp_buf, *buf, 4);
        int no_bufs = cb/1024 + 1;
        if (no_bufs > 127) {
            tmp_buf[1] = -1;
            tmp_buf[1] = -1;
            tmp_buf[2] = no_bufs/256;
            tmp_buf[3] = no_bufs % 256;
        }
        else {
            tmp_buf[1] = cb/1024 + 1;
        }
  
        *buf = tmp_buf;
    }

    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, sz);
    free(buff);

    return sz;
}

static int
do_enq_read_buf(char **buf, int size)
{
    int sz, result;
    char c;
    char *buff;
    char *tmp_buf;
    cl_command_queue command_queue;
    cl_mem buffer;
    cl_bool blocking_read;
    int offset;
    int cb;
    int num_events_in_wait_list;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiiii", &command_queue, &buffer, &blocking_read, 
        &offset, &cb, &num_events_in_wait_list);
    rtn = tpl_map("iA(c)", &result, &c);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    buff = malloc(cb);
    int tmp = 512;
    result = clEnqueueReadBuffer( command_queue, buffer, blocking_read, offset,
                                          cb, buff, num_events_in_wait_list,
                                          NULL, NULL);

    tpl_pack(rtn, 0);
    int i;
    for (i = 0; i < cb; i++) {
        c = buff[i];
        tpl_pack(rtn, 1);
    }

    tpl_dump(rtn, TPL_GETSIZE, &sz);

    if (sz > 1020) {
        tmp_buf = malloc(sz + 4);
        memcpy(tmp_buf, *buf, 4);
        int no_bufs = cb/1024 + 1;
        if (no_bufs > 127) {
            tmp_buf[1] = -1;
            tmp_buf[2] = no_bufs/256;
            tmp_buf[3] = no_bufs % 256;
        }
        else {
            tmp_buf[1] = cb/1024 + 1;
        }
  
        *buf = tmp_buf;
    }

    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, sz);
    free(buff);

    return sz;
}

static int
do_enq_write_buf(char **buf, int size)
{
    int sz, result;
    char c;
    char *buff;
    cl_command_queue command_queue;
    cl_mem buffer;
    cl_bool blocking_write;
    int offset;
    int cb;
    int num_events_in_wait_list;
    tpl_node *stn, *rtn;

    stn = tpl_map("IIiiiiA(c)", &command_queue, &buffer, &blocking_write, 
        &offset, &cb, &num_events_in_wait_list, &c);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    buff = malloc(cb);

    int i = 0;
    while (tpl_unpack(stn, 1) > 0) {
        buff[i] = c;
        i++;
    }

    result = clEnqueueWriteBuffer( command_queue, buffer, blocking_write, offset,
                                          cb, buff, num_events_in_wait_list,
                                          NULL, NULL);

    printf("write buffer result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    (*buf)[1] = 1;
    return sz;
}

static int
do_release_mem(char **buf, int size)
{
    int result, sz;
    cl_mem memobj;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &memobj);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    result = clReleaseMemObject(memobj);
    printf("release_mem result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_release_prog(char **buf, int size)
{
    int result, sz;
    cl_program program;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &program);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    result = clReleaseProgram(program);
    printf("release prog result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_release_kern(char **buf, int size)
{
    int result, sz;
    cl_kernel kernel;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &kernel);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    result = clReleaseKernel(kernel);
    printf("release_kern result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_release_cqueue(char **buf, int size)
{
    int result, sz;
    cl_command_queue command_queue;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &command_queue);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    result = clReleaseCommandQueue(command_queue);
    printf("release_cqueue result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

static int
do_release_ctx(char **buf, int size)
{
    int result, sz;
    cl_context context;
    tpl_node *stn, *rtn;

    stn = tpl_map("I", &context);
    rtn = tpl_map("i", &result);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, (*buf) + 4, size - 4);
    tpl_unpack(stn, 0);

    result = clReleaseContext(context);
    printf("release_ctx result %d\n", result);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, (*buf) + 4, size - 4);

    return sz;
}

int
process_request(char **buf, ssize_t nread, int size)
{
    int meth = (*buf)[0];
    int len = size;
    int sz = 0;

    switch (meth) {

    case GET_PLAT_ID:
        sz = do_get_plat_id(buf, len);
        break;

    case GET_DEV_ID:
        sz = do_get_dev_id(buf, len);
        break;

    case CREATE_CTX:
        sz = do_create_ctx(buf, len);
        break;

    case CREATE_CQUEUE:
        sz = do_create_cqueue(buf, len);
        break;

    case CREATE_PROG_WS:
        sz = do_create_prog_ws(buf, len);
        break;

    case BUILD_PROG:
        sz = do_build_prog(buf, len);
        break;

    case CREATE_KERN:
        sz = do_create_kern(buf, len);
        break;

    case CREATE_BUF:
        sz = do_create_buf(buf, len);
        break;

    case SET_KERN_ARG:
        sz = do_set_kern_arg(buf, len);
        break;

    case ENQ_NDR_KERN:
        sz = do_enq_ndr_kern(buf, len);
        break;

    case FINISH:
        sz = do_finish(buf, len);
        break;

    case ENQ_MAP_BUF:
        sz = do_enq_map_buf(buf, len);
        break;

    case ENQ_READ_BUF:
        sz = do_enq_read_buf(buf, len);
        break;

    case ENQ_WRITE_BUF:
        sz = do_enq_write_buf(buf, len);
        break;

    case RELEASE_MEM:
        sz = do_release_mem(buf, len);
        break;

    case RELEASE_PROG:
        sz = do_release_prog(buf, len);
        break;

    case RELEASE_KERN:
        sz = do_release_kern(buf, len);
        break;

    case RELEASE_CQUEUE:
        sz = do_release_cqueue(buf, len);
        break;

    case RELEASE_CTX:
        sz = do_release_ctx(buf, len);
        break;

    default:
        break;

    }
    printf("size %d\n", sz);
    return sz + 4;
}


