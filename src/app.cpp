/*==========================================================================
 *  应用层：窗口重绘、对局流程、输入框子类化、命令分派、
 *  窗口过程、资源初始化。
 *========================================================================*/

#include "calc24.h"

HFONT  F_HERO   = NULL;      /* 大标题 40（中文）                    */
HFONT  F_H1     = NULL;      /* 卡片标题 32                          */
HFONT  F_H2     = NULL;      /* 小标题 26                            */
HFONT  F_H3     = NULL;      /* 按钮 / 强调 20                       */
HFONT  F_BODY   = NULL;      /* 正文 16                              */
HFONT  F_SMALL  = NULL;      /* 小字 12                              */
HFONT  F_TINY   = NULL;      /* 微字 10                              */
HFONT  F_CAP    = NULL;      /* 拉丁小标题 10（Bahnschrift）         */
HFONT  F_NUM    = NULL;      /* 数字 22                              */
HFONT  F_NUM_M  = NULL;      /* 数字 26                              */
HFONT  F_NUM_L  = NULL;      /* 数字 38                              */
HFONT  F_NUM_XL = NULL;      /* 数字 64                              */
HFONT  F_NUM_XXL= NULL;      /* 数字 104                             */
HFONT  F_CARD_NUM = NULL;    /* 牌面点数 76                          */
HFONT  F_VERT   = NULL;      /* 竖排标题 48                          */
HFONT  F_WATER  = NULL;      /* 背景水印 250                         */
HFONT  F_MONO   = NULL;      /* 等宽数字 24                          */
HFONT  F_EDIT   = NULL;      /* 输入框（设备单位）                   */
HFONT  F_SUIT_S = NULL;      /* 花色 26                              */
HFONT  F_SUIT_M = NULL;      /* 花色 38                              */
HFONT  F_SUIT_L = NULL;      /* 花色 50                              */
HBRUSH g_brEdit   = NULL;    /* edit field background brush */
HWND   g_hwnd     = NULL;    /* 主窗口                               */
HWND   g_edit     = NULL;    /* 表达式输入框                         */
WNDPROC g_editOld = NULL;    /* 输入框原窗口过程                     */
double g_scale    = 1.0;     /* 1 逻辑单位 = g_scale 像素          */
int    g_hover    = 0;       /* 悬停按钮编号                         */
int    g_press    = 0;       /* 按下按钮编号                         */
int    g_hoverSlot = -1;     /* 悬停按钮在列表中的下标（区分重复 ID）*/
int    g_pressSlot = -1;     /* 按下按钮在列表中的下标               */
bool   g_nameFromMenu = false; /* 姓名界面是“档案改名”而非开局     */
GameState g;

int sc(int v) { return (int)(v * g_scale + 0.5); }

/*
 * 根据客户区尺寸算出当前实际绘制缩放：
 * 画布始終等比缩放并居中，因此窗口放大 / 最大化时界面跟着铺满，
 * 长宽比例不同时两侧留极窄的边，不会变形。
 */
double viewScale(int cw, int ch)
{
    if (cw <= 0 || ch <= 0) return g_scale;

    double sx = cw / (double)DES_W;
    double sy = ch / (double)DES_H;
    double s  = (sx < sy) ? sx : sy;

    if (s < 0.55) s = 0.55;
    if (s > 4.0)  s = 4.0;
    return s;
}

static void repaint()
{
    if (g_hwnd != NULL) InvalidateRect(g_hwnd, NULL, FALSE);
}

/* 只失效设计坐标下的某一小块（用于每帧刷新的倒计时条） */
static void repaintDesignRect(int dx, int dy, int dw, int dh)
{
    if (g_hwnd == NULL) return;

    RECT rc;
    GetClientRect(g_hwnd, &rc);

    double s  = viewScale(rc.right, rc.bottom);
    int    cw = rc.right, ch = rc.bottom;
    int    w  = (int)(DES_W * s + 0.5);
    int    h  = (int)(DES_H * s + 0.5);
    int    ox = (cw - w) / 2;
    int    oy = (ch - h) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    RECT r;
    r.left   = ox + (int)(dx * s) - 2;
    r.top    = oy + (int)(dy * s) - 2;
    r.right  = ox + (int)((dx + dw) * s) + 2;
    r.bottom = oy + (int)((dy + dh) * s) + 2;

    InvalidateRect(g_hwnd, &r, FALSE);
}

/*
 * 双缓冲位图缓存。答题时定时器每 40 毫秒就要重绘一次，
 * 如果每次都新建/销毁 1280x800 级别的位图既费 CPU 又容易产生可见的顿挫，
 * 因此这里只在画布尺寸变化时才重建位图。
 */
static HDC     g_memDC  = NULL;
static HBITMAP g_memBmp = NULL;
static HBITMAP g_memOld = NULL;      /* DC 自带的默认位图，仅用于脱离 */
static int     g_memW   = 0;
static int     g_memH   = 0;

static void freeMemCanvas()
{
    if (g_memDC != NULL) {
        if (g_memBmp != NULL) {
            SelectObject(g_memDC, g_memOld);
            DeleteObject(g_memBmp);
            g_memBmp = NULL;
        }
        DeleteDC(g_memDC);
        g_memDC = NULL;
    }
    g_memW = 0;
    g_memH = 0;
}

static HDC getMemCanvas(HDC ref, int w, int h)
{
    if (g_memDC == NULL) {
        g_memDC  = CreateCompatibleDC(ref);
        g_memBmp = CreateCompatibleBitmap(ref, w, h);
        g_memOld = (HBITMAP)SelectObject(g_memDC, g_memBmp);
        g_memW   = w;
        g_memH   = h;
        return g_memDC;
    }

    if (w != g_memW || h != g_memH) {
        HBITMAP nb = CreateCompatibleBitmap(ref, w, h);
        HBITMAP ob = (HBITMAP)SelectObject(g_memDC, nb);
        DeleteObject(ob);                 /* 旧位图已脱离 DC，可以安全删除 */
        g_memBmp = nb;
        g_memW   = w;
        g_memH   = h;
    }
    return g_memDC;
}

static void layoutEdit();          /* 前置声明：按当前窗口尺寸摆放输入框 */

static void stopTimer()
{
    if (g_hwnd != NULL) KillTimer(g_hwnd, 1);
}

/* SetProcessDPIAware 在旧 SDK 头文件中没有声明，运行时动态获取 */
typedef BOOL (WINAPI *PFN_DPI_AWARE)(void);

void enableDpiAware()
{
    HMODULE lib = LoadLibraryA("user32.dll");
    if (lib == NULL) return;
    PFN_DPI_AWARE fn = (PFN_DPI_AWARE)(void *)GetProcAddress(lib, "SetProcessDPIAware");
    if (fn != NULL) fn();
    FreeLibrary(lib);
}

static void newRound(bool forceSolvable, bool forceUnsolvable)
{
    (void)forceSolvable;          /* 统一由 generateCards 保证题目性质 */

    generateCards(g.cards, g.suits, forceUnsolvable ? false : true);

    /* 参考答案优先给整数解法，实在没有才退回完整搜索 */
    string ans;
    if (solve24(g.cards, ans, true)) {
        g.solvable = true;
        strcpy(g.refAnswer, ans.c_str());
    } else if (solve24(g.cards, ans, false)) {
        g.solvable = true;
        strcpy(g.refAnswer, ans.c_str());
    } else {
        g.solvable     = false;
        g.refAnswer[0] = '\0';
    }

    const Difficulty &d = LEVELS[g.diff];
    g.limitMs    = d.limitSec * 1000;
    g.remainMs   = g.limitMs;
    g.warnPlayed = false;
    g.roundStart = GetTickCount();
    g.subState   = GS_INPUT;
    g.noAnswer   = false;
    g.feedback[0] = '\0';
    g.fbColor    = COL_TEXT;
    g.dealt      = 0;
    g.animTick   = GetTickCount();

    if (g_edit != NULL) {
        SetWindowTextA(g_edit, "");
        EnableWindow(g_edit, TRUE);
        ShowWindow(g_edit, SW_HIDE);        /* 发完牌再出现，先显示“正在发牌” */
        layoutEdit();
    }

    SetTimer(g_hwnd, 1, 60, NULL);
    repaint();
}

static void startGame()
{
    g.round      = 1;
    g.totalScore = 0;
    g.totalMs    = 0;
    g.rankPos    = 0;
    g.newBest    = false;
    for (int i = 0; i < ROUND_COUNT; ++i) {
        g.roundScore[i] = 0;
        g.roundMs[i]    = 0;
        g.roundRes[i]   = RES_WRONG;
    }
    g.screen = SCR_GAME;
    newRound(false, rand() % 3 == 0);
}

static void finishRound(int result, int score, int usedMs)
{
    g.roundRes[g.round - 1]   = result;
    g.roundScore[g.round - 1] = score;
    g.roundMs[g.round - 1]    = usedMs;
    g.totalScore += score;
    g.totalMs    += usedMs;

    g.subState = GS_RESULT;
    stopTimer();

    if (g_edit != NULL) {
        EnableWindow(g_edit, FALSE);
        ShowWindow(g_edit, SW_HIDE);   /* 结果区不需要输入框 */
    }

    if (result == RES_RIGHT || result == RES_NOANS_OK) playSfx(SND_RIGHT);
    else                                              playSfx(SND_WRONG);
}

static void gotoSummary()
{
    g.screen = SCR_SUMMARY;
    stopTimer();

    bool allRight = true;
    for (int i = 0; i < ROUND_COUNT; ++i) {
        if (g.roundRes[i] != RES_RIGHT && g.roundRes[i] != RES_NOANS_OK)
            allRight = false;
    }
    playSfx(allRight ? SND_ALLDONE : SND_SUMMARY);

    if (g.totalScore > 0) {
        Record rec;
        strcpy(rec.name, g.player);
        rec.score        = g.totalScore;
        rec.totalSeconds = (g.totalMs + 999) / 1000;
        strcpy(rec.level, LEVELS[g.diff].name);
        todayString(rec.date, sizeof(rec.date));

        g.recCount = loadRecords(g.recs, MAX_RECORDS);
        g.rankPos  = insertRecord(g.recs, g.recCount, rec, g.newBest);
        g.recCount = loadRecords(g.recs, MAX_RECORDS);
    } else {
        g.recCount = loadRecords(g.recs, MAX_RECORDS);
    }
}

static void doSubmit()
{
    if (g.subState != GS_INPUT) return;
    if (g.dealt < CARD_COUNT) return;

    int  usedMs = g.limitMs - g.remainMs;
    if (usedMs < 0) usedMs = 0;
    int  remainSec = (g.limitMs - usedMs + 999) / 1000;
    const Difficulty &d = LEVELS[g.diff];

    if (g.noAnswer) {
        if (!g.solvable) {
            int sc = calculateScore(d, remainSec);
            snprintf(g.feedback, sizeof(g.feedback), "判断正确！本题无解，得 %d 分", sc);
            g.fbColor = COL_GREEN;
            finishRound(RES_NOANS_OK, sc, usedMs);
        } else {
            snprintf(g.feedback, sizeof(g.feedback),
                     "判断错误！本题有解：%s", g.refAnswer);
            g.fbColor = COL_RED;
            finishRound(RES_NOANS_BAD, 0, usedMs);
        }
        repaint();
        return;
    }

    char text[256] = "";
    if (g_edit != NULL) GetWindowTextA(g_edit, text, sizeof(text));

    char msg[128];
    Fraction v;

    if (g.solvable == false) {
        /* 本题本来就无解：任何算式都不可能成立 */
        if (!evaluateExpression(text, v, msg)) {
            snprintf(g.feedback, sizeof(g.feedback), "%s", msg);
            g.fbColor = COL_RED;
            repaint();
            return;
        }
        snprintf(g.feedback, sizeof(g.feedback),
                 "本题其实无解，应当勾选「本题无解」。本题 0 分");
        g.fbColor = COL_RED;
        finishRound(RES_NOANS_BAD, 0, usedMs);
        repaint();
        return;
    }

    if (!evaluateExpression(text, v, msg)) {
        snprintf(g.feedback, sizeof(g.feedback), "%s", msg);
        g.fbColor = COL_RED;
        repaint();
        return;                        /* 格式错误：不扣分，可重试       */
    }

    if (!sameNumberSet(text, g.cards)) {
        snprintf(g.feedback, sizeof(g.feedback),
                 "表达式必须恰好使用这 4 个数字各一次");
        g.fbColor = COL_RED;
        repaint();
        return;
    }

    if (isTwentyFour(v)) {
        int sc = calculateScore(d, remainSec);
        snprintf(g.feedback, sizeof(g.feedback), "回答正确！本题得 %d 分", sc);
        g.fbColor = COL_GREEN;
        finishRound(RES_RIGHT, sc, usedMs);
    } else {
        char disp[64];
        if (v.den == 1) snprintf(disp, sizeof(disp), "%lld", v.num);
        else            snprintf(disp, sizeof(disp), "%lld/%lld", v.num, v.den);
        snprintf(g.feedback, sizeof(g.feedback),
                 "结果等于 %s，不是 24。参考答案：%s", disp, g.refAnswer);
        g.fbColor = COL_RED;
        finishRound(RES_WRONG, 0, usedMs);
    }
    repaint();
}

static void doNext()
{
    if (g.round >= ROUND_COUNT) {
        gotoSummary();
        repaint();
        return;
    }
    g.round++;
    newRound(false, rand() % 3 == 0);
}

#define WM_APP_SUBMIT (WM_APP + 1)

static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) {
            PostMessageA(g_hwnd, WM_APP_SUBMIT, 0, 0);
            return 0;
        }
        if (wp == VK_ESCAPE) {              /* 输入框里按 Esc 也能返回 */
            PostMessageA(g_hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            return 0;
        }
        if (wp == 'M' || wp == 'm') {       /* 输入框里也能静音 */
            PostMessageA(g_hwnd, WM_KEYDOWN, 'M', 0);
            return 0;
        }
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == 27)) return 0;
    return CallWindowProcA(g_editOld, hwnd, msg, wp, lp);
}

static void createEdit()
{
    if (g_edit != NULL) return;

    g_edit = CreateWindowExA(
        0, "EDIT", "",
        WS_CHILD | ES_LEFT | ES_AUTOHSCROLL,      /* 先不显示，定位好再显示 */
        0, 0, 100, 30, g_hwnd, (HMENU)9001, GetModuleHandleA(NULL), NULL);

    if (g_edit == NULL) return;

    SendMessageA(g_edit, WM_SETFONT, (WPARAM)F_EDIT, TRUE);
    SendMessageA(g_edit, EM_SETLIMITTEXT, 200, 0);
    SendMessageA(g_edit, EM_SETMARGINS, 3, (8 | (8 << 16)));   /* 左右留白 */
    g_editOld = (WNDPROC)SetWindowLongPtrA(g_edit, GWLP_WNDPROC,
                                           (LONG_PTR)EditProc);

    layoutEdit();                             /* 先摆到正确位置 */
    ShowWindow(g_edit, SW_SHOW);              /* 再显示，避免在 (0,0) 闪一下 */
}

static void destroyEdit()
{
    if (g_edit != NULL) {
        DestroyWindow(g_edit);
        g_edit = NULL;
        g_editOld = NULL;
    }
}

static void layoutEdit()
{
    if (g_edit == NULL || g_hwnd == NULL) return;

    RECT rc;
    GetClientRect(g_hwnd, &rc);

    double s  = viewScale(rc.right, rc.bottom);
    int    dw = (int)(DES_W * s + 0.5);
    int    dh = (int)(DES_H * s + 0.5);
    int    ox = (rc.right - dw) / 2;
    int    oy = (rc.bottom - dh) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    int ex = 306, ey = 522, ew = 448, eh = 36;   /* 默认：对局输入框 */
    if (g.screen == SCR_NAME) { ex = 422; ey = 310; ew = 436; eh = 36; }

    MoveWindow(g_edit,
               ox + (int)(ex * s), oy + (int)(ey * s),
               (int)(ew * s), (int)(eh * s), TRUE);

    /* 输入框挪走后，它原来盖住的地方要重画一次 */
    repaint();
}

static void enterScreen(int screen)
{
    g.screen = screen;
    g_hover  = 0;
    g_press  = 0;
    g_hoverSlot = -1;
    g_pressSlot = -1;

    if (screen == SCR_GAME || screen == SCR_NAME) createEdit();
    else                                          destroyEdit();

    layoutEdit();
    repaint();
}

/* 去掉字符串首尾空格 */
static void trimInPlace(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';

    int i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

/* 进入正式对局（姓名已确定） */
static void beginNewGame()
{
    g.screen = SCR_GAME;
    createEdit();
    startGame();
    layoutEdit();
    startBgm();            /* 一整局开始：背景音乐从头循环 */
    repaint();
}

/* 姓名输入框的「确定」 */
static void doNameOk()
{
    if (g.screen != SCR_NAME) return;

    char buf[MAX_NAME] = "";
    if (g_edit != NULL) GetWindowTextA(g_edit, buf, sizeof(buf));
    buf[MAX_NAME - 1] = '\0';
    trimInPlace(buf);

    if (buf[0] == '\0') strcpy(g.player, "玩家");
    else                strcpy(g.player, buf);

    if (g_nameFromMenu) {                 /* 从「玩家档案」进来：只改名 */
        g_nameFromMenu = false;
        enterScreen(SCR_MENU);
        return;
    }

    beginNewGame();
}

/* 主菜单点「开始对局」：先来一声“叮叮叮”，没输过名字再问名字 */
static void startGameFlow()
{
    playSfx(SND_DING);

    g_nameFromMenu = false;

    if (g.player[0] == '\0') {
        enterScreen(SCR_NAME);
        if (g_edit != NULL) {
            SetWindowTextA(g_edit, "");
            SetFocus(g_edit);
        }
    } else {
        beginNewGame();
    }
}

static void toggleSound()
{
    g_sfxOn = !g_sfxOn;

    if (g_sfxOn) {
        if (g.screen == SCR_GAME) startBgm();
    } else {
        stopSfx();
        stopBgm();
    }

    HMENU menu = GetMenu(g_hwnd);
    if (menu != NULL)
        CheckMenuItem(menu, CMD_SFX, g_sfxOn ? MF_CHECKED : MF_UNCHECKED);

    repaint();
}

static void showAbout()
{
    MessageBoxA(g_hwnd,
                "计算二十四点\n\n"
                "Windows 图形界面版（Win32 API + GDI 自绘，无第三方库）\n"
                "随机发牌 · 限时挑战 · 精确分数运算 · 排行榜 · 语音音效\n\n"
                "界面背景：粉色楼阁图片\n\n"
                "南京邮电大学  程序设计课程设计",
                "关于 计算二十四点", MB_OK | MB_ICONINFORMATION);
}

static void doCommand(int id)
{
    switch (id) {
    case ID_M_START:
    case CMD_START:
        startGameFlow();
        break;

    case ID_M_DIFF:
    case CMD_DIFF:
        enterScreen(SCR_DIFF);
        break;

    case ID_M_RANK:
    case ID_I_RANK:
    case CMD_RANK:
        g.recCount = loadRecords(g.recs, MAX_RECORDS);
        enterScreen(SCR_RANK);
        break;

    case ID_M_RULES:
    case ID_I_RULES:
    case CMD_RULES:
        enterScreen(SCR_RULES);
        break;

    case ID_M_NAME:
    case ID_I_NAME:
        g_nameFromMenu = true;
        enterScreen(SCR_NAME);
        if (g_edit != NULL) {
            SetWindowTextA(g_edit, g.player);
            SetFocus(g_edit);
        }
        break;

    case ID_M_EXIT:
    case CMD_EXIT:
        DestroyWindow(g_hwnd);
        break;

    case ID_M_ABOUT:
    case CMD_ABOUT:
        showAbout();
        break;

    case ID_M_SFX:
    case ID_I_SFX:
    case CMD_SFX:
        toggleSound();
        break;

    case ID_D_OK:
        enterScreen(SCR_MENU);
        break;

    case ID_D_0:
    case ID_D_1:
    case ID_D_2:
        g.diff = id - ID_D_0;
        repaint();
        break;

    case ID_D_BACK:
    case ID_R_BACK:
    case ID_U_BACK:
        enterScreen(SCR_MENU);
        break;

    case ID_N_OK:
        doNameOk();
        break;

    case ID_N_SKIP:
        strcpy(g.player, "玩家");
        if (g_nameFromMenu) {
            g_nameFromMenu = false;
            enterScreen(SCR_MENU);
        } else {
            beginNewGame();
        }
        break;

    case ID_N_BACK:
        g_nameFromMenu = false;
        enterScreen(SCR_MENU);
        break;

    case ID_G_SUBMIT:
        doSubmit();
        break;

    case ID_G_NOANS:
        g.noAnswer = !g.noAnswer;
        repaint();
        break;

    case ID_G_NEXT:
        doNext();
        break;

    case ID_G_BACK:
    case ID_G_BACK2:
        stopTimer();
        stopBgm();
        destroyEdit();
        enterScreen(SCR_MENU);
        break;

    case ID_S_AGAIN:
        beginNewGame();
        break;

    case ID_S_RANK:
        stopBgm();
        g.recCount = loadRecords(g.recs, MAX_RECORDS);
        enterScreen(SCR_RANK);
        break;

    case ID_S_MENU:
    case ID_S_BACK:
        stopBgm();
        enterScreen(SCR_MENU);
        break;
    }
}

/* 把客户区像素坐标换算成设计坐标（考虑居中偏移与缩放） */
static void clientToDesign(int px, int py, int &dx, int &dy)
{
    RECT rc;
    GetClientRect(g_hwnd, &rc);

    double s  = viewScale(rc.right, rc.bottom);
    int    dw = (int)(DES_W * s + 0.5);
    int    dh = (int)(DES_H * s + 0.5);
    int    ox = (rc.right - dw) / 2;
    int    oy = (rc.bottom - dh) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    dx = (int)((px - ox) / s);
    dy = (int)((py - oy) / s);
}

static void onMouseMove(int x, int y)
{
    int n = 0;
    const BtnDesc *bs = curButtons(n);
    int hit = 0;
    int slot = -1;

    for (int i = 0; i < n; ++i) {
        if (x >= bs[i].x && x < bs[i].x + bs[i].w &&
            y >= bs[i].y && y < bs[i].y + bs[i].h) {
            hit = bs[i].id;
            slot = i;
            break;
        }
    }

    if (hit != g_hover || slot != g_hoverSlot) {
        g_hover = hit;
        g_hoverSlot = slot;
        repaint();
    }
}

static void onLButtonDown(int x, int y)
{
    int n = 0;
    const BtnDesc *bs = curButtons(n);

    for (int i = 0; i < n; ++i) {
        if (x >= bs[i].x && x < bs[i].x + bs[i].w &&
            y >= bs[i].y && y < bs[i].y + bs[i].h) {
            g_press = bs[i].id;
            g_pressSlot = i;
            repaint();
            return;
        }
    }
    g_press = 0;
    g_pressSlot = -1;
}

static void onLButtonUp(int x, int y)
{
    int n = 0;
    const BtnDesc *bs = curButtons(n);
    int id = 0;

    for (int i = 0; i < n; ++i) {
        if (x >= bs[i].x && x < bs[i].x + bs[i].w &&
            y >= bs[i].y && y < bs[i].y + bs[i].h) {
            id = bs[i].id;
            break;
        }
    }

    int pressed = g_press;
    int pressedSlot = g_pressSlot;
    g_press = 0;
    g_pressSlot = -1;

    if (id != 0 && id == pressed && pressedSlot >= 0) doCommand(id);
    else repaint();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == 'M' || wp == 'm') { toggleSound(); return 0; }
        if (wp == VK_ESCAPE) {
            if (g.screen == SCR_GAME || g.screen == SCR_SUMMARY) {
                stopTimer();
                stopBgm();
                destroyEdit();
                enterScreen(SCR_MENU);
            } else if (g.screen != SCR_MENU) {
                g_nameFromMenu = false;
                enterScreen(SCR_MENU);
            }
            return 0;
        }
        break;

    case WM_CREATE:
        g_hwnd = hwnd;
        g.recCount = loadRecords(g.recs, MAX_RECORDS);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int cw = rc.right, ch = rc.bottom;

        double s  = viewScale(cw, ch);
        int    dw = (int)(DES_W * s + 0.5);
        int    dh = (int)(DES_H * s + 0.5);
        if (dw < 1)  dw = 1;
        if (dh < 1)  dh = 1;
        if (dw > cw) dw = cw;
        if (dh > ch) dh = ch;
        int ox = (cw - dw) / 2;
        int oy = (ch - dh) / 2;

        /* 只填画布外的留白，画布本身由背景位图覆盖 */
        if (ox > 0 || oy > 0) {
            HBRUSH bg = CreateSolidBrush(RGB(8, 9, 12));
            if (ox > 0) {
                RECT a = { 0, 0, ox, ch };
                RECT b = { ox + dw, 0, cw, ch };
                FillRect(hdc, &a, bg);
                FillRect(hdc, &b, bg);
            }
            if (oy > 0) {
                RECT a = { 0, 0, cw, oy };
                RECT b = { 0, oy + dh, cw, ch };
                FillRect(hdc, &a, bg);
                FillRect(hdc, &b, bg);
            }
            DeleteObject(bg);
        }

        HDC mem = getMemCanvas(hdc, dw, dh);

        drawCanvas(mem, dw, dh);
        BitBlt(hdc, ox, oy, dw, dh, mem, 0, 0, SRCCOPY);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
        doCommand((int)LOWORD(wp));
        return 0;

    case WM_APP_SUBMIT:
        if (g.screen == SCR_NAME) doNameOk();
        else                      doSubmit();
        return 0;

    case MM_MCINOTIFY:
        /* 音效播完（MCI_NOTIFY_SUCCESSFUL）→ 继续放背景音乐 */
        if (wp == MCI_NOTIFY_SUCCESSFUL) resumeBgm();
        return 0;

    case WM_TIMER: {
        if (g.screen != SCR_GAME) { KillTimer(hwnd, wp); return 0; }

        if (g.dealt < CARD_COUNT) {
            DWORD now = GetTickCount();
            if (now - g.animTick >= 90) {
                g.animTick = now;
                g.dealt++;
                if (g.dealt >= CARD_COUNT && g_edit != NULL) {
                    ShowWindow(g_edit, SW_SHOW);
                    SetFocus(g_edit);
                }
            }
            repaint();
            return 0;
        }

        if (g.subState == GS_INPUT) {
            DWORD used = GetTickCount() - g.roundStart;
            int   remain = g.limitMs - (int)used;
            if (remain < 0) remain = 0;

            if (remain != g.remainMs) {
                g.remainMs = remain;

                if (!g.warnPlayed && g.remainMs > 0 &&
                    g.remainMs * 2 <= g.limitMs) {
                    g.warnPlayed = true;
                    playSfx(SND_HURRY);
                }

                /* 只失效倒计时这一条，避免整屏重绘 */
                repaintDesignRect(28, 84, 1224, 64);
            }

            if (remain == 0) {
                stopTimer();
                g.remainMs = 0;
                snprintf(g.feedback, sizeof(g.feedback),
                         "时间到！本题超时。参考答案：%s",
                         g.solvable ? g.refAnswer : "本题无解");
                g.fbColor = COL_RED;
                finishRound(RES_TIMEOUT, 0, g.limitMs);
                repaint();
            }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        int dx = 0, dy = 0;
        clientToDesign((short)LOWORD(lp), (short)HIWORD(lp), dx, dy);
        onMouseMove(dx, dy);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int dx = 0, dy = 0;
        clientToDesign((short)LOWORD(lp), (short)HIWORD(lp), dx, dy);
        onLButtonDown(dx, dy);
        return 0;
    }

    case WM_LBUTTONUP: {
        int dx = 0, dy = 0;
        clientToDesign((short)LOWORD(lp), (short)HIWORD(lp), dx, dy);
        onLButtonUp(dx, dy);
        return 0;
    }

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g_hover != 0) {
            SetCursor(LoadCursorA(NULL, (LPCSTR)IDC_HAND));
            return TRUE;
        }
        break;

    case WM_SIZE:
        layoutEdit();
        repaint();
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 1000;
        mm->ptMinTrackSize.y = 700;
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, P_INK);
        SetBkColor(dc, P_WHITE);
        return (LRESULT)g_brEdit;
    }

    case WM_DESTROY:
        stopTimer();
        freeMemCanvas();
        shutdownAudio();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HFONT mkFont(int h, int weight, const char *face)
{
    return CreateFontA(-h, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

void initFonts()
{
    F_HERO     = mkFont(40, FW_SEMIBOLD, "微软雅黑");
    F_H1       = mkFont(32, FW_SEMIBOLD, "微软雅黑");
    F_H2       = mkFont(26, FW_SEMIBOLD, "微软雅黑");
    F_H3       = mkFont(20, FW_SEMIBOLD, "微软雅黑");
    F_BODY     = mkFont(16, FW_NORMAL,   "微软雅黑");
    F_SMALL    = mkFont(12, FW_NORMAL,   "微软雅黑");
    F_TINY     = mkFont(10, FW_NORMAL,   "微软雅黑");
    F_CAP      = mkFont(10, FW_SEMIBOLD, "Bahnschrift");
    F_NUM      = mkFont(22, FW_NORMAL,   "Bahnschrift");
    F_NUM_M    = mkFont(26, FW_SEMIBOLD, "Bahnschrift");
    F_NUM_L    = mkFont(38, FW_SEMIBOLD, "Bahnschrift");
    F_NUM_XL   = mkFont(64, FW_SEMIBOLD, "Bahnschrift");
    F_NUM_XXL  = mkFont(104, FW_BOLD,    "Bahnschrift");
    F_CARD_NUM = mkFont(76, FW_BOLD,     "Bahnschrift");
    F_VERT     = mkFont(48, FW_SEMIBOLD, "微软雅黑");
    F_WATER    = mkFont(250, FW_BOLD,    "Bahnschrift");
    F_MONO     = mkFont(24, FW_BOLD,     "Consolas");

    F_SUIT_S = CreateFontW(-26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           L"Segoe UI Symbol");
    F_SUIT_M = CreateFontW(-38, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           L"Segoe UI Symbol");
    F_SUIT_L = CreateFontW(-50, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           L"Segoe UI Symbol");

    F_EDIT = CreateFontA(-26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                         "Consolas");

    g_brEdit = CreateSolidBrush(P_WHITE);
}

void freeFonts()
{
    HFONT *f[] = { &F_HERO, &F_H1, &F_H2, &F_H3, &F_BODY, &F_SMALL, &F_TINY,
                   &F_CAP, &F_NUM, &F_NUM_M, &F_NUM_L, &F_NUM_XL, &F_NUM_XXL,
                   &F_CARD_NUM, &F_VERT, &F_WATER, &F_MONO, &F_EDIT,
                   &F_SUIT_S, &F_SUIT_M, &F_SUIT_L };
    for (int i = 0; i < (int)(sizeof(f) / sizeof(f[0])); ++i) {
        if (*f[i] != NULL) { DeleteObject(*f[i]); *f[i] = NULL; }
    }
    if (g_brEdit != NULL) { DeleteObject(g_brEdit); g_brEdit = NULL; }
}

void resetState()
{
    memset(&g, 0, sizeof(g));
    g.screen   = SCR_MENU;
    g.diff     = 1;
    g.fbColor  = COL_TEXT;
    g.dealt    = 4;
    g.subState = GS_INPUT;
    g_hover    = 0;
    g_press    = 0;
    g_hoverSlot = -1;
    g_pressSlot = -1;
    g_nameFromMenu = false;
}
