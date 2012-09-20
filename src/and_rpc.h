
#ifndef _AND_RPC_H_
#define _AND_RPC_H_

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
#define RELEASE_CQUEUE 19
#define RELEASE_CTX 20
#define FLUSH 21

#define MCAST_ADDR "239.192.1.100"
#define M_IDX 4
#define H_OFFSET 5
#define BUFSIZE 1024

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

#endif
