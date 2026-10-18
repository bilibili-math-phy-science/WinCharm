#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>

// ============ 布局常量 ============
const int TOP_STRIP_HEIGHT = 70;            // 顶部蓝色条高度 #1171D2
const int WINDOW_PADDING   = 24;            // 窗口内容边框留白
const int LAUNCH_BTN_H     = 120;           // 连点器按钮高度
// #1171D2 颜色
const COLORREF STRIP_COLOR = RGB(17, 113, 210);

#define BTN_LAUNCH 1001

HFONT g_hFont = nullptr;
HFONT g_hTitleBtnFont = nullptr;
HFONT g_hBtnFont = nullptr;   // “连点器”大按钮专用字体
HWND g_hMainWnd = nullptr;
HWND g_btnLaunch = nullptr;
HWND g_lblPath = nullptr;

std::wstring g_workingDir;     // 当前工作目录（GetCurrentDirectory）
std::wstring g_targetPath;     // 当前目录 + \Clicker.exe

bool g_bDrag = false;
POINT g_ptDragStartScreen;
RECT  g_wndStartRect;

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

void LayoutControls();

// 圆角窗口
void MakeRoundCorner(HWND hwnd, int radius)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    HRGN hRgn = CreateRoundRectRgn(0, 0, w, h, radius * 2, radius * 2);
    SetWindowRgn(hwnd, hRgn, TRUE);
    DeleteObject(hRgn);
}

void SetDpiAware()
{
     typedef BOOL(WINAPI* fnSetDpiAwareCtx)(DPI_AWARENESS_CONTEXT);
     HMODULE hUser = GetModuleHandleW(L"user32.dll");
     if (hUser)
     {
          fnSetDpiAwareCtx fn = (fnSetDpiAwareCtx)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
          if (fn) fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
     }
}

// 标题栏 - × 按钮矩形（随窗口宽度自适应贴右侧）
void GetTitleBtnRects(RECT &rcMin, RECT &rcClose)
{
     RECT cli;
     GetClientRect(g_hMainWnd, &cli);
     int bw = 44, gap = 6;
     rcClose.left   = cli.right - bw - 8;
     rcClose.right  = cli.right - 8;
     rcClose.top    = 6;
     rcClose.bottom = TOP_STRIP_HEIGHT - 6;
     rcMin.left     = rcClose.left - gap - bw;
     rcMin.right    = rcClose.left - gap;
     rcMin.top      = 6;
     rcMin.bottom   = TOP_STRIP_HEIGHT - 6;
}

// 从当前工作目录启动 Clicker.exe
void LaunchClicker(HWND hWnd)
{
     if (GetFileAttributesW(g_targetPath.c_str()) == INVALID_FILE_ATTRIBUTES)
     {
          std::wstring msg = L"未找到 Clicker.exe：\n" + g_targetPath;
          MessageBoxW(hWnd, msg.c_str(), L"错误", MB_ICONERROR);
          return;
     }
     SHELLEXECUTEINFOW sei{};
     sei.cbSize = sizeof(sei);
     sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
     sei.hwnd   = hWnd;
     sei.lpVerb = L"open";
     sei.lpFile = g_targetPath.c_str();
     sei.lpDirectory = g_workingDir.c_str();
     sei.nShow  = SW_SHOWNORMAL;
     ShellExecuteExW(&sei);
     if (sei.hProcess) CloseHandle(sei.hProcess);
}

//==================== 动态布局：一个大按钮 + 下方路径文字 ====================
void LayoutControls()
{
     if (!g_hMainWnd) return;
     RECT rc;
     GetClientRect(g_hMainWnd, &rc);
     int W = rc.right;
     int H = rc.bottom;
     if (W <= 0 || H <= 0) return;

     const int M = WINDOW_PADDING;

     // 大按钮：贴顶，横向占满
     int btnY = M;
     MoveWindow(g_btnLaunch, M, btnY, W - M*2, LAUNCH_BTN_H, TRUE);

     // 路径文字：按钮下方，占满剩余宽度
     int textY = btnY + LAUNCH_BTN_H + 16;
     int textH = H - M - textY;
     if (textH < 40) textH = 40;
     MoveWindow(g_lblPath, M, textY, W - M*2, textH, TRUE);

     InvalidateRect(g_hMainWnd, nullptr, TRUE);
}

// MinGW 要求 EnumChildWindows 回调为 __stdcall(CALLBACK)
BOOL CALLBACK SetChildFont(HWND hwnd, LPARAM lParam)
{
     SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
     return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
     switch (msg)
     {
     case WM_COMMAND:
     {
          int id = LOWORD(wParam);
          int notify = HIWORD(wParam);
          if (notify == BN_CLICKED && id == BTN_LAUNCH)
          {
               LaunchClicker(hWnd);
          }
          break;
     }
     case WM_CREATE:
     {
          g_btnLaunch = CreateWindowExW(0, L"BUTTON", L"连点器",
               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
               0,0,0,0, hWnd, (HMENU)BTN_LAUNCH, GetModuleHandle(nullptr), nullptr);
          g_lblPath = CreateWindowExW(0, L"STATIC", L"",
               WS_CHILD | WS_VISIBLE | SS_LEFT,
               0,0,0,0, hWnd, nullptr, GetModuleHandle(nullptr), nullptr);
          break;
     }
     case WM_SIZE:
     {
          LayoutControls();
          break;
     }
     case WM_SHOWWINDOW:
     {
          LayoutControls();
          break;
     }
     case WM_PAINT:
     {
          PAINTSTRUCT ps;
          BeginPaint(hWnd, &ps);
          EndPaint(hWnd, &ps);
          break;
     }
     case WM_CLOSE:
     {
          DestroyWindow(hWnd);
          break;
     }
     case WM_DESTROY:
     {
          if(g_hFont) DeleteObject(g_hFont);
          if(g_hTitleBtnFont) DeleteObject(g_hTitleBtnFont);
          if(g_hBtnFont) DeleteObject(g_hBtnFont);
          PostQuitMessage(0);
          break;
     }
     default:
          return DefWindowProc(hWnd, msg, wParam, lParam);
     }
     return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
     SetDpiAware();

     // —— 取【当前工作目录】，拼出 当前目录\Clicker.exe ——
     wchar_t cwd[MAX_PATH];
     GetCurrentDirectoryW(MAX_PATH, cwd);
     g_workingDir = cwd;
     g_targetPath = cwd;
     if (!g_targetPath.empty() && g_targetPath.back() != L'\\')
          g_targetPath += L"\\";
     g_targetPath += L"Clicker.exe";

     g_hFont = CreateFontW(
          30, 0, 0, 0, FW_NORMAL,
          FALSE, FALSE, FALSE,
          DEFAULT_CHARSET,
          OUT_DEFAULT_PRECIS,
          CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY,
          DEFAULT_PITCH | FF_DONTCARE,
          L"华文中宋"
     );
     g_hTitleBtnFont = CreateFontW(
          48, 0, 0, 0, FW_BOLD,
          FALSE, FALSE, FALSE,
          DEFAULT_CHARSET,
          OUT_DEFAULT_PRECIS,
          CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY,
          DEFAULT_PITCH | FF_DONTCARE,
          L"Segoe UI"
     );
     // “连点器”大按钮专用：比通用字体(30)更大、加粗
     g_hBtnFont = CreateFontW(
          52, 0, 0, 0, FW_BOLD,
          FALSE, FALSE, FALSE,
          DEFAULT_CHARSET,
          OUT_DEFAULT_PRECIS,
          CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY,
          DEFAULT_PITCH | FF_DONTCARE,
          L"华文中宋"
     );

     WNDCLASSEXW wc{};
     wc.lpfnWndProc = WndProc;
     wc.hInstance = hInst;
     wc.lpszClassName = L"ClickerWinClass";
     wc.cbSize = sizeof(WNDCLASSEXW);
     wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
     wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
     RegisterClassExW(&wc);

     g_hMainWnd = CreateWindowExW(
          0, wc.lpszClassName, L"WinCharm",
          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
          CW_USEDEFAULT, CW_USEDEFAULT,
          720, 300,
          nullptr, nullptr, hInst, nullptr
     );

     EnumChildWindows(g_hMainWnd, SetChildFont, 0);
     // 覆盖：“连点器”大按钮使用更大的专用字体
     SendMessageW(g_btnLaunch, WM_SETFONT, (WPARAM)g_hBtnFont, TRUE);

     // 路径文字：按钮下方显示 当前目录\Clicker.exe
     SetWindowTextW(g_lblPath, g_targetPath.c_str());

     ShowWindow(g_hMainWnd, nCmdShow);
     UpdateWindow(g_hMainWnd);
     LayoutControls();

     MSG msg{};
     while (GetMessageW(&msg, nullptr, 0, 0))
     {
          TranslateMessage(&msg);
          DispatchMessageW(&msg);
     }
     return (int)msg.wParam;
}
