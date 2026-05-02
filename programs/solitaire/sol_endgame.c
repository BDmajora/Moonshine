#include "sol_endgame.h"
#include <math.h>

BOOL g_endgame_active = FALSE;

static BounceCard bounce_cards[NUM_BOUNCE_CARDS];
static int        bounce_count = 0;
static int        next_launch  = 0;
static DWORD      last_launch  = 0;
static int        window_w     = 600;
static int        window_h     = 400;

/* Check if all 52 cards are in foundations */
BOOL EndGame_CheckWin(void)
{
    int i, total = 0;
    for (i = 0; i < 4; i++)
        total += g_Game.found_top[i];
    return total == 52;
}

void EndGame_Start(HWND hwnd)
{
    RECT rc;
    int i;

    if (g_endgame_active) return;

    GetClientRect(hwnd, &rc);
    window_w = rc.right;
    window_h = rc.bottom - STATUS_BAR_HEIGHT;

    g_endgame_active = TRUE;
    bounce_count     = 0;
    next_launch      = 0;
    last_launch      = GetTickCount();

    for (i = 0; i < NUM_BOUNCE_CARDS; i++)
        bounce_cards[i].active = FALSE;

    SetTimer(hwnd, ENDGAME_TIMER_ID, ENDGAME_TIMER_MS, NULL);
}

void EndGame_Stop(HWND hwnd)
{
    if (!g_endgame_active) return;
    g_endgame_active = FALSE;
    KillTimer(hwnd, ENDGAME_TIMER_ID);
    InvalidateRect(hwnd, NULL, TRUE);
}

static void launch_card(void)
{
    BounceCard *bc;
    int suit, face;

    if (next_launch >= NUM_BOUNCE_CARDS) return;

    bc = &bounce_cards[next_launch++];

    /* Pick a random card from the foundations to animate */
    suit = next_launch % 4;
    face = (next_launch - 1) / 4;
    bc->card = (CARD)((face * 4) + suit);

    /* Launch from a random x position along the top */
    bc->x  = (float)(10 + (rand() % (window_w - 91)));
    bc->y  = (float)(Y_MARGIN);

    /* Random horizontal velocity, always moving down */
    bc->vx = (float)((rand() % 9) - 4);   /* -4 to +4 */
    bc->vy = (float)((rand() % 4) + 2);    /* 2 to 5 */

    bc->active = TRUE;
    bounce_count++;
}

void EndGame_Tick(HWND hwnd)
{
    int i;
    DWORD now = GetTickCount();
    BOOL any_active = FALSE;

    /* Launch a new card every ~80ms until all 52 are out */
    if (next_launch < NUM_BOUNCE_CARDS && now - last_launch > 80) {
        launch_card();
        last_launch = now;
    }

    /* Update physics for each active card */
    for (i = 0; i < next_launch; i++) {
        BounceCard *bc = &bounce_cards[i];
        if (!bc->active) continue;

        bc->vy += GRAVITY;
        bc->x  += bc->vx;
        bc->y  += bc->vy;

        /* Bounce off bottom */
        if (bc->y + 96 >= window_h) {
            bc->y  = (float)(window_h - 96);
            bc->vy = -(bc->vy * BOUNCE_DAMPEN);
            /* Slow down horizontal too */
            bc->vx *= 0.95f;
            /* If barely moving stop it */
            if (bc->vy > -1.0f) bc->vy = 0.0f;
        }

        /* Bounce off left/right walls */
        if (bc->x < 0) {
            bc->x  = 0;
            bc->vx = -bc->vx * BOUNCE_DAMPEN;
        }
        if (bc->x + 71 > window_w) {
            bc->x  = (float)(window_w - 71);
            bc->vx = -bc->vx * BOUNCE_DAMPEN;
        }

        any_active = TRUE;
    }

    InvalidateRect(hwnd, NULL, TRUE);

    /* Once all cards are launched and settled, show the Play Again dialog */
    if (next_launch >= NUM_BOUNCE_CARDS && !any_active) {
        EndGame_Stop(hwnd);

        if (MessageBoxW(hwnd,
                L"You win!\n\nWould you like to play again?",
                L"Solitaire",
                MB_YESNO | MB_ICONQUESTION) == IDYES) {
            Game_Init();
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            PostQuitMessage(0);
        }
    }
}

void EndGame_Draw(HDC hdc, int width, int height)
{
    int i;
    if (!g_endgame_active) return;

    for (i = 0; i < next_launch; i++) {
        BounceCard *bc = &bounce_cards[i];
        if (!bc->active) continue;
        cdtDraw(hdc, (int)bc->x, (int)bc->y,
                bc->card & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }
}