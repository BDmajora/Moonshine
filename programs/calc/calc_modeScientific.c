/* ────────────────────────────────────────────────────────────────────
   calc_modeScientific.c — Scientific mode controls + layout.

   Matches the Windows 7 Calculator reference: the angle radios
   (Degrees / Radians / Grads) sit on the SAME row as the memory
   buttons (MC..M-).  No separate "header" band — that was the bug
   in the previous layout where radios floated above everything.
   ──────────────────────────────────────────────────────────────────── */
#include "calc_modeScientific.h"

extern HWND make_button(HWND p, int id, const WCHAR *lbl, BOOL def);
extern HWND make_radio(HWND p, int id, const WCHAR *lbl, BOOL first);

void mode_sci_create(HWND hwnd) {
    /* Angle radio group — 3 buttons, share row 0 */
    make_radio(hwnd, ID_RAD_DEG,  L"Degrees", TRUE);
    make_radio(hwnd, ID_RAD_RAD,  L"Radians", FALSE);
    make_radio(hwnd, ID_RAD_GRAD, L"Grads",   FALSE);
    SendMessageW(GetDlgItem(hwnd, ID_RAD_DEG), BM_SETCHECK, BST_CHECKED, 0);

    /* Memory row */
    make_button(hwnd, ID_MC,     L"MC",      FALSE);
    make_button(hwnd, ID_MR,     L"MR",      FALSE);
    make_button(hwnd, ID_MS,     L"MS",      FALSE);
    make_button(hwnd, ID_MPLUS,  L"M+",      FALSE);
    make_button(hwnd, ID_MMINUS, L"M\x2212", FALSE);

    /* Row 1: Inv ln ( ) ← CE C ± √   (col 0 stays empty, like Win7) */
    make_button(hwnd, ID_SCI_INV,    L"Inv",      FALSE);
    make_button(hwnd, ID_SCI_LN,     L"ln",       FALSE);
    make_button(hwnd, ID_SCI_LPAREN, L"(",        FALSE);
    make_button(hwnd, ID_SCI_RPAREN, L")",        FALSE);
    make_button(hwnd, ID_BACK,       L"\x2190",   FALSE);
    make_button(hwnd, ID_CE,         L"CE",       FALSE);
    make_button(hwnd, ID_CLR,        L"C",        FALSE);
    make_button(hwnd, ID_SIGN,       L"\xb1",     FALSE);
    make_button(hwnd, ID_SQRT,       L"\x221a",   FALSE);

    /* Row 2: Int sinh sin x² n!  7 8 9 / % */
    make_button(hwnd, ID_SCI_INT,    L"Int",      FALSE);
    make_button(hwnd, ID_SCI_SINH,   L"sinh",     FALSE);
    make_button(hwnd, ID_SCI_SIN,    L"sin",      FALSE);
    make_button(hwnd, ID_SCI_POW2,   L"x\xb2",    FALSE);
    make_button(hwnd, ID_SCI_FACT,   L"n!",       FALSE);
    make_button(hwnd, ID_7,          L"7",        FALSE);
    make_button(hwnd, ID_8,          L"8",        FALSE);
    make_button(hwnd, ID_9,          L"9",        FALSE);
    make_button(hwnd, ID_DIV,        L"/",        FALSE);
    make_button(hwnd, ID_PERCENT,    L"%",        FALSE);

    /* Row 3: dms cosh cos x^y y√x  4 5 6 × 1/x */
    make_button(hwnd, ID_SCI_DMS,    L"dms",      FALSE);
    make_button(hwnd, ID_SCI_COSH,   L"cosh",     FALSE);
    make_button(hwnd, ID_SCI_COS,    L"cos",      FALSE);
    make_button(hwnd, ID_SCI_POWY,   L"x^y",      FALSE);
    make_button(hwnd, ID_SCI_YROOTX, L"y\x221ax", FALSE);
    make_button(hwnd, ID_4,          L"4",        FALSE);
    make_button(hwnd, ID_5,          L"5",        FALSE);
    make_button(hwnd, ID_6,          L"6",        FALSE);
    make_button(hwnd, ID_MUL,        L"\xd7",     FALSE);
    make_button(hwnd, ID_RECIP,      L"1/x",      FALSE);

    /* Row 4: π tanh tan x³ ³√x  1 2 3 − = */
    make_button(hwnd, ID_SCI_PI,     L"\x3c0",    FALSE);
    make_button(hwnd, ID_SCI_TANH,   L"tanh",     FALSE);
    make_button(hwnd, ID_SCI_TAN,    L"tan",      FALSE);
    make_button(hwnd, ID_SCI_POW3,   L"x\xb3",    FALSE);
    make_button(hwnd, ID_SCI_CUBE,   L"\x221bx",  FALSE);
    make_button(hwnd, ID_1,          L"1",        FALSE);
    make_button(hwnd, ID_2,          L"2",        FALSE);
    make_button(hwnd, ID_3,          L"3",        FALSE);
    make_button(hwnd, ID_SUB,        L"\x2212",   FALSE);
    make_button(hwnd, ID_EQ,         L"=",        TRUE);

    /* Row 5: F-E Exp Mod log 10^x  0 (spans 2) . + */
    make_button(hwnd, ID_SCI_FE,     L"F-E",      FALSE);
    make_button(hwnd, ID_SCI_EXP,    L"Exp",      FALSE);
    make_button(hwnd, ID_SCI_MOD,    L"Mod",      FALSE);
    make_button(hwnd, ID_SCI_LOG,    L"log",      FALSE);
    make_button(hwnd, ID_SCI_EXP+1,  L"10\x02e3", FALSE);  /* "10ˣ" */
    make_button(hwnd, ID_0,          L"0",        FALSE);
    make_button(hwnd, ID_DOT,        L".",        FALSE);
    make_button(hwnd, ID_ADD,        L"+",        FALSE);
}

void mode_sci_layout(HWND hwnd, Layout *L) {
    int x  = L->disp_x;
    int y  = L->grid_y;
    int bw = L->bw;
    int bh = L->bh;
    int g  = L->gap;

    /* Row 0: Radio group (5 cols wide), then memory buttons (5 cols).
       Each radio gets ~5/3 cols of space for the labels to fit. */
    {
        int radio_total = bw * 5 + g * 4;     /* width of 5 columns */
        int rw = (radio_total - g * 2) / 3;   /* 3 radios with 2 gaps */
        move_ctrl(hwnd, ID_RAD_DEG,  x,                  y, rw, bh);
        move_ctrl(hwnd, ID_RAD_RAD,  x + rw + g,         y, rw, bh);
        move_ctrl(hwnd, ID_RAD_GRAD, x + (rw + g) * 2,   y, rw, bh);
    }
    move_ctrl(hwnd, ID_MC,     x+(bw+g)*5, y, bw, bh);
    move_ctrl(hwnd, ID_MR,     x+(bw+g)*6, y, bw, bh);
    move_ctrl(hwnd, ID_MS,     x+(bw+g)*7, y, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,  x+(bw+g)*8, y, bw, bh);
    move_ctrl(hwnd, ID_MMINUS, x+(bw+g)*9, y, bw, bh);

    /* Row 1 — col 0 left empty (matches Win7 reference) */
    move_ctrl(hwnd, ID_SCI_INV,    x+(bw+g)*1, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_LN,     x+(bw+g)*2, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_LPAREN, x+(bw+g)*3, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_RPAREN, x+(bw+g)*4, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_BACK,       x+(bw+g)*5, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CE,         x+(bw+g)*6, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CLR,        x+(bw+g)*7, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SIGN,       x+(bw+g)*8, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SQRT,       x+(bw+g)*9, y+(bh+g)*1, bw, bh);

    /* Row 2 */
    move_ctrl(hwnd, ID_SCI_INT,    x+(bw+g)*0, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_SCI_SINH,   x+(bw+g)*1, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_SCI_SIN,    x+(bw+g)*2, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_SCI_POW2,   x+(bw+g)*3, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_SCI_FACT,   x+(bw+g)*4, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_7,          x+(bw+g)*5, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_8,          x+(bw+g)*6, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_9,          x+(bw+g)*7, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_DIV,        x+(bw+g)*8, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_PERCENT,    x+(bw+g)*9, y+(bh+g)*2, bw, bh);

    /* Row 3 */
    move_ctrl(hwnd, ID_SCI_DMS,    x+(bw+g)*0, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_SCI_COSH,   x+(bw+g)*1, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_SCI_COS,    x+(bw+g)*2, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_SCI_POWY,   x+(bw+g)*3, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_SCI_YROOTX, x+(bw+g)*4, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_4,          x+(bw+g)*5, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_5,          x+(bw+g)*6, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_6,          x+(bw+g)*7, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_MUL,        x+(bw+g)*8, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_RECIP,      x+(bw+g)*9, y+(bh+g)*3, bw, bh);

    /* Row 4 — = spans 2 rows in col 9 */
    move_ctrl(hwnd, ID_SCI_PI,     x+(bw+g)*0, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_SCI_TANH,   x+(bw+g)*1, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_SCI_TAN,    x+(bw+g)*2, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_SCI_POW3,   x+(bw+g)*3, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_SCI_CUBE,   x+(bw+g)*4, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_1,          x+(bw+g)*5, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_2,          x+(bw+g)*6, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_3,          x+(bw+g)*7, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_SUB,        x+(bw+g)*8, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_EQ,         x+(bw+g)*9, y+(bh+g)*4, bw, bh*2+g);

    /* Row 5 — 0 spans 2 cols (col 5 + col 6) */
    move_ctrl(hwnd, ID_SCI_FE,     x+(bw+g)*0, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP,    x+(bw+g)*1, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_MOD,    x+(bw+g)*2, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_LOG,    x+(bw+g)*3, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP+1,  x+(bw+g)*4, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_0,          x+(bw+g)*5, y+(bh+g)*5, bw*2+g, bh);
    move_ctrl(hwnd, ID_DOT,        x+(bw+g)*7, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_ADD,        x+(bw+g)*8, y+(bh+g)*5, bw, bh);
}

void mode_sci_get_grid(int *cols, int *rows, int *header_h) {
    /* No separate header band — the radios share row 0 with the
       memory buttons, all inside the standard grid. */
    *cols = 10; *rows = 6; *header_h = 0;
}