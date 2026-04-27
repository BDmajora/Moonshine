#ifndef CALC_LOGIC_H
#define CALC_LOGIC_H

#include <windows.h>
#include "calc.h"

/* Global State accessible by UI */
extern double current;
extern double operand;
extern double memory;
extern int    op;
extern BOOL   new_input;
extern BOOL   has_operand;
extern BOOL   error;
extern WCHAR  display_str[MAX_DISPLAY];

/* Logic Function Prototypes */
void set_display(double val);
void calc_error(HWND hwnd, const WCHAR* msg);
void do_equals(HWND hwnd);
void handle_op(HWND hwnd, int new_op);
void handle_digit(HWND hwnd, WCHAR ch);
void handle_dot(HWND hwnd);
void handle_back(HWND hwnd);

#endif