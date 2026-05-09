#include "calc_ui.h"

/* ── Shared geometry — must match GM_* in calc_appMenu.c ──────────── */
#define LY_BTN_W    44
#define LY_BTN_H    34
#define LY_RADIO_W  (LY_BTN_W + LY_BTN_W / 2)  /* programmer radio col */

void layout_standard(HWND hwnd, Layout *L) {
    int x   = L->disp_x;
    int y   = L->grid_y;
    int bw  = L->bw;
    int bh  = L->bh;
    int g   = L->gap;
    int row = 0;

    if (!g_basic_mode) {
        move_ctrl(hwnd, ID_MC,     x+(bw+g)*0, y+(bh+g)*row, bw, bh);
        move_ctrl(hwnd, ID_MR,     x+(bw+g)*1, y+(bh+g)*row, bw, bh);
        move_ctrl(hwnd, ID_MS,     x+(bw+g)*2, y+(bh+g)*row, bw, bh);
        move_ctrl(hwnd, ID_MPLUS,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
        move_ctrl(hwnd, ID_MMINUS, x+(bw+g)*4, y+(bh+g)*row, bw, bh);
        row = 1;
    }
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

void layout_scientific(HWND hwnd, Layout *L) {
    int x  = L->disp_x;
    int y0 = L->disp_y + L->disp_h + L->margin;
    int bw = L->bw;
    int bh = L->bh;
    int g  = L->gap;
    int y  = L->grid_y;

    move_ctrl(hwnd, ID_RAD_DEG,  x,          y0, bw*2,   bh);
    move_ctrl(hwnd, ID_RAD_RAD,  x+bw*2+g,   y0, bw*2,   bh);
    move_ctrl(hwnd, ID_RAD_GRAD, x+bw*4+g*2, y0, bw*2,   bh);

    move_ctrl(hwnd, ID_MC,           x+(bw+g)*5, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MR,           x+(bw+g)*6, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MS,           x+(bw+g)*7, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,        x+(bw+g)*8, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MMINUS,       x+(bw+g)*9, y+(bh+g)*0, bw, bh);

    move_ctrl(hwnd, ID_SCI_INV,    x+(bw+g)*0, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_LN,     x+(bw+g)*1, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_LPAREN, x+(bw+g)*2, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_RPAREN, x+(bw+g)*3, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_BACK,       x+(bw+g)*4, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CE,         x+(bw+g)*5, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CLR,        x+(bw+g)*6, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SIGN,       x+(bw+g)*7, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SQRT,       x+(bw+g)*8, y+(bh+g)*1, bw, bh);

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

    move_ctrl(hwnd, ID_SCI_FE,     x+(bw+g)*0, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP,    x+(bw+g)*1, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_MOD,    x+(bw+g)*2, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_LOG,    x+(bw+g)*3, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP+1,  x+(bw+g)*4, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_0,          x+(bw+g)*5, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_DOT,        x+(bw+g)*6, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_ADD,        x+(bw+g)*7, y+(bh+g)*5, bw, bh);
}

void layout_programmer(HWND hwnd, Layout *L) {
    int x, g, y, bw, bh, rcw, gx, bit_y, bit_h, ry;

    x   = L->disp_x;
    g   = L->gap;
    y   = L->grid_y;

    /* Fixed geometry — matches GM_* in calc_window.c */
    bw  = LY_BTN_W;
    bh  = LY_BTN_H;
    rcw = LY_RADIO_W;       /* radio sidebar width */
    gx  = x + rcw + g;      /* button grid left edge */

    /* Bit strip: full grid width, above the radios */
    bit_y = L->disp_y + L->disp_h + g;
    bit_h = 20;
    move_ctrl(hwnd, ID_DISP_BITS, x, bit_y, L->grid_w, bit_h);

    /* Radio columns — stacked down the left sidebar */
    ry = bit_y + bit_h + g;
    move_ctrl(hwnd, ID_RADIO_HEX,   x, ry,              rcw, bh);
    move_ctrl(hwnd, ID_RADIO_DEC,   x, ry+(bh+g),       rcw, bh);
    move_ctrl(hwnd, ID_RADIO_OCT,   x, ry+(bh+g)*2,     rcw, bh);
    move_ctrl(hwnd, ID_RADIO_BIN,   x, ry+(bh+g)*3,     rcw, bh);
    move_ctrl(hwnd, ID_RADIO_QWORD, x, ry+(bh+g)*4+g,   rcw, bh);
    move_ctrl(hwnd, ID_RADIO_DWORD, x, ry+(bh+g)*5+g,   rcw, bh);
    move_ctrl(hwnd, ID_RADIO_WORD,  x, ry+(bh+g)*6+g,   rcw, bh);
    move_ctrl(hwnd, ID_RADIO_BYTE,  x, ry+(bh+g)*7+g,   rcw, bh);

    /* Button grid: 8 columns, 6 rows, starts at grid_y */
    move_ctrl(hwnd, ID_MC,     gx+(bw+g)*3, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MR,     gx+(bw+g)*4, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MS,     gx+(bw+g)*5, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,  gx+(bw+g)*6, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MMINUS, gx+(bw+g)*7, y+(bh+g)*0, bw, bh);

    move_ctrl(hwnd, ID_PROG_MOD, gx+(bw+g)*0, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_A,        gx+(bw+g)*1, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_BACK,     gx+(bw+g)*2, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CE,       gx+(bw+g)*3, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CLR,      gx+(bw+g)*4, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SIGN,     gx+(bw+g)*5, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SQRT,     gx+(bw+g)*6, y+(bh+g)*1, bw, bh);

    move_ctrl(hwnd, ID_PROG_ROL, gx+(bw+g)*0, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_PROG_ROR, gx+(bw+g)*1, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_B,        gx+(bw+g)*2, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_7,        gx+(bw+g)*3, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_8,        gx+(bw+g)*4, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_9,        gx+(bw+g)*5, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_DIV,      gx+(bw+g)*6, y+(bh+g)*2, bw, bh);
    move_ctrl(hwnd, ID_PERCENT,  gx+(bw+g)*7, y+(bh+g)*2, bw, bh);

    move_ctrl(hwnd, ID_PROG_OR,  gx+(bw+g)*0, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_PROG_XOR, gx+(bw+g)*1, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_C_HEX,    gx+(bw+g)*2, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_4,        gx+(bw+g)*3, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_5,        gx+(bw+g)*4, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_6,        gx+(bw+g)*5, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_MUL,      gx+(bw+g)*6, y+(bh+g)*3, bw, bh);
    move_ctrl(hwnd, ID_RECIP,    gx+(bw+g)*7, y+(bh+g)*3, bw, bh);

    move_ctrl(hwnd, ID_PROG_LSH, gx+(bw+g)*0, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_PROG_RSH, gx+(bw+g)*1, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_D,        gx+(bw+g)*2, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_1,        gx+(bw+g)*3, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_2,        gx+(bw+g)*4, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_3,        gx+(bw+g)*5, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_SUB,      gx+(bw+g)*6, y+(bh+g)*4, bw, bh);
    move_ctrl(hwnd, ID_EQ,       gx+(bw+g)*7, y+(bh+g)*4, bw, bh*2+g);

    move_ctrl(hwnd, ID_PROG_NOT, gx+(bw+g)*0, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_PROG_AND, gx+(bw+g)*1, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_E_HEX,    gx+(bw+g)*2, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_0,        gx+(bw+g)*3, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_DOT,      gx+(bw+g)*4, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_ADD,      gx+(bw+g)*5, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_F,        gx+(bw+g)*6, y+(bh+g)*5, bw, bh);
}

void layout_statistics(HWND hwnd, Layout *L) {
    int x      = L->disp_x;
    int y0     = L->disp_y + L->disp_h + L->margin;
    int bw     = L->bw;
    int bh     = L->bh;
    int g      = L->gap;
    int list_h = (int)(L->win_h * 0.22);
    int list_w = L->grid_w - bh - g;
    int y      = y0 + list_h + g;
    int row    = 0;

    move_ctrl(hwnd, ID_DISP_HISTORY, x, y0, list_w, list_h);
    move_ctrl(hwnd, 400, x+list_w+g, y0,                bh, list_h/2-g/2);
    move_ctrl(hwnd, 401, x+list_w+g, y0+list_h/2+g/2,   bh, list_h/2-g/2);

    move_ctrl(hwnd, ID_MC,         x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MR,         x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MS,         x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,      x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MMINUS,     x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;
    move_ctrl(hwnd, ID_BACK,       x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_CAD,   x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_CLR,        x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SCI_FE,     x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP,    x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;
    move_ctrl(hwnd, ID_7,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_8,          x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_9,          x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_MEAN,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_MEAN2, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;
    move_ctrl(hwnd, ID_4,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_5,          x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_6,          x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SUMX,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SUMX2, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;
    move_ctrl(hwnd, ID_1,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_2,          x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_3,          x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SDEV,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SDEV1, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;
    move_ctrl(hwnd, ID_0,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_DOT,        x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SIGN,       x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_ADD,   x+(bw+g)*3, y+(bh+g)*row, bw*2+g, bh);
}

void layout_panel(HWND hwnd, Layout *L) {
    int px   = L->panel_x + L->margin;
    int pw   = L->panel_w - L->margin * 2;
    int py   = L->margin;
    int lh   = 18;
    int eh   = 24;
    int ch   = 24;
    int bh2  = 28;
    int g    = L->gap;
    int wide = pw;
    int i;
    static const int lbl_ids[] = {ID_WS_LBL1,ID_WS_LBL2,ID_WS_LBL3,
                                   ID_WS_LBL4,ID_WS_LBL5,ID_WS_LBL6};
    static const int inp_ids[] = {ID_WS_IN1,ID_WS_IN2,ID_WS_IN3,
                                   ID_WS_IN4,ID_WS_IN5,ID_WS_IN6};

    switch (g_panel) {
        case PANEL_UNIT:
            move_ctrl(hwnd, 350,              px, py, wide, lh); py += lh+g;
            move_ctrl(hwnd, ID_UNIT_TYPE,     px, py, wide, ch); py += ch+g*2;
            move_ctrl(hwnd, 351,              px, py, wide, lh); py += lh+g;
            move_ctrl(hwnd, ID_UNIT_FROM_VAL, px, py, wide, eh); py += eh+g;
            move_ctrl(hwnd, ID_UNIT_FROM_UNT, px, py, wide, ch); py += ch+g*2;
            move_ctrl(hwnd, 352,              px, py, wide, lh); py += lh+g;
            move_ctrl(hwnd, ID_UNIT_TO_VAL,   px, py, wide, eh); py += eh+g;
            move_ctrl(hwnd, ID_UNIT_TO_UNT,   px, py, wide, ch);
            break;

        case PANEL_DATE:
            move_ctrl(hwnd, 353,          px,          py, wide,   lh); py += lh+g;
            move_ctrl(hwnd, ID_DATE_TYPE, px,          py, wide,   ch); py += ch+g*2;
            move_ctrl(hwnd, 354,          px,          py, wide/2, lh);
            move_ctrl(hwnd, 355,          px+wide/2+g, py, wide/2, lh);
            py += lh+g;
            move_ctrl(hwnd, ID_DATE_FROM, px,          py, wide/2, eh);
            move_ctrl(hwnd, ID_DATE_TO,   px+wide/2+g, py, wide/2, eh);
            py += eh+g*2;
            move_ctrl(hwnd, ID_DATE_CALC, px, py, wide,   bh2); py += bh2+g*2;
            move_ctrl(hwnd, ID_DATE_RES1, px, py, wide,   lh*2); py += lh*2+g;
            move_ctrl(hwnd, ID_DATE_RES2, px, py, wide,   lh);
            break;

        case PANEL_HISTORY:
            move_ctrl(hwnd, 356,            px, py, wide, lh); py += lh+g;
            move_ctrl(hwnd, ID_PANEL_COMBO, px, py, wide,
                      L->win_h - py - bh2 - g*2 - L->margin);
            py = L->win_h - bh2 - L->margin;
            move_ctrl(hwnd, ID_PANEL_CLOSE, px, py, wide, bh2);
            break;

        case PANEL_WORKSHEET: {
            HWND hc;
            int lw, iw;
            
            move_ctrl(hwnd, 357,          px, py, wide, lh * 2); py += lh * 2 + g;
            move_ctrl(hwnd, ID_WS_COMBO,  px, py, wide, ch); py += ch + g * 2;
            
            hc = GetDlgItem(hwnd, ID_WS_COMBO);
            if (hc) SendMessageW(hc, CB_SETDROPPEDWIDTH, wide, 0);
            
            /* Inputs only hold numbers; 75px is sufficient. 
               This grants labels the maximum available horizontal space. */
            iw = 75;
            lw = wide - iw - g;
            
            for (i = 0; i < 6; i++) {
                HWND hl = GetDlgItem(hwnd, lbl_ids[i]);
                HWND hi = GetDlgItem(hwnd, inp_ids[i]);
                if (hl && hi) {
                    move_ctrl(hwnd, lbl_ids[i], px,          py, lw, lh);
                    move_ctrl(hwnd, inp_ids[i], px + lw + g, py, iw, lh);
                    py += lh + g;
                }
            }
            
            move_ctrl(hwnd, ID_WS_CALC, px, py, wide, lh + 6); py += lh + 6 + g;
            move_ctrl(hwnd, ID_WS_RES,  px, py, wide, lh * 2);
            break;
        }
    }
}