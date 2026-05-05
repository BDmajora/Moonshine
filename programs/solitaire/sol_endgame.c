#include "sol_endgame.h"
#include "sol_layout.h"
#include "solitaire.h"
#include "sol_timer.h"
#include <math.h>
#include <stdlib.h>

// Global flag for victory animation status
BOOL g_endgame_active = FALSE;

// Internal state for bouncing cards and window dimensions
static BounceCard bounce_cards[NUM_BOUNCE_CARDS];
static int        next_launch   = 0;
static int        window_w      = 600;
static int        window_h      = 400;
static HWND       g_hwnd        = NULL;
static int        g_final_bonus = 0;

// Movement tracking for animation acceleration
static int        last_mx       = -1;
static int        last_my       = -1;
static int        g_extra_steps = 0;

// Reset animation state and kill the game timer
void EndGame_Stop(HWND hwnd)
{
    if (!g_endgame_active) return;
    g_endgame_active = FALSE;
    KillTimer(hwnd, ENDGAME_TIMER_ID);
    InvalidateRect(hwnd, NULL, TRUE);
}

// Handle user choice to restart or exit after victory
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

// Calculate time-based score bonus upon winning
static int CalculateBonus(void) 
{
    DWORD elapsed; 
    if (g_Game.score == 0) return 0; 
    elapsed = Timer_GetElapsed();
    return (elapsed >= 30) ? (700000 / elapsed) : 0;
}

// Initialize victory sequence parameters and start timer
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
    g_final_bonus    = CalculateBonus();

    for (i = 0; i < NUM_BOUNCE_CARDS; i++) {
        bounce_cards[i].active = FALSE;
    }

    SetTimer(hwnd, ENDGAME_TIMER_ID, ENDGAME_TIMER_MS, NULL);
}

// Spawn the next card from the foundation piles
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

// Accelerate animation based on mouse travel distance
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

// Process physics steps and render cards to device context
void EndGame_Tick(HWND hwnd)
{
    int i, step, total_steps;
    BounceCard *bc;
    BOOL any_moving;
    HDC hdc;
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

            // Bounce card off bottom of window
            if (bc->y + CARD_HEIGHT >= window_h) {
                bc->y  = (float)(window_h - CARD_HEIGHT);
                bc->vy = -(bc->vy * BOUNCE_DAMPEN);
                if (bc->vy > -1.0f) bc->vy = 0;
            }

            cdtDraw(hdc, (int)bc->x, (int)bc->y, 
                    bc->card & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);

            // Check if card has exited horizontal bounds
            if (bc->x + CARD_WIDTH < 0 || bc->x > window_w) {
                bc->active = FALSE; 
            } else {
                any_moving = TRUE;
            }
        }

        // Launch next card or conclude animation
        if (!any_moving) {
            if (next_launch < NUM_BOUNCE_CARDS) {
                launch_card();
            } else {
                ReleaseDC(hwnd, hdc);
                EndGame_Dismiss(hwnd);
                return;
            }
        }
    }

    ReleaseDC(hwnd, hdc);
    
    // Invalidate status bar for bonus updates
    rcStatus.left = 0;
    rcStatus.top = window_h;
    rcStatus.right = window_w;
    rcStatus.bottom = window_h + STATUS_BAR_HEIGHT;
    InvalidateRect(hwnd, &rcStatus, TRUE);
}

// Display calculated bonus in the status bar area
void EndGame_Draw(HDC hdc, int width, int height)
{
    HFONT hfont, hOldFont;
    RECT rcStatus;
    WCHAR buf[64];

    rcStatus.left = 0; 
    rcStatus.top = window_h;
    rcStatus.right = width; 
    rcStatus.bottom = height;

    hfont = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"MS Sans Serif");
    hOldFont = (HFONT)SelectObject(hdc, hfont);
    
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, RGB(255, 255, 255));

    wsprintfW(buf, L"Bonus: %d", g_final_bonus);
    DrawTextW(hdc, buf, -1, &rcStatus, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    SelectObject(hdc, hOldFont);
    DeleteObject(hfont);
}

// Verify if foundation piles are complete
BOOL EndGame_CheckWin(void)
{
    int i, total = 0;
    for (i = 0; i < 4; i++) {
        total += g_Game.found_top[i];
    }
    return total == 52;
}