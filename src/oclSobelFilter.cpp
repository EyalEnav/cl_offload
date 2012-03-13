/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

// oclSobelFilter is a multi-GPU enabled RGB Sobel filter
//*****************************************************************************
// Command line switches:
//   --device=<n>    Sets a single specific device (if in range) at 100% of load
//                   Default (no --device switch) runs on:
//                          > Only available device if there's only 1,
//                          > All available devices having min acceptable perf if there are several
//                            Load balancing is performed based upon polled device characteristics and estimated perf                              
//                          > Best available device if there are more than 1 and none have min acceptable perf  
//   --noprompt      Runs a few seconds with full copy/compute/copy graphics loop and then quits without a prompt
//   --qatest        Runs special TestNoGL function (copy/compute/copy without graphics) then quits without a prompt
//
// Keyboard Options:
//   F toggle full screen
//   P toggeles processor (GPU / CPU)
//   <esc> or Q quits

// Includes
//*****************************************************************************

// MultiGPU helper class
#include "DeviceManager.h"

// Utilities, OpenCL and system includes
#include <oclUtils.h>
#include <shrQATest.h>
#include <iostream>

#ifndef min
#define min(a,b) (a < b ? a : b)
#endif

int *pArgc = NULL;
char **pArgv = NULL;

// Import host computation function for functional and perf comparison
extern "C" double SobelFilterHost(unsigned int* uiInputImage, unsigned int* uiOutputImage, 
                                  unsigned int uiWidth, unsigned int uiHeight, float fThresh);

// Defines and globals for Sobel filter processing demo
//*****************************************************************************
#define REFRESH_DELAY	  10 //ms

int iBlockDimX = 16;
int iBlockDimY = 4;
float fThresh = 80.0f;             // min display value for output 

// Global declarations
//*****************************************************************************
// Image data vars
const char* cImageFile = "StoneRGB.ppm";
unsigned int uiImageWidth = 1920;   // Image width
unsigned int uiImageHeight = 1080;  // Image height

// OpenGL and Display Globals
int iGLUTWindowHandle;              // handle to the GLUT window
int iGLUTMenuHandle;                // handle to the GLUT menu
int iGraphicsWinPosX = 0;           // GL Window X location
int iGraphicsWinPosY = 0;           // GL Window Y location
int iGraphicsWinWidth = 1024;       // GL Window width
int iGraphicsWinHeight = ((float)uiImageHeight / (float)uiImageWidth) * iGraphicsWinWidth;  // GL Windows height
float fZoom = 1.0f;                 // pixel display zoom   
int iFrameCount = 0;                // FPS count for averaging
int iFrameTrigger = 90;             // FPS trigger for sampling
int iFramesPerSec = 60;             // frames per second
double dProcessingTime = 0.0;       // Computation time accumulator
bool bFullScreen = false;           // state var for full screen mode or not

// app configuration parms
const char* cProcessor [] = {"OpenCL GPU", "Host C++ CPU"};
bool bFilter = true;                // state var for whether filter is enaged or not
int iProcFlag = 0;                  // 0 = GPU, 1 = CPU
shrBOOL bNoPrompt = shrFALSE;		// false = normal GL loop, true = Finite period of GL loop (a few seconds)
shrBOOL bQATest = shrFALSE;			// false = normal GL loop, true = run No-GL test sequence
int iTestSets = 3;                  // # of loop set retriggers before auto exit when bNoPrompt = shrTrue     

// OpenCL vars
const char* clSourcefile = "SobelFilter.cl";  // OpenCL kernel source file
char* cPathAndName = NULL;          // var for full paths to data, src, etc.
char* cSourceCL = NULL;             // Buffer to hold source for compilation 
cl_platform_id cpPlatform;          // OpenCL platform
cl_context cxGPUContext;            // OpenCL context
cl_command_queue* cqCommandQueue;   // OpenCL command queue array
cl_uint* uiInHostPixOffsets;        // Host input buffer pixel offset for image portion worked on by each device 
cl_uint* uiOutHostPixOffsets;       // Host output buffer pixel offsets for image portion worked on by each device 
cl_program cpProgram;               // OpenCL program
cl_kernel* ckSobel;                 // OpenCL Kernel array for Sobel
cl_mem cmPinnedBufIn;               // OpenCL host memory input buffer object:  pinned 
cl_mem cmPinnedBufOut;              // OpenCL host memory output buffer object:  pinned
cl_mem* cmDevBufIn;                 // OpenCL device memory input buffer object  
cl_mem* cmDevBufOut;                // OpenCL device memory output buffer object
cl_uint* uiInput = NULL;            // Mapped Pointer to pinned Host input buffer for host processing
cl_uint* uiOutput = NULL;           // Mapped Pointer to pinned Host output buffer for host processing
size_t szBuffBytes;                 // Size of main image buffers
size_t* szAllocDevBytes;            // Array of Sizes of device buffers
cl_uint* uiDevImageHeight;          // Array of heights of Image portions for each device
size_t szGlobalWorkSize[2];         // 2D global work items (ND range) for Median kernel
size_t szLocalWorkSize[2];          // 2D local work items (work group) for Median kernel
size_t szParmDataBytes;			    // Byte size of context information
size_t szKernelLength;			    // Byte size of kernel code
cl_int ciErrNum;			        // Error code var
const char* cExecutableName = NULL;

// MultiGPU helper obj
DeviceManager* GpuDevMngr; 

// Forward Function declarations
//*****************************************************************************
// OpenCL functionality
double SobelFilterGPU(unsigned int* uiInputImage, unsigned int* uiOutputImage);

// Helpers
void TestNoGL();
void TriggerFPSUpdate();
void GetDeviceLoadProportions(float* fLoadProportions, cl_device_id* cdDevices, cl_uint uiDevCount);
void ShowMenuItems();
void Cleanup(int iExitCode);
void (*pCleanup)(int) = &Cleanup;

// Main program
//*****************************************************************************
int main(int argc, char** argv)
{
    //Get the NVIDIA platform if available, otherwise use default
    char cBuffer[1024];
    bool bNV = false;
    shrLog("Get Platform ID... ");
    ciErrNum = oclGetPlatformID(&cpPlatform);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
    ciErrNum = clGetPlatformInfo (cpPlatform, CL_PLATFORM_NAME, sizeof(cBuffer), cBuffer, NULL);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
    shrLog("%s\n\n", cBuffer);
    bNV = (strstr(cBuffer, "NVIDIA") != NULL);

    //Get the devices
    shrLog("Get Device Info...\n");
    cl_uint uiNumAllDevs = 0;
    GpuDevMngr = new DeviceManager(cpPlatform, &uiNumAllDevs, pCleanup);

    // Get selected device if specified, otherwise examine avaiable ones and choose by perf
    cl_int iSelectedDevice = 0;

    if (1)
    {
        // Use available useful devices and Compute the device load proportions
        ciErrNum = GpuDevMngr->GetDevLoadProportions(bNV);
        oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
        if (GpuDevMngr->uiUsefulDevCt == 1)
        {
            iSelectedDevice = GpuDevMngr->uiUsefulDevs[0];
        }
        shrLog("    Using %u Device(s) for Sobel Filter Computation\n", GpuDevMngr->uiUsefulDevCt); 
    }

    //Create the context
    shrLog("\nclCreateContext...\n\n");
    cxGPUContext = clCreateContext(0, uiNumAllDevs, GpuDevMngr->cdDevices, NULL, NULL, &ciErrNum);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);

    // Allocate per-device OpenCL objects for useful devices
    cqCommandQueue = new cl_command_queue[GpuDevMngr->uiUsefulDevCt];
    ckSobel = new cl_kernel[GpuDevMngr->uiUsefulDevCt];
    cmDevBufIn = new cl_mem[GpuDevMngr->uiUsefulDevCt];
    cmDevBufOut = new cl_mem[GpuDevMngr->uiUsefulDevCt];
    szAllocDevBytes = new size_t[GpuDevMngr->uiUsefulDevCt];
    uiInHostPixOffsets = new cl_uint[GpuDevMngr->uiUsefulDevCt];
    uiOutHostPixOffsets = new cl_uint[GpuDevMngr->uiUsefulDevCt];
    uiDevImageHeight = new cl_uint[GpuDevMngr->uiUsefulDevCt];

    // Create command queue(s) for device(s)     
    shrLog("clCreateCommandQueue...\n");
    for (cl_uint i = 0; i < GpuDevMngr->uiUsefulDevCt; i++) 
    {
        cqCommandQueue[i] = clCreateCommandQueue(cxGPUContext, GpuDevMngr->cdDevices[GpuDevMngr->uiUsefulDevs[i]], 0, &ciErrNum);
        oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
        shrLog("  CommandQueue %u, Device %u, Device Load Proportion = %.2f, ", i, GpuDevMngr->uiUsefulDevs[i], GpuDevMngr->fLoadProportions[i]); 
        oclPrintDevName(LOGBOTH, GpuDevMngr->cdDevices[GpuDevMngr->uiUsefulDevs[i]]);  
        shrLog("\n");
    }

    // Allocate pinned input and output host image buffers:  mem copy operations to/from pinned memory is much faster than paged memory
    szBuffBytes = uiImageWidth * uiImageHeight * sizeof (unsigned int);
    cmPinnedBufIn = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, szBuffBytes, NULL, &ciErrNum);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
    cmPinnedBufOut = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, szBuffBytes, NULL, &ciErrNum);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
    shrLog("\nclCreateBuffer (Input and Output Pinned Host buffers)...\n"); 

    // Get mapped pointers for writing to pinned input and output host image pointers 
    uiInput = (cl_uint*)clEnqueueMapBuffer(cqCommandQueue[0], cmPinnedBufIn, CL_TRUE, CL_MAP_WRITE, 0, szBuffBytes, 0, NULL, NULL, &ciErrNum);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
    uiOutput = (cl_uint*)clEnqueueMapBuffer(cqCommandQueue[0], cmPinnedBufOut, CL_TRUE, CL_MAP_READ, 0, szBuffBytes, 0, NULL, NULL, &ciErrNum);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
    shrLog("clEnqueueMapBuffer (Pointer to Input and Output pinned host buffers)...\n"); 

    fgetc(stdin);

    // Load image data from file to pinned input host buffer
    ciErrNum = shrLoadPPM4ub("tmp.ppm", (unsigned char **)&uiInput, &uiImageWidth, &uiImageHeight);
    oclCheckErrorEX(ciErrNum, shrTRUE, pCleanup);
    shrLog("Load Input Image to Input pinned host buffer...\n"); 

    // Read the kernel in from file
    cSourceCL = oclLoadProgSource("SobelFilter.cl", "// My comment\n", &szKernelLength);
    oclCheckErrorEX(cSourceCL != NULL, shrTRUE, pCleanup);
    shrLog("Load OpenCL Prog Source from File...\n"); 

    // Create the program object
    cpProgram = clCreateProgramWithSource(cxGPUContext, 1, (const char **)&cSourceCL, &szKernelLength, &ciErrNum);
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
    shrLog("clCreateProgramWithSource...\n"); 

    // Build the program with 'mad' Optimization option
#ifdef MAC
    char *flags = "-cl-fast-relaxed-math -DMAC";
#else
    char *flags = "-cl-fast-relaxed-math";
#endif

    ciErrNum = clBuildProgram(cpProgram, 0, NULL, flags, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        // On error: write out standard error, Build Log and PTX, then cleanup and exit
        shrLogEx(LOGBOTH | ERRORMSG, ciErrNum, STDERROR);
        oclLogBuildInfo(cpProgram, oclGetFirstDev(cxGPUContext));
        oclLogPtx(cpProgram, oclGetFirstDev(cxGPUContext), "oclSobelFilter.ptx");
        shrQAFinish(argc, (const char **)argv, QA_FAILED);
        Cleanup(EXIT_FAILURE);
    }
    shrLog("clBuildProgram...\n\n"); 

    // Determine, the size/shape of the image portions for each dev and create the device buffers
    unsigned uiSumHeight = 0;
    for (cl_uint i = 0; i < GpuDevMngr->uiUsefulDevCt; i++)
    {
        // Create kernel instance
        ckSobel[i] = clCreateKernel(cpProgram, "ckSobel", &ciErrNum);
        oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
        shrLog("clCreateKernel (ckSobel), Device %u...\n", i); 

        // Allocations and offsets for the portion of the image worked on by each device
        if (GpuDevMngr->uiUsefulDevCt == 1)
        {
            // One device processes the whole image with no offset 
            uiDevImageHeight[i] = uiImageHeight; 
            uiInHostPixOffsets[i] = 0;
            uiOutHostPixOffsets[i] = 0;
            szAllocDevBytes[i] = uiDevImageHeight[i] * uiImageWidth * sizeof(cl_uint);
        }
        else if (i == 0)
        {
            // Multiple devices, top stripe zone including topmost row of image:  
            // Over-allocate on device by 1 row 
            // Set offset and size to copy extra 1 padding row H2D (below bottom of stripe)
            // Won't return the last row (dark/garbage row) D2H
            uiInHostPixOffsets[i] = 0;
            uiOutHostPixOffsets[i] = 0;
            uiDevImageHeight[i] = (cl_uint)(GpuDevMngr->fLoadProportions[GpuDevMngr->uiUsefulDevs[i]] * (float)uiImageHeight);     // height is proportional to dev perf 
            uiSumHeight += uiDevImageHeight[i];
            uiDevImageHeight[i] += 1;
            szAllocDevBytes[i] = uiDevImageHeight[i] * uiImageWidth * sizeof(cl_uint);
        }
        else if (i < (GpuDevMngr->uiUsefulDevCt - 1))
        {
            // Multiple devices, middle stripe zone:  
            // Over-allocate on device by 2 rows 
            // Set offset and size to copy extra 2 padding rows H2D (above top and below bottom of stripe)
            // Won't return the first and last rows (dark/garbage rows) D2H
            uiInHostPixOffsets[i] = (uiSumHeight - 1) * uiImageWidth;
            uiOutHostPixOffsets[i] = uiInHostPixOffsets[i] + uiImageWidth;
            uiDevImageHeight[i] = (cl_uint)(GpuDevMngr->fLoadProportions[GpuDevMngr->uiUsefulDevs[i]] * (float)uiImageHeight);     // height is proportional to dev perf 
            uiSumHeight += uiDevImageHeight[i];
            uiDevImageHeight[i] += 2;
            szAllocDevBytes[i] = uiDevImageHeight[i] * uiImageWidth * sizeof(cl_uint);
        }
        else 
        {
            // Multiple devices, last boundary tile:  
            // Over-allocate on device by 1 row 
            // Set offset and size to copy extra 1 padding row H2D (above top of stripe)
            // Won't return the first row (dark/garbage rows D2H 
            uiInHostPixOffsets[i] = (uiSumHeight - 1) * uiImageWidth;
            uiOutHostPixOffsets[i] = uiInHostPixOffsets[i] + uiImageWidth;
            uiDevImageHeight[i] = uiImageHeight - uiSumHeight;                              // "leftover" rows 
            uiSumHeight += uiDevImageHeight[i];
            uiDevImageHeight[i] += 1;
            szAllocDevBytes[i] = uiDevImageHeight[i] * uiImageWidth * sizeof(cl_uint);
        }
        shrLog("Image Height (rows) for Device %u = %u...\n", i, uiDevImageHeight[i]); 

        // Create the device buffers in GMEM on each device
        cmDevBufIn[i] = clCreateBuffer(cxGPUContext, CL_MEM_READ_ONLY, szAllocDevBytes[i], NULL, &ciErrNum);
        oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
        cmDevBufOut[i] = clCreateBuffer(cxGPUContext, CL_MEM_WRITE_ONLY, szAllocDevBytes[i], NULL, &ciErrNum);
        oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
        shrLog("clCreateBuffer (Input and Output GMEM buffers, Device %u)...\n", i); 

        // Set the common argument values for the Median kernel instance for each device
        int iLocalPixPitch = iBlockDimX + 2;
        ciErrNum = clSetKernelArg(ckSobel[i], 0, sizeof(cl_mem), (void*)&cmDevBufIn[i]);
        ciErrNum |= clSetKernelArg(ckSobel[i], 1, sizeof(cl_mem), (void*)&cmDevBufOut[i]);
        ciErrNum |= clSetKernelArg(ckSobel[i], 2, (iLocalPixPitch * (iBlockDimY + 2) * sizeof(cl_uchar4)), NULL);
        ciErrNum |= clSetKernelArg(ckSobel[i], 3, sizeof(cl_int), (void*)&iLocalPixPitch);
        ciErrNum |= clSetKernelArg(ckSobel[i], 4, sizeof(cl_uint), (void*)&uiImageWidth);
        ciErrNum |= clSetKernelArg(ckSobel[i], 6, sizeof(cl_float), (void*)&fThresh);        
        oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);
        shrLog("clSetKernelArg (0-4), Device %u...\n\n", i); 
    }

    // Set common global and local work sizes for Median kernel
    szLocalWorkSize[0] = iBlockDimX;
    szLocalWorkSize[1] = iBlockDimY;
    szGlobalWorkSize[0] = shrRoundUp((int)szLocalWorkSize[0], uiImageWidth); 

    // init running timers
    shrDeltaT(0);   // timer 0 used for computation timing 
    shrDeltaT(1);   // timer 1 used for fps computation

    //TestNoGL();

    SobelFilterGPU (uiInput, uiOutput);
    shrSavePPM4ub("out.ppm", (unsigned char *)uiOutput, uiImageWidth, uiImageHeight);

    //shrQAFinish(argc, (const char **)argv, QA_PASSED);

    //Cleanup(EXIT_SUCCESS);
}

// OpenCL computation function for 1 or more GPUs  
// Copies input data from pinned host buf to the device, runs kernel, copies output data back to pinned output host buf
//*****************************************************************************
double SobelFilterGPU(cl_uint* uiInputImage, cl_uint* uiOutputImage)
{
    // If this is a video application, fresh data in pinned host buffer is needed beyond here 
    //      This line could be a sync point assuring that an asynchronous acqusition is complete.
    //      That ascynchronous acquisition would do a map, update and unmap for the pinned input buffer
    //
    //      Otherwise a synchronous acquisition call ('get next frame') could be placed here, but that would be less optimal.

    // For each device: copy fresh input H2D 
    ciErrNum = CL_SUCCESS;
    for (cl_uint i = 0; i < GpuDevMngr->uiUsefulDevCt; i++)
    {
        // Nonblocking Write of input image data from host to device
        ciErrNum |= clEnqueueWriteBuffer(cqCommandQueue[i], cmDevBufIn[i], CL_FALSE, 0, szAllocDevBytes[i], 
                                        (void*)&uiInputImage[uiInHostPixOffsets[i]], 0, NULL, NULL);
    }

    // Sync all queues to host and start computation timer on host to get computation elapsed wall clock  time
    // (Only for timing... can be omitted in a production app)
    for (cl_uint j = 0; j < GpuDevMngr->uiUsefulDevCt; j++)
    {
        ciErrNum |= clFinish(cqCommandQueue[j]);
    }
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);

    // For each device: Process
    shrDeltaT(0);
    for (cl_uint i = 0; i < GpuDevMngr->uiUsefulDevCt; i++)
    {
        // Determine configuration bytes, offsets and launch config, based on position of device region vertically in image
        if (GpuDevMngr->uiUsefulDevCt == 1)
        {
            // One device processes the whole image with no offset tricks needed
            szGlobalWorkSize[1] = shrRoundUp((int)szLocalWorkSize[1], (int)uiDevImageHeight[i]);
        }
        else if (i == 0)
        {
            // Multiple devices, top boundary tile:  
            // Process whole device allocation, including extra row 
            // No offset, but don't return the last row (dark/garbage row) D2H 
            szGlobalWorkSize[1] = shrRoundUp((int)szLocalWorkSize[1], (int)uiDevImageHeight[i]);
        }
        else if (i < (GpuDevMngr->uiUsefulDevCt - 1))
        {
            // Multiple devices, middle tile:  
            // Process whole device allocation, including extra 2 rows 
            // Offset down by 1 row, and don't return the first and last rows (dark/garbage rows) D2H 
            szGlobalWorkSize[1] = shrRoundUp((int)szLocalWorkSize[1], (int)uiDevImageHeight[i]);
        }
        else 
        {   
            // Multiple devices, last boundary tile:  
            // Process whole device allocation, including extra row 
            // Offset down by 1 row, and don't return the first row (dark/garbage row) D2H 
            szGlobalWorkSize[1] = shrRoundUp((int)szLocalWorkSize[1], (int)uiDevImageHeight[i]);
        }

        // Pass in dev image height (# of rows worked on) for this device
        ciErrNum |= clSetKernelArg(ckSobel[i], 5, sizeof(cl_uint), (void*)&uiDevImageHeight[i]);

        // Launch Sobel kernel(s) into queue(s) and push to device(s)
        ciErrNum |= clEnqueueNDRangeKernel(cqCommandQueue[i], ckSobel[i], 2, NULL, szGlobalWorkSize, szLocalWorkSize, 0, NULL, NULL);

        // Push to device(s) so subsequent clFinish in queue 0 doesn't block driver from issuing enqueue command for higher queues
        ciErrNum |= clFlush(cqCommandQueue[i]);
    }

    // Sync all queues to host and get elapsed wall clock time for computation in all queues
    // (Only for timing... can be omitted in a production app)
    for (cl_uint j = 0; j < GpuDevMngr->uiUsefulDevCt; j++)
    {
        ciErrNum |= clFinish(cqCommandQueue[j]);
    }
    double dKernelTime = shrDeltaT(0); // Time from launch of first compute kernel to end of all compute kernels 
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);

    // For each device: copy fresh output D2H
    for (cl_uint i = 0; i < GpuDevMngr->uiUsefulDevCt; i++)
    {
        // Determine configuration bytes and offsets based on position of device region vertically in image
        size_t szReturnBytes;
        cl_uint uiOutDevByteOffset;        
        if (GpuDevMngr->uiUsefulDevCt == 1)
        {
            // One device processes the whole image with no offset tricks needed
            szReturnBytes = szBuffBytes;
            uiOutDevByteOffset = 0;
        } 
        else if (i == 0)
        {
            // Multiple devices, top boundary tile:  
            // Process whole device allocation, including extra row 
            // No offset, but don't return the last row (dark/garbage row) D2H 
            szReturnBytes = szAllocDevBytes[i] - (uiImageWidth * sizeof(cl_uint));
            uiOutDevByteOffset = 0;
        }
        else if (i < (GpuDevMngr->uiUsefulDevCt - 1))
        {
            // Multiple devices, middle tile:  
            // Process whole device allocation, including extra 2 rows 
            // Offset down by 1 row, and don't return the first and last rows (dark/garbage rows) D2H 
            szReturnBytes = szAllocDevBytes[i] - ((uiImageWidth * sizeof(cl_uint)) * 2);
            uiOutDevByteOffset = uiImageWidth * sizeof(cl_uint);
        }        
        else 
        {   
            // Multiple devices, last boundary tile:  
            // Process whole device allocation, including extra row 
            // Offset down by 1 row, and don't return the first row (dark/garbage row) D2H 
            szReturnBytes = szAllocDevBytes[i] - (uiImageWidth * sizeof(cl_uint));
            uiOutDevByteOffset = uiImageWidth * sizeof(cl_uint);
        }        
        
        // Non Blocking Read of output image data from device to host 
        ciErrNum |= clEnqueueReadBuffer(cqCommandQueue[i], cmDevBufOut[i], CL_FALSE, uiOutDevByteOffset, szReturnBytes, 
                                       (void*)&uiOutputImage[uiOutHostPixOffsets[i]], 0, NULL, NULL);
    }

    // Finish all queues and check for errors before returning 
    // The block here assures valid output data for subsequent host processing
    for (cl_uint j = 0; j < GpuDevMngr->uiUsefulDevCt; j++)
    {
        ciErrNum |= clFinish(cqCommandQueue[j]);
    }
    oclCheckErrorEX(ciErrNum, CL_SUCCESS, pCleanup);

    return dKernelTime;
}

// Run a test sequence without any GL 
//*****************************************************************************
void TestNoGL()
{
    // Warmup call to assure OpenCL driver is awake
    // note this function has a finish for all queues at its end, so no further host sync is needed
    SobelFilterGPU (uiInput, uiOutput);
    shrSavePPM4ub("out.ppm", (unsigned char *)uiOutput, uiImageWidth, uiImageHeight);

    // Start timer 0 and process n loops on the GPU
    const int iCycles = 150;
    dProcessingTime = 0.0;
    shrLog("\nRunning SobelFilterGPU for %d cycles...\n\n", iCycles);
    shrDeltaT(2); 
    for (int i = 0; i < iCycles; i++)
    {
        // note this function has a finish for all queues at its end, so no further host sync is needed
        dProcessingTime += SobelFilterGPU (uiInput, uiOutput);
    }

    // Get round-trip and average computation time
    double dRoundtripTime = shrDeltaT(2)/(double)iCycles;
    dProcessingTime /= (double)iCycles;

    // log throughput, timing and config info to sample and master logs
    shrLogEx(LOGBOTH | MASTER, 0, "oclSobelFilter, Throughput = %.4f M RGB Pixels/s, Time = %.5f s, Size = %u RGB Pixels, NumDevsUsed = %u, Workgroup = %u\n", 
           (1.0e-6 * uiImageWidth * uiImageHeight)/dProcessingTime, dProcessingTime, (uiImageWidth * uiImageHeight), GpuDevMngr->uiUsefulDevCt, szLocalWorkSize[0] * szLocalWorkSize[1]); 
    shrLog("\nRoundTrip Time = %.5f s, Equivalent FPS = %.1f\n\n", dRoundtripTime, 1.0/dRoundtripTime);

    // Compute on host 
    cl_uint* uiGolden = (cl_uint*)malloc(szBuffBytes);
    SobelFilterHost(uiInput, uiGolden, uiImageWidth, uiImageHeight, fThresh);

    // Compare GPU and Host results:  Allow variance of 1 GV in up to 0.01% of pixels 
    shrLog("Comparing GPU Result to CPU Result...\n"); 
    shrBOOL bMatch = shrCompareuit(uiGolden, uiOutput, (uiImageWidth * uiImageHeight), 1.0f, 0.0001f);
    shrLog("\nGPU Result %s CPU Result within tolerance...\n", (bMatch == shrTRUE) ? "matches" : "DOESN'T match"); 

    // Cleanup and exit
    free(uiGolden);
    shrQAFinish2(true, *pArgc, (const char **)pArgv, (bMatch == shrTRUE) ? QA_PASSED : QA_FAILED);
    Cleanup((bMatch == shrTRUE) ? EXIT_SUCCESS : EXIT_FAILURE);
}


// Function to clean up and exit
//*****************************************************************************
void Cleanup(int iExitCode)
{
    // Cleanup allocated objects
    shrLog("\nStarting Cleanup...\n\n");

    // Release all the OpenCL Objects
    if(cpProgram)clReleaseProgram(cpProgram);
    for (cl_uint i = 0; i < GpuDevMngr->uiUsefulDevCt; i++)
    {
        if(ckSobel[i])clReleaseKernel(ckSobel[i]);
        if(cmDevBufIn[i])clReleaseMemObject(cmDevBufIn[i]);
        if(cmDevBufOut[i])clReleaseMemObject(cmDevBufOut[i]);
    }
    if(uiInput)clEnqueueUnmapMemObject(cqCommandQueue[0], cmPinnedBufIn, (void*)uiInput, 0, NULL, NULL);
    if(uiOutput)clEnqueueUnmapMemObject(cqCommandQueue[0], cmPinnedBufOut, (void*)uiOutput, 0, NULL, NULL);
    if(cmPinnedBufIn)clReleaseMemObject(cmPinnedBufIn);
    if(cmPinnedBufOut)clReleaseMemObject(cmPinnedBufOut);
    for (cl_uint i = 0; i < GpuDevMngr->uiUsefulDevCt; i++)
    {
        if(cqCommandQueue[i])clReleaseCommandQueue(cqCommandQueue[i]);
    }
    if(cxGPUContext)clReleaseContext(cxGPUContext);

    // free the host allocs
    if(cSourceCL)free(cSourceCL);
    if(cPathAndName)free(cPathAndName);
    if(cmDevBufIn) delete [] cmDevBufIn;
    if(cmDevBufOut) delete [] cmDevBufOut;
    if(szAllocDevBytes) delete [] szAllocDevBytes;
    if(uiInHostPixOffsets) delete [] uiInHostPixOffsets;
    if(uiOutHostPixOffsets) delete [] uiOutHostPixOffsets;
    if(uiDevImageHeight) delete [] uiDevImageHeight;
    if(GpuDevMngr) delete GpuDevMngr;
    if(cqCommandQueue) delete [] cqCommandQueue;

    shrLogEx(LOGBOTH | CLOSELOG, 0, "%s Exiting...\n", cExecutableName);
    exit (iExitCode);
}
