#include "detector.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static bool is_known_non_game(const char *name) {
    if (!name || !*name) return false;
    char lower[MAX_PROCESS_NAME];
    int i = 0;
    while (name[i] && i < (int)sizeof(lower) - 1) {
        lower[i] = (char)tolower((unsigned char)name[i]);
        i++;
    }
    lower[i] = '\0';

    static const char *list[] = {
        "firefox.exe", "chrome.exe", "msedge.exe", "opera.exe", "brave.exe",
        "code.exe", "codep.exe",
        "windowsterminal.exe", "cmd.exe", "powershell.exe", "pwsh.exe",
        "explorer.exe", "mstsc.exe",
        "teams.exe", "zoom.exe", "slack.exe",
        "searchhost.exe", "searchapp.exe",
        "startmenuexperiencehost.exe",
        "shellexperiencehost.exe", "sihost.exe",
        "textinputhost.exe", "runtimebroker.exe",
        "taskhostw.exe", "applicationframehost.exe",
        "systemsettings.exe", "settingsapp.exe",
        "rainmeter.exe",
        "wallpaper64.exe", "wallpaper32.exe",
        "discord.exe",
        "spotify.exe",
        "steam.exe", "epicgameslauncher.exe", "battle.net.exe",
        "devenv.exe",
        "idea64.exe", "rider64.exe", "clion64.exe",
        "sublime_text.exe",
        "notepad++.exe",
        "powertoys.exe",
        "xnviewmp.exe",
        "onecommander.exe",
        NULL
    };

    for (int j = 0; list[j]; j++) {
        if (strcmp(lower, list[j]) == 0)
            return true;
    }
    return false;
}

/* ── Blacklist ────────────────────────────────────────────────────────
 * A plain-text file (gamemode_blacklist.txt) listing process names,
 * one per line, that should never trigger game detection. */

#define MAX_BLACKLIST_ENTRIES 64

static char user_blacklist[MAX_BLACKLIST_ENTRIES][MAX_PROCESS_NAME];
static int  user_blacklist_count = 0;

void blacklist_load(const wchar_t *path) {
    user_blacklist_count = 0;
    FILE *f = _wfopen(path, L"r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f) && user_blacklist_count < MAX_BLACKLIST_ENTRIES) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
            continue;

        char *end = p + strlen(p) - 1;
        while (end >= p && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t'))
            *end-- = '\0';

        if (*p == '\0') continue;

        char *dst = user_blacklist[user_blacklist_count];
        int i = 0;
        while (p[i] && i < MAX_PROCESS_NAME - 1) {
            dst[i] = (char)tolower((unsigned char)p[i]);
            i++;
        }
        dst[i] = '\0';
        user_blacklist_count++;
    }
    fclose(f);
}

static bool is_user_blacklisted(const char *name) {
    if (!name || !*name || user_blacklist_count == 0)
        return false;
    char lower[MAX_PROCESS_NAME];
    int i = 0;
    while (name[i] && i < MAX_PROCESS_NAME - 1) {
        lower[i] = (char)tolower((unsigned char)name[i]);
        i++;
    }
    lower[i] = '\0';
    for (int j = 0; j < user_blacklist_count; j++) {
        if (strcmp(lower, user_blacklist[j]) == 0)
            return true;
    }
    return false;
}

bool detect_game(GameInfo *info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));

    HWND hwnd = GetForegroundWindow();
    if (!hwnd || !IsWindowVisible(hwnd))
        return false;

    GetWindowThreadProcessId(hwnd, &info->pid);
    if (info->pid == 0)
        return false;

    wchar_t title[256] = {0};
    GetWindowTextW(hwnd, title, 256);
    WideCharToMultiByte(CP_UTF8, 0, title, -1, info->windowTitle,
                        sizeof(info->windowTitle), NULL, NULL);

    RECT wr;
    if (!GetWindowRect(hwnd, &wr)) return false;

    HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hm) return false;

    MONITORINFO mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(hm, &mi)) return false;

    bool fullscreen =
        wr.left   <= mi.rcMonitor.left   &&
        wr.top    <= mi.rcMonitor.top    &&
        wr.right  >= mi.rcMonitor.right  &&
        wr.bottom >= mi.rcMonitor.bottom;

    if (!fullscreen) return false;

    /* ── Process info ──────────────────────────────────────────
     * Try limited rights first (works even for elevated/system processes).
     * The full path is checked so that ANY process under C:\Windows\ is
     * rejected outright – this catches Start Menu / Search / etc. even
     * if their process names change between Windows builds. */

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info->pid);

    if (hProcess) {
        char path[MAX_PATH];
        DWORD sz = sizeof(path);
        if (QueryFullProcessImageNameA(hProcess, 0, path, &sz)) {
            /* Extract filename for the blacklist */
            char *name = strrchr(path, '\\');
            if (name) {
                strcpy(info->processName, name + 1);
                *name = '\0';    /* truncate to directory for system check */
            } else {
                strcpy(info->processName, path);
            }

            /* Reject anything living under C:\Windows\ (system components) */
            for (int i = 0; path[i]; i++)
                path[i] = (char)tolower((unsigned char)path[i]);
            if (strncmp(path, "c:\\windows\\", 11) == 0) {
                strcpy(info->processName, "system");
                CloseHandle(hProcess);
                return false;
            }
        }
        CloseHandle(hProcess);
    }

    /* Unknown processes cannot be reliably checked against exclusions. */
    if (!info->processName[0]) return false;

    if (is_known_non_game(info->processName))
        return false;

    if (is_user_blacklisted(info->processName))
        return false;

    return fullscreen;
}
