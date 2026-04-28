#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include "solitaire.h"

/* --- Constants for XP Dimensions & Spacing --- */
#define X_MARGIN        12
#define Y_MARGIN        12
#define X_SPACING       82   
#define TAB_Y_TOP       110  
#define STACK_FACE_DOWN 3    
#define STACK_FACE_UP   15   

#define ID_GHOST_CARD   52   
#define ID_CARD_BACK    54   

/* --- Global Game State --- */
CARD_STACK tableau[7][20]; 
int tableau_count[7];
int foundation_count[4] = {0, 0, 0, 0};
int stock_count = 24;
int waste_count = 0;

void InitGame(void) {
    int i, j;
    srand((unsigned int)time(NULL));
    for (i = 0; i < 7; i++) {
        tableau_count[i] = i + 1;
        for (j = 0; j <= i; j++) {
            tableau[i][j].card_id = rand() % 52;
            tableau[i][j].face_up = (j == i); 
        }
    }
}

void DrawBoard(HDC hdc, int width, int height) {
    /* C90: All declarations must be at the top of the function */
    int i, j, x, current_y, s;
    WCHAR statusText[64];
    RECT rcStatus;
    HBRUSH hbrWhite;
    HPEN hPen, oldPen;
    HFONT hFont, hOldFont;

    /* --- 1. TOP ROW: STOCK (3D Stacking Effect) --- */
    if (stock_count > 0) {
        /* Draw 2 offset 'depth' cards before the top card */
        for(s = 2; s > 0; s--) {
            cdtDraw(hdc, X_MARGIN + s, Y_MARGIN + s, ID_CARD_BACK, MODE_FACEDOWN, SOL_BG_COLOR);
        }
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, ID_CARD_BACK, MODE_FACEDOWN, SOL_BG_COLOR);
    } else {
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* Waste Pile (Column 1) */
    if (waste_count > 0) {
        cdtDraw(hdc, X_MARGIN + X_SPACING, Y_MARGIN, 0, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* --- 2. TOP ROW: FOUNDATIONS (Columns 3-6) --- */
    for (i = 0; i < 4; i++) {
        x = X_MARGIN + ((3 + i) * X_SPACING);
        if (foundation_count[i] == 0)
            cdtDraw(hdc, x, Y_MARGIN, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);
        else
            cdtDraw(hdc, x, Y_MARGIN, 0, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* --- 3. TABLEAU: THE 7 COLUMNS --- */
    for (i = 0; i < 7; i++) {
        x = X_MARGIN + (i * X_SPACING);
        if (tableau_count[i] == 0) {
            cdtDraw(hdc, x, TAB_Y_TOP, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);
            continue;
        }
        current_y = TAB_Y_TOP;
        for (j = 0; j < tableau_count[i]; j++) {
            if (tableau[i][j].face_up) {
                cdtDraw(hdc, x, current_y, tableau[i][j].card_id, MODE_FACEUP, SOL_BG_COLOR);
                current_y += STACK_FACE_UP;
            } else {
                cdtDraw(hdc, x, current_y, ID_CARD_BACK, MODE_FACEDOWN, SOL_BG_COLOR);
                current_y += STACK_FACE_DOWN;
            }
        }
    }

    /* --- 4. STATUS BAR --- */
    rcStatus.left = 0;
    rcStatus.top = height - 18;
    rcStatus.right = width;
    rcStatus.bottom = height;

    hbrWhite = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rcStatus, hbrWhite);
    DeleteObject(hbrWhite);

    /* Grey separator line for depth */
    hPen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    oldPen = SelectObject(hdc, hPen);
    MoveToEx(hdc, 0, height - 18, NULL);
    LineTo(hdc, width, height - 18);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);

    hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
    
    hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    
    wsprintfW(statusText, L"Score: 0   Time: 0");
    TextOutW(hdc, width - 125, height - 16, statusText, lstrlenW(statusText));

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}