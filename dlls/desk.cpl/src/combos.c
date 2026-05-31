/* desk.cpl — main page combo population. */

#include "desk_private.h"
#include "combos.h"
#include "display_state.h"


static const WCHAR *orientation_labels[] =
{
    L"Landscape",
    L"Portrait",
    L"Landscape (flipped)",
    L"Portrait (flipped)",
};

void populate_output_combo(HWND hwnd)
{
    HWND combo = GetDlgItem(hwnd, IDC_OUTPUT_COMBO);
    unsigned int i;
    int first_enabled = -1;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    sel_output = -1;

    for (i = 0; i < wlr_data.num_outputs; i++)
    {
        const struct wlr_output_info *out = &wlr_data.outputs[i];
        WCHAR buf[384], wname[64], wdesc[256];

        MultiByteToWideChar(CP_UTF8, 0, out->name, -1, wname, 64);

        if (out->description[0])
        {
            MultiByteToWideChar(CP_UTF8, 0, out->description, -1, wdesc, 256);
            swprintf(buf, ARRAY_SIZE(buf), L"%u. %s", i + 1, wdesc);
        }
        else
            swprintf(buf, ARRAY_SIZE(buf), L"%u. %s", i + 1, wname);

        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)buf);
        SendMessageW(combo, CB_SETITEMDATA, i, (LPARAM)i);

        if (first_enabled < 0 && out->enabled)
            first_enabled = (int)i;
    }

    if (first_enabled < 0 && wlr_data.num_outputs > 0)
        first_enabled = 0;

    if (first_enabled >= 0)
    {
        SendMessageW(combo, CB_SETCURSEL, first_enabled, 0);
        sel_output = first_enabled;
    }
}

void populate_resolution_combo(HWND hwnd)
{
    HWND combo = GetDlgItem(hwnd, IDC_RESOLUTION_COMBO);
    const struct wlr_output_info *out = get_output();
    unsigned int pairs[MAX_WLR_MODES][2];
    int          is_preferred[MAX_WLR_MODES];
    unsigned int unique = 0, i, j;
    int cur_sel = -1;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    staged_w = staged_h = 0;

    if (!out) return;

    /* Collect unique WxH, preserving wlr-randr order (largest first). */
    for (i = 0; i < out->num_modes; i++)
    {
        unsigned int w = out->modes[i].width, h = out->modes[i].height;
        BOOL dup = FALSE;

        for (j = 0; j < unique; j++)
            if (pairs[j][0] == w && pairs[j][1] == h) { dup = TRUE; break; }

        if (!dup && unique < MAX_WLR_MODES)
        {
            pairs[unique][0] = w;
            pairs[unique][1] = h;
            is_preferred[unique] = 0;
            unique++;
        }
    }

    /* Mark preferred. */
    for (i = 0; i < out->num_modes; i++)
    {
        if (!out->modes[i].preferred) continue;
        for (j = 0; j < unique; j++)
            if (pairs[j][0] == out->modes[i].width &&
                pairs[j][1] == out->modes[i].height)
                is_preferred[j] = 1;
    }

    /* Populate. */
    for (i = 0; i < unique; i++)
    {
        WCHAR buf[80];
        if (is_preferred[i])
            swprintf(buf, ARRAY_SIZE(buf),
                     L"%u \u00d7 %u (Recommended)", pairs[i][0], pairs[i][1]);
        else
            swprintf(buf, ARRAY_SIZE(buf),
                     L"%u \u00d7 %u", pairs[i][0], pairs[i][1]);

        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)buf);

        /* Is this the current resolution? */
        for (j = 0; j < out->num_modes; j++)
        {
            if (out->modes[j].current &&
                out->modes[j].width  == pairs[i][0] &&
                out->modes[j].height == pairs[i][1])
                cur_sel = (int)i;
        }
    }

    if (cur_sel < 0 && unique > 0) cur_sel = 0;
    if (cur_sel >= 0)
    {
        SendMessageW(combo, CB_SETCURSEL, cur_sel, 0);
        staged_w = pairs[cur_sel][0];
        staged_h = pairs[cur_sel][1];
    }
}

void populate_orientation_combo(HWND hwnd)
{
    HWND combo = GetDlgItem(hwnd, IDC_ORIENTATION_COMBO);
    const struct wlr_output_info *out = get_output();
    unsigned int i;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    staged_transform = WLR_TRANSFORM_NORMAL;

    for (i = 0; i < 4; i++)
    {
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)orientation_labels[i]);
        SendMessageW(combo, CB_SETITEMDATA, i, (LPARAM)i);
    }

    if (out) staged_transform = out->transform;
    SendMessageW(combo, CB_SETCURSEL, staged_transform, 0);
}

/* Read back the resolution from the combo (parses "W x H ..."). */
void read_resolution_combo(HWND hwnd)
{
    int idx = (int)SendDlgItemMessageW(hwnd, IDC_RESOLUTION_COMBO,
                                       CB_GETCURSEL, 0, 0);
    WCHAR buf[80];
    unsigned int w = 0, h = 0;

    if (idx < 0) return;
    SendDlgItemMessageW(hwnd, IDC_RESOLUTION_COMBO,
                        CB_GETLBTEXT, idx, (LPARAM)buf);

    /* Parse "1920 x 1080" — the separator is U+00D7 but we skip non-digits. */
    if (swscanf(buf, L"%u %*c %u", &w, &h) >= 2 ||
        swscanf(buf, L"%u%*c%u", &w, &h) >= 2)
    {
        staged_w = w;
        staged_h = h;
    }
}