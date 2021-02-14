// z80emu.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "z80emu.h"
#include "gfx.h"
#include "intf.h"
#include "z80.h"

z80Emu *theEmu;
Interface *theUI;

enum { EMU_NONEXT, EMU_PAUSED, EMU_STEP, EMU_RUN, EMU_RESET };
int currStat, nextStat;

HWND theWin;
Gfx theGfx;
HDC GfxDC;
HBITMAP GfxBitmap;
HGDIOBJ hFon;
int initCmdLine = 0;
char initFileName[MAX_PATH];

const int SCREEN_X = 100, SCREEN_Y = 100; // , SCREEN_W=500, SCREEN_H=300;
int ScreenW = 500, ScreenH = 300;


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
TCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_Z80EMU, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    theEmu = new z80Emu;
    theUI = new Interface;
    currStat = EMU_PAUSED;
    nextStat = EMU_RESET;

    // old OpenFile code removed - going to rewrite the command line stuff: filename.z80 is executable code; filename.conf is IO configuration
    // for now, let's just create a simple mock-up
    ioSevenSeg *ss1, *ss2;
    ioButton *bnUp, *bnDown;

    ss1 = new ioSevenSeg(10, 10, 20);
    ss2 = new ioSevenSeg(32, 10, 20);
    bnUp = new ioButton(55, 10, 40, 18);
    bnDown = new ioButton(55, 32, 40, 18);

    ss1->setid(1);
    ss2->setid(2);
    bnUp->setid(3);
    bnDown->setid(4);
    ss1->setmem(0xff00);
    ss2->setmem(0xff01);
    bnUp->setmem(0xff02);
    bnDown->setmem(0xff03);

    ss1->set(0);
    ss2->set(0);
    bnUp->setLabel("up");
    bnDown->setLabel("down");

    theUI->add(ss1);
    theUI->add(ss2);
    theUI->add(bnUp);
    theUI->add(bnDown);
    
    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_Z80EMU));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    delete theUI;
    delete theEmu;
    
    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_Z80EMU));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_Z80EMU);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
       SCREEN_X, SCREEN_Y, ScreenW, ScreenH, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

BOOL UpdateWindowText(HWND hWnd, char *txt)
{
    char *last_txt = 0;
    BOOL ret = 0;
    int sz = GetWindowTextLength(hWnd);
    if (sz)
    {
        last_txt = (char*)malloc(sz + 1);
        if (last_txt)
        {
            GetWindowText(hWnd, last_txt, sz + 1);
            if (!strcmp(last_txt, txt))
            {
                ret = 1;
            }
            free(last_txt);
        }
    }
    if (!ret)
    {
        ret = SetWindowText(hWnd, txt);
    }
    return ret;
}

void strip_comment(char *t)
{
    for (int i = 0; t[i]; i++)
    {
        if (t[i] == ';')
        {
            t[i] = 0;
            return;
        }
    }
}

// ptr points to a pointer that we update
// token found is put into token, max maxchars long, or comma separated.
// Long tokens are not flagged, but are split, and any errors will be of
// the "token not recognised" variety.
// return value: 
//   0 at EOL
//   1 for a token of some sort
int gettokfromline(char **lineptr, char *token, int maxchars, int include_spaces)
{
    int i = 0;
    char *ptr = *lineptr;
    if (!*ptr)
        return 0;

    token[0] = 0;
    while (*ptr && *ptr != ',')
    {
        if (include_spaces || !isspace(*ptr))
        {
            token[i++] = *ptr;
            token[i] = 0;
        }
        ptr++;
    }
    if (*ptr == ',')
        ptr++;
    *lineptr = ptr;

    return token[0] ? 1 : 0;
}

int hexval(char *p)
{
    int val = 0;
    for (int i = 0; p[i]; i++)
    {
        if (p[i] >= '0' && p[i] <= '9')
            val = val * 16 + p[i] - '0';
        else if (p[i] >= 'a' && p[i] <= 'f')
            val = val * 16 + p[i] - 'a' + 10;
        else if (p[i] >= 'A' && p[i] <= 'F')
            val = val * 16 + p[i] - 'A' + 10;
    }
    return val;
}

void readUIConfig(HWND hWnd, char *FileName)
{
    const int itemlen = 32;
    char item[itemlen], line[1024];
    char id[itemlen], addr[itemlen], xpos[itemlen], ypos[itemlen], dx[itemlen], dy[itemlen], label[itemlen];
    theUI->reset();
    FILE *fp;
    fopen_s(&fp, FileName, "r");
    if (fp)
    {
        while (fgets(line, 1020, fp))
        {
            strip_comment(line);
            char *ptr = line;
            if (gettokfromline(&ptr, item, itemlen, 0))
            {
                int err = 1;
                if (!strcmp(item, "sevenseg"))
                {
                    if (gettokfromline(&ptr, id, itemlen, 0) &&
                        gettokfromline(&ptr, addr, itemlen, 0) &&
                        gettokfromline(&ptr, xpos, itemlen, 0) &&
                        gettokfromline(&ptr, ypos, itemlen, 0) &&
                        gettokfromline(&ptr, dx, itemlen, 0))
                    {
                        ioSevenSeg *p = new ioSevenSeg(atoi(xpos), atoi(ypos), atoi(dx));
                        p->setid(atoi(id));
                        p->setmem(hexval(addr));
                        p->set(0);
                        theUI->add(p);
                        err = 0;
                    }
                }
                else if (!strcmp(item, "button"))
                {
                    if (gettokfromline(&ptr, id, itemlen, 0) &&
                        gettokfromline(&ptr, addr, itemlen, 0) &&
                        gettokfromline(&ptr, xpos, itemlen, 0) &&
                        gettokfromline(&ptr, ypos, itemlen, 0) &&
                        gettokfromline(&ptr, dx, itemlen, 0) &&
                        gettokfromline(&ptr, dy, itemlen, 0) &&
                        gettokfromline(&ptr, label, itemlen, 0))
                    {
                        ioButton *p = new ioButton(atoi(xpos), atoi(ypos), atoi(dx), atoi(dy));
                        p->setid(atoi(id));
                        p->setmem(hexval(addr));
                        p->setLabel(label);
                        theUI->add(p);
                        err = 0;
                    }
                }
                else if (!strcmp(item, "window"))
                {
                    if (gettokfromline(&ptr, dx, itemlen, 0) &&
                        gettokfromline(&ptr, dy, itemlen, 0))
                    {
                        ScreenW = atoi(dx);
                        ScreenH = atoi(dy);
                        SetWindowPos(hWnd, 0, 0, 0, ScreenW, ScreenW, SWP_NOMOVE | SWP_NOZORDER);
                        DeleteObject((HGDIOBJ)GfxBitmap);
                        HDC hdc = GetDC(hWnd);
                        GfxBitmap = CreateCompatibleBitmap(hdc, ScreenW, ScreenH);
                        ReleaseDC(hWnd, hdc);
                        SelectObject(GfxDC, GfxBitmap);
                        err = 0;
                    }
                }
                else if (!strcmp(item, "regs"))
                {
                    if (gettokfromline(&ptr, xpos, itemlen, 0) &&
                        gettokfromline(&ptr, ypos, itemlen, 0))
                    {
                        theUI->show_regs(atoi(xpos), atoi(ypos));
                        err = 0;
                    }
                }
                else if (!strcmp(item, "mem"))
                {
                    if (gettokfromline(&ptr, xpos, itemlen, 0) &&
                        gettokfromline(&ptr, ypos, itemlen, 0))
                    {
                        theUI->show_memdump(atoi(xpos), atoi(ypos));
                        err = 0;
                    }
                }
                else if (!strcmp(item, "loadimage"))
                {
                    char FileName[132];
                    if (gettokfromline(&ptr, FileName, 130, 1) &&
                        gettokfromline(&ptr, addr, itemlen, 0))
                    {
                        if (theEmu->loadFileAt(FileName, atoi(addr)))
                            err = 0;
                    }
                }
                if (err)
                {
                    MessageBox(0, line, "Invalid line", MB_OK);
                }
            }
        }
        fclose(fp);
        InvalidateRect(hWnd, 0, 1);
    }
    else
    {
        char err[64];
        wsprintf(err, "errno = %d", errno);
        MessageBox(0, err, "readUIConfig file open failed", MB_OK);
    }
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HDC hdc = GetDC(hWnd);
        GfxDC = CreateCompatibleDC(0);
        hFon = GetStockObject(ANSI_FIXED_FONT);
        GfxBitmap = CreateCompatibleBitmap(hdc, ScreenW, ScreenH);
        SelectObject(GfxDC, GfxBitmap);
        SelectObject(GfxDC, hFon);
        ReleaseDC(hWnd, hdc);
        SetTimer(hWnd, 1, 50, 0); // todo: make 50 configurable

        theWin = hWnd;
    }
    break;
    
    case WM_DESTROY:
        DeleteObject((HGDIOBJ)GfxBitmap);
        DeleteDC(GfxDC);
        PostQuitMessage(0);
        KillTimer(hWnd, 1);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc;
        char rep[256], mem[16 * 32];
        theEmu->memdump(mem, 16*32, 15);
        theEmu->report(rep,256);
        theGfx.Clear();
        theUI->redraw(rep, mem);
        hdc = BeginPaint(hWnd, &ps);
        StretchBlt(hdc, 0, 0, ScreenW, ScreenH, GfxDC, 0, 0, ScreenW, ScreenH, SRCCOPY);
        EndPaint(hWnd, &ps);
    }
    break;
    
    case WM_LBUTTONUP:
    {
        int xPos = LOWORD(lParam);
        int yPos = HIWORD(lParam);

        theUI->click(xPos, yPos);
    }
    break;

    case WM_CHAR: // new code
    {
        TCHAR chCharCode = (TCHAR)wParam;    // character code
                                             // long lKeyData = lParam;              // key data
        switch (chCharCode)
        {
        case ' ': // All we do here is to set nextStat to EMU_STEP and existing code should handle the rest.  This is what WM_COMMAND/IDM_EMU_STEP does.
            nextStat = EMU_STEP;
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    /*case WM_CHAR: // old code
    {
        TCHAR chCharCode = (TCHAR)wParam;    // character code
                                             // long lKeyData = lParam;              // key data
        switch (chCharCode)
        {
        case ' ':
            if (!emuStat)
            {
                emuStat = 1;
                SendMessage(hWnd, WM_TIMER, 1, 0);
                InvalidateRect(hWnd, 0, 0);
                emuStat = 0;
            }
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;*/

    case WM_COMMAND:
    {
        int wNotifyCode = HIWORD(wParam); // notification code 
        int wID = LOWORD(wParam);         // item, control, or accelerator identifier 
                                          // hwndCtl = (HWND) lParam;      // handle of control 
        if (wNotifyCode)
            return DefWindowProc(hWnd, message, wParam, lParam);
        else
        {	// Message is from a menu
            switch (wID)
            {
            case IDM_EMU_RESET:
                nextStat = EMU_RESET;
                break;

            case IDM_EMU_RUN:
                nextStat = EMU_RUN;
                break;

            case IDM_EMU_STOP:
                nextStat = EMU_PAUSED;
                break;

            case IDM_EMU_STEP:
                nextStat = EMU_STEP;
                break;

            case IDM_ABOUT:
                DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
                break;

            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;

/* todo: Old File-Open code -- needs rewriting.

            case IDM_FILE_OPEN_CONF:
            {
                OPENFILENAME of;
                char FileName[260];
                int ok = 0;

                emuStat = 0;
                *FileName *= 0;
                UpdateWindowText(hWnd, "Loading config...");
                memset(&of, 0, sizeof(of));
                of.lStructSize = sizeof(of);
                of.hwndOwner = hWnd;
                of.hInstance = hInst;
                of.lpstrFilter = "Emulator Config Files (*.emu)\0*.emu\0\0";
                of.lpstrFile = FileName;
                of.nMaxFile = 259;
                of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                if (GetOpenFileName(&of))
                {
                    readUIConfig(hWnd, FileName);
                    init = 0;
                    InvalidateRect(hWnd, 0, 0);
                }
            }
            break;

            case IDM_FILE_OPEN:
            {
                OPENFILENAME of;
                char FileName[260];
                int ok = 0;

                emuStat = 0;
                *FileName *= 0;
                memset(&of, 0, sizeof(of));
                of.lStructSize = sizeof(of);
                of.hwndOwner = hWnd;
                of.hInstance = hInst;
                of.lpstrFilter = "Z80 Files (*.z80)\0*.z80\0\0";
                of.lpstrFile = FileName;
                of.nMaxFile = 259;
                of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                if (GetOpenFileName(&of))
                {
                    if (theEmu->loadFile(FileName))
                    {
                        ok = 1;
                        init = 0;
                        InvalidateRect(hWnd, 0, 0);
                        ss1->set(0);
                        ss2->set(0);
                    }
                }
                if (!ok)
                {
                    MessageBox(hWnd, "File Open Failed", "Yo", MB_OK);
                }
            }
            break;
*/
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
    }
    break;

    case WM_TIMER: // runs continuously
    {
        // Process nextStat and set currStat accordingly
        int i, op_time = 0, updateDisplay = 0;
        switch (nextStat)
        {
        case EMU_NONEXT:
            break;

        case EMU_RESET:
            theEmu->warm_reset();
            currStat = EMU_PAUSED;
            updateDisplay = 1;
            break;

        case EMU_RUN:
            currStat = EMU_RUN;
            for (i = 0; i<49; i++) // todo: make 49 configurable
            {
                theEmu->execute_op(&op_time);
            }
            break;

        case EMU_PAUSED:
            updateDisplay = 1;
            currStat = EMU_PAUSED;
            break;

        case EMU_STEP:
            updateDisplay = 1;
            theEmu->execute_op(&op_time);
            currStat = EMU_PAUSED;
            break;
        }
        nextStat = EMU_NONEXT;

        // If appropriate, update the display
        if (updateDisplay)
        {
            // Set title accordingly
            char wintext[256];
            char instr[128];
            ushort pc;

            switch (currStat)
            {
            case EMU_PAUSED: // title - paused (pc=0x%04x instr=%s)
                pc = theEmu->getpc();
                theEmu->disass(instr, 128, &pc);
                wsprintf(wintext, "%s - paused (pc=0x%04x instr='%s')", szTitle, pc, instr);
                UpdateWindowText(hWnd, wintext);
                break;

            case EMU_RUN: // title - running
                wsprintf(wintext, "%s - running", szTitle);
                UpdateWindowText(hWnd, wintext);
                break;
            }
            // Redraw the window
            InvalidateRect(hWnd, 0, 0);
            UpdateWindow(hWnd);
        }
    }
    break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Gfx functions

void Gfx::Rect(int x, int y, int cx, int cy, int col)
{
    HDC hdc = GfxDC;
    HGDIOBJ g;
    switch (col)
    {
    case Gfx::Black:
    default:
        g = GetStockObject(BLACK_BRUSH);
        break;
    }
    g = SelectObject(hdc, g);
    Rectangle(hdc, x, y, x + cx, y + cy);
    g = SelectObject(hdc, g);
}

void Gfx::Line(int x1, int y1, int x2, int y2, int col)
{
    HDC hdc = GfxDC;
    HPEN p;
    switch (col)
    {
    case Gfx::DkGreen: p = CreatePen(PS_SOLID, 1, COLORREF(RGB(0, 128, 0))); break;
    case Gfx::LtGreen: p = CreatePen(PS_SOLID, 1, COLORREF(RGB(0, 255, 0))); break;
    case Gfx::Black:
    default: p = CreatePen(PS_SOLID, 1, COLORREF(RGB(0, 0, 0))); break;
    }
    p = (HPEN)SelectObject(hdc, (HGDIOBJ)p);
    MoveToEx(hdc, x1, y1, 0);
    LineTo(hdc, x2, y2);
    p = (HPEN)SelectObject(hdc, (HGDIOBJ)p);
    DeleteObject((HGDIOBJ)p);
}

void Gfx::Frame(int x, int y, int cx, int cy, int col)
{
    HDC hdc = GfxDC;
    RECT r;
    HBRUSH b;
    switch (col)
    {
    case Gfx::Black:
    default: b = CreateSolidBrush(COLORREF(RGB(0, 0, 0)));
    }
    SetRect(&r, x, y, x + cx, y + cy);
    FrameRect(hdc, &r, b);
    DeleteObject((HGDIOBJ)b);
}

/*
Modes
0: Single line, H and V centred
1: Multiline, H centred
2: Multiline, left aligned
*/
void Gfx::Text(int x, int y, int cx, int cy, char *text, int col, int mode)
{
    HDC hdc = GfxDC;

    RECT r;
    SetRect(&r, x, y, x + cx, y + cy);

    HPEN p;
    switch (col)
    {
    case Gfx::DkGreen: p = CreatePen(PS_SOLID, 1, COLORREF(RGB(0, 128, 0))); break;
    case Gfx::LtGreen: p = CreatePen(PS_SOLID, 1, COLORREF(RGB(0, 255, 0))); break;
    default: p = CreatePen(PS_SOLID, 1, COLORREF(RGB(0, 0, 0))); break;
    }

    p = (HPEN)SelectObject(hdc, (HGDIOBJ)p);
    SetBkColor(hdc, GetSysColor(COLOR_WINDOW));

    UINT flags;
    switch (mode)
    {
    case 1:
        flags = DT_CENTER;
        break;

    case 2:
        flags = DT_LEFT;
        break;

    case 0:
    default:
        flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE; // original Text mode
        break;
    }
    DrawText(hdc, text, strlen(text), &r, flags);

    p = (HPEN)SelectObject(hdc, (HGDIOBJ)p);

    DeleteObject((HGDIOBJ)p);
}

void Gfx::Clear()
{
    HDC hdc = GfxDC;
    COLORREF col = GetSysColor(COLOR_WINDOW);
    HBRUSH bk = CreateSolidBrush(col);
    HGDIOBJ p = SelectObject(hdc, bk);
    Rectangle(hdc, 0, 0, ScreenW, ScreenH);
    SelectObject(hdc, p);
    DeleteObject(bk);
}

// Adjust the window size so that the client area is (x,y)
void Gfx::setClient(int x, int y)
{
    RECT win, cli;
    GetWindowRect(theWin, &win);
    GetClientRect(theWin, &cli);

    int dx = x + (win.right - win.left) - cli.right;
    int dy = y + (win.bottom - win.top) - cli.bottom;

    SetWindowPos(theWin, 0, 0, 0, dx, dy, SWP_NOZORDER | SWP_NOMOVE);
}
