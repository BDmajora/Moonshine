#ifndef CALC_MEMORY_H
#define CALC_MEMORY_H

#include <windows.h>
#include "calc_state.h"

/* ── History helpers ─────────────────────────────────────────────── */
void history_push(const WCHAR *entry);
void history_clear(void);

/* ── Statistics helpers ──────────────────────────────────────────── */
void stat_add(double v);
void stat_clear_all(void);
double stat_mean(void);
double stat_mean_sq(void);
double stat_sum(void);
double stat_sum_sq(void);
double stat_sdev(BOOL population); /* FALSE = sample (n-1) */

#endif /* CALC_MEMORY_H */