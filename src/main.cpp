/*==========================================================================
 *  程序入口 WinMain。
 *========================================================================*/

#include "calc24.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nCmdShow)
{
    (void)hPrev;

    srand((unsigned)time(NULL));

    /* 命令行模式：--selftest 自检 / --shots 导出界面截图 */
    if (cmdLine != NULL && strstr(cmdLine, "--selftest") != NULL) {
        if (strstr(cmdLine, "--log") != NULL) {
            freopen("selftest.log", "w", stdout);
        } else if (AllocConsole()) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
        resetState();
        initFonts();
        int rc = runSelfTest();
        freeFonts();
        FreeConsole();
        return rc;
    }

    enableDpiAware();

    HDC screenDC = GetDC(NULL);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
    ReleaseDC(NULL, screenDC);

    /* 既跟随系统 DPI，又保证整块画布能完整显示在屏幕上 */
    double dpiScale = dpi / 96.0;
    double fitW     = (sw - 90.0) / (double)DES_W;
    double fitH     = (sh - 140.0) / (double)DES_H;
    double fitScale = (fitW < fitH) ? fitW : fitH;

    g_scale = (dpiScale < fitScale) ? dpiScale : fitScale;
    if (g_scale > 2.5) g_scale = 2.5;
    if (g_scale < 0.75) g_scale = 0.75;

    resetState();
    initFonts();

    if (cmdLine != NULL && strstr(cmdLine, "--shots") != NULL) {
        exportShots();
        freeFonts();
        return 0;
    }

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hIcon         = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "Calc24GameWnd";

    if (!RegisterClassExA(&wc)) {
        freeFonts();
        return 1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    RECT  rc;
    rc.left = 0; rc.top = 0;
    rc.right = sc(DES_W);
    rc.bottom = sc(DES_H);
    AdjustWindowRect(&rc, style, FALSE);   /* 不使用系统菜单栏 */

    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    int wx = (sw - winW) / 2;
    int wy = (sh - winH) / 2;
    if (wy < 0) wy = 0;
    if (wx < 0) wx = 0;

    HWND hwnd = CreateWindowExA(
        0, "Calc24GameWnd", "计算二十四点",
        style, wx, wy, winW, winH,
        NULL, NULL, hInst, NULL);

    if (hwnd == NULL) {
        freeFonts();
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }

    freeFonts();
    freeBackground();
    return (int)m.wParam;
}




















