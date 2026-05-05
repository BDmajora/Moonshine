#ifndef CALC_STATE_H
#define CALC_STATE_H

#include <windows.h>
#include "calc.h"

/* ── Global state ────────────────────────────────────────────────── */
extern double  current;
extern double  operand;
extern double  memory;
extern int     op;
extern BOOL    new_input;
extern BOOL    has_operand;
extern BOOL    error;
extern BOOL    inv_mode;          /* scientific Inv toggle */
extern BOOL    fe_mode;           /* F-E (engineering notation) toggle */
extern BOOL    digit_grouping;    /* thousands separator */
extern int     angle_unit;        /* ANGLE_DEG / RAD / GRAD */
extern int     prog_base;         /* BASE_HEX/DEC/OCT/BIN */
extern int     prog_word;         /* WORD_QWORD/DWORD/WORD/BYTE */
extern int     paren_depth;       /* open-paren count (scientific) */

extern WCHAR   display_str[MAX_DISPLAY];

/* Statistics data */
#define MAX_STAT 256
extern double  stat_data[MAX_STAT];
extern int     stat_count;

/* History */
#define MAX_HIST 50
extern WCHAR   history[MAX_HIST][MAX_DISPLAY * 3];
extern int     hist_count;

/* ── Core State Functions ────────────────────────────────────────── */
void update_ui_text(HWND hwnd);
void set_display(double val);
void set_display_int(long long val);
void calc_error(HWND hwnd, const WCHAR *msg);

/* ── Programmer Helpers ──────────────────────────────────────────── */
long long apply_word_mask(long long v);
long long prog_value(void);
void prog_set_display(long long v);

#endif /* CALC_STATE_H */