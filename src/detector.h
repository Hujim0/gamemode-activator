#ifndef DETECTOR_H
#define DETECTOR_H
#include <windows.h>
#include <stdbool.h>
#define MAX_PROCESS_NAME 260
typedef struct {
    DWORD pid;
    char processName[MAX_PROCESS_NAME];
    char windowTitle[1024];
} GameInfo;
bool detect_game(GameInfo *info);
void blacklist_load(const wchar_t *path);
#endif
