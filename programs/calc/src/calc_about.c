#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "calc.h"
#include "calc_about.h"

/* Search current directory and up to 4 parent levels for project files */
static FILE* OpenProjectFile(const char* filename) {
    FILE *f = NULL;
    char path[MAX_PATH], prefix[MAX_PATH] = "";
    int i;

    for (i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "%s%s", prefix, filename);
        f = fopen(path, "r");
        if (f) return f;
        
        strcat(prefix, "../");
    }
    return NULL;
}

/* Utility to read a whole file and convert it to a wide string buffer */
static wchar_t* ReadFileToWideString(const char* filename) {
    FILE *f = OpenProjectFile(filename);
    char *buf;
    wchar_t *wBuf = NULL;
    long size;
    int wSize;

    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = malloc(size + 1);
    if (buf) {
        fread(buf, 1, size, f);
        buf[size] = '\0';

        wSize = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
        wBuf = malloc(wSize * sizeof(wchar_t));
        if (wBuf) MultiByteToWideChar(CP_UTF8, 0, buf, -1, wBuf, wSize);
        
        free(buf);
    }
    fclose(f);
    return wBuf;
}

/* Logic for populating the authors listbox from the AUTHORS file */
static void PopulateAuthorsList(HWND hList) {
    FILE *f = OpenProjectFile("AUTHORS");
    char line[256];
    wchar_t wLine[256];

    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        MultiByteToWideChar(CP_UTF8, 0, line, -1, wLine, 256);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wLine);
    }
    fclose(f);
}

/* Dialog procedure for the About box */
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG:
            PopulateAuthorsList(GetDlgItem(hDlg, IDC_ABOUT_AUTHORS));
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wp));
                return TRUE;
            }
            if (LOWORD(wp) == IDC_ABOUT_LICENSE) {
                wchar_t *licenseText = ReadFileToWideString("LICENSE");
                if (licenseText) {
                    MessageBoxW(hDlg, licenseText, L"Licence", MB_OK);
                    free(licenseText);
                } else {
                    MessageBoxW(hDlg, L"Error: LICENSE file not found.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            break;
    }
    return FALSE;
}

void About_ShowDialog(HWND hwnd) {
    DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ABOUT), hwnd, AboutDlgProc);
}