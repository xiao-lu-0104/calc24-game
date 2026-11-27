/*==========================================================================
 *  绘图基础：绘图原语、切角卡片、图标、背景图、页脚、扑克牌。
 *========================================================================*/

#include "calc24.h"

/*====================== 十、界面基础绘制 ======================*/

/* 颜色在第二节定义：P_ 开头为浅色卡面主题 */

void hline(HDC dc, int x, int y, int w, COLORREF c, int t)
{
    fillRect(dc, x, y, w, t, c);
}

void vline(HDC dc, int x, int y, int h, COLORREF c, int t)
{
    fillRect(dc, x, y, t, h, c);
}

static void cutPts(POINT p[8], int x, int y, int w, int h, int cut)
{
    p[0].x = x + cut;       p[0].y = y;
    p[1].x = x + w - cut;   p[1].y = y;
    p[2].x = x + w;         p[2].y = y + cut;
    p[3].x = x + w;         p[3].y = y + h - cut;
    p[4].x = x + w - cut;   p[4].y = y + h;
    p[5].x = x + cut;       p[5].y = y + h;
    p[6].x = x;             p[6].y = y + h - cut;
    p[7].x = x;             p[7].y = y + cut;
}

void polySolid(HDC dc, const POINT *p, int n, COLORREF c)
{
    HBRUSH br = CreateSolidBrush(c);
    HBRUSH ob = (HBRUSH)SelectObject(dc, br);
    HPEN   op = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, (POINT *)p, n);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
}

static void polyOutline(HDC dc, const POINT *p, int n, COLORREF c, int t)
{
    HPEN pn = CreatePen(PS_SOLID, t, c);
    HPEN op = (HPEN)SelectObject(dc, pn);
    Polyline(dc, p, n);
    SelectObject(dc, op);
    DeleteObject(pn);
}

void cutFill(HDC dc, int x, int y, int w, int h, int cut, COLORREF c)
{
    POINT p[8];
    cutPts(p, x, y, w, h, cut);
    polySolid(dc, p, 8, c);
}

void cutBorder(HDC dc, int x, int y, int w, int h, int cut,
                      COLORREF c, int t)
{
    POINT p[9];
    cutPts(p, x, y, w, h, cut);
    p[8] = p[0];
    polyOutline(dc, p, 9, c, t);
}

/* 有投影的浅色卡片 */
void paperCard(HDC dc, int x, int y, int w, int h, int cut,
                      COLORREF fill, COLORREF border, int t, bool shadow)
{
    if (shadow) cutFill(dc, x + 6, y + 7, w, h, cut, RGB(8, 10, 14));
    cutFill(dc, x, y, w, h, cut, fill);
    if (t > 0) cutBorder(dc, x, y, w, h, cut, border, t);
}



void diamond(HDC dc, int cx, int cy, int r, COLORREF c, bool solid)
{
    POINT p[4];
    p[0].x = cx;     p[0].y = cy - r;
    p[1].x = cx + r; p[1].y = cy;
    p[2].x = cx;     p[2].y = cy + r;
    p[3].x = cx - r; p[3].y = cy;
    if (solid) polySolid(dc, p, 4, c);
    else {
        POINT q[5];
        for (int i = 0; i < 4; ++i) q[i] = p[i];
        q[4] = p[0];
        polyOutline(dc, q, 5, c, 2);
    }
}

void barcode(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    static const int bars[15] = { 2, 4, 1, 3, 1, 5, 2, 1, 4, 2, 3, 1, 1, 4, 2 };
    int cx = x, i = 0;

    while (cx < x + w) {
        int bw = bars[i % 15];
        if (cx + bw > x + w) bw = x + w - cx;
        fillRect(dc, cx, y, bw, h, c);
        cx += bw + 2;
        i++;
    }
}

/* 用折线逼近圆弧，避免 GDI Arc 的方向差异 */
void arcPoly(HDC dc, int cx, int cy, int r, double a0, double a1,
                    COLORREF c, int t)
{
    POINT p[26];
    int   n = 0;
    int   steps = 24;

    if (n > 25) n = 25;
    for (int i = 0; i <= steps && n < 26; ++i) {
        double a = (a0 + (a1 - a0) * i / steps) * 3.14159265358979 / 180.0;
        p[n].x = cx + (int)(cos(a) * r + 0.5);
        p[n].y = cy - (int)(sin(a) * r + 0.5);
        n++;
    }
    polyOutline(dc, p, n, c, t);
}

/* 线性图标：线宽统一 2 */
void drawIcon(HDC dc, int kind, int cx, int cy, int s, COLORREF c)
{
    int h = s / 2;
    HPEN pn = CreatePen(PS_SOLID, 2, c);
    HPEN op = (HPEN)SelectObject(dc, pn);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));

    switch (kind) {
    case 0:                                     /* 齿轮 */
        Ellipse(dc, cx - h * 55 / 100, cy - h * 55 / 100,
                cx + h * 55 / 100, cy + h * 55 / 100);
        Ellipse(dc, cx - h * 18 / 100, cy - h * 18 / 100,
                cx + h * 18 / 100, cy + h * 18 / 100);
        for (int k = 0; k < 8; ++k) {
            double a = k * 3.14159265 / 4.0;
            MoveToEx(dc, cx + (int)(cos(a) * h * 0.66), cy + (int)(sin(a) * h * 0.66), NULL);
            LineTo(dc, cx + (int)(cos(a) * h), cy + (int)(sin(a) * h));
        }
        break;

    case 1: {                                   /* 警告三角 */
        POINT p[4];
        p[0].x = cx;          p[0].y = cy - h;
        p[1].x = cx + h;      p[1].y = cy + h * 80 / 100;
        p[2].x = cx - h;      p[2].y = cy + h * 80 / 100;
        p[3] = p[0];
        Polyline(dc, p, 4);
        MoveToEx(dc, cx, cy - h * 35 / 100, NULL);
        LineTo(dc, cx, cy + h * 25 / 100);
        fillRect(dc, cx - 1, cy + h * 50 / 100, 3, 4, c);
        break;
    }

    case 2:                                     /* 信封 */
        Rectangle(dc, cx - h, cy - h * 65 / 100, cx + h, cy + h * 65 / 100);
        MoveToEx(dc, cx - h, cy - h * 65 / 100, NULL);
        LineTo(dc, cx, cy + h * 10 / 100);
        LineTo(dc, cx + h, cy - h * 65 / 100);
        break;

    case 3:                                     /* 日历 */
        Rectangle(dc, cx - h, cy - h * 80 / 100, cx + h, cy + h * 80 / 100);
        MoveToEx(dc, cx - h, cy - h * 35 / 100, NULL);
        LineTo(dc, cx + h, cy - h * 35 / 100);
        MoveToEx(dc, cx - h * 5 / 10, cy - h, NULL);
        LineTo(dc, cx - h * 5 / 10, cy - h * 6 / 10);
        MoveToEx(dc, cx + h * 5 / 10, cy - h, NULL);
        LineTo(dc, cx + h * 5 / 10, cy - h * 6 / 10);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                fillRect(dc, cx - h * 55 / 100 + i * h * 6 / 10,
                         cy - h * 5 / 100 + j * h * 4 / 10, 4, 4, c);
        break;

    case 4:                                     /* 奖杯 */
        Rectangle(dc, cx - h * 45 / 100, cy - h * 80 / 100,
                  cx + h * 45 / 100, cy + h * 25 / 100);
        arcPoly(dc, cx - h * 62 / 100, cy - h * 32 / 100, h * 34 / 100, 90, 270, c, 2);
        arcPoly(dc, cx + h * 62 / 100, cy - h * 32 / 100, h * 34 / 100, 270, 450, c, 2);
        MoveToEx(dc, cx, cy + h * 25 / 100, NULL);
        LineTo(dc, cx, cy + h * 60 / 100);
        MoveToEx(dc, cx - h * 45 / 100, cy + h * 80 / 100, NULL);
        LineTo(dc, cx + h * 45 / 100, cy + h * 80 / 100);
        break;

    case 5:                                     /* 信息 */
        Ellipse(dc, cx - h, cy - h, cx + h, cy + h);
        MoveToEx(dc, cx, cy - h * 25 / 100, NULL);
        LineTo(dc, cx, cy + h * 50 / 100);
        fillRect(dc, cx - 1, cy - h * 60 / 100, 3, 4, c);
        break;

    case 6:                                     /* 扬声器 */
    case 7: {                                   /* 静音 */
        POINT p[6];
        p[0].x = cx - h;       p[0].y = cy - h * 30 / 100;
        p[1].x = cx - h * 45 / 100; p[1].y = cy - h * 30 / 100;
        p[2].x = cx + h * 10 / 100; p[2].y = cy - h * 80 / 100;
        p[3].x = cx + h * 10 / 100; p[3].y = cy + h * 80 / 100;
        p[4].x = cx - h * 45 / 100; p[4].y = cy + h * 30 / 100;
        p[5].x = cx - h;       p[5].y = cy + h * 30 / 100;
        polySolid(dc, p, 6, c);
        arcPoly(dc, cx + h * 10 / 100, cy, h * 35 / 100, 60, -60, c, 2);
        if (kind == 7) {
            HPEN xp = CreatePen(PS_SOLID, 3, RGB(220, 80, 90));
            HPEN xo = (HPEN)SelectObject(dc, xp);
            MoveToEx(dc, cx - h * 80 / 100, cy - h * 80 / 100, NULL);
            LineTo(dc, cx + h * 80 / 100, cy + h * 80 / 100);
            SelectObject(dc, xo);
            DeleteObject(xp);
        }
        break;
    }

    case 8:                                     /* 电源 */
        arcPoly(dc, cx, cy, h * 70 / 100, 120, 420, c, 2);
        MoveToEx(dc, cx, cy - h, NULL);
        LineTo(dc, cx, cy);
        break;

    case 9:                                     /* 箭头 */
        MoveToEx(dc, cx - h * 60 / 100, cy, NULL);
        LineTo(dc, cx + h * 50 / 100, cy);
        MoveToEx(dc, cx + h * 10 / 100, cy - h * 45 / 100, NULL);
        LineTo(dc, cx + h * 55 / 100, cy);
        LineTo(dc, cx + h * 10 / 100, cy + h * 45 / 100);
        break;

    case 10:                                    /* 返回 */
        MoveToEx(dc, cx + h * 40 / 100, cy - h * 70 / 100, NULL);
        LineTo(dc, cx - h * 40 / 100, cy);
        LineTo(dc, cx + h * 40 / 100, cy + h * 70 / 100);
        break;
    }

    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pn);
}

/* 粉色 “24” 放射徽记 */
void emblem(HDC dc, int cx, int cy, int r, COLORREF col, COLORREF dark)
{
    HPEN pn = CreatePen(PS_SOLID, (r >= 40) ? 3 : 2, col);
    HPEN op = (HPEN)SelectObject(dc, pn);

    for (int k = 0; k < 24; ++k) {
        double a  = k * 3.14159265358979 / 12.0;
        double r2 = (k % 2 == 0) ? r : r * 0.72;
        MoveToEx(dc, cx + (int)(cos(a) * r * 0.45), cy + (int)(sin(a) * r * 0.45), NULL);
        LineTo(dc, cx + (int)(cos(a) * r2), cy + (int)(sin(a) * r2));
    }
    SelectObject(dc, op);
    DeleteObject(pn);

    HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH ob = (HBRUSH)SelectObject(dc, br);
    HPEN   np = CreatePen(PS_SOLID, 2, col);
    HPEN   opn = (HPEN)SelectObject(dc, np);
    Ellipse(dc, cx - r * 52 / 100, cy - r * 52 / 100,
            cx + r * 52 / 100, cy + r * 52 / 100);
    SelectObject(dc, ob);
    SelectObject(dc, opn);
    DeleteObject(br);
    DeleteObject(np);

    if (r >= 20) {
        HFONT f = (r >= 40) ? F_H3 : F_TINY;
        tx(dc, "24", cx - r, cy - r * 40 / 100, r * 2, r * 80 / 100,
           f, dark, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}


/*====================== 十一、背景与页脚 ======================*/

static COLORREF mixColor(COLORREF a, COLORREF b, double t)
{
    int r  = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t + 0.5);
    int g  = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t + 0.5);
    int bl = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t + 0.5);
    return RGB(r, g, bl);
}

static void vGradient(HDC dc, int x, int y, int w, int h,
                      COLORREF top, COLORREF bottom)
{
    if (w <= 0 || h <= 0) return;

    double span = (h > 1) ? (double)(h - 1) : 1.0;
    for (int i = 0; i < h; ++i)
        fillRect(dc, x, y + i, w, 1, mixColor(top, bottom, i / span));
}

static void hGradient(HDC dc, int x, int y, int w, int h,
                      COLORREF left, COLORREF right)
{
    if (w <= 0 || h <= 0) return;

    double span = (w > 1) ? (double)(w - 1) : 1.0;
    for (int i = 0; i < w; ++i)
        fillRect(dc, x + i, y, 1, h, mixColor(left, right, i / span));
}

/* 内嵌背景图（图片\背景.bmp，已裁掉原图水印），资源 ID 与 .rc 里的一致 */
static const int BG_RESID     = 201;
static const int BG_ALPHA     = 205;    /* 背景图不透明度 0~255，越小越淡     */

static HBITMAP g_bgImage      = NULL;   /* 内嵌的写意山水背景图               */
static bool    g_bgImageTried = false;

static HBITMAP backgroundImage()
{
    if (!g_bgImageTried) {
        g_bgImageTried = true;
        g_bgImage = (HBITMAP)LoadImageA(GetModuleHandleA(NULL),
                                        MAKEINTRESOURCEA(BG_RESID),
                                        IMAGE_BITMAP, 0, 0,
                                        LR_CREATEDIBSECTION);
    }
    return g_bgImage;
}

/*
 * 背景图等比铺满 (x,y,w,h)：先按目标比例从中间裁掉源图多余的一条，
 * 再半透明叠到渐变底上，既透出画面，又不压过卡片和文字。
 * 背景图缺失（比如没编资源）时什么都不画，只留渐变底。
 */
static void paintBackdropImage(HDC dc, int x, int y, int w, int h)
{
    HBITMAP bmp = backgroundImage();
    if (bmp == NULL) return;

    BITMAP bm;
    if (GetObjectA(bmp, sizeof(bm), &bm) == 0) return;

    int sw = bm.bmWidth, sh = bm.bmHeight;
    if (sw <= 0 || sh <= 0 || w <= 0 || h <= 0) return;

    int srcW = sw, srcH = sh;
    if ((long long)sw * h > (long long)w * sh)
        srcW = (int)((long long)sh * w / h);        /* 源图偏宽：裁左右 */
    else
        srcH = (int)((long long)sw * h / w);        /* 源图偏高：裁上下 */
    if (srcW < 1) srcW = 1;
    if (srcH < 1) srcH = 1;

    HDC mem = CreateCompatibleDC(dc);
    if (mem == NULL) return;
    HGDIOBJ old = SelectObject(mem, bmp);

    BLENDFUNCTION bf;
    bf.BlendOp             = AC_SRC_OVER;
    bf.BlendFlags          = 0;
    bf.SourceConstantAlpha = (BYTE)BG_ALPHA;
    bf.AlphaFormat         = 0;
    AlphaBlend(dc, x, y, w, h, mem, (sw - srcW) / 2, (sh - srcH) / 2,
               srcW, srcH, bf);

    SelectObject(mem, old);
    DeleteDC(mem);
}

/* 1280x800 的版面：淡粉渐变打底，叠一张半透明山水背景图，上下压黑带 */
static void paintBackground(HDC dc)
{
    const int TOP_H  = 130;
    const int BOT_Y  = 748;
    const int BODY_H = BOT_Y - TOP_H;

    /* 1. 底色：上淡粉、下纯白 */
    vGradient(dc, 0, 0, DES_W, DES_H, RGB(255, 240, 246), P_WHITE);

    /* 2. 中部横向渐变：中间最亮，向两侧轻轻加深 */
    hGradient(dc, 0, TOP_H, DES_W / 2, BODY_H, RGB(255, 243, 248), P_WHITE);
    hGradient(dc, DES_W / 2, TOP_H, DES_W - DES_W / 2, BODY_H,
              P_WHITE, RGB(255, 243, 248));

    /* 3. 写意山水背景图：等比铺满正文区，半透明叠在渐变底上 */
    paintBackdropImage(dc, 0, TOP_H, DES_W, BODY_H);

    /* 4. 上下黑带 */
    vGradient(dc, 0, 0, DES_W, TOP_H, RGB(5, 7, 10), RGB(46, 54, 64));
    vGradient(dc, 0, BOT_Y, DES_W, DES_H - BOT_Y, RGB(46, 54, 64), RGB(5, 7, 10));

    /* 5. 黑带与内容区的交界线 */
    hline(dc, 0, TOP_H - 2, DES_W, RGB(158, 62, 96), 2);
    hline(dc, 0, BOT_Y, DES_W, RGB(158, 62, 96), 2);
}

/* 背景只在首次绘制时渲染一遍，之后直接贴图 */
static HBITMAP g_bgBitmap    = NULL;
static HDC     g_bgDC        = NULL;
static HBITMAP g_bgOldBitmap = NULL;

void freeBackground()
{
    if (g_bgDC != NULL) {
        if (g_bgOldBitmap != NULL) SelectObject(g_bgDC, g_bgOldBitmap);
        DeleteDC(g_bgDC);
        g_bgDC        = NULL;
        g_bgOldBitmap = NULL;
    }
    if (g_bgBitmap != NULL) {
        DeleteObject(g_bgBitmap);
        g_bgBitmap = NULL;
    }
    if (g_bgImage != NULL) {
        DeleteObject(g_bgImage);
        g_bgImage      = NULL;
        g_bgImageTried = false;
    }
}

static void buildBackground()
{
    HDC screen = GetDC(NULL);
    if (screen == NULL) return;

    int bw = sc(DES_W), bh = sc(DES_H);
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;

    g_bgDC     = CreateCompatibleDC(screen);
    g_bgBitmap = CreateCompatibleBitmap(screen, bw, bh);
    ReleaseDC(NULL, screen);

    if (g_bgDC == NULL || g_bgBitmap == NULL) {
        freeBackground();
        return;
    }

    g_bgOldBitmap = (HBITMAP)SelectObject(g_bgDC, g_bgBitmap);

    /* 背景按实际像素缓存，paintBackground 里照旧写 1280x800 逻辑坐标 */
    SetMapMode(g_bgDC, MM_ANISOTROPIC);
    SetWindowExtEx(g_bgDC, DES_W, DES_H, NULL);
    SetViewportExtEx(g_bgDC, bw, bh, NULL);
    SetWindowOrgEx(g_bgDC, 0, 0, NULL);
    SetViewportOrgEx(g_bgDC, 0, 0, NULL);

    paintBackground(g_bgDC);
}

void drawBackdrop(HDC dc)
{
    if (g_bgDC == NULL) buildBackground();

    if (g_bgDC == NULL) {
        paintBackground(dc);
        return;
    }
    BitBlt(dc, 0, 0, DES_W, DES_H, g_bgDC, 0, 0, SRCCOPY);
}

void drawFooter(HDC dc, const char *left, const char *right)
{
    hline(dc, 28, 766, 1224, RGB(54, 62, 72), 1);
    tx(dc, left, 28, 772, 1000, 22, F_SMALL, RGB(130, 138, 148),
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (right != NULL && right[0] != '\0')
        tx(dc, right, 1052, 772, 200, 22, F_SMALL, RGB(120, 128, 138),
           DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

int bestScore()
{
    int b = 0;
    for (int i = 0; i < g.recCount; ++i)
        if (g.recs[i].score > b) b = g.recs[i].score;
    return b;
}

int playerLevel()
{
    int lv = 1 + bestScore() / 150;
    if (lv > 99) lv = 99;
    return lv;
}

void drawPlayingCard(HDC dc, int x, int y, int w, int h,
                            int value, int suit, bool faceDown, int idx)
{
    cutFill(dc, x + 6, y + 8, w, h, 8, RGB(9, 12, 16));

    if (faceDown) {
        cutFill(dc, x, y, w, h, 8, RGB(27, 37, 48));
        cutBorder(dc, x, y, w, h, 8, RGB(90, 100, 112), 1);
        emblem(dc, x + w / 2, y + h / 2 - 8, 34, P_PINK, P_PINK_D);
        tx(dc, "? ?", x, y + h - 56, w, 24, F_H2, RGB(150, 160, 172),
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    cutFill(dc, x, y, w, h, 8, RGB(248, 246, 241));
    cutBorder(dc, x, y, w, h, 8, P_INK, 2);
    fillRect(dc, x, y, 5, h, P_PINK);

    COLORREF ink = suitColor(suit);
    txChar(dc, suitChar(suit), x + 16, y + 6, 40, 36, F_SUIT_S, ink,
           DT_LEFT | DT_TOP);
    txChar(dc, suitChar(suit), x + w - 56, y + h - 76, 40, 36, F_SUIT_S, ink,
           DT_RIGHT | DT_TOP);

    char val[8];
    snprintf(val, sizeof(val), "%d", value);
    tx(dc, val, x, y + 42, w, 100, F_CARD_NUM, ink,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fillRect(dc, x, y + h - 24, w, 24, P_INK);
    char tag[16];
    snprintf(tag, sizeof(tag), "CARD %02d", idx + 1);
    tx(dc, tag, x + 14, y + h - 24, w - 20, 24, F_CAP, RGB(238, 238, 238),
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (idx == 0) {
        POINT p[3] = { { x + w - 26, y }, { x + w, y }, { x + w, y + 26 } };
        polySolid(dc, p, 3, P_PINK);
    }
}