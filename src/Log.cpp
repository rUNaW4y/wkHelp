#include "Log.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace {
std::wstring g_path;
std::mutex g_mutex;
}

void Log::initialize(void* module) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(module), path, MAX_PATH);
    std::wstring value(path);
    const auto slash = value.find_last_of(L"\\/");
    g_path = value.substr(0, slash + 1) + L"wkHelp.log";
    DeleteFileW(g_path.c_str());
    write("=== wkHelp diagnostic log started ===");
}

void Log::write(const char* format, ...) {
    if (g_path.empty()) return;
    char message[2048]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME now{};
    GetLocalTime(&now);
    char line[2300]{};
    sprintf_s(line, sizeof(line),
        "[%02u:%02u:%02u.%03u] %s\r\n",
        now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, message);

    std::lock_guard<std::mutex> lock(g_mutex);
    HANDLE file = CreateFileW(g_path.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written{};
        WriteFile(file, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
        CloseHandle(file);
    }
}
