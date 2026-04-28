#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include "solitaire.h"

/* --- Constants for XP Dimensions & Spacing --- */
#define X_MARGIN        12
#define Y_MARGIN        6    /* Moved top row up */
#define X_SPACING       79   
#define TAB_Y_TOP       108  /* Tightened gap between rows */
#define STACK_FACE_DOWN 3    
#define STACK_FACE_UP   20   

/* Reverted to original XP sizes */
#define STATUS_BAR_HEIGHT 16 
#define STATUS_FONT_SIZE  15

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
    int i, j, x, current_y, s;
    WCHAR statusText[64];
    RECT rcStatus, rcText;
    HBRUSH hbrWhite;
    HPEN hPen, oldPen;
    HFONT hFont, hOldFont;

    /* --- 1. TOP ROW: STOCK & WASTE --- */
    if (stock_count > 0) {
        for(s = 2; s > 0; s--) {
            cdtDraw(hdc, X_MARGIN - s, Y_MARGIN - s, ID_CARD_BACK, MODE_FACEDOWN, SOL_BG_COLOR);
        }
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, ID_CARD_BACK, MODE_FACEDOWN, SOL_BG_COLOR);
    } else {
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);
    }

    if (waste_count > 0) {
        cdtDraw(hdc, X_MARGIN + X_SPACING, Y_MARGIN, 0, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* --- 2. TOP ROW: FOUNDATIONS --- */
    for (i = 0; i < 4; i++) {
        x = X_MARGIN + ((3 + i) * X_SPACING);
        if (foundation_count[i] == 0)
            cdtDraw(hdc, x, Y_MARGIN, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);
        else
            cdtDraw(hdc, x, Y_MARGIN, 0, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* --- 3. TABLEAU --- */
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
    rcStatus.top = height - STATUS_BAR_HEIGHT; 
    rcStatus.right = width;
    rcStatus.bottom = height;

    hbrWhite = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rcStatus, hbrWhite);
    DeleteObject(hbrWhite);

    hPen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    oldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, 0, height - STATUS_BAR_HEIGHT, NULL);
    LineTo(hdc, width, height - STATUS_BAR_HEIGHT);
    /* FIXED: Corrected SelectObject arguments to avoid build error */
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);

    hFont = CreateFontW(STATUS_FONT_SIZE, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
    
    hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    
    rcText = rcStatus;
    rcText.right -= 12; 
    
    wsprintfW(statusText, L"Score: 0   Time: 0");
    DrawTextW(hdc, statusText, -1, &rcText, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}