#include "sol_endgame.h"
#include "sol_layout.h"
#include "solitaire.h"  // For g_Game
#include "sol_timer.h"
#include <math.h>
#include <stdlib.h>

BOOL g_endgame_active = FALSE;

static BounceCard bounce_cards[NUM_BOUNCE_CARDS];
static int        next_launch  = 0;
static int        window_w     = 600;
static int        window_h     = 400;
static HWND       g_hwnd       = NULL;
static int        g_final_bonus = 0; // Calculated once at start

static int        last_mx       = -1;
static int        last_my       = -1;
static int        g_extra_steps = 0;

void EndGame_Stop(HWND hwnd)
{
    if (!g_endgame_active) return;
    g_endgame_active = FALSE;
    KillTimer(hwnd, ENDGAME_TIMER_ID);
    InvalidateRect(hwnd, NULL, TRUE);
}

void EndGame_Dismiss(HWND hwnd)
{
    if (!g_endgame_active) return;
    EndGame_Stop(hwnd);
    if (MessageBoxW(hwnd, L"Deal again?", L"Solitaire", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        Game_Init();
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        PostQuitMessage(0);
    }
}

void EndGame_Start(HWND hwnd)
{
    RECT rc;
    int i;
    if (g_endgame_active) return;

    g_hwnd = hwnd;
    GetClientRect(hwnd, &rc);
    window_w = rc.right;
    window_h = rc.bottom - STATUS_BAR_HEIGHT;

    g_endgame_active = TRUE;
    next_launch      = 0;
    last_mx          = -1;
    last_my          = -1;
    g_extra_steps    = 0;

    for (i = 0; i < NUM_BOUNCE_CARDS; i++)
        bounce_cards[i].active = FALSE;

    // USE SOLITAIRE - Simple If/Else for bonus calculation
    // A legitimate win always has a non-zero score. 
    // Input_CheatWin() explicitly sets g_Game.score = 0 before calling this.
    if (g_Game.score == 0) { 
        g_final_bonus = 0;
    } else {
        DWORD elapsed = Timer_GetElapsed();
        if (elapsed >= 30) {
            g_final_bonus = 700000 / elapsed; // Standard Solitaire time bonus formula
        } else {
            g_final_bonus = 0;
        }
    }

    SetTimer(hwnd, ENDGAME_TIMER_ID, ENDGAME_TIMER_MS, NULL);
}

static void launch_card(void)
{
    BounceCard *bc;
    int suit, face, found_idx;

    if (next_launch >= NUM_BOUNCE_CARDS) return;

    bc = &bounce_cards[next_launch];
    found_idx = next_launch % 4; 
    suit = found_idx;
    face = 13 - (next_launch / 4) - 1; 

    bc->card   = (CARD)((face * 4) + suit);
    bc->x      = (float)(X_MARGIN + (3 + found_idx) * X_SPACING);
    bc->y      = (float)(Y_MARGIN);
    
    bc->vx     = (float)((rand() % 9) - 4); 
    if (bc->vx == 0) bc->vx = 2.0f;
    
    bc->vy     = -7.0f; 
    bc->active = TRUE;
    
    next_launch++;
}

void EndGame_MouseMove(int mx, int my)
{
    if (!g_endgame_active) return;
    
    if (last_mx != -1 && last_my != -1) {
        int dist = abs(mx - last_mx) + abs(my - last_my);
        if (dist > 0) {
            g_extra_steps += (dist / 4); 
        }
    }
    last_mx = mx;
    last_my = my;
}

void EndGame_Tick(HWND hwnd)
{
    int i, step;
    BounceCard *bc;
    BOOL any_moving;
    HDC hdc;
    int total_steps;
    RECT rcStatus;

    hdc = GetDC(hwnd); 
    total_steps = 1 + g_extra_steps;
    g_extra_steps = 0; 

    if (total_steps > 20) total_steps = 20;

    for (step = 0; step < total_steps; step++) {
        any_moving = FALSE;

        for (i = 0; i < next_launch; i++) {
            bc = &bounce_cards[i];
            if (!bc->active) continue;

            bc->vy += GRAVITY; 
            bc->x  += bc->vx;
            bc->y  += bc->vy;

            if (bc->y + CARD_HEIGHT >= window_h) {
                bc->y  = (float)(window_h - CARD_HEIGHT);
                bc->vy = -(bc->vy * BOUNCE_DAMPEN);
                if (bc->vy > -1.0f) bc->vy = 0;
            }

            cdtDraw(hdc, (int)bc->x, (int)bc->y, 
                    bc->card & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);

            if (bc->x + CARD_WIDTH < 0 || bc->x > window_w) {
                bc->active = FALSE; 
            } else {
                any_moving = TRUE;
            }
        }

        if (!any_moving) {
            if (next_launch < NUM_BOUNCE_CARDS) {
                launch_card();
                // Notice there is no bonus manipulation happening here anymore
            } else {
                ReleaseDC(hwnd, hdc);
                EndGame_Dismiss(hwnd);
                return;
            }
        }
    }

    ReleaseDC(hwnd, hdc);
    
    rcStatus.left = 0;
    rcStatus.top = window_h;
    rcStatus.right = window_w;
    rcStatus.bottom = window_h + STATUS_BAR_HEIGHT;
    InvalidateRect(hwnd, &rcStatus, TRUE);
}

void EndGame_Draw(HDC hdc, int width, int height)
{
    HFONT hfont, hOldFont;
    RECT rcStatus;
    WCHAR buf[64];

    rcStatus.left = 0; rcStatus.top = window_h;
    rcStatus.right = width; rcStatus.bottom = height;

    hfont = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"MS Sans Serif");
    hOldFont = (HFONT)SelectObject(hdc, hfont);
    
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, RGB(255, 255, 255));

    // Uses the locked, single-calculated bonus
    wsprintfW(buf, L"Bonus: %d", g_final_bonus);
    DrawTextW(hdc, buf, -1, &rcStatus, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    SelectObject(hdc, hOldFont);
    DeleteObject(hfont);
}

BOOL EndGame_CheckWin(void)
{
    int i, total = 0;
    for (i = 0; i < 4; i++)
        total += g_Game.found_top[i];
    return total == 52;
}