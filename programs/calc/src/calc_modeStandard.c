/* ────────────────────────────────────────────────────────────────────
   calc_modeStandard.c — Standard mode controls + layout.
   ──────────────────────────────────────────────────────────────────── */
#include "calc_modeStandard.h"

extern HWND make_button(HWND p, int id, const WCHAR *lbl, BOOL def);

static void create_mem_row(HWND hwnd) {
    make_button(hwnd, ID_MC,     L"MC",      FALSE);
    make_button(hwnd, ID_MR,     L"MR",      FALSE);
    make_button(hwnd, ID_MS,     L"MS",      FALSE);
    make_button(hwnd, ID_MPLUS,  L"M+",      FALSE);
    make_button(hwnd, ID_MMINUS, L"M\x2212", FALSE);
}

void mode_std_create(HWND hwnd) {
    create_mem_row(hwnd);
    make_button(hwnd, ID_BACK,    L"\x2190",  FALSE);
    make_button(hwnd, ID_CE,      L"CE",      FALSE);
    make_button(hwnd, ID_CLR,     L"C",       FALSE);
    make_button(hwnd, ID_SIGN,    L"\xb1",    FALSE);
    make_button(hwnd, ID_SQRT,    L"\x221a",  FALSE);
    make_button(hwnd, ID_7,       L"7",       FALSE);
    make_button(hwnd, ID_8,       L"8",       FALSE);
    make_button(hwnd, ID_9,       L"9",       FALSE);
    make_button(hwnd, ID_DIV,     L"/",       FALSE);
    make_button(hwnd, ID_PERCENT, L"%",       FALSE);
    make_button(hwnd, ID_4,       L"4",       FALSE);
    make_button(hwnd, ID_5,       L"5",       FALSE);
    make_button(hwnd, ID_6,       L"6",       FALSE);
    make_button(hwnd, ID_MUL,     L"\xd7",    FALSE);
    make_button(hwnd, ID_RECIP,   L"1/x",     FALSE);
    make_button(hwnd, ID_1,       L"1",       FALSE);
    make_button(hwnd, ID_2,       L"2",       FALSE);
    make_button(hwnd, ID_3,       L"3",       FALSE);
    make_button(hwnd, ID_SUB,     L"\x2212",  FALSE);
    make_button(hwnd, ID_EQ,      L"=",       TRUE);
    make_button(hwnd, ID_0,       L"0",       FALSE);
    make_button(hwnd, ID_DOT,     L".",       FALSE);
    make_button(hwnd, ID_ADD,     L"+",       FALSE);
}

void mode_std_layout(HWND hwnd, Layout *L) {
    int x   = L->disp_x;
    int y   = L->grid_y;
    int bw  = L->bw;
    int bh  = L->bh;
    int g   = L->gap;
    int row = 0;

    move_ctrl(hwnd, ID_MC,     x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MR,     x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MS,     x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MMINUS, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_BACK,    x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_CE,      x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_CLR,     x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SIGN,    x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SQRT,    x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_7,       x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_8,       x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_9,       x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_DIV,     x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_PERCENT, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_4,       x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_5,       x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_6,       x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MUL,     x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_RECIP,   x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_1,       x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_2,       x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_3,       x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SUB,     x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_EQ,      x+(bw+g)*4, y+(bh+g)*row, bw, bh*2+g); row++;

    move_ctrl(hwnd, ID_0,       x+(bw+g)*0, y+(bh+g)*row, bw*2+g, bh);
    move_ctrl(hwnd, ID_DOT,     x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_ADD,     x+(bw+g)*3, y+(bh+g)*row, bw, bh);
}

void mode_std_get_grid(int *cols, int *rows, int *header_h) {
    *cols = 5; *rows = 6; *header_h = 0;
}