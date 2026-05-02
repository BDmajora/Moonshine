#include "sol_endgame.h"

BOOL g_endgame_active = FALSE;

static BounceCard bounce_cards[NUM_BOUNCE_CARDS];
static int        next_launch  = 0;
static DWORD      last_launch  = 0;
static int        window_w     = 600;
static int        window_h     = 400;
static int        bonus_score  = 0;
static HWND       g_hwnd       = NULL;

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

    g_hwnd = hwnd;
    GetClientRect(hwnd, &rc);
    window_w = rc.right;
    window_h = rc.bottom - STATUS_BAR_HEIGHT;

    g_endgame_active = TRUE;
    next_launch      = 0;
    last_launch      = GetTickCount();
    bonus_score      = 0;

    for (i = 0; i < NUM_BOUNCE_CARDS; i++)
        bounce_cards[i].active = FALSE;

    /* Clear the board immediately - fill with green */
    InvalidateRect(hwnd, NULL, TRUE);

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

    suit = (next_launch - 1) % 4;
    face = (next_launch - 1) / 4;
    bc->card   = (CARD)((face * 4) + suit);
    bc->x      = (float)(10 + (rand() % (window_w - 91)));
    bc->y      = (float)(Y_MARGIN);
    bc->vx     = (float)((rand() % 9) - 4);
    bc->vy     = (float)((rand() % 4) + 2);
    bc->active = TRUE;
}

void EndGame_Dismiss(HWND hwnd)
{
    if (!g_endgame_active) return;
    EndGame_Stop(hwnd);
    if (MessageBoxW(hwnd,
            L"Deal again?",
            L"Solitaire",
            MB_YESNO | MB_ICONQUESTION) == IDYES) {
        Game_Init();
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        PostQuitMessage(0);
    }
}

void EndGame_Tick(HWND hwnd)
{
    int i;
    DWORD now = GetTickCount();
    BOOL any_moving = FALSE;

    if (next_launch < NUM_BOUNCE_CARDS && now - last_launch > 80) {
        launch_card();
        last_launch = now;
        bonus_score += 5;
    }

    for (i = 0; i < next_launch; i++) {
        BounceCard *bc = &bounce_cards[i];
        if (!bc->active) continue;

        bc->vy += GRAVITY;
        bc->x  += bc->vx;
        bc->y  += bc->vy;

        if (bc->y + 96 >= window_h) {
            bc->y  = (float)(window_h - 96);
            bc->vy = -(bc->vy * BOUNCE_DAMPEN);
            bc->vx *= 0.95f;
            if (bc->vy > -1.5f) bc->vy = 0.0f;
        }

        if (bc->x < 0) {
            bc->x  = 0;
            bc->vx = -bc->vx * BOUNCE_DAMPEN;
        }
        if (bc->x + 71 > window_w) {
            bc->x  = (float)(window_w - 71);
            bc->vx = -bc->vx * BOUNCE_DAMPEN;
        }

        if (bc->vy != 0.0f || fabsf(bc->vx) > 0.5f)
            any_moving = TRUE;
    }

    /* Don't erase — leave trails like XP */
    InvalidateRect(hwnd, NULL, FALSE);

    if (next_launch >= NUM_BOUNCE_CARDS && !any_moving) {
        EndGame_Dismiss(hwnd);
    }
}

void EndGame_Draw(HDC hdc, int width, int height)
{
    int i;
    HFONT hfont, hOldFont;
    RECT rcStatus, rcText;
    WCHAR buf[64];

    if (!g_endgame_active) return;

    /* Draw bouncing cards */
    for (i = 0; i < next_launch; i++) {
        BounceCard *bc = &bounce_cards[i];
        if (!bc->active) continue;
        cdtDraw(hdc, (int)bc->x, (int)bc->y,
                bc->card & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* Redraw status bar on top so it stays readable */
    rcStatus.left   = 0;
    rcStatus.top    = height - STATUS_BAR_HEIGHT;
    rcStatus.right  = width;
    rcStatus.bottom = height;

    {
        HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rcStatus, hbr);
        DeleteObject(hbr);
    }
    {
        HPEN hpen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
        HPEN hOld = (HPEN)SelectObject(hdc, hpen);
        MoveToEx(hdc, 0, height - STATUS_BAR_HEIGHT, NULL);
        LineTo(hdc, width, height - STATUS_BAR_HEIGHT);
        SelectObject(hdc, hOld);
        DeleteObject(hpen);
    }

    hfont = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"MS Sans Serif");
    hOldFont = (HFONT)SelectObject(hdc, hfont);
    SetBkMode(hdc, TRANSPARENT);

    /* Left: Bonus and instruction */
    wsprintfW(buf, L"Bonus: %d  Press Esc or a mouse button to stop...", bonus_score);
    rcText = rcStatus;
    rcText.left += STATUS_MARGIN;
    DrawTextW(hdc, buf, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    /* Right: Score and Time */
    wsprintfW(buf, L"Score: %d  Time: %d",
              g_Game.score + bonus_score,
              (int)((GetTickCount() - g_Game.start_tick) / 1000));
    rcText = rcStatus;
    rcText.right -= STATUS_MARGIN;
    DrawTextW(hdc, buf, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    SelectObject(hdc, hOldFont);
    DeleteObject(hfont);
}