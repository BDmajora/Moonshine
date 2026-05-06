#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>
#include "calc.h"
#include "calc_logic.h"
#include "calc_ui.h"
#include "calc_panel.h"
#include "calc_appMenu.h"

void on_command(HWND hwnd, int id) {

    /* ── Mode switching ── */
    /* ui_switch_mode handles: set g_mode → resize → rebuild → layout → menu */
    if (id >= ID_VIEW_STANDARD && id <= ID_VIEW_STATISTICS) {
        ui_switch_mode(hwnd, id - ID_VIEW_STANDARD);
        return;
    }
    if (id == ID_VIEW_BASIC) {
        g_basic_mode = !g_basic_mode;
        /* basic mode only affects button row count, same mode */
        ui_switch_mode(hwnd, g_mode);
        return;
    }

    /* ── Panel switching ── */
    /* ui_show_panel toggles: same panel → PANEL_NONE, new panel → open */
    if (id == ID_VIEW_HISTORY) { ui_show_panel(hwnd, PANEL_HISTORY); return; }
    if (id == ID_PANEL_UNIT)   { ui_show_panel(hwnd, PANEL_UNIT);    return; }
    if (id == ID_PANEL_DATE)   { ui_show_panel(hwnd, PANEL_DATE);    return; }

    /* ── Worksheet switching ── */
    /* ui_show_worksheet toggles: same worksheet → close, new → open */
    if (id >= ID_WS_MORTGAGE && id <= ID_WS_FUEL_LKM) {
        ui_show_worksheet(hwnd, id - ID_WS_MORTGAGE);
        return;
    }

    if (id == ID_VIEW_DIGIT_GRP) {
        digit_grouping = !digit_grouping;
        ui_update_digit_grouping(hwnd);
        ui_update_menu_check(GetMenu(hwnd));
        return;
    }

    /* ── History panel clear ── */
    if (id == ID_PANEL_CLOSE) {
        history_clear();
        ui_update_history_panel(hwnd);
        return;
    }

    /* ── Panel calculate buttons ── */
    if (id == ID_DATE_CALC) { panel_date_calculate(hwnd); return; }
    if (id == ID_WS_CALC)   { panel_ws_calculate(hwnd, g_worksheet); return; }

    /* ── Unit panel: repopulate on type change ── */
    if (id == ID_UNIT_TYPE) {
        HWND hf, ht;
        int cat, i;
        cat = (int)SendMessageW(GetDlgItem(hwnd, ID_UNIT_TYPE), CB_GETCURSEL, 0, 0);
        hf  = GetDlgItem(hwnd, ID_UNIT_FROM_UNT);
        ht  = GetDlgItem(hwnd, ID_UNIT_TO_UNT);
        SendMessageW(hf, CB_RESETCONTENT, 0, 0);
        SendMessageW(ht, CB_RESETCONTENT, 0, 0);
        if (cat >= 0 && cat < g_unit_cat_count) {
            for (i = 0; i < g_unit_cats[cat].unit_count; i++) {
                SendMessageW(hf, CB_ADDSTRING, 0, (LPARAM)g_unit_cats[cat].units[i].name);
                SendMessageW(ht, CB_ADDSTRING, 0, (LPARAM)g_unit_cats[cat].units[i].name);
            }
            SendMessageW(hf, CB_SETCURSEL, 0, 0);
            SendMessageW(ht, CB_SETCURSEL, (1 < g_unit_cats[cat].unit_count ? 1 : 0), 0);
        }
        return;
    }
    if (id == ID_UNIT_FROM_UNT || id == ID_UNIT_TO_UNT) {
        panel_unit_convert(hwnd); return;
    }

    /* ── Programmer base radios ── */
    if (id == ID_RADIO_HEX) { prog_base=BASE_HEX; prog_set_display(prog_value()); ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return; }
    if (id == ID_RADIO_DEC) { prog_base=BASE_DEC; set_display((double)prog_value()); ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return; }
    if (id == ID_RADIO_OCT) { prog_base=BASE_OCT; prog_set_display(prog_value()); ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return; }
    if (id == ID_RADIO_BIN) { prog_base=BASE_BIN; prog_set_display(prog_value()); ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return; }

    /* ── Programmer word radios ── */
    if (id == ID_RADIO_QWORD) { prog_word=WORD_QWORD; prog_set_display(prog_value()); ui_update_display(hwnd); if(g_mode==MODE_PROGRAMMER)ui_update_bit_display(hwnd); return; }
    if (id == ID_RADIO_DWORD) { prog_word=WORD_DWORD; prog_set_display(prog_value()); ui_update_display(hwnd); if(g_mode==MODE_PROGRAMMER)ui_update_bit_display(hwnd); return; }
    if (id == ID_RADIO_WORD)  { prog_word=WORD_WORD;  prog_set_display(prog_value()); ui_update_display(hwnd); if(g_mode==MODE_PROGRAMMER)ui_update_bit_display(hwnd); return; }
    if (id == ID_RADIO_BYTE)  { prog_word=WORD_BYTE;  prog_set_display(prog_value()); ui_update_display(hwnd); if(g_mode==MODE_PROGRAMMER)ui_update_bit_display(hwnd); return; }

    /* ── Scientific angle radios ── */
    if (id == ID_RAD_DEG)  { angle_unit = ANGLE_DEG;  return; }
    if (id == ID_RAD_RAD)  { angle_unit = ANGLE_RAD;  return; }
    if (id == ID_RAD_GRAD) { angle_unit = ANGLE_GRAD; return; }

    /* ── Digit buttons ── */
    if (id >= ID_0 && id <= ID_9) {
        handle_digit(hwnd, (WCHAR)(L'0' + (id - ID_0)));
        if (g_mode == MODE_PROGRAMMER) ui_update_bit_display(hwnd);
        return;
    }
    if (id==ID_A){handle_digit(hwnd,L'A');return;}
    if (id==ID_B){handle_digit(hwnd,L'B');return;}
    if (id==ID_C_HEX){handle_digit(hwnd,L'C');return;}
    if (id==ID_D){handle_digit(hwnd,L'D');return;}
    if (id==ID_E_HEX){handle_digit(hwnd,L'E');return;}
    if (id==ID_F){handle_digit(hwnd,L'F');return;}

    /* ── Basic ops ── */
    if (id==ID_DOT) { handle_dot(hwnd); return; }
    if (id==ID_ADD) { handle_op(hwnd, OP_ADD); return; }
    if (id==ID_SUB) { handle_op(hwnd, OP_SUB); return; }
    if (id==ID_MUL) { handle_op(hwnd, OP_MUL); return; }
    if (id==ID_DIV) { handle_op(hwnd, OP_DIV); return; }
    if (id==ID_EQ)  {
        do_equals(hwnd);
        if (g_panel == PANEL_HISTORY) ui_update_history_panel(hwnd);
        return;
    }
    if (id==ID_BACK) { handle_back(hwnd); return; }
    if (id==ID_SIGN) {
        if (!error) { set_display(-_wtof(display_str)); ui_update_display(hwnd); }
        return;
    }
    if (id==ID_SQRT) {
        double v;
        if (error) return;
        v = _wtof(display_str);
        if (v < 0) { calc_error(hwnd, L"Invalid input"); return; }
        set_display(sqrt(v)); ui_update_display(hwnd); new_input = TRUE;
        return;
    }
    if (id==ID_PERCENT) {
        if (!error) { set_display(_wtof(display_str)/100.0); ui_update_display(hwnd); }
        return;
    }
    if (id==ID_RECIP) {
        double v;
        if (error) return;
        v = _wtof(display_str);
        if (v==0) { calc_error(hwnd, L"Cannot divide by zero"); return; }
        set_display(1.0/v); ui_update_display(hwnd); new_input=TRUE;
        return;
    }
    if (id==ID_CLR) {
        current=operand=0.0; op=OP_NONE;
        has_operand=FALSE; new_input=TRUE; error=FALSE;
        wcscpy(display_str, L"0"); ui_update_display(hwnd);
        if (g_mode==MODE_STATISTICS) { stat_clear_all(); ui_update_stat_display(hwnd); }
        return;
    }
    if (id==ID_CE) {
        error=FALSE; wcscpy(display_str, L"0"); new_input=TRUE;
        ui_update_display(hwnd); return;
    }

    /* ── Memory ── */
    if(id==ID_MC)    {memory=0.0;return;}
    if(id==ID_MR)    {if(!error){set_display(memory);ui_update_display(hwnd);new_input=TRUE;}return;}
    if(id==ID_MS)    {if(!error){memory=_wtof(display_str);new_input=TRUE;}return;}
    if(id==ID_MPLUS) {if(!error)memory+=_wtof(display_str);return;}
    if(id==ID_MMINUS){if(!error)memory-=_wtof(display_str);return;}

    /* ── Scientific ── */
    if(id==ID_SCI_INV){inv_mode=!inv_mode;return;}
    if(id==ID_SCI_FE){fe_mode=!fe_mode;if(!error){set_display(_wtof(display_str));ui_update_display(hwnd);}return;}
    if(id==ID_SCI_PI){set_display(3.14159265358979323846);ui_update_display(hwnd);new_input=TRUE;return;}
    if(id==ID_SCI_MOD){handle_op(hwnd,OP_MOD);return;}
    if(id==ID_SCI_POWY){handle_op(hwnd,OP_POW);return;}
    if(id==ID_SCI_YROOTX){handle_op(hwnd,OP_NROOT);return;}
    if(id==ID_SCI_LPAREN){handle_paren_open(hwnd);return;}
    if(id==ID_SCI_RPAREN){handle_paren_close(hwnd);return;}
    if (id==ID_SCI_SIN||id==ID_SCI_COS||id==ID_SCI_TAN||
        id==ID_SCI_SINH||id==ID_SCI_COSH||id==ID_SCI_TANH||
        id==ID_SCI_LOG||id==ID_SCI_LN||id==ID_SCI_POW2||
        id==ID_SCI_POW3||id==ID_SCI_CUBE||id==ID_SCI_FACT||
        id==ID_SCI_INT||id==ID_SCI_DMS) { sci_unary(hwnd,id); return; }

    /* ── Programmer bitwise ── */
    if(id==ID_PROG_AND){handle_op(hwnd,OP_AND);return;}
    if(id==ID_PROG_OR) {handle_op(hwnd,OP_OR); return;}
    if(id==ID_PROG_XOR){handle_op(hwnd,OP_XOR);return;}
    if(id==ID_PROG_MOD){handle_op(hwnd,OP_MOD);return;}
    if(id==ID_PROG_LSH){handle_op(hwnd,OP_LSH);return;}
    if(id==ID_PROG_RSH){handle_op(hwnd,OP_RSH);return;}
    if(id==ID_PROG_NOT){
        long long v=prog_value();
        prog_set_display(~v);
        ui_update_display(hwnd);ui_update_bit_display(hwnd);new_input=TRUE;return;
    }
    if(id==ID_PROG_ROL||id==ID_PROG_ROR){
        unsigned long long v=(unsigned long long)prog_value();
        int bits=(prog_word==WORD_BYTE?8:prog_word==WORD_WORD?16:prog_word==WORD_DWORD?32:64);
        unsigned long long mask=(bits==64)?~0ULL:(1ULL<<bits)-1;
        v&=mask;
        v=(id==ID_PROG_ROL)?((v<<1)|(v>>(bits-1)))&mask:((v>>1)|(v<<(bits-1)))&mask;
        prog_set_display((long long)v);
        ui_update_display(hwnd);ui_update_bit_display(hwnd);new_input=TRUE;return;
    }

    /* ── Statistics ── */
    if(id==ID_STAT_ADD){
        if(!error){stat_add(_wtof(display_str));ui_update_stat_display(hwnd);wcscpy(display_str,L"0");new_input=TRUE;}
        return;
    }
    if(id==ID_STAT_CAD){
        HWND hl=GetDlgItem(hwnd,ID_DISP_HISTORY);
        int sel=(int)SendMessageW(hl,LB_GETCURSEL,0,0),i;
        if(sel>=0&&sel<stat_count){
            for(i=sel;i<stat_count-1;i++) stat_data[i]=stat_data[i+1];
            stat_count--;ui_update_stat_display(hwnd);}
        return;
    }
    if(id==ID_STAT_MEAN) {set_display(stat_mean());     ui_update_display(hwnd);new_input=TRUE;return;}
    if(id==ID_STAT_MEAN2){set_display(stat_mean_sq());  ui_update_display(hwnd);new_input=TRUE;return;}
    if(id==ID_STAT_SUMX) {set_display(stat_sum());      ui_update_display(hwnd);new_input=TRUE;return;}
    if(id==ID_STAT_SUMX2){set_display(stat_sum_sq());   ui_update_display(hwnd);new_input=TRUE;return;}
    if(id==ID_STAT_SDEV) {set_display(stat_sdev(TRUE)); ui_update_display(hwnd);new_input=TRUE;return;}
    if(id==ID_STAT_SDEV1){set_display(stat_sdev(FALSE));ui_update_display(hwnd);new_input=TRUE;return;}

    /* ── Edit ── */
    if(id==500){
        if(OpenClipboard(hwnd)){
            SIZE_T sz=(wcslen(display_str)+1)*sizeof(WCHAR);
            HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,sz);
            if(hg){WCHAR*p=(WCHAR*)GlobalLock(hg);if(p){wcscpy(p,display_str);GlobalUnlock(hg);EmptyClipboard();SetClipboardData(CF_UNICODETEXT,hg);}}
            CloseClipboard();}
        return;
    }
    if(id==501){
        if(OpenClipboard(hwnd)){
            HANDLE h=GetClipboardData(CF_UNICODETEXT);
            if(h){WCHAR*p=(WCHAR*)GlobalLock(h);if(p){lstrcpynW(display_str,p,MAX_DISPLAY-1);display_str[MAX_DISPLAY-1]=L'\0';ui_update_display(hwnd);new_input=TRUE;}GlobalUnlock(h);}
            CloseClipboard();}
        return;
    }
    if(id==ID_HELP_ABOUT){
        MessageBoxW(hwnd,L"Calculator\n\nWindows 7 Calculator Clone\nBuilt in C/Win32",
                    L"About Calculator",MB_OK|MB_ICONINFORMATION);
        return;
    }
}