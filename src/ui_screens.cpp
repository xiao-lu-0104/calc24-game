/*==========================================================================
 *  界面：按钮描述表与绘制、主菜单、难度 / 规则 / 排行榜、
 *  对局 / 姓名 / 结算、总绘制入口。
 *========================================================================*/

#include "calc24.h"

/*====================== 十二、按钮描述表 ======================*/

/* style: 0=纸面按钮 1=黑底主按钮 2=纸面文字按钮 3=难度卡(自绘)
          4=复选框 5=深色文字按钮 7=自绘卡片 8=图标按钮 9=返回图标 */

static const BtnDesc BTN_MENU[] = {
    { 700, 128, 548, 192, ID_M_START,  7, "" },   /* 主视觉：开始对局（并入原右上角横幅区） */
    { 700, 340, 270, 152, ID_M_DIFF,   7, "" },   /* 难度设置           */
    { 978, 340, 270, 152, ID_M_NAME,   7, "" },   /* 玩家档案           */
    { 700, 508, 178,  92, ID_M_RANK,   7, "" },   /* 排行榜             */
    { 884, 508, 178,  92, ID_M_RULES,  7, "" },   /* 游戏规则           */
    { 1068, 508, 178, 92, ID_M_EXIT,   7, "" },   /* 退出               */
    { 978, 616, 270, 112, ID_M_ABOUT,  7, "" },   /* 关于               */
    { 336, 674, 100,  82, ID_M_SFX,    7, "" },   /* 音效开关           */
    { 28, 26, 40, 40, ID_I_SFX,   8, "" },
    { 78, 26, 40, 40, ID_I_RULES, 8, "" },
    { 128, 26, 40, 40, ID_I_RANK, 8, "" },
    { 178, 26, 40, 40, ID_I_NAME, 8, "" }
};
static const int BTN_MENU_N = 12;

static const BtnDesc BTN_DIFF[] = {
    { 160, 210, 300, 270, ID_D_0, 3, "" },
    { 490, 210, 300, 270, ID_D_1, 3, "" },
    { 820, 210, 300, 270, ID_D_2, 3, "" },
    { 28, 24, 44, 44, ID_D_BACK, 9, "" },
    { 520, 560, 240, 54, ID_D_OK, 1, "应 用 并 返 回" }
};
static const int BTN_DIFF_N = 5;

static const BtnDesc BTN_RANK[] = {
    { 28, 24, 44, 44, ID_R_BACK, 9, "" }
};
static const int BTN_RANK_N = 1;

static const BtnDesc BTN_RULES[] = {
    { 28, 24, 44, 44, ID_U_BACK, 9, "" }
};
static const int BTN_RULES_N = 1;

static const BtnDesc BTN_NAME[] = {
    { 28, 24, 44, 44, ID_N_BACK, 9, "" },
    { 450, 418, 180, 52, ID_N_OK,   1, "确  定" },
    { 650, 418, 180, 52, ID_N_SKIP, 0, "跳  过" }
};
static const int BTN_NAME_N = 3;

static const BtnDesc BTN_GAME_INPUT[] = {
    { 28, 24, 44, 44, ID_G_BACK, 9, "" },
    { 774, 510, 126, 36, ID_G_NOANS,  4, "本题无解" },
    { 774, 554, 186, 46, ID_G_SUBMIT, 1, "提  交" }
};
static const int BTN_GAME_INPUT_N = 3;

static const BtnDesc BTN_GAME_RESULT[] = {
    { 28, 24, 44, 44, ID_G_BACK, 9, "" },
    { 540, 624, 200, 56, ID_G_NEXT, 1, "下 一 题" },
    { 760, 624, 180, 56, ID_G_BACK2, 5, "返回主菜单" }
};
static const int BTN_GAME_RESULT_N = 3;

static const BtnDesc BTN_SUMMARY[] = {
    { 28, 24, 44, 44, ID_S_BACK, 9, "" },
    { 300, 600, 200, 56, ID_S_AGAIN, 1, "再 来 一 局" },
    { 530, 600, 200, 56, ID_S_RANK,  0, "查看排行榜" },
    { 760, 600, 200, 56, ID_S_MENU,  5, "返回主菜单" }
};
static const int BTN_SUMMARY_N = 4;

const BtnDesc *curButtons(int &n)
{
    switch (g.screen) {
    case SCR_MENU:    n = BTN_MENU_N;    return BTN_MENU;
    case SCR_DIFF:    n = BTN_DIFF_N;    return BTN_DIFF;
    case SCR_RANK:    n = BTN_RANK_N;    return BTN_RANK;
    case SCR_RULES:   n = BTN_RULES_N;   return BTN_RULES;
    case SCR_NAME:    n = BTN_NAME_N;    return BTN_NAME;
    case SCR_SUMMARY: n = BTN_SUMMARY_N; return BTN_SUMMARY;
    case SCR_GAME:
        if (g.subState == GS_INPUT) { n = BTN_GAME_INPUT_N;  return BTN_GAME_INPUT;  }
        n = BTN_GAME_RESULT_N; return BTN_GAME_RESULT;
    }
    n = 0;
    return NULL;
}

/*====================== 十三、按钮绘制 ======================*/

static int iconKindOfId(int id)
{
    switch (id) {
    case ID_I_SFX: case ID_M_SFX:   return g_sfxOn ? 6 : 7;
    case ID_I_RULES:               return 1;
    case ID_I_RANK:                return 4;
    case ID_I_NAME:                return 3;
    }
    return 0;
}

static void drawButton(HDC dc, const BtnDesc &b, bool hover, bool press)
{
    int x = b.x, y = b.y, w = b.w, h = b.h;
    int dy = press ? 1 : 0;

    if (b.style == 1) {                       /* 黑底主按钮 */
        COLORREF bg = press ? RGB(38, 40, 46) : (hover ? RGB(30, 32, 38) : P_INK);
        cutFill(dc, x, y + dy, w, h, 4, bg);
        fillRect(dc, x, y + dy, 5, h, P_PINK);
        tx(dc, b.text, x + 6, y + dy, w - 6, h, F_H3, P_WHITE,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (hover) cutBorder(dc, x - 2, y + dy - 2, w + 4, h + 4, 6, P_PINK, 2);
    } else if (b.style == 4) {                /* 复选框 */
        int by = y + dy + h / 2 - 8;
        fillRect(dc, x + 10, by, 16, 16, g.noAnswer ? P_INK : P_PAPER2);
        {
            HPEN pn = CreatePen(PS_SOLID, 1, g.noAnswer ? P_INK : P_GRAY);
            HPEN op = (HPEN)SelectObject(dc, pn);
            HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, x + 10, by, x + 26, by + 16);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(pn);
        }
        if (g.noAnswer) {
            HPEN pn = CreatePen(PS_SOLID, 2, P_WHITE);
            HPEN op = (HPEN)SelectObject(dc, pn);
            MoveToEx(dc, x + 14, by + 8, NULL);
            LineTo(dc, x + 17, by + 11);
            LineTo(dc, x + 22, by + 4);
            SelectObject(dc, op);
            DeleteObject(pn);
        }
        tx(dc, b.text, x + 34, y + dy, w - 36, h, F_SMALL,
           hover ? P_PINK_D : P_INK, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else if (b.style == 5) {                /* 深色底上的文字按钮 */
        COLORREF c = (press || hover) ? P_PINK : P_INK2;
        tx(dc, b.text, x, y + dy, w, h, F_BODY, c,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (hover) hline(dc, x + w / 2 - 24, y + h - 8, 48, P_PINK, 2);
    } else if (b.style == 2) {                /* 纸面文字按钮 */
        COLORREF c = (press || hover) ? P_PINK_D : P_INK2;
        tx(dc, b.text, x, y + dy, w, h, F_BODY, c,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (hover) hline(dc, x + w / 2 - 24, y + h - 8, 48, P_PINK, 2);
    } else {                                  /* 纸面按钮 */
        COLORREF bg = press ? P_PAPER2 : P_PAPER;
        cutFill(dc, x, y + dy, w, h, 4, bg);
        cutBorder(dc, x, y + dy, w, h, 4, hover ? P_PINK : P_INK, hover ? 2 : 1);
        tx(dc, b.text, x, y + dy, w, h, F_H3, P_INK,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawAllButtons(HDC dc)
{
    int n = 0;
    const BtnDesc *bs = curButtons(n);

    for (int i = 0; i < n; ++i) {
        if (bs[i].style == 3 || bs[i].style == 7) continue;

        bool hover = (g_hover == bs[i].id);
        bool press = (g_press == bs[i].id);

        if (bs[i].style == 8) {
            int cx = bs[i].x + bs[i].w / 2;
            int cy = bs[i].y + bs[i].h / 2;
            if (hover) {
                HPEN pn = CreatePen(PS_SOLID, 2, P_PINK);
                HPEN op = (HPEN)SelectObject(dc, pn);
                HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
                Ellipse(dc, cx - 21, cy - 21, cx + 21, cy + 21);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(pn);
            }
            drawIcon(dc, iconKindOfId(bs[i].id), cx, cy, 30,
                     hover ? P_PINK : RGB(226, 230, 235));
        } else if (bs[i].style == 9) {
            int cx = bs[i].x + bs[i].w / 2;
            int cy = bs[i].y + bs[i].h / 2;
            if (hover) {
                HPEN pn = CreatePen(PS_SOLID, 2, P_PINK);
                HPEN op = (HPEN)SelectObject(dc, pn);
                HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
                Ellipse(dc, cx - 22, cy - 22, cx + 22, cy + 22);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(pn);
            }
            drawIcon(dc, 10, cx, cy, 26, hover ? P_PINK : RGB(228, 232, 237));
        } else {
            drawButton(dc, bs[i], hover, press);
        }
    }
}

/*====================== 十四、主菜单 ======================*/

static bool slotHover(int i) { return g_hoverSlot == i; }
static bool slotPress(int i) { return g_pressSlot == i; }

static void menuTopBar(HDC dc)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    char date[64];
    snprintf(date, sizeof(date), "%04d/%02d/%02d  %02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    tx(dc, date, 980, 12, 268, 20, F_CAP, RGB(168, 174, 182),
       DT_RIGHT | DT_TOP | DT_SINGLELINE);

    const int gx[3] = { 940, 1050, 1160 };
    char buf[32];

    diamond(dc, gx[0] + 17, 50, 9, RGB(86, 156, 206), true);
    diamond(dc, gx[1] + 17, 50, 9, P_PINK, false);
    diamond(dc, gx[2] + 17, 50, 9, P_GOLD, false);

    snprintf(buf, sizeof(buf), "%d", g.totalScore);
    tx(dc, buf, gx[0] + 34, 34, 90, 28, F_NUM_M, P_WHITE, DT_LEFT | DT_TOP);
    tx(dc, "总分", gx[0] + 34, 66, 60, 18, F_SMALL, RGB(150, 156, 164),
       DT_LEFT | DT_TOP);

    snprintf(buf, sizeof(buf), "%d", bestScore());
    tx(dc, buf, gx[1] + 34, 34, 90, 28, F_NUM_M, P_WHITE, DT_LEFT | DT_TOP);
    tx(dc, "最高", gx[1] + 34, 66, 60, 18, F_SMALL, RGB(150, 156, 164),
       DT_LEFT | DT_TOP);

    tx(dc, LEVELS[g.diff].name, gx[2] + 34, 36, 70, 26, F_H3, P_WHITE,
       DT_LEFT | DT_TOP);
    tx(dc, "难度", gx[2] + 34, 66, 60, 18, F_SMALL, RGB(150, 156, 164),
       DT_LEFT | DT_TOP);

    hline(dc, 916, 84, 332, RGB(84, 92, 102), 1);
    vline(dc, 916, 70, 15, RGB(120, 128, 138), 2);
}

static void menuLeftColumn(HDC dc)
{
    static const char *VCH[4] = { "二", "十", "四", "点" };

    for (int i = 0; i < 4; ++i)
        tx(dc, VCH[i], 60, 130 + i * 72, 70, 72, F_VERT,
           RGB(96, 22, 50), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    hline(dc, 58, 118, 70, P_PINK, 3);
    vline(dc, 136, 132, 338, RGB(255, 226, 238), 1);

    tx(dc, "24", 140, 100, 440, 300, F_WATER, RGB(255, 216, 230),
       DT_LEFT | DT_TOP);

    /* 等级徽章 */
    const int cx = 112, cy = 470, r = 45;
    HBRUSH br = CreateSolidBrush(RGB(16, 19, 24));
    HBRUSH ob = (HBRUSH)SelectObject(dc, br);
    HPEN   pn = CreatePen(PS_SOLID, 2, RGB(226, 230, 235));
    HPEN   op = (HPEN)SelectObject(dc, pn);
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pn);

    arcPoly(dc, cx, cy, r - 1, 200, 340, P_GOLD, 4);

    char lv[8];
    snprintf(lv, sizeof(lv), "%02d", playerLevel());
    tx(dc, lv, cx - 45, cy - 30, 90, 46, F_NUM_L, P_WHITE,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "L V", cx - 30, cy + 22, 60, 16, F_CAP, RGB(190, 196, 203),
       DT_CENTER | DT_TOP | DT_SINGLELINE);

    tx(dc, g.player[0] ? g.player : "玩家", 58, 516, 300, 42, F_HERO,
       RGB(88, 18, 44), DT_LEFT | DT_TOP | DT_SINGLELINE);
    tx(dc, "ID: LOCAL-0001", 58, 558, 260, 18, F_CAP, RGB(122, 28, 60),
       DT_LEFT | DT_TOP | DT_SINGLELINE);

    /* 对白框 */
    paperCard(dc, 56, 586, 478, 76, 8, RGB(14, 17, 22), RGB(190, 196, 204), 1, false);
    tx(dc, "V O I C E", 70, 591, 100, 14, F_CAP, P_PINK, DT_LEFT | DT_TOP);
    tx(dc, "四条运算，二十四点。", 70, 608, 440, 24, F_BODY, P_WHITE,
       DT_LEFT | DT_TOP | DT_SINGLELINE);
    tx(dc, "限时之内，一算即中。", 70, 634, 440, 24, F_BODY,
       RGB(206, 211, 218), DT_LEFT | DT_TOP | DT_SINGLELINE);

    /* 最新战报 */
    cutFill(dc, 61, 679, 268, 82, 8, RGB(8, 10, 14));
    cutFill(dc, 56, 674, 268, 82, 8, RGB(18, 15, 20));
    POINT tag[4] = { { 300, 674 }, { 324, 674 }, { 324, 756 }, { 292, 756 } };
    polySolid(dc, tag, 4, P_PINK);
    tx(dc, "B E S T   R E C O R D", 70, 681, 200, 14, F_CAP,
       RGB(255, 235, 242), DT_LEFT | DT_TOP);
    {
        char b[48];
        snprintf(b, sizeof(b), "最高 %d 分", bestScore());
        tx(dc, b, 70, 696, 220, 34, F_H2, P_WHITE, DT_LEFT | DT_TOP);
    }
    tx(dc, bestScore() > 0 ? "BEST RECORD" : "NO RECORD YET", 70, 732, 220, 16,
       F_CAP, RGB(255, 226, 236), DT_LEFT | DT_TOP);

    /* 音效开关 */
    paperCard(dc, 336, 674, 100, 82, 6, P_PAPER, P_INK, 1, true);
    if (slotHover(7)) cutBorder(dc, 334, 672, 104, 86, 8, P_PINK, 2);
    drawIcon(dc, g_sfxOn ? 6 : 7, 386, 700, 20, P_INK);
    tx(dc, g_sfxOn ? "音效 开" : "音效 关", 336, 716, 100, 22, F_SMALL, P_INK,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* 题数信息块 */
    paperCard(dc, 444, 674, 90, 82, 6, RGB(30, 35, 42), RGB(120, 128, 136), 1, true);
    tx(dc, "3", 444, 688, 90, 30, F_NUM_M, P_WHITE,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "题 / 局", 444, 720, 90, 18, F_SMALL, RGB(190, 196, 203),
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void menuHero(HDC dc)
{
    const int x = 700, y = 128, w = 548, h = 192;
    bool hover = slotHover(0);
    bool press = slotPress(0);

    paperCard(dc, x, y, w, h, 12, P_PAPER, P_INK, 2, true);
    if (press) cutBorder(dc, x - 3, y - 3, w + 6, h + 6, 14, P_PINK_D, 3);
    else if (hover) cutBorder(dc, x - 3, y - 3, w + 6, h + 6, 14, P_PINK, 2);

    fillRect(dc, x + 20, y + 22, 10, 10, P_PINK);
    tx(dc, "T E R M I N A L   0 1", x + 38, y + 16, 180, 18, F_CAP,
       RGB(110, 110, 110), DT_LEFT | DT_TOP);
    emblem(dc, x + 514, y + 30, 22, P_PINK, P_PINK_D);

    tx(dc, "24", x + 20, y + 30, 170, 120, F_NUM_XXL, P_INK, DT_LEFT | DT_TOP);

    vline(dc, x + 170, y + 44, 114, P_INK, 3);
    vline(dc, x + 176, y + 44, 114, RGB(160, 160, 160), 1);

    cutFill(dc, x + 190, y + 40, 126, 22, 3, P_PINK);
    {
        char tag[64];
        snprintf(tag, sizeof(tag), "当前难度 · %s", LEVELS[g.diff].name);
        tx(dc, tag, x + 190, y + 41, 126, 20, F_TINY, P_WHITE,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    tx(dc, "开始对局", x + 190, y + 66, 190, 52, F_HERO, P_INK,
       DT_LEFT | DT_TOP);
    {
        char info[96];
        const Difficulty &cd = LEVELS[g.diff];
        snprintf(info, sizeof(info), "3 题一局 · %d 秒 / 题", cd.limitSec);
        tx(dc, info, x + 190, y + 120, 190, 22, F_SMALL, P_INK2,
           DT_LEFT | DT_TOP);
        snprintf(info, sizeof(info), "单题满分 %d 分", cd.baseScore + cd.bonusScore);
        tx(dc, info, x + 190, y + 142, 190, 20, F_SMALL,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
    }

    fillRect(dc, x, y + h - 34, w, 34, P_INK);
    tx(dc, "点击进入终端", x + 18, y + h - 34, 280, 34, F_SMALL, P_WHITE,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawIcon(dc, 9, x + 512, y + h - 17, 20, P_PINK);
}

static void menuGrid(HDC dc)
{
    /* 难度设置 */
    {
        const int x = 700, y = 340, w = 270, h = 152;
        paperCard(dc, x, y, w, h, 10, P_PAPER, P_INK, 2, true);
        if (slotHover(1)) cutBorder(dc, x - 2, y - 2, w + 4, h + 4, 12, P_PINK, 2);
        tx(dc, "难度设置", x + 18, y + 6, 200, 44, F_H1, P_INK, DT_LEFT | DT_TOP);
        tx(dc, "D I F F I C U L T Y", x + 20, y + 50, 180, 16, F_CAP,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
        emblem(dc, 936, 432, 27, P_PINK, P_PINK_D);

        static const char *PIP[3] = { "简单", "普通", "困难" };
        for (int i = 0; i < 3; ++i) {
            int px = 718 + i * 64;
            if (g.diff == i) {
                fillRect(dc, px, 410, 56, 28, P_INK);
                tx(dc, PIP[i], px, 410, 56, 28, F_SMALL, P_WHITE,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                HPEN pn = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
                HPEN op = (HPEN)SelectObject(dc, pn);
                HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
                Rectangle(dc, px, 410, px + 56, 438);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(pn);
                tx(dc, PIP[i], px, 410, 56, 28, F_SMALL, P_INK2,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        {
            const Difficulty &cd = LEVELS[g.diff];
            char b[64];
            snprintf(b, sizeof(b), "限时 %d 秒 / 满分 %d 分",
                     cd.limitSec, cd.baseScore + cd.bonusScore);
            tx(dc, b, x + 18, y + 112, 240, 20, F_SMALL,
               RGB(110, 110, 110), DT_LEFT | DT_TOP);
        }
    }

    /* 玩家档案 */
    {
        const int x = 978, y = 340, w = 270, h = 152;
        paperCard(dc, x, y, w, h, 10, P_PAPER, P_INK, 2, true);
        if (slotHover(2)) cutBorder(dc, x - 2, y - 2, w + 4, h + 4, 12, P_PINK, 2);
        tx(dc, "玩家档案", x + 18, y + 6, 200, 44, F_H1, P_INK, DT_LEFT | DT_TOP);
        tx(dc, "P L A Y E R", x + 20, y + 50, 140, 16, F_CAP,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
        barcode(dc, 1170, 360, 56, 96, RGB(40, 40, 40));
        tx(dc, g.player[0] ? g.player : "玩家", x + 18, y + 74, 180, 34,
           F_H2, P_INK, DT_LEFT | DT_TOP);
        char b[64];
        snprintf(b, sizeof(b), "最高分 %d  ·  记录 %d 条", bestScore(), g.recCount);
        tx(dc, b, x + 18, y + 112, 200, 20, F_SMALL, RGB(110, 110, 110),
           DT_LEFT | DT_TOP);
    }

    /* 排行榜 */
    {
        const int x = 700, y = 508, w = 178, h = 92;
        cutFill(dc, x + 5, y + 5, w, h, 8, RGB(9, 12, 16));
        cutFill(dc, x, y, w, h, 8, P_PINK);
        if (slotHover(3)) cutBorder(dc, x - 2, y - 2, w + 4, h + 4, 10, P_WHITE, 2);
        tx(dc, "排行榜", x + 16, y + 10, 140, 34, F_H2, P_WHITE, DT_LEFT | DT_TOP);
        tx(dc, "R A N K I N G", x + 17, y + 46, 140, 16, F_CAP,
           RGB(240, 220, 228), DT_LEFT | DT_TOP);
        tx(dc, "查看高手成绩", x + 16, y + 66, 150, 18, F_SMALL, P_WHITE,
           DT_LEFT | DT_TOP);
        fillRect(dc, x, y + h - 4, w, 4, P_PINK_D);
    }

    /* 游戏规则 */
    {
        const int x = 884, y = 508, w = 178, h = 92;
        paperCard(dc, x, y, w, h, 8, P_PAPER, P_INK, 1, true);
        if (slotHover(4)) cutBorder(dc, x - 2, y - 2, w + 4, h + 4, 10, P_PINK, 2);
        tx(dc, "游戏规则", x + 16, y + 10, 150, 34, F_H2, P_INK, DT_LEFT | DT_TOP);
        tx(dc, "R U L E S", x + 17, y + 46, 140, 16, F_CAP,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
        tx(dc, "四张牌 · 凑 24", x + 16, y + 66, 150, 18, F_SMALL,
           RGB(70, 70, 70), DT_LEFT | DT_TOP);
    }

    /* 退出 */
    {
        const int x = 1068, y = 508, w = 178, h = 92;
        cutFill(dc, x + 5, y + 5, w, h, 8, RGB(9, 12, 16));
        cutFill(dc, x, y, w, h, 8, RGB(24, 27, 33));
        cutBorder(dc, x, y, w, h, 8, slotHover(5) ? P_PINK : RGB(150, 120, 132),
                  slotHover(5) ? 2 : 1);
        tx(dc, "退出", x + 16, y + 10, 120, 34, F_H2, P_WHITE, DT_LEFT | DT_TOP);
        tx(dc, "Q U I T", x + 17, y + 46, 120, 16, F_CAP, RGB(196, 202, 210),
           DT_LEFT | DT_TOP);
        drawIcon(dc, 8, x + 142, y + 38, 22, P_PINK);
    }

    /* 今日战报 */
    {
        const int x = 700, y = 616, w = 270, h = 112;
        paperCard(dc, x, y, w, h, 10, P_PAPER, P_INK, 2, true);
        tx(dc, "训练战报", x + 18, y + 8, 180, 34, F_H2, P_INK, DT_LEFT | DT_TOP);
        tx(dc, "R E P O R T", x + 20, y + 46, 120, 16, F_CAP,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
        char b[32];
        snprintf(b, sizeof(b), "%d", bestScore());
        tx(dc, b, x + 130, y + 2, 132, 76, F_NUM_XL, P_PINK, DT_RIGHT | DT_TOP);
        snprintf(b, sizeof(b), "记录 %d 条  ·  最高 %d 分", g.recCount, bestScore());
        tx(dc, b, x + 18, y + 84, 240, 20, F_SMALL, RGB(110, 110, 110),
           DT_LEFT | DT_TOP);
    }

    /* 关于 */
    {
        const int x = 978, y = 616, w = 270, h = 112;
        paperCard(dc, x, y, w, h, 10, P_PAPER, P_INK, 2, true);
        if (slotHover(6)) cutBorder(dc, x - 2, y - 2, w + 4, h + 4, 12, P_PINK, 2);
        tx(dc, "关于", x + 18, y + 8, 150, 34, F_H2, P_INK, DT_LEFT | DT_TOP);
        tx(dc, "A B O U T", x + 20, y + 46, 120, 16, F_CAP,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
        tx(dc, "计算二十四点 · 课程设计", x + 18, y + 70, 240, 20, F_SMALL,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
        hline(dc, x + 18, y + 96, 234, RGB(198, 194, 186), 1);
    }
}

static void menuTooltip(HDC dc)
{
    static const char *TIP[4] = { "音效", "规则", "排行", "档案" };

    if (g_hoverSlot >= 8 && g_hoverSlot <= 11) {
        int i = g_hoverSlot - 8;
        int x = 28 + i * 50 - 20;
        fillRect(dc, x, 72, 64, 24, RGB(18, 21, 26));
        hline(dc, x, 72, 64, P_PINK, 2);
        tx(dc, TIP[i], x, 72, 64, 24, F_SMALL, P_WHITE,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawMenuScreen(HDC dc)
{
    drawBackdrop(dc);
    menuTopBar(dc);
    menuLeftColumn(dc);

    menuHero(dc);

    menuGrid(dc);
    menuTooltip(dc);

    drawFooter(dc, "CALC-24  TERMINAL  //  计算二十四点", "V2.0");
}

/*====================== 十五、难度 / 规则 / 排行榜 ======================*/

static void drawDiffScreen(HDC dc)
{
    drawBackdrop(dc);
    tx(dc, "难度设置", 94, 20, 320, 40, F_H2, P_WHITE, DT_LEFT | DT_TOP);
    tx(dc, "D I F F I C U L T Y   S E L E C T", 96, 58, 360, 16, F_CAP,
       RGB(150, 157, 166), DT_LEFT | DT_TOP);

    static const char *NAME[3] = { "简单", "普通", "困难" };
    static const char *EN[3]   = { "E A S Y", "N O R M A L", "H A R D" };
    static const char *DESC[3] = {
        "时间充裕，适合熟悉玩法",
        "限时收紧，考验运算速度",
        "极限挑战，高手之间的较量"
    };

    for (int i = 0; i < 3; ++i) {
        int x = BTN_DIFF[i].x, y = BTN_DIFF[i].y;
        int w = BTN_DIFF[i].w, h = BTN_DIFF[i].h;
        bool sel = (g.diff == i);
        bool hov = (g_hover == BTN_DIFF[i].id);

        paperCard(dc, x, y, w, h, 12, sel ? P_PAPER : RGB(238, 235, 229),
                  P_INK, 2, true);
        if (sel) {
            fillRect(dc, x, y, w, 8, P_PINK);
            cutFill(dc, x + 16, y + 24, 72, 24, 3, P_INK);
            tx(dc, "当 前", x + 16, y + 26, 72, 20, F_TINY, P_WHITE,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (hov) {
            cutBorder(dc, x - 2, y - 2, w + 4, h + 4, 14, P_PINK, 2);
        }

        char num[8];
        snprintf(num, sizeof(num), "0%d", i + 1);
        tx(dc, num, x + w - 100, y + 20, 80, 78, F_NUM_XL,
           sel ? P_PINK : RGB(196, 192, 184), DT_RIGHT | DT_TOP);

        tx(dc, NAME[i], x + 24, y + 60, 180, 52, F_HERO, P_INK, DT_LEFT | DT_TOP);
        tx(dc, EN[i], x + 26, y + 112, 180, 18, F_CAP, RGB(110, 110, 110),
           DT_LEFT | DT_TOP);

        hline(dc, x + 24, y + 148, w - 48, RGB(198, 194, 186), 1);

        char b[64];
        snprintf(b, sizeof(b), "每题限时  %d 秒", LEVELS[i].limitSec);
        tx(dc, b, x + 24, y + 162, 240, 22, F_BODY, P_INK, DT_LEFT | DT_TOP);
        snprintf(b, sizeof(b), "基础 %d + 时间 %d", LEVELS[i].baseScore,
                 LEVELS[i].bonusScore);
        tx(dc, b, x + 24, y + 190, 240, 20, F_SMALL, RGB(90, 90, 90),
           DT_LEFT | DT_TOP);
        snprintf(b, sizeof(b), "单题满分  %d 分",
                 LEVELS[i].baseScore + LEVELS[i].bonusScore);
        tx(dc, b, x + 24, y + 214, 240, 22, F_H3, P_PINK_D, DT_LEFT | DT_TOP);
        tx(dc, DESC[i], x + 24, y + 242, w - 40, 20, F_SMALL,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);
    }

    drawFooter(dc, "选择难度后应用返回，下一局生效", "ESC 返回");
}

static void drawRulesScreen(HDC dc)
{
    drawBackdrop(dc);
    paperCard(dc, 150, 110, 980, 620, 12, P_PAPER, P_INK, 2, true);

    fillRect(dc, 150, 110, 100, 620, P_INK);
    static const char *VCH[2] = { "规", "则" };
    for (int i = 0; i < 2; ++i)
        tx(dc, VCH[i], 160, 150 + i * 66, 80, 66, F_VERT, P_WHITE,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "R U L E S", 160, 292, 80, 18, F_CAP, P_PINK,
       DT_CENTER | DT_TOP);

    tx(dc, "游戏规则", 290, 142, 320, 44, F_H1, P_INK, DT_LEFT | DT_TOP);
    tx(dc, "先看清规则，再开始计时", 292, 186, 400, 20, F_SMALL,
       RGB(110, 110, 110), DT_LEFT | DT_TOP);
    hline(dc, 290, 216, 780, P_INK, 2);

    static const char *LINE[7] = {
        "每局 3 题，每题随机发 4 张牌，点数取 1~10（A 记为 1）。",
        "用 + - * / 和括号，把 4 个数字各用一次，算出 24。",
        "题目保证全程整数运算可解，不需要使用分数。",
        "若判断本题无解，勾选「本题无解」后提交；判断正确同样得分。",
        "超时自动判负，本题 0 分，直接进入下一题。",
        "得分 = 基础分 + 时间奖励 × 剩余秒数 ÷ 本题限时。",
        "得分越高、用时越短，排行越靠前；同名只保留最好成绩。"
    };

    for (int i = 0; i < 7; ++i) {
        char no[8];
        snprintf(no, sizeof(no), "%02d", i + 1);
        tx(dc, no, 290, 240 + i * 54, 40, 20, F_CAP, P_PINK_D, DT_LEFT | DT_TOP);
        tx(dc, LINE[i], 336, 234 + i * 54, 720, 26, F_BODY, P_INK,
           DT_LEFT | DT_TOP);
        hline(dc, 290, 268 + i * 54, 780, RGB(214, 210, 202), 1);
    }

    tx(dc, "单题满分  简单 100  ·  普通 130  ·  困难 160", 290, 638, 760, 24,
       F_SMALL, RGB(90, 90, 90), DT_LEFT | DT_TOP);
    tx(dc, "按 Esc 返回主菜单", 290, 676, 400, 22, F_SMALL, P_PINK_D,
       DT_LEFT | DT_TOP);

    drawFooter(dc, "训练手册  //  24 POINT MANUAL", "V2.0");
}

static void drawRankScreen(HDC dc)
{
    drawBackdrop(dc);
    paperCard(dc, 120, 110, 1040, 620, 12, P_PAPER, P_INK, 2, true);

    fillRect(dc, 120, 110, 1040, 58, P_INK);
    tx(dc, "高手排行榜", 144, 112, 260, 36, F_H2, P_WHITE, DT_LEFT | DT_TOP);
    tx(dc, "R A N K I N G   B O A R D", 146, 146, 260, 14, F_CAP,
       RGB(200, 206, 214), DT_LEFT | DT_TOP);

    static const char *HEAD[6] = { "名次", "玩家", "总分", "用时", "难度", "日期" };
    static const int   CX[6]   = { 160, 260, 560, 690, 810, 960 };
    for (int i = 0; i < 6; ++i)
        tx(dc, HEAD[i], CX[i], 178, 160, 22, F_SMALL, P_INK2, DT_LEFT | DT_TOP);
    hline(dc, 144, 202, 992, P_INK, 2);

    if (g.recCount == 0) {
        tx(dc, "暂时还没有成绩，快去挑战一局吧！", 160, 300, 960, 40, F_H2,
           P_GRAY, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        for (int i = 0; i < 8; ++i)
            hline(dc, 144, 214 + i * 48, 992, RGB(214, 210, 202), 1);
    }

    int nShow = (g.recCount < 10) ? g.recCount : 10;
    for (int i = 0; i < nShow; ++i) {
        const Record &r = g.recs[i];
        int ry = 210 + i * 48;
        if (i % 2 == 1) fillRect(dc, 130, ry, 1020, 47, RGB(236, 232, 224));

        COLORREF mc = P_INK;
        if (i == 0) mc = RGB(176, 132, 46);
        else if (i == 1) mc = RGB(120, 128, 138);
        else if (i == 2) mc = RGB(160, 100, 70);

        char b[32];
        snprintf(b, sizeof(b), "%02d", i + 1);
        tx(dc, b, CX[0], ry, 60, 48, F_NUM_M, mc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        tx(dc, r.name, CX[1], ry, 260, 48, F_BODY, mc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(b, sizeof(b), "%d", r.score);
        tx(dc, b, CX[2], ry, 100, 48, F_NUM_M, mc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(b, sizeof(b), "%d 秒", r.totalSeconds);
        tx(dc, b, CX[3], ry, 110, 48, F_BODY, mc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        COLORREF lc = P_GRAY;
        if (strcmp(r.level, "困难") == 0) lc = P_PINK_D;
        else if (strcmp(r.level, "普通") == 0) lc = P_GOLD;
        tx(dc, r.level, CX[4], ry, 110, 48, F_BODY, lc,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        tx(dc, r.date, CX[5], ry, 190, 48, F_CAP, P_GRAY,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        hline(dc, 144, ry + 47, 992, RGB(214, 210, 202), 1);
    }

    /* 记录不满 10 条时，用浅分隔线补足表格，避免大片空白 */
    if (g.recCount > 0) {
        for (int i = nShow; i < 10; ++i)
            hline(dc, 144, 210 + i * 48 + 47, 992, RGB(226, 222, 214), 1);
    }

    drawFooter(dc, "同名玩家只保留个人最好成绩", "ESC 返回");
}

static void drawGameScreen(HDC dc)
{
    const Difficulty &d = LEVELS[g.diff];
    drawBackdrop(dc);
    tx(dc, "24", 250, 110, 500, 340, F_WATER, RGB(252, 226, 236),
       DT_LEFT | DT_TOP);

    /* 顶栏 */
    tx(dc, "限时训练", 94, 18, 300, 38, F_H2, P_WHITE, DT_LEFT | DT_TOP);
    tx(dc, "T E R M I N A L   / /   R O U N D", 96, 56, 360, 16, F_CAP,
       RGB(150, 157, 166), DT_LEFT | DT_TOP);

    char buf[128];
    snprintf(buf, sizeof(buf), "第 %d / %d 题", g.round, ROUND_COUNT);
    tx(dc, buf, 944, 18, 300, 34, F_H2, P_WHITE, DT_RIGHT | DT_TOP);
    snprintf(buf, sizeof(buf), "SCORE %d", g.totalScore);
    tx(dc, buf, 944, 52, 300, 18, F_CAP, RGB(168, 174, 182), DT_RIGHT | DT_TOP);

    /* 计时 */
    tx(dc, "T I M E", 36, 96, 120, 16, F_CAP, RGB(150, 157, 166),
       DT_LEFT | DT_TOP);
    int sec = (g.remainMs + 999) / 1000;
    if (sec < 0) sec = 0;
    snprintf(buf, sizeof(buf), "%d", sec);
    tx(dc, buf, 1164, 86, 80, 34, F_NUM_M, P_WHITE, DT_RIGHT | DT_TOP);

    int ratio = (g.limitMs > 0) ? (g.remainMs * 100 / g.limitMs) : 0;
    if (ratio < 0) ratio = 0;
    if (ratio > 100) ratio = 100;
    fillRect(dc, 36, 118, 1208, 8, RGB(58, 66, 76));
    if (ratio > 0) {
        COLORREF barC = P_PINK;
        if (ratio <= 25) barC = RGB(220, 70, 82);
        else if (ratio <= 55) barC = P_GOLD;
        fillRect(dc, 36, 118, 1208 * ratio / 100, 8, barC);
    }
    for (int i = 1; i < 10; ++i)
        vline(dc, 36 + 1208 * i / 10, 118, 8, RGB(22, 26, 32), 1);

    snprintf(buf, sizeof(buf), "难度 %s  ·  每题 %d 秒", d.name, d.limitSec);
    tx(dc, buf, 36, 134, 300, 20, F_SMALL, RGB(88, 18, 44),
       DT_LEFT | DT_TOP);

    /* 四张牌 */
    const int cw = 160, ch = 220, gap = 24;
    int x0 = (DES_W - (cw * CARD_COUNT + gap * (CARD_COUNT - 1))) / 2;
    for (int i = 0; i < CARD_COUNT; ++i) {
        int cx = x0 + i * (cw + gap);
        if (i < g.dealt)
            drawPlayingCard(dc, cx, 172, cw, ch, g.cards[i], g.suits[i], false, i);
        else
            drawPlayingCard(dc, cx, 172, cw, ch, 0, 0, true, i);
    }

    /* 提示条 */
    fillRect(dc, 284, 420, 712, 38, RGB(15, 18, 23));
    fillRect(dc, 284, 420, 4, 38, P_PINK);
    tx(dc, "使用 + - * / 与括号，把四张牌各用一次，凑成 24",
       300, 420, 540, 38, F_SMALL, RGB(226, 230, 235),
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "点数 1~10   A=1", 800, 420, 190, 38, F_CAP,
       RGB(150, 157, 166), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    if (g.subState == GS_INPUT) {
        paperCard(dc, 284, 478, 712, 132, 10, P_PAPER, P_INK, 2, true);
        tx(dc, "I N P U T   E X P R E S S I O N", 304, 488, 300, 16, F_CAP,
           RGB(110, 110, 110), DT_LEFT | DT_TOP);

        /* 原生输入框的外框 */
        fillRect(dc, 304, 514, 452, 52, P_WHITE);
        {
            HPEN pn = CreatePen(PS_SOLID, 2, P_INK);
            HPEN op = (HPEN)SelectObject(dc, pn);
            HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, 304, 514, 756, 566);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(pn);
        }
        if (g.dealt < CARD_COUNT) {
            fillRect(dc, 306, 516, 448, 48, P_WHITE);
            tx(dc, "正在发牌 …", 306, 516, 448, 48, F_BODY, P_GRAY,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        if (g.feedback[0] != '\0') {
            COLORREF fc = (g.fbColor == COL_RED) ? RGB(200, 60, 70) : P_PINK_D;
            tx(dc, g.feedback, 284, 618, 712, 24, F_SMALL, fc,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            tx(dc, "输入表达式后按回车提交", 284, 618, 712, 24, F_SMALL,
               RGB(150, 157, 166), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    } else {
        paperCard(dc, 284, 478, 712, 132, 10, P_PAPER, P_INK, 2, true);
        COLORREF fc = (g.fbColor == COL_RED) ? RGB(196, 54, 66) : RGB(24, 130, 78);
        tx(dc, g.feedback, 304, 492, 672, 54, F_H3, fc,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (g.solvable) {
            snprintf(buf, sizeof(buf), "参考答案：%s", g.refAnswer);
            tx(dc, buf, 304, 548, 672, 24, F_SMALL, RGB(90, 90, 90),
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            tx(dc, "本题确实无解", 304, 548, 672, 24, F_SMALL, RGB(90, 90, 90),
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    /* 三题进度 */
    tx(dc, "R O U N D", 36, 638, 120, 16, F_CAP, RGB(88, 18, 44),
       DT_LEFT | DT_TOP);
    for (int i = 0; i < ROUND_COUNT; ++i) {
        int bx = 36 + i * 64;
        fillRect(dc, bx, 660, 52, 6,
                 (i + 1 <= g.round) ? P_INK : RGB(216, 152, 180));
        snprintf(buf, sizeof(buf), "%02d", i + 1);
        tx(dc, buf, bx, 672, 52, 20, F_CAP,
           (i + 1 <= g.round) ? P_INK : RGB(122, 42, 72), DT_LEFT | DT_TOP);
    }

    drawFooter(dc, "回车提交 · Esc 返回菜单 · M 静音 · 只允许 + - * / ( ) 和数字", "V2.0");
}

static void drawNameScreen(HDC dc)
{
    drawBackdrop(dc);
    tx(dc, "玩家登记", 94, 20, 300, 40, F_H2, P_WHITE, DT_LEFT | DT_TOP);
    tx(dc, "P L A Y E R   R E G I S T R A T I O N", 96, 58, 380, 16, F_CAP,
       RGB(150, 157, 166), DT_LEFT | DT_TOP);

    paperCard(dc, 380, 180, 520, 380, 12, P_PAPER, P_INK, 2, true);
    tx(dc, "留下你的名字", 420, 204, 400, 38, F_H1, P_INK, DT_LEFT | DT_TOP);
    tx(dc, "成绩会以这个名字记入排行榜", 422, 246, 420, 20, F_SMALL,
       RGB(110, 110, 110), DT_LEFT | DT_TOP);
    hline(dc, 420, 278, 440, P_INK, 2);

    fillRect(dc, 420, 300, 440, 56, P_WHITE);
    {
        HPEN pn = CreatePen(PS_SOLID, 2, P_INK);
        HPEN op = (HPEN)SelectObject(dc, pn);
        HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, 420, 300, 860, 356);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(pn);
    }
    tx(dc, "最多 16 个字符；留空则记为「玩家」", 420, 364, 440, 20, F_SMALL,
       RGB(110, 110, 110), DT_LEFT | DT_TOP);
    tx(dc, "排行榜上同名玩家只保留最好成绩", 420, 386, 440, 20, F_SMALL,
       RGB(110, 110, 110), DT_LEFT | DT_TOP);

    tx(dc, "输入后按回车或点「确定」", 420, 484, 440, 22, F_SMALL, P_PINK_D,
       DT_LEFT | DT_TOP);

    drawFooter(dc, "身份登记  //  PLAYER ID", "ESC 返回");
}

static void drawSummaryScreen(HDC dc)
{
    drawBackdrop(dc);
    tx(dc, "本局结算", 94, 20, 300, 40, F_H2, P_WHITE, DT_LEFT | DT_TOP);
    tx(dc, "R E S U L T   R E P O R T", 96, 58, 360, 16, F_CAP,
       RGB(150, 157, 166), DT_LEFT | DT_TOP);

    paperCard(dc, 150, 110, 980, 440, 12, P_PAPER, P_INK, 2, true);

    tx(dc, "T O T A L   S C O R E", 180, 136, 300, 18, F_CAP, RGB(110, 110, 110),
       DT_LEFT | DT_TOP);
    char buf[128];
    snprintf(buf, sizeof(buf), "%d", g.totalScore);
    tx(dc, buf, 176, 156, 300, 130, F_NUM_XXL, P_PINK_D, DT_LEFT | DT_TOP);
    tx(dc, "分", 368, 210, 60, 34, F_BODY, P_INK2, DT_LEFT | DT_TOP);

    tx(dc, "本局评价", 180, 288, 120, 20, F_SMALL, RGB(110, 110, 110),
       DT_LEFT | DT_TOP);
    {
        int good = 0;
        for (int i = 0; i < ROUND_COUNT; ++i)
            if (g.roundRes[i] == RES_RIGHT || g.roundRes[i] == RES_NOANS_OK) good++;
        static const char *RATE[4] = { "再接再厉", "渐入佳境", "出手不凡", "算无遗策" };
        tx(dc, RATE[good], 180, 306, 260, 40, F_H1, P_INK, DT_LEFT | DT_TOP);
    }

    vline(dc, 520, 140, 200, RGB(198, 194, 186), 1);

    const Difficulty &d = LEVELS[g.diff];
    tx(dc, "难度", 560, 150, 100, 20, F_SMALL, RGB(110, 110, 110),
       DT_LEFT | DT_TOP);
    tx(dc, d.name, 660, 144, 200, 28, F_H3, P_INK, DT_LEFT | DT_TOP);

    tx(dc, "累计用时", 560, 190, 100, 20, F_SMALL, RGB(110, 110, 110),
       DT_LEFT | DT_TOP);
    snprintf(buf, sizeof(buf), "%.1f 秒", g.totalMs / 1000.0);
    tx(dc, buf, 660, 184, 240, 28, F_H3, P_INK, DT_LEFT | DT_TOP);

    tx(dc, "榜上名次", 560, 230, 100, 20, F_SMALL, RGB(110, 110, 110),
       DT_LEFT | DT_TOP);
    if (g.rankPos > 0) snprintf(buf, sizeof(buf), "第 %d 名", g.rankPos);
    else               strcpy(buf, "未上榜");
    tx(dc, buf, 660, 224, 240, 28, F_H3, P_PINK_D, DT_LEFT | DT_TOP);

    if (g.newBest && g.rankPos > 0) {
        fillRect(dc, 560, 272, 200, 28, P_PINK);
        tx(dc, "★ 刷新个人最好成绩", 560, 272, 200, 28, F_SMALL, P_WHITE,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 三题明细 */
    hline(dc, 180, 360, 920, P_INK, 2);
    static const char *HEAD[4] = { "题号", "判定", "用时", "得分" };
    static const int   CX[4]   = { 200, 360, 620, 820 };
    for (int i = 0; i < 4; ++i)
        tx(dc, HEAD[i], CX[i], 368, 160, 20, F_SMALL, RGB(110, 110, 110),
           DT_LEFT | DT_TOP);

    static const char *RESNAME[5] = {
        "回答正确", "回答错误", "回答超时", "无解判断正确", "无解判断错误"
    };

    for (int i = 0; i < ROUND_COUNT; ++i) {
        int ry = 400 + i * 48;
        snprintf(buf, sizeof(buf), "第 %d 题", i + 1);
        tx(dc, buf, CX[0], ry, 120, 44, F_BODY, P_INK, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        COLORREF c = (g.roundRes[i] == RES_RIGHT || g.roundRes[i] == RES_NOANS_OK)
                   ? RGB(24, 130, 78) : RGB(196, 54, 66);
        tx(dc, RESNAME[g.roundRes[i]], CX[1], ry, 240, 44, F_BODY, c,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(buf, sizeof(buf), "%.1f 秒", g.roundMs[i] / 1000.0);
        tx(dc, buf, CX[2], ry, 160, 44, F_BODY, P_INK,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(buf, sizeof(buf), "%d 分", g.roundScore[i]);
        tx(dc, buf, CX[3], ry, 120, 44, F_NUM_M, P_INK,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        hline(dc, 180, ry + 44, 920, RGB(214, 210, 202), 1);
    }

    drawFooter(dc, "成绩已写入排行榜  //  RANK SAVED", "ESC 返回");
}

/*====================== 十七、总绘制入口 ======================*/

void drawCanvas(HDC dc, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    int om = SetMapMode(dc, MM_ANISOTROPIC);
    SetWindowExtEx(dc, DES_W, DES_H, NULL);
    SetViewportExtEx(dc, w, h, NULL);
    SetWindowOrgEx(dc, 0, 0, NULL);
    SetViewportOrgEx(dc, 0, 0, NULL);

    switch (g.screen) {
    case SCR_MENU:    drawMenuScreen(dc);       break;
    case SCR_DIFF:    drawDiffScreen(dc);       break;
    case SCR_RULES:   drawRulesScreen(dc);      break;
    case SCR_RANK:    drawRankScreen(dc);       break;
    case SCR_NAME:    drawNameScreen(dc);       break;
    case SCR_GAME:    drawGameScreen(dc);       break;
    case SCR_SUMMARY: drawSummaryScreen(dc);    break;
    }

    drawAllButtons(dc);

    SetMapMode(dc, om);
}


