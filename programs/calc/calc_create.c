#include "calc_defs.h"
#include "calc_panel.h"
#include "calc_mortgage.h"

/* Wrapper: raw CreateWindowW + register in one call */
static HWND make_raw(HWND parent, LPCWSTR cls, LPCWSTR text,
                     DWORD style, int id) {
    HWND h = CreateWindowW(cls, text, style,
                           0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    ui_register_child(h);
    return h;
}

static void create_display(HWND hwnd) {
    make_raw(hwnd, L"STATIC", L"0",
             WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE | SS_SUNKEN,
             ID_DISPLAY);
}

static void create_mem_row(HWND hwnd) {
    make_button(hwnd, ID_MC,     L"MC",      FALSE);
    make_button(hwnd, ID_MR,     L"MR",      FALSE);
    make_button(hwnd, ID_MS,     L"MS",      FALSE);
    make_button(hwnd, ID_MPLUS,  L"M+",      FALSE);
    make_button(hwnd, ID_MMINUS, L"M\x2212", FALSE);
}

static void create_standard_buttons(HWND hwnd) {
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

static void create_scientific_buttons(HWND hwnd) {
    make_radio(hwnd, ID_RAD_DEG,  L"Degrees", TRUE);
    make_radio(hwnd, ID_RAD_RAD,  L"Radians", FALSE);
    make_radio(hwnd, ID_RAD_GRAD, L"Grads",   FALSE);
    SendMessageW(GetDlgItem(hwnd, ID_RAD_DEG), BM_SETCHECK, BST_CHECKED, 0);
    create_mem_row(hwnd);
    make_button(hwnd, ID_SCI_INV,    L"Inv",      FALSE);
    make_button(hwnd, ID_SCI_LN,     L"ln",       FALSE);
    make_button(hwnd, ID_SCI_LPAREN, L"(",        FALSE);
    make_button(hwnd, ID_SCI_RPAREN, L")",        FALSE);
    make_button(hwnd, ID_BACK,       L"\x2190",   FALSE);
    make_button(hwnd, ID_CE,         L"CE",       FALSE);
    make_button(hwnd, ID_CLR,        L"C",        FALSE);
    make_button(hwnd, ID_SIGN,       L"\xb1",     FALSE);
    make_button(hwnd, ID_SQRT,       L"\x221a",   FALSE);
    make_button(hwnd, ID_SCI_INT,    L"Int",      FALSE);
    make_button(hwnd, ID_SCI_SINH,   L"sinh",     FALSE);
    make_button(hwnd, ID_SCI_SIN,    L"sin",      FALSE);
    make_button(hwnd, ID_SCI_POW2,   L"x\xb2",   FALSE);
    make_button(hwnd, ID_SCI_FACT,   L"n!",       FALSE);
    make_button(hwnd, ID_7,          L"7",        FALSE);
    make_button(hwnd, ID_8,          L"8",        FALSE);
    make_button(hwnd, ID_9,          L"9",        FALSE);
    make_button(hwnd, ID_DIV,        L"/",        FALSE);
    make_button(hwnd, ID_PERCENT,    L"%",        FALSE);
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
    make_button(hwnd, ID_SCI_PI,     L"\x3c0",    FALSE);
    make_button(hwnd, ID_SCI_TANH,   L"tanh",     FALSE);
    make_button(hwnd, ID_SCI_TAN,    L"tan",      FALSE);
    make_button(hwnd, ID_SCI_POW3,   L"x\xb3",   FALSE);
    make_button(hwnd, ID_SCI_CUBE,   L"\x221bx",  FALSE);
    make_button(hwnd, ID_1,          L"1",        FALSE);
    make_button(hwnd, ID_2,          L"2",        FALSE);
    make_button(hwnd, ID_3,          L"3",        FALSE);
    make_button(hwnd, ID_SUB,        L"\x2212",   FALSE);
    make_button(hwnd, ID_EQ,         L"=",        TRUE);
    make_button(hwnd, ID_SCI_FE,     L"F-E",      FALSE);
    make_button(hwnd, ID_SCI_EXP,    L"Exp",      FALSE);
    make_button(hwnd, ID_SCI_MOD,    L"Mod",      FALSE);
    make_button(hwnd, ID_SCI_LOG,    L"log",      FALSE);
    make_button(hwnd, ID_SCI_EXP+1,  L"10x",      FALSE);
    make_button(hwnd, ID_0,          L"0",        FALSE);
    make_button(hwnd, ID_DOT,        L".",        FALSE);
    make_button(hwnd, ID_ADD,        L"+",        FALSE);
}

static void create_programmer_buttons(HWND hwnd) {
    make_raw(hwnd, L"STATIC", L"",
             WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN, ID_DISP_BITS);
    make_radio(hwnd, ID_RADIO_HEX,   L"Hex",   TRUE);
    make_radio(hwnd, ID_RADIO_DEC,   L"Dec",   FALSE);
    make_radio(hwnd, ID_RADIO_OCT,   L"Oct",   FALSE);
    make_radio(hwnd, ID_RADIO_BIN,   L"Bin",   FALSE);
    SendMessageW(GetDlgItem(hwnd, ID_RADIO_DEC), BM_SETCHECK, BST_CHECKED, 0);
    make_radio(hwnd, ID_RADIO_QWORD, L"Qword", TRUE);
    make_radio(hwnd, ID_RADIO_DWORD, L"Dword", FALSE);
    make_radio(hwnd, ID_RADIO_WORD,  L"Word",  FALSE);
    make_radio(hwnd, ID_RADIO_BYTE,  L"Byte",  FALSE);
    SendMessageW(GetDlgItem(hwnd, ID_RADIO_QWORD), BM_SETCHECK, BST_CHECKED, 0);
    create_mem_row(hwnd);
    make_button(hwnd, ID_PROG_MOD,  L"Mod",     FALSE);
    make_button(hwnd, ID_A,         L"A",       FALSE);
    make_button(hwnd, ID_BACK,      L"\x2190",  FALSE);
    make_button(hwnd, ID_CE,        L"CE",      FALSE);
    make_button(hwnd, ID_CLR,       L"C",       FALSE);
    make_button(hwnd, ID_SIGN,      L"\xb1",    FALSE);
    make_button(hwnd, ID_SQRT,      L"\x221a",  FALSE);
    make_button(hwnd, ID_PROG_ROL,  L"RoL",    FALSE);
    make_button(hwnd, ID_PROG_ROR,  L"RoR",    FALSE);
    make_button(hwnd, ID_B,         L"B",      FALSE);
    make_button(hwnd, ID_7,         L"7",      FALSE);
    make_button(hwnd, ID_8,         L"8",      FALSE);
    make_button(hwnd, ID_9,         L"9",      FALSE);
    make_button(hwnd, ID_DIV,       L"/",      FALSE);
    make_button(hwnd, ID_PERCENT,   L"%",      FALSE);
    make_button(hwnd, ID_PROG_OR,   L"Or",     FALSE);
    make_button(hwnd, ID_PROG_XOR,  L"Xor",    FALSE);
    make_button(hwnd, ID_C_HEX,     L"C",      FALSE);
    make_button(hwnd, ID_4,         L"4",      FALSE);
    make_button(hwnd, ID_5,         L"5",      FALSE);
    make_button(hwnd, ID_6,         L"6",      FALSE);
    make_button(hwnd, ID_MUL,       L"\xd7",   FALSE);
    make_button(hwnd, ID_RECIP,     L"1/x",    FALSE);
    make_button(hwnd, ID_PROG_LSH,  L"Lsh",    FALSE);
    make_button(hwnd, ID_PROG_RSH,  L"Rsh",    FALSE);
    make_button(hwnd, ID_D,         L"D",      FALSE);
    make_button(hwnd, ID_1,         L"1",      FALSE);
    make_button(hwnd, ID_2,         L"2",      FALSE);
    make_button(hwnd, ID_3,         L"3",      FALSE);
    make_button(hwnd, ID_SUB,       L"\x2212", FALSE);
    make_button(hwnd, ID_EQ,        L"=",      TRUE);
    make_button(hwnd, ID_PROG_NOT,  L"Not",    FALSE);
    make_button(hwnd, ID_PROG_AND,  L"And",    FALSE);
    make_button(hwnd, ID_E_HEX,     L"E",      FALSE);
    make_button(hwnd, ID_0,         L"0",      FALSE);
    make_button(hwnd, ID_DOT,       L".",      FALSE);
    make_button(hwnd, ID_ADD,       L"+",      FALSE);
    make_button(hwnd, ID_F,         L"F",      FALSE);
}

static void create_statistics_buttons(HWND hwnd) {
    make_raw(hwnd, L"LISTBOX", NULL,
             WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
             ID_DISP_HISTORY);
    make_raw(hwnd, L"BUTTON", L"\x25b2",
             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 400);
    make_raw(hwnd, L"BUTTON", L"\x25bc",
             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 401);
    create_mem_row(hwnd);
    make_button(hwnd, ID_BACK,       L"\x2190",      FALSE);
    make_button(hwnd, ID_STAT_CAD,   L"CAD",         FALSE);
    make_button(hwnd, ID_CLR,        L"C",           FALSE);
    make_button(hwnd, ID_SCI_FE,     L"F-E",         FALSE);
    make_button(hwnd, ID_SCI_EXP,    L"Exp",         FALSE);
    make_button(hwnd, ID_7,          L"7",           FALSE);
    make_button(hwnd, ID_8,          L"8",           FALSE);
    make_button(hwnd, ID_9,          L"9",           FALSE);
    make_button(hwnd, ID_STAT_MEAN,  L"x\x305",      FALSE);
    make_button(hwnd, ID_STAT_MEAN2, L"x\x305\xb2",  FALSE);
    make_button(hwnd, ID_4,          L"4",           FALSE);
    make_button(hwnd, ID_5,          L"5",           FALSE);
    make_button(hwnd, ID_6,          L"6",           FALSE);
    make_button(hwnd, ID_STAT_SUMX,  L"\x3a3x",      FALSE);
    make_button(hwnd, ID_STAT_SUMX2, L"\x3a3x\xb2",  FALSE);
    make_button(hwnd, ID_1,          L"1",           FALSE);
    make_button(hwnd, ID_2,          L"2",           FALSE);
    make_button(hwnd, ID_3,          L"3",           FALSE);
    make_button(hwnd, ID_STAT_SDEV,  L"\x3c3n",      FALSE);
    make_button(hwnd, ID_STAT_SDEV1, L"\x3c3n-1",    FALSE);
    make_button(hwnd, ID_0,          L"0",           FALSE);
    make_button(hwnd, ID_DOT,        L".",           FALSE);
    make_button(hwnd, ID_SIGN,       L"\xb1",        FALSE);
    make_button(hwnd, ID_STAT_ADD,   L"Add",         TRUE);
}

static void create_unit_panel(HWND hwnd) {
    make_label(hwnd, 350,            L"Select the type of unit you want to convert");
    make_combo(hwnd, ID_UNIT_TYPE);
    make_label(hwnd, 351,            L"From");
    make_edit( hwnd, ID_UNIT_FROM_VAL, L"Enter value");
    make_combo(hwnd, ID_UNIT_FROM_UNT);
    make_label(hwnd, 352,            L"To");
    make_edit( hwnd, ID_UNIT_TO_VAL,   L"");
    make_combo(hwnd, ID_UNIT_TO_UNT);
    panel_unit_populate(hwnd);
}

static void create_date_panel(HWND hwnd) {
    make_label( hwnd, 353,         L"Select the date calculation you want");
    make_combo( hwnd, ID_DATE_TYPE);
    make_label( hwnd, 354,         L"From");
    make_edit(  hwnd, ID_DATE_FROM,L"YYYY-MM-DD");
    make_label( hwnd, 355,         L"To");
    make_edit(  hwnd, ID_DATE_TO,  L"YYYY-MM-DD");
    make_button(hwnd, ID_DATE_CALC,L"Calculate", TRUE);
    make_label( hwnd, ID_DATE_RES1,L"");
    make_label( hwnd, ID_DATE_RES2,L"");
    panel_date_populate(hwnd);
}

static void create_history_panel(HWND hwnd) {
    make_label(hwnd, 356, L"History");
    make_raw(hwnd, L"LISTBOX", NULL,
             WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
             ID_PANEL_COMBO);
    make_button(hwnd, ID_PANEL_CLOSE, L"Clear History", FALSE);
}

static void create_worksheet_panel(HWND hwnd, int ws_type) {
    int i;
    static const int lbl_ids[] = {ID_WS_LBL1,ID_WS_LBL2,ID_WS_LBL3,
                                   ID_WS_LBL4,ID_WS_LBL5,ID_WS_LBL6};
    static const int inp_ids[] = {ID_WS_IN1,ID_WS_IN2,ID_WS_IN3,
                                   ID_WS_IN4,ID_WS_IN5,ID_WS_IN6};
    make_label(hwnd, 357, L"Select the value you want to calculate");
    make_combo(hwnd, ID_WS_COMBO);
    /* Combo entries are populated by panel_ws_populate (which calls
       into calc_mortgage.c for mortgage worksheets).  Don't hardcode
       entries here — that was the old bug where mortgage only had
       3 of the 5 solve-for options. */
    for (i = 0; i < 6; i++) {
        make_label(hwnd, lbl_ids[i], L"");
        make_edit( hwnd, inp_ids[i], L"Enter value");
    }
    make_button(hwnd, ID_WS_CALC, L"Calculate", TRUE);
    make_label( hwnd, ID_WS_RES,  L"");
    panel_ws_populate(hwnd, ws_type);
}

void ui_create_controls(HWND hwnd) {
    create_display(hwnd);
    switch (g_mode) {
        case MODE_SCIENTIFIC: create_scientific_buttons(hwnd);         break;
        case MODE_PROGRAMMER: create_programmer_buttons(hwnd);         break;
        case MODE_STATISTICS: create_statistics_buttons(hwnd);         break;
        default:              create_standard_buttons(hwnd);           break;
    }
    switch (g_panel) {
        case PANEL_HISTORY:   create_history_panel(hwnd);              break;
        case PANEL_UNIT:      create_unit_panel(hwnd);                 break;
        case PANEL_DATE:      create_date_panel(hwnd);                 break;
        case PANEL_WORKSHEET: create_worksheet_panel(hwnd,g_worksheet);break;
    }
}