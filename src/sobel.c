#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <CL/cl.h>

#include "and_rpc_clnt.h"

const unsigned int PGMHeaderSize = 0x40;

int loadPPM(const char* file, unsigned char** data, 
            unsigned int *w, unsigned int *h, unsigned int *channels) 
{
    FILE* fp = 0;

        // open the file for binary read
        if ((fp = fopen(file, "rb")) == 0)
        {
            // if error on attempt to open, be sure the file is null or close it, then return negative error code
            if (fp)
            {
                fclose (fp);
            }
            fprintf(stderr, "open failed\n");
            return -1;
        }

    // check header
    char header[PGMHeaderSize];
    if ((fgets( header, PGMHeaderSize, fp) == NULL) && ferror(fp))
    {
        if (fp)
        {
            fclose (fp);
        }
        fprintf(stderr, "not a valid ppm file\n");
        *channels = 0;
        return -1;
    }

    if (strncmp(header, "P5", 2) == 0)
    {
        *channels = 1;
    }
    else if (strncmp(header, "P6", 2) == 0)
    {
        *channels = 3;
    }
    else
    {
        fprintf(stderr, "loadPPM() : File is not a PPM or PGM image\n");
        *channels = 0;
        return -1;
    }

    // parse header, read maxval, width and height
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int maxval = 0;
    unsigned int i = 0;
    while(i < 3) 
    {
        if ((fgets(header, PGMHeaderSize, fp) == NULL) && ferror(fp))
        {
            if (fp)
            {
                fclose (fp);
            }
            fprintf(stderr, "loadPPM() : File is not a valid PPM or PGM image\n");
            return -1;
        }
        if(header[0] == '#') continue;

            if(i == 0) 
            {
                i += sscanf(header, "%u %u %u", &width, &height, &maxval);
            }
            else if (i == 1) 
            {
                i += sscanf(header, "%u %u", &height, &maxval);
            }
            else if (i == 2) 
            {
                i += sscanf(header, "%u", &maxval);
            }
    }

    // check if given handle for the data is initialized
    if(NULL != *data) 
    {
        if (*w != width || *h != height) 
        {
            fclose(fp);
            fprintf(stderr, "loadPPM() : Invalid image dimensions.\n");
            return -1;
        }
    } 
    else 
    {
        *data = (unsigned char*)malloc( sizeof(unsigned char) * width * height * *channels);
        printf("malloc1 %d\n", sizeof(unsigned char) * width * height * *channels);
        *w = width;
        *h = height;
    }

    // read and close file
    if (fread(*data, sizeof(unsigned char), width * height * *channels, fp) != width * height * *channels)
    {
        fclose(fp);
        fprintf(stderr, "loadPPM() : Invalid image.\n");
        return -1;
    }
    fclose(fp);

    return 0;
}

int shrLoadPPM4ub( const char* file, unsigned char** OutData, 
                unsigned int *w, unsigned int *h)
{
    // Load file data into a temporary buffer with automatic allocation
    unsigned char* cLocalData = 0;
    unsigned int channels;
    int bLoadOK = loadPPM(file, &cLocalData, w, h, &channels);   // this allocates cLocalData, which must be freed later

    // If the data loaded OK from file to temporary buffer, then go ahead with padding and transfer 
    if (bLoadOK == 0)
    {
        // if the receiving buffer is null, allocate it... caller must free this 
        int size = *w * *h;
        if (*OutData == NULL)
        {
            *OutData = (unsigned char*)malloc(sizeof(unsigned char) * size * 4);
        }

        // temp pointers for incrementing
        unsigned char* cTemp = cLocalData;
        unsigned char* cOutPtr = *OutData;

        // transfer data, padding 4th element
        int i;
        for(i=0; i<size; i++) 
        {
            *cOutPtr++ = *cTemp++;
            *cOutPtr++ = *cTemp++;
            *cOutPtr++ = *cTemp++;
            *cOutPtr++ = 0;
        }

        // free temp lcoal buffer and return OK
        free(cLocalData);
        return 0;
    }
    else
    {
        // image wouldn't load
        free(cLocalData);
        return -1;
    }
}

int savePPM( const char* file, unsigned char *data, 
             unsigned int w, unsigned int h, unsigned int channels) 
{
    FILE* fp = 0;
    if ((fp = fopen(file, "wb")) == 0) {
        if (fp) {
            fclose (fp);
        }
        fprintf(stderr, "open failed\n");
        return -1;
    }

    if (channels == 1) {
        fwrite("P5\n", sizeof(char), 3, fp);
    }
    else if (channels == 3) {
        fwrite("P6\n", sizeof(char), 3, fp);
    }
    else {
        fprintf(stderr, "savePPM() : Invalid number of channels.\n");
        return -1;
    }

    int maxval = 255;
    char header[100];
    int len = sprintf(header, "%d %d\n%d\n", w, h, maxval);

    fwrite(header, sizeof(char), len, fp);

    if (fwrite(data, sizeof(unsigned char), w * h * channels, fp) < 0) {
        fprintf(stderr, "fwrite failed\n");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int shrSavePPM4ub( const char* file, unsigned char *data, 
               unsigned int w, unsigned int h) 
{
    // strip 4th component
    int size = w * h;
    unsigned char *ndata = (unsigned char*) malloc( sizeof(unsigned char) * size*3);
    unsigned char *ptr = ndata;
    int i;
    for(i=0; i<size; i++) {
        *ptr++ = *data++;
        *ptr++ = *data++;
        *ptr++ = *data++;
        data++;
    }
    
    int succ = savePPM(file, ndata, w, h, 3);
    free(ndata);
    return succ;
}

static char *load_program_source(const char *filename)
{
    struct stat statbuf;
    FILE *fh;
    char *source;

    fh = fopen(filename, "r");
    if (fh == 0) return 0;

    stat(filename, &statbuf);
    source = (char *) malloc(statbuf.st_size + 1);
    fread(source, statbuf.st_size, 1, fh);
    source[statbuf.st_size] = '\0';
    return source;
}

// Round Up Division function
size_t shrRoundUp(int group_size, int global_size)
{
    int r = global_size % group_size;
    if(r == 0)
    {
        return global_size;
    } else
    {
        return global_size + group_size - r;
    }
}


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

    err = clSetKernelArg(kernel, 3, 1001, (void*)&iLocalPixPitch);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 4, 1002, (void*)&img_width);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 5, 1002, (void*)&img_height);
    printf("err %d\n", err);

    err = clSetKernelArg(kernel, 6, 1003, (void*)&fThresh);
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

