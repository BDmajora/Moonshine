/* desk.cpl — monitor-layout preview view. */

#include "desk_private.h"
#include "preview.h"
#include "display_state.h"


static RECT compute_bounding(void)
{
    RECT b = {0};
    unsigned int i;

    for (i = 0; i < wlr_data.num_outputs; i++)
    {
        const struct wlr_output_info *o = &wlr_data.outputs[i];
        unsigned int w = 0, h = 0, m;
        RECT r;

        if (!o->enabled) continue;

        for (m = 0; m < o->num_modes; m++)
            if (o->modes[m].current) { w = o->modes[m].width; h = o->modes[m].height; break; }
        if (!w && o->num_modes) { w = o->modes[0].width; h = o->modes[0].height; }
        if (!w) { w = 1920; h = 1080; }

        SetRect(&r, o->pos_x, o->pos_y, o->pos_x + (int)w, o->pos_y + (int)h);
        if (i == 0) b = r;
        else        UnionRect(&b, &b, &r);
    }
    if (b.right <= b.left) { b.right = 1920; b.bottom = 1080; }
    return b;
}

static void draw_monitor(HDC hdc, const struct wlr_output_info *o,
                         RECT client, RECT virt, float scale, BOOL is_sel,
                         unsigned int index)
{
    unsigned int w = 0, h = 0, m;
    RECT src, dst;
    WCHAR label[32];

    for (m = 0; m < o->num_modes; m++)
        if (o->modes[m].current) { w = o->modes[m].width; h = o->modes[m].height; break; }
    if (!w && o->num_modes) { w = o->modes[0].width; h = o->modes[0].height; }
    if (!w) { w = 1920; h = 1080; }

    SetRect(&src, o->pos_x, o->pos_y, o->pos_x + (int)w, o->pos_y + (int)h);

    /* Map virtual -> client coordinates. */
    OffsetRect(&src, -(virt.left + virt.right) / 2, -(virt.top + virt.bottom) / 2);
    dst.left   = (LONG)(src.left   * scale) + (client.left + client.right)  / 2;
    dst.top    = (LONG)(src.top    * scale) + (client.top  + client.bottom) / 2;
    dst.right  = (LONG)(src.right  * scale) + (client.left + client.right)  / 2;
    dst.bottom = (LONG)(src.bottom * scale) + (client.top  + client.bottom) / 2;

    /* Fill. */
    SelectObject(hdc, GetStockObject(DC_BRUSH));
    SelectObject(hdc, GetStockObject(DC_PEN));
    SetDCBrushColor(hdc, GetSysColor(is_sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    SetDCPenColor(hdc, GetSysColor(COLOR_WINDOWFRAME));
    Rectangle(hdc, dst.left, dst.top, dst.right, dst.bottom);

    /* Number label (1-based like Windows). */
    swprintf(label, ARRAY_SIZE(label), L"%u", index + 1);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(is_sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    {
        HFONT big = CreateFontW(-28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH, L"Ms Shell Dlg");
        HFONT old = SelectObject(hdc, big);
        DrawTextW(hdc, label, -1, &dst, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
        SelectObject(hdc, old);
        DeleteObject(big);
    }
}

LRESULT CALLBACK desktop_view_proc(HWND hwnd, UINT msg,
                                   WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        RECT client, virt;
        float scale;
        unsigned int i;
        HDC hdc;

        GetClientRect(hwnd, &client);
        hdc = BeginPaint(hwnd, &ps);

        /* Dark-blue gradient background (simplified: solid). */
        {
            HBRUSH bg = CreateSolidBrush(RGB(58, 110, 165));
            FillRect(hdc, &client, bg);
            DeleteObject(bg);
        }

        virt  = compute_bounding();
        scale = min((float)(client.right  - client.left) / (float)(virt.right  - virt.left),
                    (float)(client.bottom - client.top)  / (float)(virt.bottom - virt.top));
        scale *= 0.80f;

        for (i = 0; i < wlr_data.num_outputs; i++)
        {
            if (!wlr_data.outputs[i].enabled) continue;
            draw_monitor(hdc, &wlr_data.outputs[i], client, virt, scale,
                         (int)i == sel_output, i);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    if (msg == WM_LBUTTONDOWN)
    {
        /* TODO: hit-test for multi-monitor selection. */
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void create_desktop_view(HWND hwnd)
{
    HWND parent = GetDlgItem(hwnd, IDC_VIRTUAL_DESKTOP);
    RECT rc;
    LONG m;

    GetClientRect(parent, &rc);
    rc.top += 4;
    m = (rc.bottom - rc.top) * 4 / 100;
    InflateRect(&rc, -m, -m);

    {
        HWND view = CreateWindowW(L"DeskCplDesktop", NULL, WS_CHILD,
                                  rc.left, rc.top,
                                  rc.right - rc.left, rc.bottom - rc.top,
                                  parent, NULL, NULL, module);
        SetWindowLongPtrW(view, GWLP_USERDATA, (UINT_PTR)hwnd);
        ShowWindow(view, SW_SHOW);
    }
}