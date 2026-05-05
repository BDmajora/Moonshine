#include "calc_ui.h"

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
    int x   = L->disp_x;
    int y0  = L->disp_y + L->disp_h + L->margin;
    int bw  = L->bw;
    int bh  = L->bh;
    int g   = L->gap;
    int rh  = bh;
    int y;

    move_ctrl(hwnd, ID_RAD_DEG,  x,           y0, bw*2,   rh);
    move_ctrl(hwnd, ID_RAD_RAD,  x+bw*2+g,    y0, bw*2,   rh);
    move_ctrl(hwnd, ID_RAD_GRAD, x+bw*4+g*2,  y0, bw*2,   rh);
    y = L->grid_y;

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
    int x    = L->disp_x;
    int bw   = L->bw;
    int bh   = L->bh;
    int g    = L->gap;
    int bit_y  = L->disp_y + L->disp_h + g;
    int bit_h  = (int)(L->win_h * 0.09);
    int rh     = bh;
    int radio_y = bit_y + bit_h + g;
    int y       = radio_y + rh * 2 + g * 2;
    int rx      = x;
    int rcw     = bw + bw / 2;
    int gx      = x + rcw + g;
    int gw      = bw;

    move_ctrl(hwnd, ID_DISP_BITS,    x, bit_y, L->grid_w, bit_h);

    move_ctrl(hwnd, ID_RADIO_HEX,    rx, radio_y,             rcw, rh);
    move_ctrl(hwnd, ID_RADIO_DEC,    rx, radio_y+rh+g,        rcw, rh);
    move_ctrl(hwnd, ID_RADIO_OCT,    rx, radio_y+rh*2+g*2,    rcw, rh);
    move_ctrl(hwnd, ID_RADIO_BIN,    rx, radio_y+rh*3+g*3,    rcw, rh);

    move_ctrl(hwnd, ID_RADIO_QWORD,  rx, radio_y+rh*4+g*5,    rcw, rh);
    move_ctrl(hwnd, ID_RADIO_DWORD,  rx, radio_y+rh*5+g*6,    rcw, rh);
    move_ctrl(hwnd, ID_RADIO_WORD,   rx, radio_y+rh*6+g*7,    rcw, rh);
    move_ctrl(hwnd, ID_RADIO_BYTE,   rx, radio_y+rh*7+g*8,    rcw, rh);

    move_ctrl(hwnd, ID_MC,     gx+(gw+g)*3, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MR,     gx+(gw+g)*4, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MS,     gx+(gw+g)*5, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MPLUS,  gx+(gw+g)*6, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MMINUS, gx+(gw+g)*7, y+(bh+g)*0, gw, bh);

    move_ctrl(hwnd, ID_PROG_MOD, gx+(gw+g)*0, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_A,        gx+(gw+g)*1, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_BACK,     gx+(gw+g)*2, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_CE,       gx+(gw+g)*3, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_CLR,      gx+(gw+g)*4, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_SIGN,     gx+(gw+g)*5, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_SQRT,     gx+(gw+g)*6, y+(bh+g)*1, gw, bh);

    move_ctrl(hwnd, ID_PROG_ROL, gx+(gw+g)*0, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_PROG_ROR, gx+(gw+g)*1, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_B,        gx+(gw+g)*2, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_7,        gx+(gw+g)*3, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_8,        gx+(gw+g)*4, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_9,        gx+(gw+g)*5, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_DIV,      gx+(gw+g)*6, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_PERCENT,  gx+(gw+g)*7, y+(bh+g)*2, gw, bh);

    move_ctrl(hwnd, ID_PROG_OR,  gx+(gw+g)*0, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_PROG_XOR, gx+(gw+g)*1, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_C_HEX,    gx+(gw+g)*2, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_4,        gx+(gw+g)*3, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_5,        gx+(gw+g)*4, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_6,        gx+(gw+g)*5, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_MUL,      gx+(gw+g)*6, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_RECIP,    gx+(gw+g)*7, y+(bh+g)*3, gw, bh);

    move_ctrl(hwnd, ID_PROG_LSH, gx+(gw+g)*0, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_PROG_RSH, gx+(gw+g)*1, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_D,        gx+(gw+g)*2, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_1,        gx+(gw+g)*3, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_2,        gx+(gw+g)*4, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_3,        gx+(gw+g)*5, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_SUB,      gx+(gw+g)*6, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_EQ,       gx+(gw+g)*7, y+(bh+g)*4, gw, bh*2+g);

    move_ctrl(hwnd, ID_PROG_NOT, gx+(gw+g)*0, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_PROG_AND, gx+(gw+g)*1, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_E_HEX,    gx+(gw+g)*2, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_0,        gx+(gw+g)*3, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_DOT,      gx+(gw+g)*4, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_ADD,      gx+(gw+g)*5, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_F,        gx+(gw+g)*6, y+(bh+g)*5, gw, bh);
}

void layout_statistics(HWND hwnd, Layout *L) {
    int x       = L->disp_x;
    int y0      = L->disp_y + L->disp_h + L->margin;
    int bw      = L->bw;
    int bh      = L->bh;
    int g       = L->gap;
    int list_h  = (int)(L->win_h * 0.22);
    int list_w  = L->grid_w - bh - g;
    int y       = y0 + list_h + g;
    int row;

    move_ctrl(hwnd, ID_DISP_HISTORY, x, y0, list_w, list_h);
    move_ctrl(hwnd, 400, x + list_w + g, y0,              bh, list_h/2 - g/2);
    move_ctrl(hwnd, 401, x + list_w + g, y0+list_h/2+g/2, bh, list_h/2 - g/2);

    row = 0;
    move_ctrl(hwnd, ID_MC,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MR,          x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MS,          x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,       x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MMINUS,      x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_BACK,        x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_CAD,    x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_CLR,         x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SCI_FE,      x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP,     x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_7,           x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_8,           x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_9,           x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_MEAN,   x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_MEAN2,  x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_4,           x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_5,           x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_6,           x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SUMX,   x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SUMX2,  x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_1,           x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_2,           x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_3,           x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SDEV,   x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SDEV1,  x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_0,           x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_DOT,         x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SIGN,        x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_ADD,    x+(bw+g)*3, y+(bh+g)*row, bw*2+g, bh);
}

void layout_panel(HWND hwnd, Layout *L) {
    int px  = L->panel_x + L->margin;
    int pw  = L->panel_w - L->margin * 2;
    int py  = L->margin;
    int lh  = 18;
    int eh  = 22;
    int ch  = 22;
    int bh2 = 26;
    int g   = L->gap;
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
            move_ctrl(hwnd, 353,           px,              py, wide,   lh); py += lh+g;
            move_ctrl(hwnd, ID_DATE_TYPE,  px,              py, wide,   ch); py += ch+g*2;
            move_ctrl(hwnd, 354,           px,              py, wide/2, lh);
            move_ctrl(hwnd, 355,           px+wide/2+g,     py, wide/2, lh);
            py += lh+g;
            move_ctrl(hwnd, ID_DATE_FROM,  px,              py, wide/2, eh);
            move_ctrl(hwnd, ID_DATE_TO,    px+wide/2+g,     py, wide/2, eh);
            py += eh+g*2;
            move_ctrl(hwnd, ID_DATE_CALC,  px, py, wide, bh2); py += bh2+g*2;
            move_ctrl(hwnd, ID_DATE_RES1,  px, py, wide, lh*2); py += lh*2+g;
            move_ctrl(hwnd, ID_DATE_RES2,  px, py, wide, lh);
            break;

        case PANEL_HISTORY:
            move_ctrl(hwnd, 356,           px, py, wide, lh); py += lh+g;
            move_ctrl(hwnd, ID_PANEL_COMBO,px, py, wide,
                      L->win_h - py - bh2 - g*2 - L->margin);
            py = L->win_h - bh2 - L->margin;
            move_ctrl(hwnd, ID_PANEL_CLOSE,px, py, wide, bh2);
            break;

        case PANEL_WORKSHEET:
            move_ctrl(hwnd, 357,           px, py, wide, lh); py += lh+g;
            move_ctrl(hwnd, ID_WS_COMBO,   px, py, wide, ch); py += ch+g*2;
            for (i = 0; i < 6; i++) {
                HWND hl = GetDlgItem(hwnd, lbl_ids[i]);
                if (hl && IsWindowVisible(hl)) {
                    move_ctrl(hwnd, lbl_ids[i], px,           py, wide/2,   lh);
                    move_ctrl(hwnd, inp_ids[i], px+wide/2+g,  py, wide/2-g, eh);
                    py += max(lh, eh) + g;
                }
            }
            py += g;
            move_ctrl(hwnd, ID_WS_CALC, px,          py, wide/2,   bh2);
            move_ctrl(hwnd, ID_WS_RES,  px+wide/2+g, py, wide/2-g, bh2);
            break;
    }
}