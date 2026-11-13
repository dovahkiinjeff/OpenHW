/* Draw and input share these bounds. Keep navigation in its artwork overlay,
   rather than stretching retail FIB coordinates over the whole display. */
static rectangle muiFrontendPanel(bool32 right)
{
    sdword width = MIN2(MAIN_WindowWidth * 44 / 100,
        MAX2(MAIN_WindowWidth * 32 / 100, uiScaleSize(370)));
    return muiRect(right ? MAIN_WindowWidth - width : 0, 0,
                   right ? MAIN_WindowWidth : width, MAIN_WindowHeight);
}

static rectangle muiFrontendItem(bool32 right, sdword index, sdword count)
{
    rectangle panel = muiFrontendPanel(right);
    sdword pad = MIN2(uiScaleSize(28), (panel.x1 - panel.x0) / 12);
    sdword top = MIN2(uiScaleSize(166), MAIN_WindowHeight / 4);
    sdword gap = MAX2(4, uiScaleSize(8));
    sdword height = MIN2(uiScaleSize(62),
        (MAIN_WindowHeight - top - uiScaleSize(80) - (count - 1) * gap) / count);
    return muiRect(panel.x0 + pad, top + index * (height + gap),
        panel.x1 - pad, top + index * (height + gap) + height);
}

typedef struct MuiStoryLayout {
    rectangle panel, preview, races[2], colors, thrust, start, load, back;
} MuiStoryLayout;

static void muiBuildStoryLayout(MuiStoryLayout *out)
{
    sdword w = MIN2(MAIN_WindowWidth - uiScaleSize(48), uiScaleSize(1000));
    sdword h = MIN2(MAIN_WindowHeight - uiScaleSize(48), uiScaleSize(614));
    sdword x = (MAIN_WindowWidth - w) / 2, y = (MAIN_WindowHeight - h) / 2;
    sdword p = MIN2(uiScaleSize(24), w / 32), gap = uiScaleSize(12);
    sdword foot = y + h - uiScaleSize(64), left = x + p;
    sdword side = MIN2(w / 3, uiScaleSize(280));
    sdword right = x + w - p, split = left + side;
    out->panel = muiRect(x, y, x + w, y + h);
    out->races[0] = muiRect(left, y + uiScaleSize(130), split, y + uiScaleSize(174));
    out->races[1] = muiRect(left, y + uiScaleSize(184), split, y + uiScaleSize(228));
    out->colors = muiRect(left, y + uiScaleSize(252), split, y + uiScaleSize(296));
    out->load = muiRect(left, y + uiScaleSize(314), split, y + uiScaleSize(358));
    out->preview = muiRect(split + uiScaleSize(22), y + uiScaleSize(84), right, foot - gap);
    out->thrust = muiRect(out->preview.x0 + gap, out->preview.y1 - uiScaleSize(40),
                          out->preview.x1 - gap, out->preview.y1 - uiScaleSize(8));
    out->back = muiRect(left, foot, left + uiScaleSize(126), foot + uiScaleSize(40));
    out->start = muiRect(right - uiScaleSize(200), foot, right, foot + uiScaleSize(40));
}

/* A compact palette beside a generous model viewport. This is deliberately
   separate from the three-column settings layout. */
static void muiBuildFleetColorLayout(ModernUILayout *out)
{
    sdword pad = uiScaleSize(20), gap = uiScaleSize(14), i;
    sdword rail = MIN2(uiScaleSize(190), MAIN_WindowWidth / 6);
    sdword chart = MIN2(uiScaleSize(360), MAIN_WindowWidth * 30 / 100);
    sdword bottom = MAIN_WindowHeight - pad;
    memset(out, 0, sizeof(*out));
    out->canvas = muiRect(0, 0, MAIN_WindowWidth, MAIN_WindowHeight);
    out->rail = muiRect(pad, pad, pad + rail, bottom);
    out->content = muiRect(out->rail.x1 + gap, pad,
        out->rail.x1 + gap + chart, bottom);
    out->preview = muiRect(out->content.x1 + gap, pad, MAIN_WindowWidth - pad, bottom);
    for (i = 0; i < MUI_COLOR_TAB_COUNT; ++i)
        out->tabs[i] = muiRect(out->rail.x0 + uiScaleSize(8), pad + uiScaleSize(112 + i * 46),
            out->rail.x1 - uiScaleSize(8), pad + uiScaleSize(150 + i * 46));
    out->apply = muiRect(out->content.x1 - uiScaleSize(116), bottom - uiScaleSize(56),
                         out->content.x1 - uiScaleSize(14), bottom - uiScaleSize(18));
    out->cancel = muiRect(out->content.x0 + uiScaleSize(14), out->apply.y0,
                          out->apply.x0 - uiScaleSize(10), out->apply.y1);
}
