#ifndef CALC_H
#define CALC_H

#include <windows.h>
#include <commctrl.h>

/* ── Modes ─────────────────────────────────────────────────────────── */
#define MODE_STANDARD     0
#define MODE_SCIENTIFIC   1
#define MODE_PROGRAMMER   2
#define MODE_STATISTICS   3

/* ── Side-panel types ───────────────────────────────────────────────── */
#define PANEL_NONE        0
#define PANEL_HISTORY     1
#define PANEL_UNIT        2
#define PANEL_DATE        3
#define PANEL_WORKSHEET   4  /* mortgage / vehicle-lease / fuel-mpg / fuel-lkm */

/* ── Worksheet sub-types ────────────────────────────────────────────── */
#define WS_MORTGAGE       0
#define WS_VEHICLE        1
#define WS_FUEL_MPG       2
#define WS_FUEL_LKM       3

/* ── Operators ──────────────────────────────────────────────────────── */
#define OP_NONE   0
#define OP_ADD    1
#define OP_SUB    2
#define OP_MUL    3
#define OP_DIV    4
#define OP_POW    5   /* x^y  (scientific) */
#define OP_NROOT  6   /* y√x  (scientific) */
#define OP_MOD    7   /* Mod  (programmer) */
#define OP_AND    8
#define OP_OR     9
#define OP_XOR   10
#define OP_LSH   11
#define OP_RSH   12

/* ── Programmer bases ───────────────────────────────────────────────── */
#define BASE_HEX  16
#define BASE_DEC  10
#define BASE_OCT   8
#define BASE_BIN   2

/* ── Programmer word sizes ──────────────────────────────────────────── */
#define WORD_QWORD 0
#define WORD_DWORD 1
#define WORD_WORD  2
#define WORD_BYTE  3

/* ── Trig angle units ───────────────────────────────────────────────── */
#define ANGLE_DEG  0
#define ANGLE_RAD  1
#define ANGLE_GRAD 2

/* ── Display size ───────────────────────────────────────────────────── */
#define MAX_DISPLAY 64

/* ─────────────────────── Control IDs ─────────────────────────────── */

/* Display */
#define ID_DISPLAY       200
#define ID_DISP_BITS     201   /* programmer bit-strip */
#define ID_DISP_HISTORY  202   /* statistics history list */

/* Digit buttons (shared across modes) */
#define ID_0   100
#define ID_1   101
#define ID_2   102
#define ID_3   103
#define ID_4   104
#define ID_5   105
#define ID_6   106
#define ID_7   107
#define ID_8   108
#define ID_9   109

/* Hex letter buttons (programmer) */
#define ID_A   110
#define ID_B   111
#define ID_C_HEX 112
#define ID_D   113
#define ID_E_HEX 114
#define ID_F   115

/* Standard operators */
#define ID_DOT      120
#define ID_ADD      121
#define ID_SUB      122
#define ID_MUL      123
#define ID_DIV      124
#define ID_EQ       125
#define ID_CLR      126
#define ID_CE       127
#define ID_BACK     128
#define ID_SIGN     129
#define ID_SQRT     130
#define ID_PERCENT  131
#define ID_RECIP    132

/* Memory */
#define ID_MC     140
#define ID_MR     141
#define ID_MS     142
#define ID_MPLUS  143
#define ID_MMINUS 144

/* Scientific extras */
#define ID_SCI_INV    150
#define ID_SCI_LN     151
#define ID_SCI_LOG    152
#define ID_SCI_EXP    153   /* 10^x */
#define ID_SCI_POW2   154   /* x^2  */
#define ID_SCI_POWY   155   /* x^y  */
#define ID_SCI_POW3   156   /* x^3  */
#define ID_SCI_CUBE   157   /* ∛x   */
#define ID_SCI_YROOTX 158   /* y√x  */
#define ID_SCI_SIN    160
#define ID_SCI_COS    161
#define ID_SCI_TAN    162
#define ID_SCI_SINH   163
#define ID_SCI_COSH   164
#define ID_SCI_TANH   165
#define ID_SCI_PI     166
#define ID_SCI_FE     167   /* F-E toggle */
#define ID_SCI_MOD    168
#define ID_SCI_FACT   169   /* n! */
#define ID_SCI_INT    170
#define ID_SCI_DMS    171
#define ID_SCI_LPAREN 172
#define ID_SCI_RPAREN 173

/* Angle-mode radio buttons */
#define ID_RAD_DEG  180
#define ID_RAD_RAD  181
#define ID_RAD_GRAD 182

/* Programmer extras */
#define ID_PROG_AND  190
#define ID_PROG_OR   191
#define ID_PROG_XOR  192
#define ID_PROG_NOT  193
#define ID_PROG_LSH  194
#define ID_PROG_RSH  195
#define ID_PROG_ROL  196
#define ID_PROG_ROR  197
#define ID_PROG_MOD  198

/* Programmer base radios */
#define ID_RADIO_HEX 210
#define ID_RADIO_DEC 211
#define ID_RADIO_OCT 212
#define ID_RADIO_BIN 213

/* Programmer word-size radios */
#define ID_RADIO_QWORD 220
#define ID_RADIO_DWORD 221
#define ID_RADIO_WORD  222
#define ID_RADIO_BYTE  223

/* Statistics */
#define ID_STAT_ADD   230
#define ID_STAT_CAD   231
#define ID_STAT_FE    232
#define ID_STAT_EXP   233
#define ID_STAT_MEAN  234
#define ID_STAT_MEAN2 235
#define ID_STAT_SUMX  236
#define ID_STAT_SUMX2 237
#define ID_STAT_SDEV  238
#define ID_STAT_SDEV1 239

/* Side panel / worksheet controls */
#define ID_PANEL_CLOSE   300
#define ID_PANEL_COMBO   301
#define ID_UNIT_TYPE     302
#define ID_UNIT_FROM_VAL 303
#define ID_UNIT_FROM_UNT 304
#define ID_UNIT_TO_VAL   305
#define ID_UNIT_TO_UNT   306
#define ID_DATE_TYPE     307
#define ID_DATE_FROM     308
#define ID_DATE_TO       309
#define ID_DATE_CALC     310
#define ID_DATE_RES1     311
#define ID_DATE_RES2     312
#define ID_WS_COMBO      313
#define ID_WS_CALC       314
#define ID_WS_RES        315
/* Worksheet input fields (up to 6) */
#define ID_WS_IN1  320
#define ID_WS_IN2  321
#define ID_WS_IN3  322
#define ID_WS_IN4  323
#define ID_WS_IN5  324
#define ID_WS_IN6  325
/* Worksheet labels (up to 6) */
#define ID_WS_LBL1 330
#define ID_WS_LBL2 331
#define ID_WS_LBL3 332
#define ID_WS_LBL4 333
#define ID_WS_LBL5 334
#define ID_WS_LBL6 335

/* Menu items */
#define ID_VIEW_STANDARD   401
#define ID_VIEW_SCIENTIFIC 402
#define ID_VIEW_PROGRAMMER 403
#define ID_VIEW_STATISTICS 404
#define ID_VIEW_HISTORY    410
#define ID_VIEW_DIGIT_GRP  411
#define ID_VIEW_BASIC      412
#define ID_PANEL_UNIT      413
#define ID_PANEL_DATE      414
#define ID_PANEL_WORKSHEET 415
#define ID_HELP_ABOUT      420

/* Sub-items (worksheets submenu) */
#define ID_WS_MORTGAGE  430
#define ID_WS_VEHICLE   431
#define ID_WS_FUEL_MPG  432
#define ID_WS_FUEL_LKM  433

#endif /* CALC_H */