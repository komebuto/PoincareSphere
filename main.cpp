#include <iostream>
#include <windows.h>
#include <time.h>

#include <dvcAPI.h>

#include "sequence.hpp"

#include <opencv2\opencv.hpp>

using namespace std;
using namespace cv;

Region* rFocus;

int main(int argc, char* argv[]) {
    // Initialization
    int nBuffers = NBUFFERS;
    rFocus = (Region*)calloc(1, sizeof(Region));
    pHandles = (Handles*)calloc(1, sizeof(Handles));

    // open and reset camera
    HANDLE hDevice = dvcOpenCamera(1);
    if ( hDevice == INVALID_HANDLE_VALUE ) {
        fprintf(stderr, "No DVC cameras found ?\n");
        return -1;
    }

    if ( !dvcResetCamera(hDevice) ) {
        fprintf(stderr,"dvcResetCamera:: Error %d %s\n",
                    dvcGetLastErr(),
                    dvcGetLastErrMsg());
        dvcCloseCamera(hDevice);
        return -1 ;
    }
    
    rFocus->set(0, dvcGetXDim(hDevice), 0, dvcGetYDim(hDevice));

    parseCmdLine(hDevice, argc, argv, rFocus, 1);

#ifdef USE_PIEZO
    PIconnectUSB();
#endif

    if ( !grabRingBuffer(hDevice, nBuffers, rFocus, 0) ) {
        fprintf(stderr, "Grab-Ring-Buffer Failed Error %d %s ?\n",
                    dvcGetLastErr(), dvcGetLastErrMsg());
    }

    while (rFocus) {
        Region* tmp = rFocus->next;
        delete rFocus;
        rFocus = tmp;
    }

    delete pHandles;

    dvcCloseCamera(hDevice);


    return 0;
}
