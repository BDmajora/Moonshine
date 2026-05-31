/* desk.cpl — Identify overlay (flashes a number on each display). */

#include "desk_private.h"
#include "identify.h"
#include "display_state.h"


/* Near-black field that the layered color key turns transparent, so the
 * desktop shows through everywhere except the badge.  Where the Wayland
 * driver can't color-key, the window just stays a near-black field. */
#define IDENTIFY_KEY RGB(8, 8, 8)

LRESULT CALLBACK identify_proc(HWND hwnd, UINT msg,
                               WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hwnd, 1, 5000, NULL);  /* auto-dismiss after 5 seconds */
        return 0;

    case WM_TIMER:
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        RECT rc, badge;
        HDC hdc = BeginPaint(hwnd, &ps);
        WCHAR label[16];
        HFONT big, old;
        HBRUSH key;
        int side;

        GetClientRect(hwnd, &rc);

        /* Transparent (color-keyed) field over the whole output. */
        key = CreateSolidBrush(IDENTIFY_KEY);
        FillRect(hdc, &rc, key);
        DeleteObject(key);

        /* Opaque badge anchored to the upper-left corner (Windows 10 default). */
        side = (rc.right < rc.bottom ? rc.right : rc.bottom) / 4;
        SetRect(&badge, side / 4, side / 4, side / 4 + side, side / 4 + side);
        FillRect(hdc, &badge, (HBRUSH)GetStockObject(BLACK_BRUSH));

        swprintf(label, ARRAY_SIZE(label), L"%Iu",
                 (UINT_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        big = CreateFontW(-side / 2, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, DEFAULT_PITCH, L"Ms Shell Dlg");
        old = SelectObject(hdc, big);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, label, -1, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
        SelectObject(hdc, old);
        DeleteObject(big);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void show_identify(void)
{
    unsigned int i, m;

    for (i = 0; i < wlr_data.num_outputs; i++)
    {
        const struct wlr_output_info *o = &wlr_data.outputs[i];
        unsigned int w = 0, h = 0;
        HWND win;

        if (!o->enabled) continue;

        for (m = 0; m < o->num_modes; m++)
            if (o->modes[m].current) { w = o->modes[m].width; h = o->modes[m].height; break; }
        if (!w && o->num_modes) { w = o->modes[0].width; h = o->modes[0].height; }
        if (!w) { w = 1920; h = 1080; }

        /* Cover the whole output — the compositor positions surfaces, so a
         * smaller popup would be re-centred; the badge is drawn in a corner. */
        win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                              L"DeskCplIdentify", NULL, WS_POPUP,
                              o->pos_x, o->pos_y, (int)w, (int)h,
                              NULL, NULL, module, NULL);
        if (!win) continue;
        SetLayeredWindowAttributes(win, IDENTIFY_KEY, 0, LWA_COLORKEY);
        SetWindowLongPtrW(win, GWLP_USERDATA, (LONG_PTR)(i + 1));  /* 1-based */
        ShowWindow(win, SW_SHOWNA);
        UpdateWindow(win);
    }
}