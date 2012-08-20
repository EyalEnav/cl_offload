/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "ocl_rpc.h"
#include <CL/cl.h>

plat_id_t *
get_plat_id_1_svc(plat_id_t *argp, struct svc_req *rqstp)
{
    static plat_id_t  result;

    cl_platform_id platform;
    result.result = (int) clGetPlatformIDs(argp->num, &platform, NULL);
    result.platform = (quad_t) platform;

    printf("debug get_plat %p\n", platform);

    return &result;
}

plat_info_t *
get_plat_info_1_svc(plat_info_t *argp, struct svc_req *rqstp)
{
    static plat_info_t result;

    return &result;
}

get_devs_t *
get_devs_id_1_svc(get_devs_t *argp, struct svc_req *rqstp)
{
    static get_devs_t  result;

    cl_device_id device;
    cl_platform_id platform = (cl_platform_id) argp->platform;
    result.result = (int) clGetDeviceIDs(platform, argp->device_type,
        argp->num_entries, &device, NULL);

    result.devices = (quad_t) device;
    printf("debug get_dev %p\n", device);

    return &result;
}

create_ctx_t *
create_ctx_1_svc(create_ctx_t *argp, struct svc_req *rqstp)
{
    static create_ctx_t  result;

    cl_device_id device = (cl_device_id) argp->devices;
    cl_context context = clCreateContext(NULL, argp->num_devices, &device,
        NULL, NULL, NULL);

    result.context = (quad_t) context;
    printf("debug create_ctx %p\n", context);

    return &result;
}

create_cqueue_t *
create_cqueue_1_svc(create_cqueue_t *argp, struct svc_req *rqstp)
{
    static create_cqueue_t  result;

    cl_device_id device = (cl_device_id) argp->device;
    cl_context context = (cl_context) argp->context;
    cl_command_queue queue = clCreateCommandQueue(context, device, 
        argp->properties, NULL);

    result.queue = (quad_t) queue;
    printf("debug create_cqueue %p\n", queue);

    return &result;
}

create_prog_ws_t *
create_prog_ws_1_svc(create_prog_ws_t *argp, struct svc_req *rqstp)
{
    static create_prog_ws_t  result;

    cl_context context = (cl_context) argp->context;
    cl_program prog = clCreateProgramWithSource(context, argp->count, 
        (const char **)&(argp->strings), NULL, NULL);

    // we have to set this for some reason
    result.strings = "true";
    result.prog = (quad_t) prog;
    printf("debug create_prog %p\n", prog);

    return &result;
}

build_prog_t *
build_prog_1_svc(build_prog_t *argp, struct svc_req *rqstp)
{
    static build_prog_t  result;

    cl_program program = (cl_program) argp->prog;
    cl_device_id device = (cl_device_id) argp->device_list;
    result.result = clBuildProgram(program, argp->num_devices, &device,
        NULL, NULL, NULL);

    printf("debug build_prog %d\n", result.result);

    return &result;
}

create_kern_t *
create_kern_1_svc(create_kern_t *argp, struct svc_req *rqstp)
{
    static create_kern_t  result;

    cl_program program = (cl_program) argp->prog;
    cl_kernel kernel = clCreateKernel(program, argp->kernel_name, NULL);

    // we have to set string
    result.kernel_name = "true";
    result.kernel = (quad_t) kernel;
    printf("debug create_kern %p\n", kernel);

    return &result;
}

create_buf_t *
create_buf_1_svc(create_buf_t *argp, struct svc_req *rqstp)
{
    static create_buf_t  result;

    cl_context context = (cl_context) argp->context;
    cl_mem buffer = clCreateBuffer(context, argp->flags, argp->size,
        NULL, NULL);

    result.buffer = (quad_t) buffer;
    printf("debug create_buf %p\n", buffer);

    return &result;
}

set_kern_arg_t *
set_kern_arg_1_svc(set_kern_arg_t *argp, struct svc_req *rqstp)
{
    static set_kern_arg_t  result;

    cl_kernel kernel = (cl_kernel) argp->kernel;

    if (argp->arg.arg_val != NULL) {
        result.result = clSetKernelArg(kernel, argp->arg_index, 
            argp->arg.arg_len, (void *) argp->arg.arg_val);
    }
    else {
        result.result = clSetKernelArg(kernel, argp->arg_index, 36, NULL);
    }

    printf("debug set_kern_arg %d\n", result.result);

    return &result;
}

enq_ndr_kern_t *
enq_ndr_kern_1_svc(enq_ndr_kern_t *argp, struct svc_req *rqstp)
{
    static enq_ndr_kern_t  result;

    cl_command_queue command_queue = (cl_command_queue) argp->command_queue;
    cl_kernel kernel = (cl_kernel) argp->kernel;
    const size_t global_work_size = argp->global_work_size;
    result.result = clEnqueueNDRangeKernel(command_queue, kernel,
        argp->work_dim, NULL, &global_work_size, NULL,
        argp->num_events_in_wait_list, NULL, NULL);

    printf("debug enq_ndr_kern %d\n", result.result);

    return &result;
}

finish_t *
finish_1_svc(finish_t *argp, struct svc_req *rqstp)
{
    static finish_t  result;

    cl_command_queue command_queue = (cl_command_queue) argp->command_queue;

    result.result = clFinish(command_queue);

    printf("debug finish %d\n", result.result);

    return &result;
}

enq_map_buf_t *
enq_map_buf_1_svc(enq_map_buf_t *argp, struct svc_req *rqstp)
{
    static enq_map_buf_t result;

    cl_command_queue command_queue = (cl_command_queue) argp->command_queue;
    cl_mem buffer = (cl_mem) argp->buffer;
    char *ptr = (char *) clEnqueueMapBuffer(command_queue, buffer, 
        argp->blocking_map, argp->map_flags, argp->offset, argp->cb,
        argp->num_events_in_wait_list, NULL, NULL, NULL);

    result.buf.buf_len = argp->cb;
    result.buf.buf_val = ptr;

    printf("debug enq_map_buf %p\n", ptr);

    return &result;
}


enq_read_buf_t *
enq_read_buf_1_svc(enq_read_buf_t *argp, struct svc_req *rqstp)
{
    static enq_read_buf_t  result;

    cl_command_queue command_queue = (cl_command_queue) argp->command_queue;
    cl_mem buffer = (cl_mem)argp->buffer;
    result.result = (int) clEnqueueReadBuffer(command_queue, buffer, 
        argp->blocking_read, argp->offset, argp->cb, 
        (void *)argp->ptr.ptr_val, argp->num_events_in_wait_list, NULL, NULL);
    
    printf("debug enq_read_buf %p\n", argp->ptr.ptr_val);
    
    result.ptr.ptr_len = argp->cb;
    result.ptr.ptr_val = argp->ptr.ptr_val;

    return &result;
}

enq_write_buf_t *
enq_write_buf_1_svc(enq_write_buf_t *argp, struct svc_req *rqstp)
{
    static enq_write_buf_t  result;

    cl_command_queue command_queue = (cl_command_queue) argp->command_queue;
    cl_mem buffer = (cl_mem)argp->buffer;
    result.result = (int) clEnqueueWriteBuffer(command_queue, buffer, 
        argp->blocking_write, argp->offset, argp->cb, 
        (const void *)argp->ptr.ptr_val, argp->num_events_in_wait_list, NULL, 
        NULL);

    printf("debug enq_write_buf %p\n", argp->ptr.ptr_val);

    result.ptr.ptr_len = argp->cb;
    result.ptr.ptr_val = argp->ptr.ptr_val;

    return &result;
}

release_mem_t *
release_mem_1_svc(release_mem_t *argp, struct svc_req *rqstp)
{
    static release_mem_t  result;
    
    cl_mem buffer = (cl_mem) argp->buffer;
    result.result = (int) clReleaseMemObject(buffer);

    return &result;
}

release_prog_t *
release_prog_1_svc(release_prog_t *argp, struct svc_req *rqstp)
{
    static release_prog_t  result;
    
    cl_program program = (quad_t) argp->prog;
    result.result = (int) clReleaseProgram(program);

    return &result;
}

release_kern_t *
release_kern_1_svc(release_kern_t *argp, struct svc_req *rqstp)
{
    static release_kern_t  result;

    cl_kernel kernel = (cl_kernel) argp->kernel;
    result.result = (int) clReleaseKernel(kernel);

    return &result;
}

release_cqueue_t *
release_cqueue_1_svc(release_cqueue_t *argp, struct svc_req *rqstp)
{
    static release_cqueue_t  result;

    cl_command_queue command_queue = (cl_command_queue) argp->command_queue;
    result.result = (int) clReleaseCommandQueue(command_queue);

    return &result;
}

release_ctx_t *
release_ctx_1_svc(release_ctx_t *argp, struct svc_req *rqstp)
{
    static release_ctx_t  result;
    
    cl_context context = (cl_context) argp->context;
    result.result = (int) clReleaseContext(context);

    return &result;
}