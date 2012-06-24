
#include <stdio.h>
#include "ocl_rpc_clnt.h"
#include "ocl_rpc.h"

static CLIENT *clnt;

static struct timeval TIMEOUT = { 25, 0 };

int init_rpc()
{
    clnt = clnt_create("127.0.0.1", OCL_PROG, OCL_VERS, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror("127.0.0.1");
        exit(1);
    }
    return 0;
}

cl_int clGetPlatformIDs (cl_uint num_entries,
                         cl_platform_id *platforms,
                         cl_uint *num_platforms)
{
    static plat_id_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.num = num_entries;

    if (clnt_call (clnt, GET_PLAT_ID,
        (xdrproc_t) xdr_plat_id_t, (caddr_t) &args,
        (xdrproc_t) xdr_plat_id_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (cl_int) -1; 
    }

    *platforms = (cl_platform_id) res.platform;
    return (cl_int) res.result;
}

cl_int clGetDeviceIDs (cl_platform_id platform,
                       cl_device_type device_type,
                       cl_uint num_entries,
                       cl_device_id *devices,
                       cl_uint *num_devices)
{
    static get_devs_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.platform = (quad_t) platform;
    args.device_type = device_type;
    args.num_entries = num_entries;

    if (clnt_call (clnt, GET_DEVS_ID,
        (xdrproc_t) xdr_get_devs_t, (caddr_t) &args,
        (xdrproc_t) xdr_get_devs_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (cl_int) -1; 
    }

    *devices = (cl_device_id) res.devices;
    return (cl_int) res.result;
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
    static create_ctx_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.num_devices = num_devices;
    args.devices = (quad_t) *devices;

    if (clnt_call (clnt, CREATE_CTX,
        (xdrproc_t) xdr_create_ctx_t, (caddr_t) &args,
        (xdrproc_t) xdr_create_ctx_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (NULL); 
    }

    return (cl_context) res.context;
}

cl_command_queue clCreateCommandQueue (cl_context context,
                                   cl_device_id device,
                                   cl_command_queue_properties properties,
                                   cl_int *errcode_ret)
{
    static create_cqueue_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.context = (quad_t) context;
    args.device = (quad_t) device;
    args.properties = properties;

    if (clnt_call (clnt, CREATE_CQUEUE,
        (xdrproc_t) xdr_create_cqueue_t, (caddr_t) &args,
        (xdrproc_t) xdr_create_cqueue_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (NULL); 
    }

    return (cl_command_queue) res.queue;
}

cl_program clCreateProgramWithSource (cl_context context,
                                      cl_uint count,
                                      const char **strings,
                                      const size_t *lengths,
                                      cl_int *errcode_ret)
{
    static create_prog_ws_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.context = (quad_t) context;
    args.count = count;
    args.strings = *strings;

    if (clnt_call (clnt, CREATE_PROG_WS,
        (xdrproc_t) xdr_create_prog_ws_t, (caddr_t) &args,
        (xdrproc_t) xdr_create_prog_ws_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (NULL); 
    }

    return (cl_program) res.prog;
}

cl_int clBuildProgram (cl_program program,
                       cl_uint num_devices,
                       const cl_device_id *device_list,
                       const char *options,
                       void (CL_CALLBACK *pfn_notify)(cl_program program,
                           void *user_data),
                       void *user_data)
{
    static build_prog_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.prog = (quad_t) program;
    args.num_devices = num_devices;
    args.device_list = (quad_t) *device_list;

    if (clnt_call (clnt, BUILD_PROG,
        (xdrproc_t) xdr_build_prog_t, (caddr_t) &args,
        (xdrproc_t) xdr_build_prog_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (cl_int) -1; 
    }

    return (cl_int) res.result;
}

cl_kernel clCreateKernel (cl_program program,
                          const char *kernel_name,
                          cl_int *errcode_ret)
{
    static create_kern_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.prog = (quad_t) program;
    args.kernel_name = kernel_name;

    if (clnt_call (clnt, CREATE_KERN,
        (xdrproc_t) xdr_create_kern_t, (caddr_t) &args,
        (xdrproc_t) xdr_create_kern_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (NULL); 
    }

    return (cl_kernel) res.kernel;
}

cl_mem clCreateBuffer (cl_context context,
                      cl_mem_flags flags,
                      size_t size,
                      void *host_ptr,
                      cl_int *errcode_ret)
{
    static create_buf_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.context = (quad_t) context;
    args.flags = flags;
    args.size = size;
    args.host_ptr.host_ptr_len = 0;
    args.host_ptr.host_ptr_val = NULL;

    if (clnt_call (clnt, CREATE_BUF,
        (xdrproc_t) xdr_create_buf_t, (caddr_t) &args,
        (xdrproc_t) xdr_create_buf_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (NULL); 
    }

    return (cl_mem) res.buffer;
}

cl_int clSetKernelArg (cl_kernel kernel,
                       cl_uint arg_index,
                       size_t arg_size,
                       const void *arg_value)
{
    static set_kern_arg_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.kernel = (quad_t) kernel;
    args.arg_index = arg_index;
    args.arg.arg_len = arg_size;
    args.arg.arg_val = (char *)arg_value;

    if (clnt_call (clnt, SET_KERN_ARG,
        (xdrproc_t) xdr_set_kern_arg_t, (caddr_t) &args,
        (xdrproc_t) xdr_set_kern_arg_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (cl_int) -1; 
    }

    return (cl_int) res.result;
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
    static enq_ndr_kern_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.command_queue = (quad_t) command_queue;
    args.kernel = (quad_t) kernel;
    args.work_dim = work_dim;
    args.global_work_size = *global_work_size;
    args.num_events_in_wait_list = num_events_in_wait_list;

    if (clnt_call (clnt, ENQ_NDR_KERN,
        (xdrproc_t) xdr_enq_ndr_kern_t, (caddr_t) &args,
        (xdrproc_t) xdr_enq_ndr_kern_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (cl_int) -1; 
    }

    return (cl_int) res.result;

}

cl_int clFinish (cl_command_queue command_queue)
{
    static finish_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.command_queue = (quad_t) command_queue;

    if (clnt_call (clnt, FINISH,
        (xdrproc_t) xdr_finish_t, (caddr_t) &args,
        (xdrproc_t) xdr_finish_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (cl_int) -1; 
    }

    return (cl_int) res.result;
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
    static enq_map_buf_t args, res;
    memset((char *)&args, 0, sizeof(args));
    memset((char *)&res, 0, sizeof(res));

    args.command_queue = (quad_t) command_queue;
    args.buffer = (quad_t) buffer;
    args.blocking_map = blocking_map;
    args.map_flags = map_flags;
    args.offset = offset;
    args.cb = cb;
    args.num_events_in_wait_list = num_events_in_wait_list;
    args.buf.buf_len = 0;
    args.buf.buf_val = NULL;

    if (clnt_call (clnt, ENQ_MAP_BUF,
        (xdrproc_t) xdr_enq_map_buf_t, (caddr_t) &args,
        (xdrproc_t) xdr_enq_map_buf_t, (caddr_t) &res,
        TIMEOUT) != RPC_SUCCESS) {
        return (NULL); 
    }

    return (void *) res.buf.buf_val;
}

