#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "detector.h"

static wchar_t state_path[32768], temp_path[32768];
static char previous[2048];
static ULONGLONG last_write;
static unsigned interval = 2000;
static int exit_code;

/* Replace the whole UTF-8 snapshot so readers cannot see partial records. */
static BOOL publish(const char *active, const char *title, BOOL force) {
    char state[2048], record[2304];
    size_t n = (size_t)snprintf(state, sizeof state, "active=%s\ngame=\"", active);
    for (const unsigned char *s = (const unsigned char *)title; *s && n + 4 < sizeof state; s++) {
        if (*s < 32 || *s == 127) { state[n++] = ' '; continue; }
        if (strchr("\\\"$`", *s)) state[n++] = '\\';
        state[n++] = (char)*s;
    }
    state[n++] = '"'; state[n++] = '\n'; state[n] = 0;
    ULONGLONG now = GetTickCount64();
    if (!force && !strcmp(state, previous) && now - last_write < 30000) return TRUE;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER ticks;
    ticks.LowPart = ft.dwLowDateTime; ticks.HighPart = ft.dwHighDateTime;
    unsigned long long epoch = (ticks.QuadPart - 116444736000000000ULL) / 10000000ULL;
    int len = snprintf(record, sizeof record, "%supdated_at=%llu\n", state, epoch);
    HANDLE file = CreateFileW(temp_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    BOOL ok = WriteFile(file, record, (DWORD)len, &written, NULL);
    CloseHandle(file);
    if (!ok || written != (DWORD)len) { DeleteFileW(temp_path); return FALSE; }
    if (!MoveFileExW(temp_path, state_path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temp_path); return FALSE;
    }
    strcpy(previous, state); last_write = now;
    return TRUE;
}

static void sample(void) {
    GameInfo info;
    BOOL active = detect_game(&info);
    if (!publish(active ? "true" : "false", active ? info.windowTitle : "", FALSE))
        exit_code = 1; /* Retry at next sample; freshness reveals persistent failure. */
    else exit_code = 0;
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
        case WM_TIMER: sample(); return 0;
        case WM_QUERYENDSESSION: return TRUE;
        case WM_ENDSESSION:
            if (wp) publish("unknown", "", TRUE);
            return 0;
        case WM_CLOSE: DestroyWindow(window); return 0;
        case WM_DESTROY:
            KillTimer(window, 1);
            publish("unknown", "", TRUE);
            PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command, int show) {
    (void)previous_instance; (void)command; (void)show;
    int argc = __argc;
    wchar_t **argv = __wargv;
    DWORD count = GetModuleFileNameW(NULL, state_path, 32768);
    if (!count || count >= 32768) { return 1; }
    wchar_t *slash = wcsrchr(state_path, L'\\');
    if (!slash || slash - state_path + 32 >= 32768) { return 1; }
    wcscpy(slash + 1, L"gamemode_blacklist.txt");
    blacklist_load(state_path);
    wcscpy(slash + 1, L"gamestatus");
    BOOL once = FALSE;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--once")) once = TRUE;
        else if (!wcscmp(argv[i], L"--interval") && i + 1 < argc) {
            wchar_t *end;
            unsigned long seconds = wcstoul(argv[++i], &end, 10);
            if (*end || seconds < 1 || seconds > 60) { return 2; }
            interval = (unsigned)seconds * 1000;
        } else { return 2; }
    }
    HANDLE mutex = CreateMutexW(NULL, FALSE, L"Local\\GameStateMonitor");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(mutex); return 0; }
    if (swprintf(temp_path, 32768, L"%ls.tmp", state_path) < 0) { CloseHandle(mutex); return 1; }
    /* Match monitor bounds across mixed-DPI displays without virtualization. */
    SetProcessDPIAware();
    if (once) { sample(); CloseHandle(mutex); return exit_code; }
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = window_proc; wc.hInstance = instance;
    wc.lpszClassName = L"GameStateMonitor";
    if (!RegisterClassW(&wc)) { CloseHandle(mutex); return 1; }
    HWND window = CreateWindowW(wc.lpszClassName, L"Game State Monitor", 0,
                                0, 0, 0, 0, NULL, NULL, instance, NULL);
    if (!window) { CloseHandle(mutex); return 1; }
    sample();
    if (!SetTimer(window, 1, interval, NULL)) {
        DestroyWindow(window); CloseHandle(mutex); return 1;
    }
    MSG msg;
    int result;
    while ((result = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    CloseHandle(mutex);
    return result < 0 ? 1 : exit_code;
}
