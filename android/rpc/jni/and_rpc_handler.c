
#include <stdio.h>
#include <CL/cl.h>

#include "tpl.h"

//#include "and_rpc_handler.h"

#define GET_PLAT_ID 1
#define GET_DEV_ID 2

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

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);
    tpl_free(stn);
    tpl_free(rtn);

    printf("result %d platform %p\n", result, platform);
    return sz;
}

static int
do_get_dev_id(char *buf, int size)
{
    tpl_node *stn, *rtn;
    int result, sz;
    cl_platform_id platform;
    cl_device_type device_type;
    cl_uint num_entries;
    cl_device_id devices;

    stn = tpl_map("Iii", &platform, &device_type, &num_entries);
    rtn = tpl_map("iI", &result, &devices);

    tpl_load(stn, TPL_MEM|TPL_EXCESS_OK, buf, size);
    tpl_unpack(stn, 0);

    result = clGetDeviceIDs(platform, device_type, num_entries, &devices, 
        NULL);

    tpl_pack(rtn, 0);
    tpl_dump(rtn, TPL_GETSIZE, &sz);
    tpl_dump(rtn, TPL_MEM|TPL_PREALLOCD, buf, size);

    tpl_free(stn);
    tpl_free(rtn);

    printf("result %d device %p\n", result, devices);

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

    default:
        break;

    }
    printf("size %d\n", sz);
    return sz + 4;
}


