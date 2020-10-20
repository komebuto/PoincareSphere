#include <stdio.h>
#include <iostream>
#include <dvcAPI.h>
#include <conio.h>
#include <limits>
#include <windows.h>
#include <sstream>
#include <time.h>
#include <opencv2\opencv.hpp>
#include <opencv2\core\types_c.h>
#include <opencv2\core\mat.hpp>
#include <opencv2\highgui\highgui.hpp>

#include "sequence.hpp"

using namespace std;
using namespace cv;

void showtime(time_t t, const char* name) {
    double time = (double) t * 1000.0 / CLOCKS_PER_SEC;
    std::cerr << name << time << endl;
}

void getI(unsigned short* pBuffer, Region* rFocus, float* I)
{   
    int xm, xM, ym, yM, w;
    rFocus->get(&xm, &xM, &ym, &yM);
    w = xM - xm;
    *I = 0;
    for (int y = ym; y < yM; ++y) {
        for (int x = xm; x < xM; ++x) {
            *I += pBuffer[y*w + x];
        }
    }
}

clock_t t0;
clock_t loop1=0;
clock_t loop2=0;
clock_t loop3=0;

Mat tmp_mat;

// imshow whole region (first element of rFocus)
// for other elements of rFocus, draw rectangle
// so that you know where is on focus.
void show(dvcBufStruct bufS, int nCurrentBuffer, Region* rFocus, Mat* pimg) {
    namedWindow(WINDOW_NAME);
    unsigned short* pBuffer = bufS.pBuffers[nCurrentBuffer];

    int xm, xM, ym, yM, w;
    rFocus->get(&xm, &xM, &ym, &yM);
    w = xM - xm;
    //Mat output(yM - ym, xM - xm, CV_8UC1);

    unsigned char* tmp_buf = (unsigned char*)calloc(1, (yM-ym)*(xM-xm)*sizeof(unsigned char));

    // ! loop1  time consuming (about 1/3 of whole time)
    t0 = clock();
    for (int y = ym; y < yM; ++y) {
        for (int x = xm; x < xM; ++x) {
            tmp_buf[y*w + x] = (unsigned char)(256*(float)pBuffer[y*w + x]*IPV_MAX);
        }
    }
    *pimg = Mat((yM - ym), (xM - xm), CV_8UC1, tmp_buf);
    loop1 += clock() - t0;    


	int ri = 1;
    int ti = 1;
    int depth;
    ostringstream text;

    // * loop2 ()
    t0 = clock();
	for (Region* tmp=rFocus->next; tmp; tmp=tmp->next) {
        
        if (tmp->kind == R_REFERENCE) {
            depth = 100;
            text << "r" << ri++;
        }
        else if (tmp->kind == R_TARGET) {
            depth = 200;
            text << "t" << ti++;
        }

        rectangle(*pimg, tmp->pt1, tmp->pt2, depth);
		putText(*pimg, text.str().c_str(), tmp->pt2, 0, 1, depth); 
        text.str("");
        text.clear();
	}
    loop2 += clock() - t0;
		
    // ! loop3  time consuming (about 1/2 of whole time)
    t0 = clock();
	imshow(WINDOW_NAME, *pimg);
	waitKey(1);
    loop3 += clock() - t0;
	return;
}

void analyze(dvcBufStruct bufS, int nCurrentBuffer, Region* rFocus) {
    float I;

    fprintf(stderr,
        "Buffer: %d, Frame count: %ld, Time %.2f (ms), Status %d ring %ld cyles %ld\n",
        nCurrentBuffer,
        bufS.pMeta[nCurrentBuffer].ulStreamCount,
        dvcElapseTime(bufS.pMeta[nCurrentBuffer].dFrameTime),
        bufS.pBufferStatus[nCurrentBuffer],
        bufS.pMeta[nCurrentBuffer].ulRingBuffer,
        bufS.pMeta[nCurrentBuffer].ulBufferWriteCount) ;

    fprintf(stderr, "Exposure: %.2f (ms)\n", bufS.pMeta[nCurrentBuffer].dExposeTime);

    for (Region* tmp=rFocus; tmp; tmp=tmp->next) {
        getI(bufS.pBuffers[nCurrentBuffer], tmp, &I);
        fprintf(stderr, "Pixel values' mean in region from (%d, %d) to (%d, %d): %f\n", 
                    tmp->pt1.x, tmp->pt1.y, tmp->pt2.x, tmp->pt2.y, I/(tmp->nPixels()));
    }
}

BOOL acquire_picture(Handles* pHandles, int* pnCurrentBuffer)
{   
    BOOL bRC = TRUE;
    HANDLE hDevice, hExpose, hRead;

    pHandles->get(&hDevice, &hExpose, &hRead);

    // wait exposing
    if(WaitForSingleObject(hExpose,1000) != WAIT_OBJECT_0 ) {  
        fprintf(stderr,"Error waiting expose!\n");
        bRC = FALSE ;
    }

    // wait reading
    if(WaitForSingleObject(hRead, 1000) != WAIT_OBJECT_0 ) {  
        fprintf(stderr,"Error waiting read!\n");
        bRC = FALSE ;
    }
         
    // acquire current buffer No.
    if(!dvcGetUserBufferId(hDevice, pnCurrentBuffer)) { 
        fprintf(stderr,"Error getting buffer id!\n");
        bRC = FALSE ;
    }

    return bRC;
}

// main loop
BOOL grabRingBuffer(HANDLE hDevice, int nBuffers, Region* rFocus, int call) 
{   
    clock_t tWhole = clock();
    dvcBufStruct bufS ;
    int i ;
    HANDLE hExpose, hRead ;
    int nCurrentBuffer ;
    char hit;
	BOOL bRC = TRUE;
    
    bool isClick = false;
    int key;
    Region* tmp;

    int xm, xM, ym, yM;
    rFocus->get(&xm, &xM, &ym, &yM);

    Mat img(yM - ym, xM - xm, CV_8UC1);
    Mat* pimg = &img;

    if(!dvcAllocateUserBuffers(hDevice, &bufS, nBuffers))
    {
        fprintf(stderr,"Error allocating user buffers!\n");
        return FALSE ;
    }

    hExpose = CreateEvent(NULL,FALSE,FALSE,NULL) ;
    hRead = CreateEvent(NULL,FALSE,FALSE,NULL);

    if (!dvcSetExposeCompleteEvent(hDevice, hExpose)) 
        { CLOSE(Error setting ExposeComplete!); return FALSE; }
    if (!dvcSetReadCompleteEvent(hDevice, hRead)) 
        { CLOSE(Error setting ReadComplete!); return FALSE; }
    if (!dvcSetUserBuffers(hDevice, &bufS)) 
        { CLOSE(Error setting User Buffers!); return FALSE; }

    pHandles->set(&hDevice, &hExpose, &hRead);

    bRC = dvcStartSequenceEx(hDevice, 0, 0) ;
    if(!bRC) fprintf(stderr,"Error starting sequence!\n");

    clock_t tAnalyze = 0;
    clock_t tShow = 0;
    clock_t tKey = 0;
    clock_t t0;

    for(i=0; ; i++) 
    {

        bRC = acquire_picture(pHandles, &nCurrentBuffer);

        if(bRC) 
        {
            // analyze using an acquired data
            t0 = clock();
            analyze(bufS, nCurrentBuffer, rFocus);
            tAnalyze += clock() - t0;

            // view
            t0 = clock();
            show(bufS, nCurrentBuffer, rFocus, pimg);
            tShow += clock() - t0;

            // clear the buffer's status
            // to enable refilling..
            bufS.pBufferStatus[nCurrentBuffer] = 0 ;

        }

        cv::setMouseCallback(WINDOW_NAME, mouse_callback, &isClick);
	    if (isClick) {
            Mat draw_img;
            char side = isClick;
            int depth;

            if (side == 'L') depth = 200;
            else if (side == 'R') depth = 100;
 
            key = 0;
            for (tmp = rFocus; tmp->next; tmp = tmp->next) ;
            tmp->next = (Region*)calloc(1, sizeof(Region));
            for (;;) {
		        if (isClick) {
                    draw_img = pimg->clone();
		            rectangle(draw_img, rectangle_value, depth, 1, CV_AA);
		        }
		        imshow(WINDOW_NAME, draw_img);
		        // qƒL[‚ª‰Ÿ‚³‚ê‚½‚çI—¹
                key = waitKey(1);
                if (key == 'w') {
                    tmp->next->set(xa_g, xb_g, ya_g, yb_g);
                    if (side == 'L') tmp->next->kind = R_TARGET;
                    else if (side == 'R') tmp->next->kind = R_REFERENCE;
                    break;
                }
                
                else if (key == 'r') {
                    tmp->next->set(xa_g, xb_g, ya_g, yb_g);
                    tmp->next->kind = R_REFERENCE;
                    break;
                }

                else if (key == 't') {
                    tmp->next->set(xa_g, xb_g, ya_g, yb_g);
                    tmp->next->kind = R_TARGET;
                    break;
                }

		        else if (key == 'q') {
        		    draw_img = pimg->clone();
                    tmp->next = NULL;
                    break;
		        }
            } 
        }


        // interrupt by hitting key (when focusing on viewing window)
        // TODO: sometimes it misses the input from key board.
        t0 = clock();
        key = waitKey(1);
        key_interrupt(key, hDevice, bufS, nCurrentBuffer, rFocus);
        tKey = clock() - t0;

        // interrupt by hitting key (when focusing on terminal)
        if ( _kbhit() && (hit = _getch()) ) {
            if (key_interrupt(hit, hDevice, bufS, nCurrentBuffer, rFocus)) break;
        }
	}

    tWhole -= clock();
    tWhole *= -1;

    showtime(tAnalyze, "tAnalyze: ");
    showtime(tShow,    "tShow   : ");
    showtime(loop1,    "       loop1 : ");
    showtime(loop2,    "       loop2 : ");
    showtime(loop3,    "       loop3 : ");
    showtime(tKey,     "tKey    : ");
    showtime(tWhole,   "tWhole  : ");

    dvcStopSequence(hDevice);
    dvcSetUserBuffers(hDevice, NULL);

    CLOSE(finishing...);
    return bRC;
}
