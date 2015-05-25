#include <stdio.h>
#include <stdlib.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#endif /* __linux __ */

#ifdef __WIN32
#include <windows.h>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")
#endif /* __WIN32 */

#define MAX_DEVICES 32

int main(int argc, char *argv[])
{
    const char *device_names[MAX_DEVICES];
    int amt_cameras = 0;
    int i = 0;

    #ifdef __linux__
    // Enumerate video capture devices
    while(i <= MAX_DEVICES)
    {
        int fd;
        struct v4l2_capability video_cap;
        char device_address[256];
        snprintf(device_address, sizeof device_address, "/dev/video%d",i);

        if((fd = open(device_address, O_RDONLY)) == -1) {
            break;
        }
        else {
            // Query capture device information
            if(ioctl(fd, VIDIOC_QUERYCAP, &video_cap) == -1)
                perror("cam_info: Can't get capabilities");
            else {
                //printf("Card:\t\t '%s'\n", video_cap.card);
                int name_length = sizeof(video_cap.card);
                char* name = (char*)malloc(name_length+1);
                name = video_cap.card;
                device_names[i] = name;
            }
            close(fd);
            amt_cameras = i;
        }
        ++i;
    }
    #endif /* __linux__ */
    
    #ifdef __WIN32
    HRESULT hr;
    CoInitialize(NULL);
    ICreateDevEnum *pSysDevEnum = NULL;
    hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, &IID_ICreateDevEnum, (void**)&pSysDevEnum);
    if(FAILED(hr)) {
        printf("CoCreateInstance failed()\n");
    }
    // Obtain a class enumerator for the video compressor category.
    IEnumMoniker *pEnumCat = NULL;
    hr = pSysDevEnum->lpVtbl->CreateClassEnumerator(pSysDevEnum, &CLSID_VideoInputDeviceCategory, &pEnumCat, 0);
    if(hr != S_OK) {
        pSysDevEnum->lpVtbl->Release(pSysDevEnum);
        printf("CreateClassEnumerator failed()\n");
    }

    IMoniker *pMoniker = NULL;

    ULONG cFetched;
    while(pEnumCat->lpVtbl->Next(pEnumCat, 1, &pMoniker, &cFetched) == S_OK && i <= MAX_DEVICES) {
        IPropertyBag *pPropBag;
        hr = pMoniker->lpVtbl->BindToStorage(pMoniker, 0, 0, &IID_IPropertyBag, (void **)&pPropBag);
        if(SUCCEEDED(hr)) {
            // To retrieve the filter's friendly name, do the following:
            VARIANT varName;
            VariantInit(&varName);
            hr = pPropBag->lpVtbl->Read(pPropBag, L"FriendlyName", &varName, 0);
            if (SUCCEEDED(hr)) {
                if(varName.vt == VT_BSTR) {
                    //printf("friendly name: %ls\n", varName.bstrVal);
                    int name_length = wcslen(varName.bstrVal);
                    char* name = (char*)malloc(name_length + 1);
                    wcstombs(name, varName.bstrVal, name_length + 1);
                    device_names[i] = name;
                    //free(name);
                } else {
                    //printf("unfriendly name\n");
                    device_names[i] = "Unknown Device";
                }
                ++i;
            }
            
            VariantClear(&varName);
            pPropBag->lpVtbl->Release(pPropBag);
        }
        pMoniker->lpVtbl->Release(pMoniker);
    }
    amt_cameras = i-1;
    pEnumCat->lpVtbl->Release(pEnumCat);
    pSysDevEnum->lpVtbl->Release(pSysDevEnum);
    
    #endif /* __WIN32 */

    // TODO: Windows video capture enumeration

    // Obtain user camera selection
    int select_camera = -1;
    while(select_camera < 0 || select_camera > amt_cameras || select_camera >= MAX_DEVICES) {
        printf("Select a camera from %d cameras...\n", amt_cameras+1);
        int i = 0;
        // Print all collected devices
        while(i <= amt_cameras)
        {
            printf("%d: %s\n", i, device_names[i]);
            ++i;
        }
        printf("\nCamera #:");
        scanf(" %d", &select_camera);
    }
    printf("\nPress spacebar to exit.\n\n");

    // Initialize window
    cvNamedWindow("Camera", 1);
    // Initialize camera
    CvCapture* capture = cvCreateCameraCapture(select_camera);
    char key = 0;
    // Run until spacebar is pressed
    while(key != 32 && capture != NULL) {
        // Obtains frame from camera and show it on window
        IplImage* frame = cvQueryFrame(capture);
        cvShowImage("Camera", frame);
        key = cvWaitKey(10);
    } 
    
    // Free device list allocations
    i = 0;
    while(i <= amt_cameras)
    {
        free(device_names[i]);
        ++i;
    }
    
    // Collapse camera and window
    cvReleaseCapture(&capture);
    cvDestroyWindow("Camera");

    return 0;
}