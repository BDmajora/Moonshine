#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "solitaire.h"
#include "sol_about.h"

/* 
 * Robust File Finder: Searches current dir, then climbs up 
 * to 4 levels to find the project root where AUTHORS/LICENSE live.
 */
static FILE* OpenProjectFile(const char* filename) {
    FILE *f = NULL;
    char path[MAX_PATH];
    char prefix[MAX_PATH] = "";
    int i;

    for (i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "%s%s", prefix, filename);
        f = fopen(path, "r");
        if (f) return f;
        
        /* Append another "../" to the search prefix */
        strcat(prefix, "../");
    }
    return NULL;
}

/* SRP: Handles the specific logic of the About Dialog */
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            HWND hList = GetDlgItem(hDlg, IDC_ABOUT_AUTHORS);
            FILE *f = OpenProjectFile("AUTHORS");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    wchar_t wLine[256];
                    MultiByteToWideChar(CP_UTF8, 0, line, -1, wLine, 256);
                    SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wLine);
                }
                fclose(f);
            }
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wp));
                return TRUE;
            }
            if (LOWORD(wp) == IDC_ABOUT_LICENSE) {
                FILE *f = OpenProjectFile("LICENSE");
                if (f) {
                    long size;
                    char *buf;
                    int wSize;
                    wchar_t *wBuf;

                    fseek(f, 0, SEEK_END);
                    size = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    buf = malloc(size + 1);
                    if (buf) {
                        fread(buf, 1, size, f);
                        buf[size] = '\0';

                        wSize = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
                        wBuf = malloc(wSize * sizeof(wchar_t));
                        if (wBuf) {
                            MultiByteToWideChar(CP_UTF8, 0, buf, -1, wBuf, wSize);
                            MessageBoxW(hDlg, wBuf, L"Licence", MB_OK);
                            free(wBuf);
                        }
                        free(buf);
                    }
                    fclose(f);
                } else {
                    /* If this triggers, the LICENSE file isn't in any of the expected parents */
                    MessageBoxW(hDlg, L"Critical: LICENSE file not found in project tree.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            break;
    }
    return FALSE;
}

void About_ShowDialog(HWND hwnd) {
    DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ABOUT), hwnd, AboutDlgProc);
}