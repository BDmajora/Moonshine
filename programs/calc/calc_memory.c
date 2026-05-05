#include "calc_memory.h"
#include <wchar.h>
#include <math.h>

/* ── History ─────────────────────────────────────────────────────── */

void history_push(const WCHAR *entry) {
    if (hist_count == MAX_HIST) {
        int i;
        for (i = 0; i < MAX_HIST - 1; i++)
            wcscpy(history[i], history[i+1]);
        hist_count--;
    }
    lstrcpynW(history[hist_count++], entry, MAX_DISPLAY * 3);
}

void history_clear(void) { hist_count = 0; }

/* ── Statistics ──────────────────────────────────────────────────── */

void stat_add(double v) {
    if (stat_count < MAX_STAT)
        stat_data[stat_count++] = v;
}

void stat_clear_all(void) { stat_count = 0; }

double stat_mean(void) {
    int i; double s = 0;
    if (!stat_count) return 0;
    for (i = 0; i < stat_count; i++) s += stat_data[i];
    return s / stat_count;
}

double stat_sum(void) {
    int i; double s = 0;
    for (i = 0; i < stat_count; i++) s += stat_data[i];
    return s;
}

double stat_sum_sq(void) {
    int i; double s = 0;
    for (i = 0; i < stat_count; i++) s += stat_data[i] * stat_data[i];
    return s;
}

double stat_mean_sq(void) {
    return stat_count ? stat_sum_sq() / stat_count : 0;
}

double stat_sdev(BOOL population) {
    int i; double m, s = 0;
    int n = population ? stat_count : stat_count - 1;
    if (n <= 0) return 0;
    m = stat_mean();
    for (i = 0; i < stat_count; i++) {
        double d = stat_data[i] - m;
        s += d * d;
    }
    return sqrt(s / n);
}