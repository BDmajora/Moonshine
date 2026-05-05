#include "calc_ui.h"
#include "calc_logic.h"
#include "calc_panel.h"
#include <math.h>
#include <wchar.h>
#include <commctrl.h>

/* ── Global UI state ─────────────────────────────────────────────── */
int   g_mode      = MODE_STANDARD;
int   g_panel     = PANEL_NONE;
int   g_worksheet = WS_MORTGAGE;
BOOL  g_basic_mode = FALSE;

HFONT hBtnFont   = NULL;
HFONT hDispFont  = NULL;
HFONT hSmallFont = NULL;

/* ── Helpers ─────────────────────────────────────────────────────── */

void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND c = GetDlgItem(hwnd, id);
    if (c) MoveWindow(c, x, y, w, h, TRUE);
}

HWND make_button(HWND parent, int id, const WCHAR *label, BOOL def_btn) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    if (def_btn) style |= BS_DEFPUSHBUTTON;
    return CreateWindowW(L"BUTTON", label, style,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_radio(HWND parent, int id, const WCHAR *label, BOOL first_in_group) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    if (first_in_group) style |= WS_GROUP;
    return CreateWindowW(L"BUTTON", label, style,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_label(HWND parent, int id, const WCHAR *text) {
    return CreateWindowW(L"STATIC", text,
                         WS_CHILD | WS_VISIBLE | SS_LEFT,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_edit(HWND parent, int id, const WCHAR *placeholder) {
    return CreateWindowW(L"EDIT", placeholder,
                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_combo(HWND parent, int id) {
    return CreateWindowW(L"COMBOBOX", NULL,
                         WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                         CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                         0, 0, 0, 200, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

static BOOL CALLBACK SetFontProc(HWND h, LPARAM lp) {
    SendMessageW(h, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

/* Callback to destroy each child window during rebuild */
static BOOL CALLBACK DestroyChildProc(HWND h, LPARAM lp) {
    DestroyWindow(h);
    (void)lp;
    return TRUE;
}

static void recreate_fonts(int btn_h, int disp_h) {
    if (hBtnFont)   DeleteObject(hBtnFont);
    if (hDispFont)  DeleteObject(hDispFont);
    if (hSmallFont) DeleteObject(hSmallFont);

    hBtnFont   = CreateFontW((int)(btn_h * 0.38), 0, 0, 0, FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              L"Segoe UI");
    hDispFont  = CreateFontW((int)(disp_h * 0.55), 0, 0, 0, FW_LIGHT,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              L"Segoe UI");
    hSmallFont = CreateFontW((int)(btn_h * 0.28), 0, 0, 0, FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              L"Segoe UI");
}

void compute_layout(HWND hwnd, Layout *L) {
    RECT rc;
    int header_h;

    GetClientRect(hwnd, &rc);
    L->win_w = rc.right;
    L->win_h = rc.bottom;

    L->margin = max(6,  (int)(L->win_w * 0.018));
    L->gap    = max(3,  (int)(L->win_w * 0.008));
    L->disp_h = max(50, (int)(L->win_h * 0.14));

    if (g_panel != PANEL_NONE) {
        L->panel_w = max(260, (int)(L->win_w * 0.45));
        L->panel_x = L->win_w - L->panel_w;
        L->grid_w  = L->panel_x - L->margin * 2;
    } else {
        L->panel_w = 0;
        L->panel_x = L->win_w;
        L->grid_w  = L->win_w - L->margin * 2;
    }

    L->disp_x = L->margin;
    L->disp_y = L->margin;
    L->disp_w = L->grid_w;

    switch (g_mode) {
        case MODE_SCIENTIFIC: L->cols = 10; L->rows = 7; break;
        case MODE_PROGRAMMER: L->cols = 9;  L->rows = 7; break;
        case MODE_STATISTICS: L->cols = 5;  L->rows = 7; break;
        default:              L->cols = 5;  L->rows = 6; break;
    }

    header_h = 0;
    if (g_mode == MODE_SCIENTIFIC) header_h = (int)(L->win_h * 0.05);
    if (g_mode == MODE_PROGRAMMER) header_h = (int)(L->win_h * 0.10);
    if (g_mode == MODE_STATISTICS) header_h = (int)(L->win_h * 0.25);

    L->grid_y = L->disp_y + L->disp_h + L->margin + header_h;
    L->grid_h = L->win_h - L->grid_y - L->margin;

    L->bw = (L->grid_w - L->gap * (L->cols - 1)) / L->cols;
    L->bh = (L->grid_h - L->gap * (L->rows - 1)) / L->rows;
    L->bh = max(L->bh, 24);
    L->bw = max(L->bw, 20);
}

/* ════════════════════════════════════════════════════════════════════
   CONTROL CREATION
   ════════════════════════════════════════════════════════════════════ */

static void create_display(HWND hwnd) {
    CreateWindowW(L"STATIC", L"0",
                  WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE | SS_SUNKEN,
                  0, 0, 0, 0, hwnd, (HMENU)ID_DISPLAY, NULL, NULL);
}

static void create_mem_row(HWND hwnd) {
    make_button(hwnd, ID_MC,     L"MC",    FALSE);
    make_button(hwnd, ID_MR,     L"MR",    FALSE);
    make_button(hwnd, ID_MS,     L"MS",    FALSE);
    make_button(hwnd, ID_MPLUS,  L"M+",    FALSE);
    /* M- with unicode minus: \u2212 — embed directly as UTF-8 in source */
    make_button(hwnd, ID_MMINUS, L"M\x2212", FALSE);
}

static void create_standard_buttons(HWND hwnd) {
    create_mem_row(hwnd);

    make_button(hwnd, ID_BACK,    L"\x2190",  FALSE); /* <- */
    make_button(hwnd, ID_CE,      L"CE",      FALSE);
    make_button(hwnd, ID_CLR,     L"C",       FALSE);
    make_button(hwnd, ID_SIGN,    L"\xb1",    FALSE); /* +/- */
    make_button(hwnd, ID_SQRT,    L"\x221a",  FALSE); /* sqrt */

    make_button(hwnd, ID_7,       L"7",       FALSE);
    make_button(hwnd, ID_8,       L"8",       FALSE);
    make_button(hwnd, ID_9,       L"9",       FALSE);
    make_button(hwnd, ID_DIV,     L"/",       FALSE);
    make_button(hwnd, ID_PERCENT, L"%",       FALSE);

    make_button(hwnd, ID_4,       L"4",       FALSE);
    make_button(hwnd, ID_5,       L"5",       FALSE);
    make_button(hwnd, ID_6,       L"6",       FALSE);
    make_button(hwnd, ID_MUL,     L"\xd7",    FALSE); /* x */
    make_button(hwnd, ID_RECIP,   L"1/x",     FALSE);

    make_button(hwnd, ID_1,       L"1",       FALSE);
    make_button(hwnd, ID_2,       L"2",       FALSE);
    make_button(hwnd, ID_3,       L"3",       FALSE);
    make_button(hwnd, ID_SUB,     L"\x2212",  FALSE); /* - */
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

    /* Row 1 */
    make_button(hwnd, ID_SCI_INV,    L"Inv",     FALSE);
    make_button(hwnd, ID_SCI_LN,     L"ln",      FALSE);
    make_button(hwnd, ID_SCI_LPAREN, L"(",       FALSE);
    make_button(hwnd, ID_SCI_RPAREN, L")",       FALSE);
    make_button(hwnd, ID_BACK,       L"\x2190",  FALSE);
    make_button(hwnd, ID_CE,         L"CE",      FALSE);
    make_button(hwnd, ID_CLR,        L"C",       FALSE);
    make_button(hwnd, ID_SIGN,       L"\xb1",    FALSE);
    make_button(hwnd, ID_SQRT,       L"\x221a",  FALSE);

    /* Row 2 */
    make_button(hwnd, ID_SCI_INT,    L"Int",     FALSE);
    make_button(hwnd, ID_SCI_SINH,   L"sinh",    FALSE);
    make_button(hwnd, ID_SCI_SIN,    L"sin",     FALSE);
    make_button(hwnd, ID_SCI_POW2,   L"x\xb2",  FALSE); /* x^2 */
    make_button(hwnd, ID_SCI_FACT,   L"n!",      FALSE);
    make_button(hwnd, ID_7,          L"7",       FALSE);
    make_button(hwnd, ID_8,          L"8",       FALSE);
    make_button(hwnd, ID_9,          L"9",       FALSE);
    make_button(hwnd, ID_DIV,        L"/",       FALSE);
    make_button(hwnd, ID_PERCENT,    L"%",       FALSE);

    /* Row 3 */
    make_button(hwnd, ID_SCI_DMS,    L"dms",     FALSE);
    make_button(hwnd, ID_SCI_COSH,   L"cosh",    FALSE);
    make_button(hwnd, ID_SCI_COS,    L"cos",     FALSE);
    make_button(hwnd, ID_SCI_POWY,   L"x^y",     FALSE);
    make_button(hwnd, ID_SCI_YROOTX, L"y\x221ax",FALSE); /* y sqrt x */
    make_button(hwnd, ID_4,          L"4",       FALSE);
    make_button(hwnd, ID_5,          L"5",       FALSE);
    make_button(hwnd, ID_6,          L"6",       FALSE);
    make_button(hwnd, ID_MUL,        L"\xd7",    FALSE);
    make_button(hwnd, ID_RECIP,      L"1/x",     FALSE);

    /* Row 4 */
    make_button(hwnd, ID_SCI_PI,     L"\x3c0",   FALSE); /* pi */
    make_button(hwnd, ID_SCI_TANH,   L"tanh",    FALSE);
    make_button(hwnd, ID_SCI_TAN,    L"tan",     FALSE);
    make_button(hwnd, ID_SCI_POW3,   L"x\xb3",  FALSE); /* x^3 */
    make_button(hwnd, ID_SCI_CUBE,   L"\x221bx", FALSE); /* cbrt */
    make_button(hwnd, ID_1,          L"1",       FALSE);
    make_button(hwnd, ID_2,          L"2",       FALSE);
    make_button(hwnd, ID_3,          L"3",       FALSE);
    make_button(hwnd, ID_SUB,        L"\x2212",  FALSE);
    make_button(hwnd, ID_EQ,         L"=",       TRUE);

    /* Row 5 */
    make_button(hwnd, ID_SCI_FE,     L"F-E",     FALSE);
    make_button(hwnd, ID_SCI_EXP,    L"Exp",     FALSE);
    make_button(hwnd, ID_SCI_MOD,    L"Mod",     FALSE);
    make_button(hwnd, ID_SCI_LOG,    L"log",     FALSE);
    make_button(hwnd, ID_SCI_EXP+1,  L"10x",     FALSE); /* 10^x placeholder */
    make_button(hwnd, ID_0,          L"0",       FALSE);
    make_button(hwnd, ID_DOT,        L".",       FALSE);
    make_button(hwnd, ID_ADD,        L"+",       FALSE);
}

static void create_programmer_buttons(HWND hwnd) {
    CreateWindowW(L"STATIC", L"",
                  WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN,
                  0, 0, 0, 0, hwnd, (HMENU)ID_DISP_BITS, NULL, NULL);

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

    make_button(hwnd, ID_PROG_MOD,  L"Mod",    FALSE);
    make_button(hwnd, ID_A,         L"A",      FALSE);
    make_button(hwnd, ID_BACK,      L"\x2190", FALSE);
    make_button(hwnd, ID_CE,        L"CE",     FALSE);
    make_button(hwnd, ID_CLR,       L"C",      FALSE);
    make_button(hwnd, ID_SIGN,      L"\xb1",   FALSE);
    make_button(hwnd, ID_SQRT,      L"\x221a", FALSE);

    make_button(hwnd, ID_PROG_ROL,  L"RoL",   FALSE);
    make_button(hwnd, ID_PROG_ROR,  L"RoR",   FALSE);
    make_button(hwnd, ID_B,         L"B",     FALSE);
    make_button(hwnd, ID_7,         L"7",     FALSE);
    make_button(hwnd, ID_8,         L"8",     FALSE);
    make_button(hwnd, ID_9,         L"9",     FALSE);
    make_button(hwnd, ID_DIV,       L"/",     FALSE);
    make_button(hwnd, ID_PERCENT,   L"%",     FALSE);

    make_button(hwnd, ID_PROG_OR,   L"Or",    FALSE);
    make_button(hwnd, ID_PROG_XOR,  L"Xor",   FALSE);
    make_button(hwnd, ID_C_HEX,     L"C",     FALSE);
    make_button(hwnd, ID_4,         L"4",     FALSE);
    make_button(hwnd, ID_5,         L"5",     FALSE);
    make_button(hwnd, ID_6,         L"6",     FALSE);
    make_button(hwnd, ID_MUL,       L"\xd7",  FALSE);
    make_button(hwnd, ID_RECIP,     L"1/x",   FALSE);

    make_button(hwnd, ID_PROG_LSH,  L"Lsh",   FALSE);
    make_button(hwnd, ID_PROG_RSH,  L"Rsh",   FALSE);
    make_button(hwnd, ID_D,         L"D",     FALSE);
    make_button(hwnd, ID_1,         L"1",     FALSE);
    make_button(hwnd, ID_2,         L"2",     FALSE);
    make_button(hwnd, ID_3,         L"3",     FALSE);
    make_button(hwnd, ID_SUB,       L"\x2212",FALSE);
    make_button(hwnd, ID_EQ,        L"=",     TRUE);

    make_button(hwnd, ID_PROG_NOT,  L"Not",   FALSE);
    make_button(hwnd, ID_PROG_AND,  L"And",   FALSE);
    make_button(hwnd, ID_E_HEX,     L"E",     FALSE);
    make_button(hwnd, ID_0,         L"0",     FALSE);
    make_button(hwnd, ID_DOT,       L".",     FALSE);
    make_button(hwnd, ID_ADD,       L"+",     FALSE);
    make_button(hwnd, ID_F,         L"F",     FALSE);
}

static void create_statistics_buttons(HWND hwnd) {
    CreateWindowW(L"LISTBOX", NULL,
                  WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                  0, 0, 0, 0, hwnd, (HMENU)ID_DISP_HISTORY, NULL, NULL);
    CreateWindowW(L"BUTTON", L"\x25b2",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  0, 0, 0, 0, hwnd, (HMENU)400, NULL, NULL);
    CreateWindowW(L"BUTTON", L"\x25bc",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  0, 0, 0, 0, hwnd, (HMENU)401, NULL, NULL);

    create_mem_row(hwnd);

    make_button(hwnd, ID_BACK,       L"\x2190",  FALSE);
    make_button(hwnd, ID_STAT_CAD,   L"CAD",     FALSE);
    make_button(hwnd, ID_CLR,        L"C",       FALSE);
    make_button(hwnd, ID_SCI_FE,     L"F-E",     FALSE);
    make_button(hwnd, ID_SCI_EXP,    L"Exp",     FALSE);

    make_button(hwnd, ID_7,          L"7",       FALSE);
    make_button(hwnd, ID_8,          L"8",       FALSE);
    make_button(hwnd, ID_9,          L"9",       FALSE);
    /* x-bar and x-bar-squared: use plain ASCII labels to avoid escape issues */
    make_button(hwnd, ID_STAT_MEAN,  L"x\x305",  FALSE);
    make_button(hwnd, ID_STAT_MEAN2, L"x\x305\xb2", FALSE);

    make_button(hwnd, ID_4,          L"4",       FALSE);
    make_button(hwnd, ID_5,          L"5",       FALSE);
    make_button(hwnd, ID_6,          L"6",       FALSE);
    make_button(hwnd, ID_STAT_SUMX,  L"\x3a3x",  FALSE);  /* sigma x */
    make_button(hwnd, ID_STAT_SUMX2, L"\x3a3x\xb2", FALSE);

    make_button(hwnd, ID_1,          L"1",       FALSE);
    make_button(hwnd, ID_2,          L"2",       FALSE);
    make_button(hwnd, ID_3,          L"3",       FALSE);
    make_button(hwnd, ID_STAT_SDEV,  L"\x3c3n",     FALSE);  /* sigma-n */
    make_button(hwnd, ID_STAT_SDEV1, L"\x3c3n-1",   FALSE);

    make_button(hwnd, ID_0,          L"0",       FALSE);
    make_button(hwnd, ID_DOT,        L".",       FALSE);
    make_button(hwnd, ID_SIGN,       L"\xb1",    FALSE);
    make_button(hwnd, ID_STAT_ADD,   L"Add",     TRUE);
}

/* ── Panel creation ─────────────────────────────────────────────── */

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
    CreateWindowW(L"LISTBOX", NULL,
                  WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                  0, 0, 0, 0, hwnd, (HMENU)ID_PANEL_COMBO, NULL, NULL);
    make_button(hwnd, ID_PANEL_CLOSE, L"Clear History", FALSE);
}

static void create_worksheet_panel(HWND hwnd, int ws_type) {
    int i;
    static const int lbl_ids[] = {ID_WS_LBL1,ID_WS_LBL2,ID_WS_LBL3,
                                   ID_WS_LBL4,ID_WS_LBL5,ID_WS_LBL6};
    static const int inp_ids[] = {ID_WS_IN1,ID_WS_IN2,ID_WS_IN3,
                                   ID_WS_IN4,ID_WS_IN5,ID_WS_IN6};
    HWND hc;

    make_label(hwnd, 357, L"Select the value you want to calculate");
    make_combo(hwnd, ID_WS_COMBO);
    hc = GetDlgItem(hwnd, ID_WS_COMBO);
    SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)L"Monthly payment");
    SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)L"Purchase price");
    SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)L"Down payment");
    SendMessageW(hc, CB_SETCURSEL, 0, 0);

    for (i = 0; i < 6; i++) {
        make_label(hwnd, lbl_ids[i], L"");
        make_edit( hwnd, inp_ids[i], L"Enter value");
    }
    make_button(hwnd, ID_WS_CALC, L"Calculate", TRUE);
    make_label( hwnd, ID_WS_RES,  L"");
    panel_ws_populate(hwnd, ws_type);
}

/* ════════════════════════════════════════════════════════════════════
   PUBLIC: create all controls
   ════════════════════════════════════════════════════════════════════ */

void ui_create_controls(HWND hwnd) {
    create_display(hwnd);

    switch (g_mode) {
        case MODE_SCIENTIFIC: create_scientific_buttons(hwnd);        break;
        case MODE_PROGRAMMER: create_programmer_buttons(hwnd);        break;
        case MODE_STATISTICS: create_statistics_buttons(hwnd);        break;
        default:              create_standard_buttons(hwnd);          break;
    }

    switch (g_panel) {
        case PANEL_HISTORY:   create_history_panel(hwnd);             break;
        case PANEL_UNIT:      create_unit_panel(hwnd);                break;
        case PANEL_DATE:      create_date_panel(hwnd);                break;
        case PANEL_WORKSHEET: create_worksheet_panel(hwnd,g_worksheet);break;
    }
}

/* Rebuild: destroy all children then recreate */
void ui_rebuild_mode(HWND hwnd) {
    EnumChildWindows(hwnd, DestroyChildProc, 0);
    ui_create_controls(hwnd);
    ui_update_display(hwnd);
}

/* ════════════════════════════════════════════════════════════════════
   LAYOUT: position all controls
   ════════════════════════════════════════════════════════════════════ */

static void layout_standard(HWND hwnd, Layout *L) {
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

static void layout_scientific(HWND hwnd, Layout *L) {
    int x   = L->disp_x;
    int y0  = L->disp_y + L->disp_h + L->margin;
    int bw  = L->bw;
    int bh  = L->bh;
    int g   = L->gap;
    int rh  = bh;
    int y;

    /* Angle radios */
    move_ctrl(hwnd, ID_RAD_DEG,  x,           y0, bw*2,   rh);
    move_ctrl(hwnd, ID_RAD_RAD,  x+bw*2+g,    y0, bw*2,   rh);
    move_ctrl(hwnd, ID_RAD_GRAD, x+bw*4+g*2,  y0, bw*2,   rh);
    y = L->grid_y;

    /* Memory row (right 5 columns) */
    move_ctrl(hwnd, ID_MC,           x+(bw+g)*5, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MR,           x+(bw+g)*6, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MS,           x+(bw+g)*7, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,        x+(bw+g)*8, y+(bh+g)*0, bw, bh);
    move_ctrl(hwnd, ID_MMINUS,       x+(bw+g)*9, y+(bh+g)*0, bw, bh);

    /* Row 1 */
    move_ctrl(hwnd, ID_SCI_INV,    x+(bw+g)*0, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_LN,     x+(bw+g)*1, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_LPAREN, x+(bw+g)*2, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SCI_RPAREN, x+(bw+g)*3, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_BACK,       x+(bw+g)*4, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CE,         x+(bw+g)*5, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_CLR,        x+(bw+g)*6, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SIGN,       x+(bw+g)*7, y+(bh+g)*1, bw, bh);
    move_ctrl(hwnd, ID_SQRT,       x+(bw+g)*8, y+(bh+g)*1, bw, bh);

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

    /* Row 4 */
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

    /* Row 5 */
    move_ctrl(hwnd, ID_SCI_FE,     x+(bw+g)*0, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP,    x+(bw+g)*1, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_MOD,    x+(bw+g)*2, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_LOG,    x+(bw+g)*3, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP+1,  x+(bw+g)*4, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_0,          x+(bw+g)*5, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_DOT,        x+(bw+g)*6, y+(bh+g)*5, bw, bh);
    move_ctrl(hwnd, ID_ADD,        x+(bw+g)*7, y+(bh+g)*5, bw, bh);
}

static void layout_programmer(HWND hwnd, Layout *L) {
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

    /* Memory row */
    move_ctrl(hwnd, ID_MC,     gx+(gw+g)*3, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MR,     gx+(gw+g)*4, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MS,     gx+(gw+g)*5, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MPLUS,  gx+(gw+g)*6, y+(bh+g)*0, gw, bh);
    move_ctrl(hwnd, ID_MMINUS, gx+(gw+g)*7, y+(bh+g)*0, gw, bh);

    /* Row 1 */
    move_ctrl(hwnd, ID_PROG_MOD, gx+(gw+g)*0, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_A,        gx+(gw+g)*1, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_BACK,     gx+(gw+g)*2, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_CE,       gx+(gw+g)*3, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_CLR,      gx+(gw+g)*4, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_SIGN,     gx+(gw+g)*5, y+(bh+g)*1, gw, bh);
    move_ctrl(hwnd, ID_SQRT,     gx+(gw+g)*6, y+(bh+g)*1, gw, bh);

    /* Row 2 */
    move_ctrl(hwnd, ID_PROG_ROL, gx+(gw+g)*0, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_PROG_ROR, gx+(gw+g)*1, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_B,        gx+(gw+g)*2, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_7,        gx+(gw+g)*3, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_8,        gx+(gw+g)*4, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_9,        gx+(gw+g)*5, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_DIV,      gx+(gw+g)*6, y+(bh+g)*2, gw, bh);
    move_ctrl(hwnd, ID_PERCENT,  gx+(gw+g)*7, y+(bh+g)*2, gw, bh);

    /* Row 3 */
    move_ctrl(hwnd, ID_PROG_OR,  gx+(gw+g)*0, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_PROG_XOR, gx+(gw+g)*1, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_C_HEX,   gx+(gw+g)*2, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_4,        gx+(gw+g)*3, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_5,        gx+(gw+g)*4, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_6,        gx+(gw+g)*5, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_MUL,      gx+(gw+g)*6, y+(bh+g)*3, gw, bh);
    move_ctrl(hwnd, ID_RECIP,    gx+(gw+g)*7, y+(bh+g)*3, gw, bh);

    /* Row 4 */
    move_ctrl(hwnd, ID_PROG_LSH, gx+(gw+g)*0, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_PROG_RSH, gx+(gw+g)*1, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_D,        gx+(gw+g)*2, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_1,        gx+(gw+g)*3, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_2,        gx+(gw+g)*4, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_3,        gx+(gw+g)*5, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_SUB,      gx+(gw+g)*6, y+(bh+g)*4, gw, bh);
    move_ctrl(hwnd, ID_EQ,       gx+(gw+g)*7, y+(bh+g)*4, gw, bh*2+g);

    /* Row 5 */
    move_ctrl(hwnd, ID_PROG_NOT, gx+(gw+g)*0, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_PROG_AND, gx+(gw+g)*1, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_E_HEX,   gx+(gw+g)*2, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_0,        gx+(gw+g)*3, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_DOT,      gx+(gw+g)*4, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_ADD,      gx+(gw+g)*5, y+(bh+g)*5, gw, bh);
    move_ctrl(hwnd, ID_F,        gx+(gw+g)*6, y+(bh+g)*5, gw, bh);
}

static void layout_statistics(HWND hwnd, Layout *L) {
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

static void layout_panel(HWND hwnd, Layout *L) {
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

/* ── Main layout dispatch ─────────────────────────────────────────── */

void ui_update_layout(HWND hwnd) {
    Layout L;
    compute_layout(hwnd, &L);
    recreate_fonts(L.bh, L.disp_h);

    move_ctrl(hwnd, ID_DISPLAY, L.disp_x, L.disp_y, L.disp_w, L.disp_h);

    switch (g_mode) {
        case MODE_SCIENTIFIC: layout_scientific(hwnd, &L); break;
        case MODE_PROGRAMMER: layout_programmer(hwnd, &L); break;
        case MODE_STATISTICS: layout_statistics(hwnd, &L); break;
        default:              layout_standard(hwnd, &L);   break;
    }

    if (g_panel != PANEL_NONE) layout_panel(hwnd, &L);

    EnumChildWindows(hwnd, SetFontProc, (LPARAM)hBtnFont);
    SendMessageW(GetDlgItem(hwnd, ID_DISPLAY), WM_SETFONT, (WPARAM)hDispFont, TRUE);

    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Helper updates ──────────────────────────────────────────────── */

void ui_update_display(HWND hwnd) {
    HWND hd = GetDlgItem(hwnd, ID_DISPLAY);
    if (hd) SetWindowTextW(hd, display_str);
}

void ui_update_bit_display(HWND hwnd) {
    HWND hb = GetDlgItem(hwnd, ID_DISP_BITS);
    WCHAR buf[256];
    unsigned long long uv;
    long long v;
    int bits, i, pos, buflen;

    if (!hb) return;
    buf[0] = L'\0';
    v   = prog_value();
    uv  = (unsigned long long)v;
    bits = 64;
    switch (prog_word) {
        case WORD_BYTE:  bits = 8;  uv &= 0xFFULL;        break;
        case WORD_WORD:  bits = 16; uv &= 0xFFFFULL;      break;
        case WORD_DWORD: bits = 32; uv &= 0xFFFFFFFFULL;  break;
        default:                                           break;
    }
    pos = 0;
    for (i = bits - 1; i >= 0; i--) {
        buf[pos++] = ((uv >> i) & 1) ? L'1' : L'0';
        if (i % 4 == 0 && i > 0) buf[pos++] = L' ';
    }
    buf[pos] = L'\0';
    buflen = pos; (void)buflen;
    SetWindowTextW(hb, buf);
}

void ui_update_stat_display(HWND hwnd) {
    HWND hl = GetDlgItem(hwnd, ID_DISP_HISTORY);
    WCHAR lbl[64];
    int i;
    if (!hl) return;
    SendMessageW(hl, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < stat_count; i++) {
        swprintf(lbl, 64, L"%.10g", stat_data[i]);
        SendMessageW(hl, LB_ADDSTRING, 0, (LPARAM)lbl);
    }
    swprintf(lbl, 64, L"Count = %d", stat_count);
    SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), lbl);
}

void ui_update_history_panel(HWND hwnd) {
    HWND hl = GetDlgItem(hwnd, ID_PANEL_COMBO);
    int i;
    if (!hl) return;
    SendMessageW(hl, LB_RESETCONTENT, 0, 0);
    for (i = hist_count - 1; i >= 0; i--)
        SendMessageW(hl, LB_ADDSTRING, 0, (LPARAM)history[i]);
}

void ui_update_prog_buttons(HWND hwnd) {
    static const int all_hex[] = {ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    static const int dec_off[] = {ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    static const int oct_off[] = {ID_8,ID_9,ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    static const int bin_off[] = {ID_2,ID_3,ID_4,ID_5,ID_6,ID_7,ID_8,ID_9,
                                   ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    const int *off;
    int i;

    for (i = 0; all_hex[i]; i++)
        EnableWindow(GetDlgItem(hwnd, all_hex[i]), prog_base == BASE_HEX);

    switch (prog_base) {
        case BASE_HEX: return;
        case BASE_DEC: off = dec_off; break;
        case BASE_OCT: off = oct_off; break;
        default:       off = bin_off; break;
    }
    for (i = 0; off[i]; i++)
        EnableWindow(GetDlgItem(hwnd, off[i]), FALSE);
}

void ui_update_digit_grouping(HWND hwnd) {
    WCHAR tmp[MAX_DISPLAY];
    WCHAR out[MAX_DISPLAY * 2];
    double v;
    long long iv;
    BOOL neg;
    int len, i2, j;

    if (!digit_grouping) { ui_update_display(hwnd); return; }
    v = _wtof(display_str);
    if (error || !(v == (long long)v && fabs(v) < 1e15)) return;

    iv  = (long long)v;
    neg = (iv < 0);
    if (neg) iv = -iv;
    swprintf(tmp, MAX_DISPLAY, L"%lld", iv);
    len = (int)wcslen(tmp);
    j = 0;
    for (i2 = 0; i2 < len; i2++) {
        if (i2 > 0 && (len - i2) % 3 == 0) out[j++] = L',';
        out[j++] = tmp[i2];
    }
    out[j] = L'\0';

    if (neg)
        swprintf(display_str, MAX_DISPLAY, L"-%s", out);
    else
        lstrcpynW(display_str, out, MAX_DISPLAY - 1);

    ui_update_display(hwnd);
}

void ui_show_panel(HWND hwnd, int panel) {
    g_panel = panel;
    ui_rebuild_mode(hwnd);
    ui_update_layout(hwnd);
}

void ui_show_worksheet(HWND hwnd, int ws_type) {
    g_worksheet = ws_type;
    g_panel = PANEL_WORKSHEET;
    ui_rebuild_mode(hwnd);
    ui_update_layout(hwnd);
}

void ui_update_menu_check(HMENU hMenu) {
    int modes[4];
    int i;
    modes[0] = ID_VIEW_STANDARD;
    modes[1] = ID_VIEW_SCIENTIFIC;
    modes[2] = ID_VIEW_PROGRAMMER;
    modes[3] = ID_VIEW_STATISTICS;
    for (i = 0; i < 4; i++)
        CheckMenuItem(hMenu, modes[i], MF_BYCOMMAND |
            (g_mode == i ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_HISTORY,
        g_panel == PANEL_HISTORY ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_VIEW_DIGIT_GRP,
        digit_grouping ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_VIEW_BASIC,
        g_basic_mode ? MF_CHECKED : MF_UNCHECKED);
}
