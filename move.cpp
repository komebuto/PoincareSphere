#include <iostream>
#include <cmath>
#include <windows.h>
#include <dvcAPI.h>
#include <E816_DLL.h>

#include "sequence.hpp"

#define AXIS "A"
#define LAMBDA 1053
#define EPS 1e-1

double step[] = {1.0};

int PIconnectUSB() 
{
    int ID;
	char szDevices[1000];
	int nrDevices = E816_EnumerateUSB(szDevices, 999, NULL);
	
    if (nrDevices<=0)
	{
		printf("No devices connected to USB");
		return NULL;
	}

    char** context;
	char* p = strtok_s(szDevices, "\n", context);
	
    printf("Found %d devices, connecting to first: \"%s\"\n", nrDevices, szDevices);
	ID = E816_ConnectUSB(szDevices);

    // turn on servo
    BOOL t[] = {TRUE};
    E816_SVO(ID, AXIS, t);

    return ID;
}

void move_toOrigin()
{
    double origin[] = {0.0};
    E816_MOV(ID, AXIS, origin);
}

// move piezo and get Imax at the region
void move_findImax(Region* rFocus, dvcBufStruct bufS)
{
    float I;
    int nCurrentBuffer;
    PUSHORT pBuffer;

    move_toOrigin();
    for (double z = 0.0; z <= LAMBDA; z += step[0]) {
        
        acquire_picture(pHandles, &nCurrentBuffer);
        pBuffer = bufS.pBuffers[nCurrentBuffer];
        getI(pBuffer, rFocus, &I);
        
        if (rFocus->Imax < I) {
            rFocus->Imax = I;
        }

        E816_MVR(ID, AXIS, step);
    }
}

// get piezo position where I equals to Iref
// return NULL if not found
// return  1 if I is decreasing compared to previous step
// return -1 if I is increasing compared to previous step
int move_Iref(float Iref, Region* rFocus, dvcBufStruct bufS)
{
    float I = 0.0, Iprev = NULL;
    int nCurrentBuffer;
    PUSHORT pBuffer;

    int isDescending = 0;

    move_toOrigin();
    for (double z = 0.0; z <= LAMBDA; z += step[0]) {
        acquire_picture(pHandles, &nCurrentBuffer);
        pBuffer = bufS.pBuffers[nCurrentBuffer];
        getI(pBuffer, rFocus, &I);

        if (Iprev == NULL)  Iprev = I;
        else if (I > Iprev) isDescending = -1;
        else if (I < Iprev) isDescending = 1;
        else                isDescending = 0;

        if (abs(I - Iref) < EPS) return isDescending;

        Iprev = I;
        E816_MVR(ID, AXIS, step);
    }
    return NULL;
}

// move piezo so that Icurr stays around Imax/2
void move_feedback(float Icurr, Region* rFocus, int isDescending)
{   
    float Imax = rFocus->Imax;
    double dz[] = { isDescending * ( sqrt(2.0)/ (LAMBDA*Imax) ) * (Icurr - Imax/2.0) };
    E816_MOV(ID, AXIS, dz);
    return;
}
