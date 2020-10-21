#include <stdio.h>
#include <iostream>
#include <windows.h>

#include <dvcAPI.h>

#include "sequence.hpp"

using namespace std;

int isDelimiter(char p, char delim) {
    return p == delim;
}

int split(char* dst[], char* src, char delim) {
    int count = 0;

    for (;;) {
        while (isDelimiter(*src, delim)) {
            src++;
        }

        if (*src == '\0') break;

        dst[count++] = src;

        while (*src && !isDelimiter(*src, delim)) {
            src++;
        }

        if (*src == '\0') break;

        *src++ = '\0';
    }
    return count;
}

// add rectangular from rFocus
void add_rec(char* argv[], int* i, Region* rFocus)
{
    Region* tmp;
    for (tmp = rFocus; tmp->next; tmp=tmp->next) ;
            
    tmp->next = (Region*)calloc(1, sizeof(Region));
    tmp->next->set(atoi(argv[*i+1]), atoi(argv[*i+2]), atoi(argv[*i+3]), atoi(argv[*i+4]));
    *i += 4;
}

// delete rectangular from rFocus
void del_rec(char* argv[], int* i, Region* rFocus)
{
    Region* tmp = rFocus;
    Region* del;
    int idx = atoi(argv[++*i]);
    if (idx <= 0) {
        cerr << "Choose integer larger than 0.\n";
        return;
    }

    del = (*rFocus)[idx];
    tmp = (*rFocus)[idx-1];

    if (del) { 
        tmp->next = del->next;
        delete del;
    }
    else cerr << "You cannot choose that number.\n";
}

void del_rec(Region* rFocus, RegionKind rkind)
{
    if (!rFocus->next);
    else {
        for (Region* tmp = rFocus; tmp->next; tmp = tmp->next) {
            if (tmp->next->kind == rkind) {
                Region* delregion = tmp->next;
                tmp->next = delregion->next;
                delete delregion;
                break;
            }
        }
    }
}

// set exposure time
void set_exp(char* argv[], int* i, HANDLE hDevice)
{
    double dExpose = atof(argv[++*i]);
    double dExposeSet = dvcSetExposeMsec(hDevice, dExpose);

    if(dExpose != dExposeSet)
        fprintf(stderr,
            "dvcSetExposeMsec:: Request %.2f ms Return %.2f ms\n",
            dExpose, dExposeSet);
}

// set gain
void set_gain(char* argv[], int* i, HANDLE hDevice)
{
    double dGain = atof(argv[++*i]);

    if(!dvcSetGaindB(hDevice, dGain)) {
        fprintf(stderr,"dvcSetGaindB:: Error %d %s\n",
                        dvcGetLastErr(),
                        dvcGetLastErrMsg());
    } else {
        double dGainSet ;

        dvcGetGaindB(hDevice, &dGainSet);

        if(dGain != dGainSet){
            fprintf(stderr,
            "dvcSetGaindB:: Request %.2f dB Return %.2f dB\n",
            dGain, dGainSet);
        }
    }
}

// set offset
void set_offset(char* argv[], int* i, HANDLE hDevice)
{
    double dOffset = atof(argv[++*i]);

    if(!dvcSetOffsetFS(hDevice, dOffset)) {
        fprintf(stderr,"dvcSetOffsetFS:: Error %d %s\n",
                    dvcGetLastErr(),
                    dvcGetLastErrMsg());
    } else {
        double dOffsetSet ;
        dvcGetOffsetFS(hDevice, &dOffsetSet);

        if(dOffset != dOffsetSet){
            fprintf(stderr,
            "dvcSetOffsetFS:: Request %.2f %%FS Return %.2f %%FS\n",
            dOffset, dOffsetSet);
        }
    }
}

// set bin
void set_bin(char* argv[], int* i, HANDLE hDevice)
{
    int nBin = atoi(argv[++*i]);

    if(!dvcSetBinning(hDevice, nBin)) {
        fprintf(stderr,"dvcSetBinning:: Error %d %s\n",
            dvcGetLastErr(),
            dvcGetLastErrMsg());
    }
}

// open file with name <char*>
void open_file(char* argv[], int* i)
{   
    strcpy_s(fname, 64, argv[++*i]);
    file.open(fname, std::ios::out);
    isFileOpen = TRUE;
    std::cerr << "opened " << fname << endl;
}

// close file 
void close_file(char* argv[])
{   
    if (!file) 
        cerr << "no file opened!" << endl;
    else { 
        file.close();
        isFileOpen = FALSE;
    }
}

// capture I right now write in file.
void capture(dvcBufStruct bufS, int nCurretBuffer, Region* rFocus, int nCaptured)
{
    Region* tmp = rFocus;
    PUSHORT pBuffer = bufS.pBuffers[nCurretBuffer];
    float I;

    file << nCaptured << ":\n";
    for (tmp = rFocus; tmp; tmp = tmp->next) {
        getI(pBuffer, tmp, &I);
        file << "   " << "(" << tmp->pt1.x << ", " << tmp->pt1.y << ") ("
                             << tmp->pt2.x << ", " << tmp->pt2.y << ") : "
                             << I << endl;
    }
}

// parse commands
void parseCmdLine( HANDLE hDevice, int argc, char *argv[], Region* rFocus, int start )
{
    int i;
    for(i=start ; i < argc ; i++) {

        // add recutangular region to watch
        // RADD_FLAG <x1> <y1> <x2> <y2>
        if(!strcmp(argv[i], RADD_FLAG) && i+4 < argc) {
            add_rec(argv, &i, rFocus);
        }

        // delte the region from list
        // RDEL_FLAG <int>
        else if(!strcmp(argv[i], RDEL_FLAG) && i+1 < argc) {
            del_rec(argv, &i, rFocus);
        }

        else if (!strcmp(argv[i], FOPEN_FLAG) && i+1 < argc) {
            open_file(argv, &i);
        }

        else if (!strcmp(argv[i], FCLOSE_FLAG) && i < argc) {
            close_file(argv);
        }

        // Set the exposure time
        // -e <ms>
        else if(!strcmp(argv[i],"-e") && i+1 < argc) {
            set_exp(argv, &i, hDevice);
        }

        // Set the gain
        // -g <dB>
        else if(!strcmp(argv[i],"-g") && i+1 < argc) {
            set_gain(argv, &i, hDevice);
        }

        // Set the offset 
        // -o <%FS>
        else if(!strcmp(argv[i],"-o") && i+1 < argc) {
            set_offset(argv, &i, hDevice);
        }
    }
    return ;
}

void print_rFocuses(Region* rFocus)
{    
    int ri = 0;
    int ti = 0;
    int i, alli=0;
    char rk;
    std::cerr << alli++ << "     " << " :   (" << rFocus->pt1.x << ", " << rFocus->pt1.y << ") ";
    std::cerr << ", (" << rFocus->pt2.x << ", " << rFocus->pt2.y << ")\n";
    for (Region* tmp = rFocus->next; tmp; tmp = tmp->next) {
        if (tmp->kind == R_REFERENCE) {
            rk = 'r';
            i = ++ri;
        }
        else if (tmp->kind == R_TARGET) {
            rk = 't';
            i = ++ti;
        }
        std::cerr << alli++ << "   " << rk << i << " :   (" << tmp->pt1.x << ", " << tmp->pt1.y << ") ";
        std::cerr << ", (" << tmp->pt2.x << ", " << tmp->pt2.y << ")\n";
    }
}

// print help when interrupted with 'h' key
void print_help(HANDLE hDevice, Region* rFocus)
{   
    double gaindb, offset, exposure;

    dvcGetGaindB(hDevice, &gaindb);
    dvcGetOffsetFS(hDevice, &offset);
    exposure = dvcGetExposeMsec(hDevice);

    std::cerr << "Options available:\n";
    std::cerr << "   exit                    : exit\n";
    std::cerr << "   -g  <dB>   : gain. " << gaindb << endl;
    std::cerr << "   -o  <%FS>  : offset. " << offset << endl;
    std::cerr << "   -e  <ms>   : exposure time. " << exposure << endl;
    std::cerr << "   " << RADD_FLAG <<   " x1 x2 y1 y2 : add the region to see." << endl;
    std::cerr << "   " << RDEL_FLAG <<   " <int>       : delte the region to see." << endl;
    std::cerr << "   " << FOPEN_FLAG <<  " <char*> : open file with name <char*> ";
    if (isFileOpen) std::cerr << fname;
    std::cerr << "\n   " << FCLOSE_FLAG << "        : close file." << endl;
    std::cerr << "\n   " << PIEZO_FLAG << "        : setup piezo" << endl;
    std::cerr << "\nCurrently, regions on focus are:\n";
    print_rFocuses(rFocus);

    std::cerr << '\n';
}

void setup_piezo(dvcBufStruct bufS, Region* rFocus)
{   
    int i;
    print_rFocuses(rFocus);
    cerr << "\nWhich area do you use for feed back?" << endl;
    cin >> i; 
    Region* region_feedback = (*rFocus)[i];
    float Iref;
    move_findImax(rFocus, bufS);

    Iref = rFocus->Imax / 2.0f;
    move_Iref(Iref, rFocus, bufS);
}

// process key_interrupt on command line
// return TRUE if Esc or "exit"
BOOL key_interrupt(char hit, HANDLE hDevice, dvcBufStruct bufS, int nCurrentBuffer, Region* rFocus)
{
    char options[256];
    int argc;
    char* argv[32];

    switch (hit) {
    case ESC:
        return TRUE;

    // delete region from index 1
    case 'd':
        if (!rFocus->next);
        else {
            Region* tmp = rFocus->next;
            rFocus->next = tmp->next;
            delete tmp;
        }
        return FALSE;
    
    // delete region of which kind is R_REFERENCE
    case 'r':
        del_rec(rFocus, R_REFERENCE);
        return FALSE;
    // delete region of which kind is R_TARGET
    case 't':
        del_rec(rFocus, R_TARGET);
        return FALSE;

    // capture
    case 'c':
        if (!isFileOpen) 
            cerr << "file is not opened!\n";
        else 
            capture(bufS, nCurrentBuffer, rFocus, nCaptured++);
        return FALSE;
    
    // set piezo
    case 'p':
#ifdef USE_PIEZO
        setup_piezo();
#endif
        return FALSE;

    case 'h':
        print_help(hDevice, rFocus);

        // read key input
        cin.get(options, 256);
        if (cin.good() == 0) {
            // clear if there were no character and just enter key.
            cin.clear();
            cin.ignore(1024, '\n');
            return FALSE;
        } else cin.get();

        if (!strncmp(options, "exit", 4)) {
            return TRUE;
        }

        argc = split(argv, options, ' ');
        parseCmdLine(hDevice, argc, argv, rFocus, 0);
        return FALSE;

    default:
        return FALSE;
    }
}

// process mouse interrupt on window showing picture
void mouse_callback(int event, int x, int y, int flags, void *userdata)
{
    char *isClick = static_cast<char *>(userdata);

    if (event == EVENT_LBUTTONDOWN) {
        *isClick = 'L';
        xa_g = x;
        ya_g = y;
        cout << "Draw rectangle\n"
            << " start position (x, y) : " << x << ", " << y << endl;
        rectangle_value = Rect_<int>(x, y, 0, 0);
    }    
    
    if (event == EVENT_RBUTTONDOWN) {
        *isClick = 'R';
        xa_g = x;
        ya_g = y;
        cout << "Draw rectangle\n"
            << " start position (x, y) : " << x << ", " << y << endl;
        rectangle_value = Rect_<int>(x, y, 0, 0);
    }

    if (event == EVENT_LBUTTONUP || event == EVENT_RBUTTONUP) {
        *isClick = NULL;
        xb_g = x;
        yb_g = y;
        cout << " end   position (x, y) : " << x << ", " << y << endl;
        rectangle_value.width = x - rectangle_value.x;
        rectangle_value.height = y - rectangle_value.y;
    }

    if (event == EVENT_MOUSEMOVE) {
        if (*isClick) {
            rectangle_value.width = x - rectangle_value.x;
            rectangle_value.height = y - rectangle_value.y;
        }
    }

	return;
}

void mouse_interrupt(Region* rFocus, Mat* pimg, char isClick)
{
	if (isClick) {
        Mat draw_img;
        char side = isClick;
        int depth;

        if (side == 'L') depth = 200;
        else if (side == 'R') depth = 100;
 
        int key = 0;
        Region* tmp = rFocus->end();
        tmp->next = (Region*)calloc(1, sizeof(Region));
        for (;;) {
		    if (isClick) {
                draw_img = pimg->clone();
		        rectangle(draw_img, rectangle_value, depth, 1, CV_AA);
		    }
		    imshow(WINDOW_NAME, draw_img);
		    // qキーが押されたら終了
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
}