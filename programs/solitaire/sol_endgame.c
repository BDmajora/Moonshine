#include "sol_endgame.h"
#include <math.h>
#include <stdlib.h>

BOOL g_endgame_active = FALSE;

static BounceCard bounce_cards[NUM_BOUNCE_CARDS];
static int        next_launch  = 0;
static int        window_w     = 600;
static int        window_h     = 400;
static int        bonus_score  = 0;
static HWND       g_hwnd       = NULL;

/* Mouse speed tracking */
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
    bonus_score      = 0;
    last_mx = -1;
    last_my = -1;
    g_extra_steps = 0;

    for (i = 0; i < NUM_BOUNCE_CARDS; i++)
        bounce_cards[i].active = FALSE;

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
    
    /* Horizontal velocity is randomized; vertical is fixed for the rhythm pattern */
    bc->vx     = (float)((rand() % 9) - 4); 
    if (bc->vx == 0) bc->vx = 2.0f;
    
    bc->vy     = -7.0f; /* Fixed "pop" height */
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

    /* Declare all variables at the top for ISO C90 compliance */
    hdc = GetDC(hwnd); 
    total_steps = 1 + g_extra_steps;
    g_extra_steps = 0; 

    if (total_steps > 20) total_steps = 20;

    for (step = 0; step < total_steps; step++) {
        any_moving = FALSE;

        for (i = 0; i < next_launch; i++) {
            bc = &bounce_cards[i];
            if (!bc->active) continue;

            /* Physics calculation */
            bc->vy += GRAVITY; 
            bc->x  += bc->vx;
            bc->y  += bc->vy;

            /* Floor collision */
            if (bc->y + 96 >= window_h) {
                bc->y  = (float)(window_h - 96);
                bc->vy = -(bc->vy * BOUNCE_DAMPEN);
                if (bc->vy > -1.0f) bc->vy = 0;
            }

            /* Draw directly to DC for every step to create the dense trail */
            cdtDraw(hdc, (int)bc->x, (int)bc->y, 
                    bc->card & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);

            /* Check if card is off-screen */
            if (bc->x + 71 < 0 || bc->x > window_w) {
                bc->active = FALSE; 
            } else {
                any_moving = TRUE;
            }
        }

        /* Wait for current card to finish before launching the next foundation card */
        if (!any_moving) {
            if (next_launch < NUM_BOUNCE_CARDS) {
                launch_card();
                bonus_score += 5;
            } else {
                ReleaseDC(hwnd, hdc);
                EndGame_Dismiss(hwnd);
                return;
            }
        }
    }

    ReleaseDC(hwnd, hdc);
    
    /* Repaint the status bar area only */
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

    wsprintfW(buf, L"Bonus: %d", bonus_score);
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