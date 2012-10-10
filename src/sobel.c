#include <stdio.h>
#include <string.h>
#include <CL/cl.h>

#include "and_rpc_utils.h"
#include "and_rpc_clnt.h"

int main(int argc, char *argv[])
{
    init_rpc(argv[1]);

    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_mem input;
    cl_mem output;

    int iBlockDimX = 16;
    int iBlockDimY = 4;
    float fThresh = 80.0f;

    size_t global_work_size[2];
    size_t local_work_size[2] = { iBlockDimX, iBlockDimY };

    cl_int iLocalPixPitch = iBlockDimX + 2;
    cl_uint img_size, img_width, img_height;

    unsigned char *data = NULL, *out_img;

    shrLoadPPM4ub("tmp.ppm", &data, &img_width, &img_height);
    img_size = img_width * img_height * sizeof(unsigned int);
    out_img = (unsigned char *) malloc(img_size);

    printf("w %d h %d\n", img_width, img_height);

    global_work_size[0] = shrRoundUp((int)local_work_size[0], img_width);
    global_work_size[1] = shrRoundUp((int)local_work_size[1], img_height);

    const char *source = load_program_source("SobelFilter.cl");
    size_t source_len = strlen(source);;
    cl_uint err = 0;

    char *flags = "-cl-fast-relaxed-math";

    clGetPlatformIDs(1, &platform, NULL);
    printf("platform %p err %d\n", platform, err);

    clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, &err);
    printf("device %p err %d\n", device, err);

    context = clCreateContext(0, 1, &device, NULL, NULL, &err);
    printf("context %p err %d\n", context, err);

    queue = clCreateCommandQueue(context, device, 0, &err);
    printf("queue %p err %d\n", queue, err);

    program = clCreateProgramWithSource(context, 1, &source, &source_len, &err);
    printf("program %p err %d\n", program, err);

    err = clBuildProgram(program, 0, NULL, flags, NULL, NULL);
    printf("err %d\n", err);

    kernel = clCreateKernel(program, "ckSobel", NULL);
    printf("kernel %p\n", kernel);

    input = clCreateBuffer(context, CL_MEM_READ_ONLY, img_size, NULL, NULL);
    printf("input %p\n", input);

    output = clCreateBuffer(context, CL_MEM_READ_ONLY, img_size, NULL, NULL);
    printf("output %p\n", output);

    printf("img_size %d\n", img_size);

    err = clEnqueueWriteBuffer(queue, input, CL_TRUE, 0, img_size, data, 0,
          NULL, NULL);
    printf("err %d\n", err);

    err = 0;
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &output);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 2, (iLocalPixPitch * (iBlockDimY + 2) * 
           sizeof(cl_uchar4)), NULL);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 3, sizeof(cl_int), (void*)&iLocalPixPitch);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 4, sizeof(cl_uint), (void*)&img_width);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 5, sizeof(cl_uint), (void*)&img_height);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 6, sizeof(float), (void*)&fThresh);
    printf("err %d\n", err);


    err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_work_size,
        local_work_size, 0, NULL, NULL);
    printf("err %d\n", err);

    err = clFlush(queue);
    printf("err %d\n", err);

    err = clFinish(queue);
    printf("err %d\n", err);


    err = clEnqueueReadBuffer(queue, output, CL_TRUE, 0, img_size, out_img, 0,
          NULL, NULL);
    printf("err %d\n", err);


    shrSavePPM4ub("out.ppm", out_img, img_width, img_height);

    clReleaseMemObject(input);
    clReleaseMemObject(output);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
}

