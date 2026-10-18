#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <cstdio>
#include <cmath>
#include <cwchar>
#include <string>
#include <vector>
#include <map>
// ============ 布局常量【放大修改】 ============
const int TOP_STRIP_HEIGHT = 70;
const int WINDOW_MARGIN   = 24;   // 四周留白
const int CTRL_GAP        = 16;   // 控件之间的间距
const int MODE_BTN_HEIGHT = 56;   // 模式按钮高度
const int EXPORT_BTN_HEIGHT = 70; // 导出按钮高度
// #1171D2 颜色
const COLORREF STRIP_COLOR = RGB(17, 113, 210);
#define BTN_CLICK_MODE      1001
#define BTN_LONG_MODE       1002
#define BTN_LEFT            1003
#define BTN_RIGHT           1004
#define BTN_EXPORT          1005
#define BTN_LONG_LEFT       1006
#define BTN_LONG_RIGHT      1007
#define EDIT_SPEED          3001
#define EDIT_COUNT          3002
#define WM_USER_LOAD_WCC    (WM_USER + 100)
//==================== WCC配置数据结构 (必须和DLL内完全一致！) ====================
enum TaskType
{
    TASK_NONE,
    TASK_CLICK,
    TASK_LONG
};
struct WccTask
{
    TaskType type = TASK_NONE;
    int speedMs = 100;
    int infinite = 1; //1无限 0有限
    int count = 0;
    int button = 0; // 0左键 1右键
};
std::vector<WccTask> g_taskList;
std::wstring g_delayedWccPath;
//==================== DLL动态加载 ====================
typedef int (__cdecl *FnExportWcc)(const wchar_t*, const WccTask*);
typedef int (__cdecl *FnLoadWcc)(const wchar_t*, WccTask*);
HMODULE g_hWccDll = nullptr;
FnExportWcc fnExport = nullptr;
FnLoadWcc fnLoad = nullptr;
// 获取EXE自身所在目录，拼接DLL路径，彻底解决相对路径问题
std::wstring GetExeDirectory()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    size_t pos = path.find_last_of(L'\\');
    if(pos != std::wstring::npos)
        path = path.substr(0, pos + 1);
    return path;
}
bool LoadWccDll()
{
    std::wstring dllPath = GetExeDirectory() + L"Release\\WccSaveOpen.dll";
    g_hWccDll = LoadLibraryW(dllPath.c_str());
    if(!g_hWccDll)
    {
        MessageBoxW(nullptr, L"找不到 WccSaveOpen.dll", L"DLL缺失", MB_ICONERROR);
        return false;
    }
    fnExport = reinterpret_cast<FnExportWcc>(GetProcAddress(g_hWccDll, "ExportWccConfig"));
    fnLoad   = reinterpret_cast<FnLoadWcc>(GetProcAddress(g_hWccDll, "LoadWccConfig"));
    if(!fnExport || !fnLoad)
    {
        MessageBoxW(nullptr, L"WccSaveOpen.dll导出函数缺失！检查DLL是否extern \"C\"", L"DLL错误", MB_ICONERROR);
        FreeLibrary(g_hWccDll);
        g_hWccDll = nullptr;
        return false;
    }
    return true;
}
void FreeWccDll()
{
    if(g_hWccDll)
    {
        FreeLibrary(g_hWccDll);
        g_hWccDll = nullptr;
        fnExport = nullptr;
        fnLoad = nullptr;
    }
}
HFONT g_hFont = nullptr;
// 标题栏大号字体（专门放大 - ×）
HFONT g_hTitleBtnFont = nullptr;
HWND g_hMainWnd = nullptr;
//==================== 语言包（依赖 exe 同目录的 *.json，zh-cn.json 优先） ====================
static std::map<std::wstring, std::wstring> g_lang;
static const wchar_t* GetText(const wchar_t* key, const wchar_t* def)
{
    auto it = g_lang.find(key);
    return (it != g_lang.end() && !it->second.empty()) ? it->second.c_str() : def;
}
static void ParseJson(const std::wstring& s)
{
    g_lang.clear();
    size_t i = 0;
    while (i < s.size())
    {
        i = s.find((wchar_t)0x22, i);
        if (i == std::wstring::npos) break;
        std::wstring key; i++;
        while (i < s.size() && s[i] != (wchar_t)0x22) { key.push_back(s[i]); i++; }
        i = s.find(L':', i);
        if (i == std::wstring::npos) break;
        i = s.find((wchar_t)0x22, i);
        if (i == std::wstring::npos) break;
        std::wstring val; i++;
        while (i < s.size() && s[i] != (wchar_t)0x22) { val.push_back(s[i]); i++; }
        g_lang[key] = val;
        i++;
    }
}
static bool LoadLanguage(const std::wstring& exeDir)
{
    std::wstring path = exeDir + L"zh-cn.json";
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((exeDir + L"*.json").c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return false;
        FindClose(hFind);
        path = exeDir + fd.cFileName;
        hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;
    }
    DWORD size = GetFileSize(hFile, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0) { CloseHandle(hFile); return false; }
    std::vector<char> buf(size + 1, 0);
    DWORD rd = 0;
    ReadFile(hFile, buf.data(), size, &rd, nullptr);
    CloseHandle(hFile);

    size_t off = 0;
    if (rd >= 3 && (unsigned char)buf[0]==0xEF && (unsigned char)buf[1]==0xBB && (unsigned char)buf[2]==0xBF) off = 3;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, buf.data() + off, (int)(rd - off), nullptr, 0);
    if (wlen <= 0) return false;
    std::wstring ws(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, buf.data() + off, (int)(rd - off), &ws[0], wlen);

    ParseJson(ws);
    return !g_lang.empty();
}
// ==== 全部控件句柄（用于动态布局）====
HWND g_btnClickMode, g_btnLongMode;
HWND g_frameClick, g_frameHold;
HWND g_lblSpeed, g_edtSpeed, g_lblClickCnt, g_edtClickCnt;
HWND g_lblHoldSec, g_edtHoldSec, g_lblHoldCnt, g_edtHoldCnt;
HWND g_btnLeft, g_btnRight, g_btnLongLeft, g_btnLongRight;
HWND g_btnExport;
HWND g_hotkeyWnd = nullptr;
bool g_running = false;
int g_mode = 0;
int g_mouseBtn = 0;
HANDLE g_workThread = nullptr;
bool g_bDrag = false;
POINT g_ptOffset;        // 鼠标按下时，相对于窗口左上角的客户端偏移
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
//===== 前置声明 =====
void SwitchPanel(int mode);
void ExportWccConfig();
bool LoadWccFile(const std::wstring &filePath);
void CreateAllControls(HWND hParent);
void LayoutControls();
static void SafeSleep(DWORD ms);
void GetTitleBtnRect(HWND hWnd, RECT &rcMin, RECT &rcClose);
// 圆角窗口
void MakeRoundCorner(HWND hwnd, int radius)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    HRGN hRgn = CreateRoundRectRgn(0,0,rc.right-rc.left, rc.bottom-rc.top, radius*2, radius*2);
    SetWindowRgn(hwnd, hRgn, TRUE);
    DeleteObject(hRgn);
}
void SetDpiAware()
{
    typedef BOOL(WINAPI* fnSetDpiAwareCtx)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser)
    {
        fnSetDpiAwareCtx fn = reinterpret_cast<fnSetDpiAwareCtx>(GetProcAddress(hUser, "SetProcessDpiAwarenessContext"));
        if (fn) fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
}
// ============【修复！强制ZeroMemory清零整个INPUT结构体，解决GCC联合体垃圾值】============
static bool SendMouseInput(DWORD mouseFlag)
{
    INPUT ip;
    ZeroMemory(&ip, sizeof(INPUT));
    ip.type = INPUT_MOUSE;
    ip.mi.dwFlags = mouseFlag;
    UINT sent = SendInput(1, &ip, sizeof(INPUT));
    return (sent == 1);
}
void MouseLeftDown()
{
    SendMouseInput(MOUSEEVENTF_LEFTDOWN);
}
void MouseLeftUp()
{
    SendMouseInput(MOUSEEVENTF_LEFTUP);
}
void MouseRightDown()
{
    SendMouseInput(MOUSEEVENTF_RIGHTDOWN);
}
void MouseRightUp()
{
    SendMouseInput(MOUSEEVENTF_RIGHTUP);
}
static double MyWtof(const wchar_t* s)
{
    wchar_t* endp;
    return wcstod(s, &endp);
}
void ShowMsg(HWND parent, LPCWSTR title, LPCWSTR text)
{
    MessageBoxW(parent, text, title, MB_OK | MB_ICONINFORMATION);
}
void ReleaseAllMouse()
{
    MouseLeftUp();
    MouseRightUp();
}
//====================字符串工具====================
static std::wstring Trim(const std::wstring &s)
{
    size_t start = s.find_first_not_of(L" \t\r\n");
    size_t end = s.find_last_not_of(L" \t\r\n");
    if(start == std::wstring::npos) return L"";
    return s.substr(start, end - start + 1);
}
// 改良Sleep，1ms轮询快速退出，解决句柄残留
static void SafeSleep(DWORD ms)
{
    DWORD start = GetTickCount();
    while(g_running && (GetTickCount() - start) < ms)
    {
        Sleep(1);
    }
}
//==================== .WCC加载 (调用DLL) ====================
bool LoadWccFile(const std::wstring &filePath)
{
    if(!fnLoad)
    {
        ShowMsg(g_hMainWnd, L"错误", L"WccSaveOpen.dll未加载");
        return false;
    }
    WccTask task;
    int ret = fnLoad(filePath.c_str(), &task);
    if(!ret)
    {
        ShowMsg(g_hMainWnd, L"错误", L"无法打开 .wcc 文件");
        return false;
    }
    g_taskList.clear();
    g_taskList.push_back(task);
    if(!g_taskList.empty())
    {
        auto &t = g_taskList[0];
        if(t.type == TASK_CLICK)
        {
            SwitchPanel(0);
            wchar_t buf[64];
            int cps = static_cast<int>(1000.0 / t.speedMs + 0.5);
            wsprintfW(buf, L"%d", cps);
            SetWindowTextW(g_edtSpeed, buf);
            if(t.infinite != 0)
                SetWindowTextW(g_edtClickCnt, L"无限");
            else
            {
                wsprintfW(buf, L"%d", t.count);
                SetWindowTextW(g_edtClickCnt, buf);
            }
        }
        else if(t.type == TASK_LONG)
        {
            SwitchPanel(1);
            wchar_t buf[64];
            wsprintfW(buf, L"%.2f", t.speedMs / 1000.0);
            SetWindowTextW(g_edtHoldSec, buf);
            wsprintfW(buf, L"%d", t.count);
            SetWindowTextW(g_edtHoldCnt, buf);
        }
        g_mouseBtn = t.button;
    }
    ShowMsg(g_hMainWnd, L"成功", L".wcc配置加载完毕");
    return true;
}
//==================== 导出配置 .wcc (调用DLL) ====================
void ExportWccConfig()
{
    if(!fnExport)
    {
        ShowMsg(g_hMainWnd, L"错误", L"WccSaveOpen.dll未加载");
        return;
    }
    OPENFILENAMEW ofn{};
    wchar_t szFile[MAX_PATH] = L"重命名.wcc";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"连点器配置(*.wcc)\0*.wcc\0所有文件(*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"wcc";
    if(!GetSaveFileNameW(&ofn))
        return;
    WccTask task{};
    if(g_mode == 0)
    {
        task.type = TASK_CLICK;
        wchar_t buf[128];
        GetWindowTextW(g_edtSpeed, buf, _countof(buf));
        double cps = MyWtof(buf);
        task.speedMs = static_cast<int>(1000.0 / cps);
        GetWindowTextW(g_edtClickCnt, buf, _countof(buf));
        if(wcscmp(buf, L"无限") == 0 || wcscmp(buf, L"inf") ==0)
        {
            task.infinite =1;
            task.count =0;
        }
        else
        {
            task.infinite =0;
            task.count = _wtoi(buf);
        }
        task.button = g_mouseBtn;
    }
    else
    {
        task.type = TASK_LONG;
        wchar_t buf[128];
        GetWindowTextW(g_edtHoldSec, buf, _countof(buf));
        double sec = MyWtof(buf);
        task.speedMs = static_cast<int>(sec*1000);
        GetWindowTextW(g_edtHoldCnt, buf, _countof(buf));
        task.count = _wtoi(buf);
        task.infinite = 0;
        task.button = g_mouseBtn;
    }
    int ok = fnExport(szFile, &task);
    if(ok)
        ShowMsg(g_hMainWnd, L"导出成功", L".wcc配置文件已保存");
    else
        ShowMsg(g_hMainWnd, L"错误", L"保存失败，无法写入文件");
}
DWORD WINAPI WorkThread(LPVOID)
{
    if (g_mode == 0)
    {
        wchar_t buf[64];
        GetWindowTextW(g_edtSpeed, buf, _countof(buf));
        double perSec = MyWtof(buf);
        if (perSec <= 0)
        {
            PostMessageW(g_hMainWnd, WM_USER+200, 0, (LPARAM)L"点击速度必须大于0");
            g_running = false;
            return 0;
        }
        DWORD intervalMs = static_cast<DWORD>(1000.0 / perSec);
        GetWindowTextW(g_edtClickCnt, buf, _countof(buf));
        int total =0;
        bool isInfinite = true;
        if(wcscmp(buf, L"无限") !=0 && wcscmp(buf, L"inf")!=0)
        {
            total = _wtoi(buf);
            isInfinite = false;
        }
        int cnt =0;
        while (g_running)
        {
            if(!isInfinite && cnt >= total)
            {
                g_running = false;
                PostMessageW(g_hMainWnd, WM_USER+200, 0, (LPARAM)L"已完成指定次数点击");
                break;
            }
            if (g_mouseBtn ==0)
            {
                MouseLeftDown();
                SafeSleep(1);
                MouseLeftUp();
            }
            else
            {
                MouseRightDown();
                SafeSleep(1);
                MouseRightUp();
            }
            cnt++;
            SafeSleep(intervalMs);
        }
    }
    else
    {
        wchar_t buf[64];
        GetWindowTextW(g_edtHoldSec, buf, _countof(buf));
        double holdSec = MyWtof(buf);
        if(holdSec <=0)
        {
            PostMessageW(g_hMainWnd, WM_USER+200,0,(LPARAM)L"长按时间必须大于0");
            g_running = false;
            return 0;
        }
        DWORD holdMs = static_cast<DWORD>(holdSec *1000);
        GetWindowTextW(g_edtHoldCnt, buf, _countof(buf));
        int maxLoop = _wtoi(buf);
        if(maxLoop <=0)
        {
            PostMessageW(g_hMainWnd, WM_USER+200,0,(LPARAM)L"长按次数必须大于0");
            g_running = false;
            return 0;
        }
        int loop =0;
        while(g_running)
        {
            if(loop >= maxLoop)
            {
                g_running = false;
                PostMessageW(g_hMainWnd, WM_USER+200,0,(LPARAM)L"已完成指定次数长按");
                break;
            }
            if(g_mouseBtn ==0)
                MouseLeftDown();
            else
                MouseRightDown();
            SafeSleep(holdMs);
            if(g_mouseBtn ==0)
                MouseLeftUp();
            else
                MouseRightUp();
            loop++;
            SafeSleep(100);
        }
    }
    ReleaseAllMouse();
    g_workThread = nullptr;
    return 0;
}
void StartTask(int btn)
{
    if (g_running || g_workThread != nullptr)
    {
        ShowMsg(g_hMainWnd, L"警告", L"正在运行，请先停止！Ctrl+Shift+S停止");
        return;
    }
    g_mouseBtn = btn;
    g_running = true;
    g_workThread = CreateThread(nullptr, 0, WorkThread, nullptr, 0, nullptr);
    // 连点开始：把主窗口隐藏到后台，不遮挡操作目标；按 Ctrl+Shift+S 停止后自动恢复
    ShowWindow(g_hMainWnd, SW_HIDE);
}
void StopTask()
{
    if (!g_running && !g_workThread)
        return;
    g_running = false;
    if(g_workThread)
    {
        WaitForSingleObject(g_workThread, 1000); //最多等待1秒超时
        CloseHandle(g_workThread);
        g_workThread = nullptr;
    }
    ReleaseAllMouse();
    // 停止后恢复窗口并置前
    ShowWindow(g_hMainWnd, SW_SHOW);
    SetForegroundWindow(g_hMainWnd);
    ShowMsg(g_hMainWnd, L"提示", L"已停止操作");
}
void SwitchPanel(int mode)
{
    g_mode = mode;
    bool click = (mode == 0);
    // 切换两个 groupbox 外框
    ShowWindow(g_frameClick, click ? SW_SHOW : SW_HIDE);
    ShowWindow(g_frameHold,  click ? SW_HIDE : SW_SHOW);
    // 关键修复：所有输入控件都是主窗口的直接子窗口且恒为 WS_VISIBLE，
    // 必须在此一并切换显隐，否则非活动面板的控件会与活动面板重叠显示
    ShowWindow(g_lblSpeed,     click ? SW_SHOW : SW_HIDE);
    ShowWindow(g_edtSpeed,     click ? SW_SHOW : SW_HIDE);
    ShowWindow(g_lblClickCnt,  click ? SW_SHOW : SW_HIDE);
    ShowWindow(g_edtClickCnt,  click ? SW_SHOW : SW_HIDE);
    ShowWindow(g_btnLeft,      click ? SW_SHOW : SW_HIDE);
    ShowWindow(g_btnRight,     click ? SW_SHOW : SW_HIDE);
    ShowWindow(g_lblHoldSec,   click ? SW_HIDE : SW_SHOW);
    ShowWindow(g_edtHoldSec,   click ? SW_HIDE : SW_SHOW);
    ShowWindow(g_lblHoldCnt,   click ? SW_HIDE : SW_SHOW);
    ShowWindow(g_edtHoldCnt,   click ? SW_HIDE : SW_SHOW);
    ShowWindow(g_btnLongLeft,  click ? SW_HIDE : SW_SHOW);
    ShowWindow(g_btnLongRight, click ? SW_HIDE : SW_SHOW);
}
LRESULT CALLBACK HotkeyProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY)
    {
        StopTask();
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
void CreateHotkeyWin()
{
    WNDCLASSEXW wc{};
    wc.lpfnWndProc = HotkeyProc;
    wc.lpszClassName = L"HotkeyMsgWin";
    wc.cbSize = sizeof(WNDCLASSEXW);
    RegisterClassExW(&wc);
    g_hotkeyWnd = CreateWindowExW(0, L"HotkeyMsgWin", L"", 0,0,0,0,0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    RegisterHotKey(g_hotkeyWnd, 1, MOD_CONTROL | MOD_SHIFT, 'S');
}
void GetTitleBtnRect(HWND hWnd, RECT &rcMin, RECT &rcClose)
{
    RECT cli;
    GetClientRect(hWnd, &cli);
    int btnW = 52;
    int gap = 6;
    rcClose.left  = cli.right - btnW - 6;
    rcClose.right = cli.right -6;
    rcClose.top = 6;
    rcClose.bottom = TOP_STRIP_HEIGHT -6;
    rcMin.left  = rcClose.left - gap - btnW;
    rcMin.right = rcClose.left - gap;
    rcMin.top   = 6;
    rcMin.bottom= TOP_STRIP_HEIGHT -6;
}
void CreateAllControls(HWND hParent)
{
    // —— 模式切换按钮 ——
    g_btnClickMode = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Click_Button", L"点击模式"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0, hParent, (HMENU)BTN_CLICK_MODE, nullptr, nullptr);
    g_btnLongMode  = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Long_Button", L"长按模式"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0, hParent, (HMENU)BTN_LONG_MODE, nullptr, nullptr);

    // —— 点击模式面板 frame ——
    g_frameClick = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Click_Click_Settings_Text", L"点击设置"), WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_lblSpeed = CreateWindowExW(0, L"STATIC", GetText(L"Clicker:Click_Click_Speed_Text", L"点击速度（次/秒）"), WS_CHILD|WS_VISIBLE|SS_LEFT,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_edtSpeed = CreateWindowExW(0, L"EDIT", L"10", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
        0,0,0,0, hParent, (HMENU)EDIT_SPEED, nullptr, nullptr);
    g_lblClickCnt = CreateWindowExW(0, L"STATIC", GetText(L"Clicker:Click_Click_Number_Text", L"点击次数"), WS_CHILD|WS_VISIBLE|SS_LEFT,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_edtClickCnt = CreateWindowExW(0, L"EDIT", L"无限", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
        0,0,0,0, hParent, (HMENU)EDIT_COUNT, nullptr, nullptr);
    g_btnLeft  = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Click_Left_Click_Button", L"左键连点"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0, hParent, (HMENU)BTN_LEFT, nullptr, nullptr);
    g_btnRight = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Click_Right_Click_Button", L"右键连点"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0, hParent, (HMENU)BTN_RIGHT, nullptr, nullptr);

    // —— 长按模式面板 frame ——
    g_frameHold = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Long_Long_Settings_Text", L"长按设置"), WS_CHILD|BS_GROUPBOX,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_lblHoldSec = CreateWindowExW(0, L"STATIC", GetText(L"Clicker:Long_Long_continue_Text", L"长按持续秒数"), WS_CHILD|WS_VISIBLE|SS_LEFT,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_edtHoldSec = CreateWindowExW(0, L"EDIT", L"0.5", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_lblHoldCnt = CreateWindowExW(0, L"STATIC", GetText(L"Clicker:Long_Long_Number_Text", L"长按次数"), WS_CHILD|WS_VISIBLE|SS_LEFT,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_edtHoldCnt = CreateWindowExW(0, L"EDIT", L"10", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
        0,0,0,0, hParent, nullptr, nullptr, nullptr);
    g_btnLongLeft  = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Long_Left_Long_Button", L"左键长按"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0, hParent, (HMENU)BTN_LONG_LEFT, nullptr, nullptr);
    g_btnLongRight = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Long_Right_Long_Button", L"右键长按"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0, hParent, (HMENU)BTN_LONG_RIGHT, nullptr, nullptr);

    // —— 导出按钮 ——
    g_btnExport = CreateWindowExW(0, L"BUTTON", GetText(L"Clicker:Every_Output_Wcc_Button", L"导出Wcc配置文件"), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0, hParent, (HMENU)BTN_EXPORT, nullptr, nullptr);

    // 启动即切换为点击面板：隐藏长按面板及其全部输入控件，避免“必须点击才刷新”
    SwitchPanel(0);
}
//==================== 动态布局：控件填满窗口、随窗口缩放自适应 ====================
void LayoutControls()
{
    if(!g_hMainWnd) return;
    RECT rc;
    GetClientRect(g_hMainWnd, &rc);
    int W = rc.right;
    int H = rc.bottom;
    if(W <= 0 || H <= 0) return;

    const int M = WINDOW_MARGIN;
    const int gap = CTRL_GAP;

    // —— 顶栏模式按钮行 ——
    int modeY = M;   // 标准标题栏：客户区顶部即内容区，不再有蓝色条
    int modeW = (W - M*2 - gap) / 2;
    MoveWindow(g_btnClickMode, M, modeY, modeW, MODE_BTN_HEIGHT, TRUE);
    MoveWindow(g_btnLongMode,  M + modeW + gap, modeY, modeW, MODE_BTN_HEIGHT, TRUE);

    // —— 导出按钮（底部通栏）——
    int exportY = H - M - EXPORT_BTN_HEIGHT;
    MoveWindow(g_btnExport, M, exportY, W - M*2, EXPORT_BTN_HEIGHT, TRUE);

    // —— 中间面板区域（模式行与导出按钮之间，尽量占满）——
    int frameTop = modeY + MODE_BTN_HEIGHT + gap;
    int frameBot = exportY - gap;
    if(frameBot - frameTop < 120) frameBot = frameTop + 120; // 防止窗口过小时崩溃

    MoveWindow(g_frameClick, M, frameTop, W - M*2, frameBot - frameTop, TRUE);
    MoveWindow(g_frameHold,  M, frameTop, W - M*2, frameBot - frameTop, TRUE);

    // —— 面板内部三行：标签+输入 / 标签+输入 / 左右按钮 ——
    int inL = M + 16;                 // 面板内容左边距
    int inR = W - M - 16;             // 面板内容右边距
    int inW = inR - inL;
    int contentTop = frameTop + 32;   // 避开groupbox标题
    int contentBot = frameBot - 12;
    int usable = contentBot - contentTop;

    int labelW = 260;                 // 字放大后标签列同步加宽，避免文字换行
    int editX = inL + labelW + 20;
    int editW = inW - labelW - 20;

    int rowH = usable / 3;            // 三行等高分布
    int ctrlH = rowH - 14;            // 行内控件高度

    // 第1行：速度/长按持续
    int y1 = contentTop + (rowH - ctrlH) / 2;
    MoveWindow(g_lblSpeed,   inL, y1, labelW, ctrlH, TRUE);
    MoveWindow(g_edtSpeed,   editX, y1, editW, ctrlH, TRUE);
    MoveWindow(g_lblHoldSec, inL, y1, labelW, ctrlH, TRUE);
    MoveWindow(g_edtHoldSec, editX, y1, editW, ctrlH, TRUE);

    // 第2行：次数
    int y2 = contentTop + rowH + (rowH - ctrlH) / 2;
    MoveWindow(g_lblClickCnt, inL, y2, labelW, ctrlH, TRUE);
    MoveWindow(g_edtClickCnt, editX, y2, editW, ctrlH, TRUE);
    MoveWindow(g_lblHoldCnt,  inL, y2, labelW, ctrlH, TRUE);
    MoveWindow(g_edtHoldCnt,  editX, y2, editW, ctrlH, TRUE);

    // 第3行：左右操作按钮
    int y3 = contentTop + rowH*2 + (rowH - ctrlH) / 2;
    int btnW = (inW - gap) / 2;
    MoveWindow(g_btnLeft,      inL, y3, btnW, ctrlH, TRUE);
    MoveWindow(g_btnRight,     inL + btnW + gap, y3, btnW, ctrlH, TRUE);
    MoveWindow(g_btnLongLeft,  inL, y3, btnW, ctrlH, TRUE);
    MoveWindow(g_btnLongRight, inL + btnW + gap, y3, btnW, ctrlH, TRUE);
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateAllControls(hWnd);
        CreateHotkeyWin();
        break;
    }
    case WM_USER+200:
    {
        // 任务自然结束/出错时，窗口此前已被隐藏，这里恢复显示并置前，再弹提示
        ShowWindow(g_hMainWnd, SW_SHOW);
        SetForegroundWindow(g_hMainWnd);
        ShowMsg(hWnd, L"提示", (LPCWSTR)lParam);
        return 0;
    }
    case WM_USER_LOAD_WCC:
    {
        if(!g_delayedWccPath.empty())
        {
            LoadWccFile(g_delayedWccPath);
            g_delayedWccPath.clear();
        }
        break;
    }
    case WM_SHOWWINDOW:
    {
        // 每次窗口显示时强制重排+重绘，确保打开即是填满布局
        LayoutControls();
        InvalidateRect(hWnd, nullptr, TRUE);
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_COMMAND:
    {
        WORD notify = HIWORD(wParam);
        WORD cid = LOWORD(wParam);
        if(notify == BN_CLICKED)
        {
            switch(cid)
            {
            case BTN_CLICK_MODE: SwitchPanel(0); break;
            case BTN_LONG_MODE:  SwitchPanel(1); break;
            case BTN_LEFT:       StartTask(0); break;
            case BTN_RIGHT:      StartTask(1); break;
            case BTN_LONG_LEFT:  StartTask(0); break;
            case BTN_LONG_RIGHT: StartTask(1); break;
            case BTN_EXPORT:     ExportWccConfig(); break;
            }
        }
    }
    break;
    case WM_CLOSE:
    {
        StopTask();
        FreeWccDll();
        if(g_hotkeyWnd)
        {
            UnregisterHotKey(g_hotkeyWnd,1);
            DestroyWindow(g_hotkeyWnd);
            g_hotkeyWnd = nullptr;
        }
        DestroyWindow(hWnd);
        break;
    }
    case WM_DESTROY:
    {
        if(g_hFont) DeleteObject(g_hFont);
        if(g_hTitleBtnFont) DeleteObject(g_hTitleBtnFont);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}
BOOL CALLBACK SetChildFont(HWND hwnd, LPARAM)
{
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    return TRUE;
}
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    SetDpiAware();
    LoadLanguage(GetExeDirectory()); // 依赖 exe 同目录的 zh-cn.json（或任意 *.json），缺失则回退内置默认
    LoadWccDll();
    //解析命令行
    int argc=0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool openWcc = false;
    std::wstring wccFile;
    if(argv)
    {
        for(int i=1;i<argc;i++)
        {
            if(wcscmp(argv[i], L"-wcc")==0 && i+1<argc)
            {
                openWcc = true;
                wccFile = argv[i+1];
                break;
            }
        }
        LocalFree(argv);
    }
    //【放大字体】
    g_hFont = CreateFontW(-40,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"华文中宋");
    g_hTitleBtnFont = CreateFontW(48,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"ClickerWinClass";
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassExW(&wc);
    // 标准系统窗口：默认标题栏（- 最小化 / × 关闭，无 □ 最大化），固定大小不可拉伸
    g_hMainWnd = CreateWindowExW(
        0, wc.lpszClassName, L"连点器",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        820, 860,
        nullptr, nullptr, hInst, nullptr
    );
    ShowWindow(g_hMainWnd, nCmdShow);
    EnumChildWindows(g_hMainWnd, SetChildFont, 0);
    LayoutControls(); // 首次排布填满窗口
    //【开始就刷新】先排布再强制重绘，保证启动即呈现填满布局
    InvalidateRect(g_hMainWnd, nullptr, TRUE);
    UpdateWindow(g_hMainWnd);
    if(openWcc && !wccFile.empty())
    {
        g_delayedWccPath = wccFile;
        PostMessageW(g_hMainWnd, WM_USER_LOAD_WCC,0,0);
    }
    MSG msg{};
    while(GetMessageW(&msg, nullptr,0,0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    FreeWccDll();
    return (int)msg.wParam;
}
