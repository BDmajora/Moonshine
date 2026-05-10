/* ────────────────────────────────────────────────────────────────────
   calc_modeProgrammer.c — Programmer mode controls + layout.

   Matches the Windows 7 Calculator reference:
     - Bit-display strip across the top (full width)
     - Left sidebar with TWO grouped radio columns:
         Hex/Dec/Oct/Bin   (base)
         Qword/Dword/Word/Byte  (word size)
     - Right side: 8-column button grid (6 rows)

   The previous build had radios stacked vertically at the far left
   AND the button grid was 9 cols, not 8.  This module fixes both.
   ──────────────────────────────────────────────────────────────────── */
#include "calc_modeProgrammer.h"

extern HWND make_button(HWND p, int id, const WCHAR *lbl, BOOL def);
extern HWND make_radio(HWND p, int id, const WCHAR *lbl, BOOL first);
extern void ui_register_child(HWND h);

#define LY_BIT_H    36     /* bit-display height (4 lines × 8 bits each) */
#define LY_RADIO_W  60     /* sidebar radio column width */
#define LY_GROUP_GAP 8     /* space between base/word radio groups */

void mode_prog_create(HWND hwnd) {
    HWND h;

    /* Bit-display strip */
    h = CreateWindowW(L"STATIC", L"",
                      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_DISP_BITS,
                      NULL, NULL);
    ui_register_child(h);

    /* Base radios (Hex/Dec/Oct/Bin) */
    make_radio(hwnd, ID_RADIO_HEX,   L"Hex",   TRUE);
    make_radio(hwnd, ID_RADIO_DEC,   L"Dec",   FALSE);
    make_radio(hwnd, ID_RADIO_OCT,   L"Oct",   FALSE);
    make_radio(hwnd, ID_RADIO_BIN,   L"Bin",   FALSE);
    SendMessageW(GetDlgItem(hwnd, ID_RADIO_DEC), BM_SETCHECK, BST_CHECKED, 0);

    /* Word-size radios (Qword/Dword/Word/Byte) — separate group */
    make_radio(hwnd, ID_RADIO_QWORD, L"Qword", TRUE);
    make_radio(hwnd, ID_RADIO_DWORD, L"Dword", FALSE);
    make_radio(hwnd, ID_RADIO_WORD,  L"Word",  FALSE);
    make_radio(hwnd, ID_RADIO_BYTE,  L"Byte",  FALSE);
    SendMessageW(GetDlgItem(hwnd, ID_RADIO_QWORD), BM_SETCHECK, BST_CHECKED, 0);

    /* Memory row (right side, row 0, cols 3-7) */
    make_button(hwnd, ID_MC,     L"MC",      FALSE);
    make_button(hwnd, ID_MR,     L"MR",      FALSE);
    make_button(hwnd, ID_MS,     L"MS",      FALSE);
    make_button(hwnd, ID_MPLUS,  L"M+",      FALSE);
    make_button(hwnd, ID_MMINUS, L"M\x2212", FALSE);

    /* Row 0 left side: Mod A */
    make_button(hwnd, ID_PROG_MOD, L"Mod",     FALSE);
    make_button(hwnd, ID_A,        L"A",       FALSE);

    /* Row 1: ( ) B  ← CE C ± √ */
    make_button(hwnd, ID_SCI_LPAREN, L"(",        FALSE);
    make_button(hwnd, ID_SCI_RPAREN, L")",        FALSE);
    make_button(hwnd, ID_B,          L"B",        FALSE);
    make_button(hwnd, ID_BACK,       L"\x2190",   FALSE);
    make_button(hwnd, ID_CE,         L"CE",       FALSE);
    make_button(hwnd, ID_CLR,        L"C",        FALSE);
    make_button(hwnd, ID_SIGN,       L"\xb1",     FALSE);
    make_button(hwnd, ID_SQRT,       L"\x221a",   FALSE);

    /* Row 2: RoL RoR C  7 8 9 / % */
    make_button(hwnd, ID_PROG_ROL, L"RoL",  FALSE);
    make_button(hwnd, ID_PROG_ROR, L"RoR",  FALSE);
    make_button(hwnd, ID_C_HEX,    L"C",    FALSE);
    make_button(hwnd, ID_7,        L"7",    FALSE);
    make_button(hwnd, ID_8,        L"8",    FALSE);
    make_button(hwnd, ID_9,        L"9",    FALSE);
    make_button(hwnd, ID_DIV,      L"/",    FALSE);
    make_button(hwnd, ID_PERCENT,  L"%",    FALSE);

    /* Row 3: Or Xor D  4 5 6 × 1/x */
    make_button(hwnd, ID_PROG_OR,  L"Or",   FALSE);
    make_button(hwnd, ID_PROG_XOR, L"Xor",  FALSE);
    make_button(hwnd, ID_D,        L"D",    FALSE);
    make_button(hwnd, ID_4,        L"4",    FALSE);
    make_button(hwnd, ID_5,        L"5",    FALSE);
    make_button(hwnd, ID_6,        L"6",    FALSE);
    make_button(hwnd, ID_MUL,      L"\xd7", FALSE);
    make_button(hwnd, ID_RECIP,    L"1/x",  FALSE);

    /* Row 4: Lsh Rsh E  1 2 3 − = */
    make_button(hwnd, ID_PROG_LSH, L"Lsh",    FALSE);
    make_button(hwnd, ID_PROG_RSH, L"Rsh",    FALSE);
    make_button(hwnd, ID_E_HEX,    L"E",      FALSE);
    make_button(hwnd, ID_1,        L"1",      FALSE);
    make_button(hwnd, ID_2,        L"2",      FALSE);
    make_button(hwnd, ID_3,        L"3",      FALSE);
    make_button(hwnd, ID_SUB,      L"\x2212", FALSE);
    make_button(hwnd, ID_EQ,       L"=",      TRUE);

    /* Row 5: Not And F  0 (spans 2) . + */
    make_button(hwnd, ID_PROG_NOT, L"Not", FALSE);
    make_button(hwnd, ID_PROG_AND, L"And", FALSE);
    make_button(hwnd, ID_F,        L"F",   FALSE);
    make_button(hwnd, ID_0,        L"0",   FALSE);
    make_button(hwnd, ID_DOT,      L".",   FALSE);
    make_button(hwnd, ID_ADD,      L"+",   FALSE);
}

void mode_prog_layout(HWND hwnd, Layout *L) {
    int x   = L->disp_x;
    int g   = L->gap;
    int bw  = L->bw;
    int bh  = L->bh;
    int gx, ry, bit_y;

    /* Bit display: full grid width, just under the main display. */
    bit_y = L->disp_y + L->disp_h + g;
    move_ctrl(hwnd, ID_DISP_BITS, x, bit_y, L->grid_w, LY_BIT_H);

    /* Right-side button grid starts after the radio sidebar. */
    gx = x + LY_RADIO_W + g * 2;

    /* Left sidebar: base radios stacked, then word radios.
       Aligned with the right-side grid rows 0..5 vertically. */
    ry = L->grid_y;
    move_ctrl(hwnd, ID_RADIO_HEX,   x, ry,                                     LY_RADIO_W, bh);
    move_ctrl(hwnd, ID_RADIO_DEC,   x, ry + (bh + g),                          LY_RADIO_W, bh);
    move_ctrl(hwnd, ID_RADIO_OCT,   x, ry + (bh + g) * 2,                      LY_RADIO_W, bh);
    move_ctrl(hwnd, ID_RADIO_BIN,   x, ry + (bh + g) * 3,                      LY_RADIO_W, bh);
    move_ctrl(hwnd, ID_RADIO_QWORD, x, ry + (bh + g) * 3 + bh + LY_GROUP_GAP,  LY_RADIO_W, bh);
    move_ctrl(hwnd, ID_RADIO_DWORD, x, ry + (bh + g) * 4 + bh + LY_GROUP_GAP,  LY_RADIO_W, bh);
    move_ctrl(hwnd, ID_RADIO_WORD,  x, ry + (bh + g) * 5 + bh + LY_GROUP_GAP,  LY_RADIO_W, bh);
    move_ctrl(hwnd, ID_RADIO_BYTE,  x, ry + (bh + g) * 6 + bh + LY_GROUP_GAP,  LY_RADIO_W, bh);

    /* Right grid: 8 columns × 6 rows.
       Memory buttons sit on row 0 in cols 3-7. */
    {
        int y = ry;

        /* Row 0: _ Mod A  MC MR MS M+ M- */
        move_ctrl(hwnd, ID_PROG_MOD, gx + (bw + g) * 1, y, bw, bh);
        move_ctrl(hwnd, ID_A,        gx + (bw + g) * 2, y, bw, bh);
        move_ctrl(hwnd, ID_MC,       gx + (bw + g) * 3, y, bw, bh);
        move_ctrl(hwnd, ID_MR,       gx + (bw + g) * 4, y, bw, bh);
        move_ctrl(hwnd, ID_MS,       gx + (bw + g) * 5, y, bw, bh);
        move_ctrl(hwnd, ID_MPLUS,    gx + (bw + g) * 6, y, bw, bh);
        move_ctrl(hwnd, ID_MMINUS,   gx + (bw + g) * 7, y, bw, bh);
        y += bh + g;

        /* Row 1: ( ) B ← CE C ± √ */
        move_ctrl(hwnd, ID_SCI_LPAREN, gx + (bw + g) * 0, y, bw, bh);
        move_ctrl(hwnd, ID_SCI_RPAREN, gx + (bw + g) * 1, y, bw, bh);
        move_ctrl(hwnd, ID_B,          gx + (bw + g) * 2, y, bw, bh);
        move_ctrl(hwnd, ID_BACK,       gx + (bw + g) * 3, y, bw, bh);
        move_ctrl(hwnd, ID_CE,         gx + (bw + g) * 4, y, bw, bh);
        move_ctrl(hwnd, ID_CLR,        gx + (bw + g) * 5, y, bw, bh);
        move_ctrl(hwnd, ID_SIGN,       gx + (bw + g) * 6, y, bw, bh);
        move_ctrl(hwnd, ID_SQRT,       gx + (bw + g) * 7, y, bw, bh);
        y += bh + g;

        /* Row 2: RoL RoR C 7 8 9 / % */
        move_ctrl(hwnd, ID_PROG_ROL, gx + (bw + g) * 0, y, bw, bh);
        move_ctrl(hwnd, ID_PROG_ROR, gx + (bw + g) * 1, y, bw, bh);
        move_ctrl(hwnd, ID_C_HEX,    gx + (bw + g) * 2, y, bw, bh);
        move_ctrl(hwnd, ID_7,        gx + (bw + g) * 3, y, bw, bh);
        move_ctrl(hwnd, ID_8,        gx + (bw + g) * 4, y, bw, bh);
        move_ctrl(hwnd, ID_9,        gx + (bw + g) * 5, y, bw, bh);
        move_ctrl(hwnd, ID_DIV,      gx + (bw + g) * 6, y, bw, bh);
        move_ctrl(hwnd, ID_PERCENT,  gx + (bw + g) * 7, y, bw, bh);
        y += bh + g;

        /* Row 3: Or Xor D 4 5 6 × 1/x */
        move_ctrl(hwnd, ID_PROG_OR,  gx + (bw + g) * 0, y, bw, bh);
        move_ctrl(hwnd, ID_PROG_XOR, gx + (bw + g) * 1, y, bw, bh);
        move_ctrl(hwnd, ID_D,        gx + (bw + g) * 2, y, bw, bh);
        move_ctrl(hwnd, ID_4,        gx + (bw + g) * 3, y, bw, bh);
        move_ctrl(hwnd, ID_5,        gx + (bw + g) * 4, y, bw, bh);
        move_ctrl(hwnd, ID_6,        gx + (bw + g) * 5, y, bw, bh);
        move_ctrl(hwnd, ID_MUL,      gx + (bw + g) * 6, y, bw, bh);
        move_ctrl(hwnd, ID_RECIP,    gx + (bw + g) * 7, y, bw, bh);
        y += bh + g;

        /* Row 4: Lsh Rsh E 1 2 3 − = (= spans 2 rows in col 7) */
        move_ctrl(hwnd, ID_PROG_LSH, gx + (bw + g) * 0, y, bw, bh);
        move_ctrl(hwnd, ID_PROG_RSH, gx + (bw + g) * 1, y, bw, bh);
        move_ctrl(hwnd, ID_E_HEX,    gx + (bw + g) * 2, y, bw, bh);
        move_ctrl(hwnd, ID_1,        gx + (bw + g) * 3, y, bw, bh);
        move_ctrl(hwnd, ID_2,        gx + (bw + g) * 4, y, bw, bh);
        move_ctrl(hwnd, ID_3,        gx + (bw + g) * 5, y, bw, bh);
        move_ctrl(hwnd, ID_SUB,      gx + (bw + g) * 6, y, bw, bh);
        move_ctrl(hwnd, ID_EQ,       gx + (bw + g) * 7, y, bw, bh * 2 + g);
        y += bh + g;

        /* Row 5: Not And F 0(span 2) . + */
        move_ctrl(hwnd, ID_PROG_NOT, gx + (bw + g) * 0, y, bw, bh);
        move_ctrl(hwnd, ID_PROG_AND, gx + (bw + g) * 1, y, bw, bh);
        move_ctrl(hwnd, ID_F,        gx + (bw + g) * 2, y, bw, bh);
        move_ctrl(hwnd, ID_0,        gx + (bw + g) * 3, y, bw * 2 + g, bh);
        move_ctrl(hwnd, ID_DOT,      gx + (bw + g) * 5, y, bw, bh);
        move_ctrl(hwnd, ID_ADD,      gx + (bw + g) * 6, y, bw, bh);
    }
}

void mode_prog_get_grid(int *cols, int *rows, int *header_h) {
    /* 8-column grid for the right side (after the radio sidebar).
       The header reservation accounts for the bit-display strip. */
    *cols = 8; *rows = 6; *header_h = LY_BIT_H + 6;
}