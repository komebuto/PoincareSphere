#ifndef __INCLUDE_SEQUENCE_HPP_
#define __INCLUDE_SEQUENCE_HPP_

#include <windows.h>
#include <fstream>
#include <string.h>
#include <opencv2\opencv.hpp>
#include <opencv2\core\types_c.h>
#include <opencv2\core\mat.hpp>
#include <opencv2\highgui\highgui.hpp>

#define NBUFFERS 5
#define ENTER 13   // Enter-key
#define ESC 27     // Esc-key
#define OPTION 111 // 'o'-key
#define RADD_FLAG "r+"
#define RDEL_FLAG "r-"
#define FOPEN_FLAG "fopen"
#define FCLOSE_FLAG "fclose"
#define PIEZO_FLAG "p"
#define PV_MAX 4096
#define IPV_MAX 1/PV_MAX
#define WINDOW_NAME "View"

// * comment out when using piezo
//#define USE_PIEZO

#define CLOSE(message) \
    std::cerr << #message << endl; \
    dvcReleaseUserBuffers(&bufS); \
    dvcSetExposeCompleteEvent(hDevice, NULL); \
    dvcSetReadCompleteEvent(hDevice, NULL); \
    CloseHandle(hExpose); \
    CloseHandle(hRead);


using namespace cv;

__declspec(selectany) Rect_<int> rectangle_value;
__declspec(selectany) int xa_g, xb_g, ya_g, yb_g;
__declspec(selectany) char fname[64];
__declspec(selectany) std::ofstream file;
__declspec(selectany) BOOL isFileOpen = FALSE;
__declspec(selectany) int nCaptured = 1;
__declspec(selectany) int ID;

class Handles {
public:
    HANDLE* phDevice;
    HANDLE* phExpose;
    HANDLE* phRead;

    void set(HANDLE* hDevice, HANDLE* hExpose, HANDLE* hRead) {
        phDevice = hDevice;
        phExpose = hExpose;
        phRead   = hRead;
    }

    void get(HANDLE* hDevice, HANDLE* hExpose, HANDLE* hRead) {
        *hDevice = *phDevice;
        *hExpose = *phExpose;
        *hRead   = *phRead;
    }
};

__declspec(selectany) Handles* pHandles;

typedef enum {
    R_REFERENCE,
    R_TARGET,
} RegionKind;

class Region : public Rect {    
public:
    RegionKind kind;
    Point2i pt1;
    Point2i pt2;
    Region* next;
    float Imax;

    Region* operator[] (int k) {
        Region* tmp = this;
        for (int i = 0; i < k; i++) {
            if (!tmp->next) return NULL;
            tmp = tmp->next;
        }
        return tmp;
    }

    Region(int xa, int xb, int ya, int yb, RegionKind kd = R_TARGET) {
        Region::set(xa, xb, ya, yb, kd);
    }
    
    int nPixels() {
        return (pt2.x - pt1.x) * (pt2.y - pt1.y);
    }

    void set(int xa, int xb, int ya, int yb, RegionKind kd = R_TARGET) {
        if (xa < xb) { pt1.x = xa;  pt2.x = xb; }
        else { pt1.x = xb;  pt2.x = xa; }
        if (ya < yb) { pt1.y = ya;  pt2.y = yb; }
        else { pt1.y = yb;  pt2.y = ya; }
        kind = kd;
        Imax = 0;
    }

    void get(int* xm, int* xM, int* ym, int* yM) {
        *xm = pt1.x;
        *xM = pt2.x;
        *ym = pt1.y;
        *yM = pt2.y;
    }

    Region* end() {
        Region* tmp;
        for (tmp = this; tmp->next; tmp = tmp->next) ;
        return tmp;
    }
};

int split(char* dst[], char* src, char delim);
BOOL key_interrupt(char hit, HANDLE hDevice, dvcBufStruct bufS, int nCurrentBuffer, Region* rFocus);
void parseCmdLine(HANDLE hDevice, int argc, char* argv[],  Region* rSee, int start);
void mouse_callback(int event, int x, int y, int flags, void *userdata);
void mouse_interrupt(Region* rFocus, Mat* pimg, char isClick);

BOOL acquire_picture(Handles* pHandles, int* nCurrentBuffer);

void getI(PUSHORT pBuffer, Region* rFocus, float* nValue);

int PIconnectUSB(void);
void move_findImax(Region* rFocus, dvcBufStruct bufS);
int move_Iref(float Iref, Region* rFocus, dvcBufStruct bufS);
void move_feedback(float Icurr, Region* rFocus, int isDescending);

BOOL grabRingBuffer(HANDLE hDevice, int nBuffers, Region* rSee, int call);

// get elapsed time for profiling
void showtime(time_t t, const char* name);

#endif