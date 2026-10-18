#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <string>
#include <sstream>
#include <cwchar>

enum TaskType
{
    TASK_NONE,
    TASK_CLICK,
    TASK_LONG
};

struct WccTask
{
    TaskType type = TASK_NONE;
    int speedMs = 100;    // 内部:点击间隔(毫秒)
    int infinite = 1;    // 1=无限 0=有限
    int count = 0;
    int button = 0;       // 0左键 1右键
};

// wstring → UTF‑8 std::string
static std::string WStringToUtf8(const std::wstring& wstr)
{
    if(wstr.empty()) return "";
    int bufLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string res(bufLen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &res[0], bufLen, nullptr, nullptr);
    return res;
}

// UTF‑8 bytes → wstring
static std::wstring Utf8ToWString(const std::string &utf8)
{
    if(utf8.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),(int)utf8.size(),nullptr,0);
    std::wstring wstr(wlen,0);
    MultiByteToWideChar(CP_UTF8,0,utf8.c_str(),(int)utf8.size(),&wstr[0],wlen);
    return wstr;
}

static std::wstring Trim(const std::wstring &s)
{
    size_t start = s.find_first_not_of(L" \t\r\n");
    if(start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

extern "C" __declspec(dllexport)
int ExportWccConfig(const wchar_t* filePath, const WccTask* task)
{
    if(!task) return 0;

    // 由毫秒间隔 → 每秒点击次数(CPS)
    int cps;
    if(task->speedMs <= 0)
    {
        cps = 10;
    }
    else
    {
        cps = (int)(1000.0 / task->speedMs + 0.5); // +0.5四舍五入
    }

    std::wstring content;

    if(task->type == TASK_CLICK)
    {
        content += L"[/click\\]\r\n";
    }
    else if(task->type == TASK_LONG)
    {
        content += L"[/long\\]\r\n";
    }

    // 直接写入CPS，你UI填100次每秒，文件speed=100
    content += L"speed = " + std::to_wstring(cps) + L"\r\n";

    if(task->infinite != 0)
    {
        content += L"count = inf\r\n";
    }
    else
    {
        content += L"count = " + std::to_wstring(task->count) + L"\r\n";
    }

    if(task->button == 0)
        content += L"left\r\n";
    else
        content += L"right\r\n";

    FILE* fp = _wfopen(filePath, L"wb");
    if(!fp) return 0;
    std::string utf8Text = WStringToUtf8(content);
    fwrite(utf8Text.data(),1,utf8Text.size(),fp);
    fclose(fp);
    return 1;
}

extern "C" __declspec(dllexport)
int LoadWccConfig(const wchar_t* filePath, WccTask* outTask)
{
    if(!outTask) return 0;

    FILE* fp = _wfopen(filePath, L"rb");
    if(!fp) return 0;
    std::string byteContent;
    char buf[4096];
    size_t n;
    while((n = fread(buf, 1, 4096, fp)) > 0)
    {
        byteContent.append(buf,n);
    }
    fclose(fp);

    std::wstring wContent = Utf8ToWString(byteContent);

    std::wistringstream wss(wContent);
    std::wstring line;

    *outTask = WccTask{};

    while(std::getline(wss, line))
    {
        std::wstring t = Trim(line);
        if(t.size() >= 2 && t.substr(0,2) == L"\\/")
            continue;

        if(t.empty()) continue;

        if(t == L"[/click\\]")
        {
            outTask->type = TASK_CLICK;
            continue;
        }
        if(t == L"[/long\\]")
        {
            outTask->type = TASK_LONG;
            continue;
        }

        size_t eqPos = t.find(L'=');
        if(eqPos != std::wstring::npos)
        {
            std::wstring key = Trim(t.substr(0, eqPos));
            std::wstring val = Trim(t.substr(eqPos+1));
            if(key == L"speed")
            {
                int cps = _wtoi(val.c_str());
                if(cps <= 0) cps = 10;
                // 每秒次数 → 内部毫秒间隔
                outTask->speedMs = (int)(1000.0 / cps + 0.5);
            }
            else if(key == L"count")
            {
                if(val == L"inf")
                {
                    outTask->infinite = 1;
                    outTask->count = 0;
                }
                else
                {
                    outTask->infinite = 0;
                    outTask->count = _wtoi(val.c_str());
                }
            }
        }
        else if(t == L"left")
        {
            outTask->button = 0;
        }
        else if(t == L"right")
        {
            outTask->button = 1;
        }
    }
    return 1;
}