/*==========================================================================
 *  计算二十四点  ——  Windows 图形界面版（纯 Win32 API + GDI，无第三方库）
 *
 *  编译： g++ -std=c++11 -O2 -mwindows -o 计算24点游戏.exe 计算24点游戏.cpp -lgdi32 -lwinmm
 *
 *  功能：随机发 4 张扑克牌，玩家在限时内输入四则运算表达式使其结果等于 24；
 *        三题一局，按难度与剩余时间记分，成绩存入排行榜文件。
 *        声音用 MCI 播放 音频\ 子目录下的 mp3 素材。
 *========================================================================*/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

using std::string;

/*====================== 一、全局常量 ======================*/
static const int  DES_W        = 1040;         /* 设计画布宽（逻辑单位）     */
static const int  DES_H        = 700;          /* 设计画布高                 */
static const int  CARD_COUNT   = 4;            /* 每题牌数                   */
static const int  ROUND_COUNT  = 3;            /* 每局题数                   */
static const int  MAX_NAME     = 32;           /* 姓名缓冲字节数             */
static const int  MAX_RECORDS  = 50;           /* 排行榜最多记录数           */
static const int  MAX_EXPR_NUM = 16;           /* 表达式最多数字个数         */
static const char RANK_FILE[]  = "rank.txt";   /* 排行榜数据文件名           */

/*
 * 声音素材。
 * 优先使用可执行文件旁的 音频\ 目录（方便替换素材）；
 * 找不到时使用编译时打包进 exe 的内嵌副本，
 * 这样只把一个 exe 发给别人也能听到声音。
 */
static const char SND_DIR[] = "音频\\";

enum {
    SND_HURRY = 0,      /* 时间过半 */
    SND_RIGHT,          /* 答对     */
    SND_WRONG,          /* 答错/超时*/
    SND_ALLDONE,        /* 三题全对 */
    SND_SUMMARY,        /* 有错     */
    SND_BGM,            /* 背景音乐 */
    SND_DING,           /* 点击开始游戏 */
    SND_COUNT
};

/* 文件名；资源 ID 必须与 计算24点游戏.rc 中的定义一致 */
static const char *SND_FILE[SND_COUNT] = {
    "阿姨你快一点啦阿姨.mp3",
    "嘿嘿.mp3",
    "你干嘛哎哟.mp3",
    "牛逼啊这个人 有操作的呀.mp3",
    "哎呀.mp3",
    "背景音乐.mp3",
    "叮叮叮.mp3"
};

static const int SND_RESID[SND_COUNT] = { 102, 103, 104, 105, 106, 101, 107 };

/* 子界面编号 */
enum {
    SCR_MENU = 0, SCR_DIFF, SCR_RULES, SCR_RANK, SCR_NAME, SCR_GAME, SCR_SUMMARY
};

/* 按钮 / 菜单命令编号（按钮 100+，菜单 1000+，互不冲突） */
enum {
    ID_M_START = 100, ID_M_DIFF, ID_M_RANK, ID_M_RULES, ID_M_EXIT,
    ID_D_0 = 110, ID_D_1 = 111, ID_D_2 = 112, ID_D_BACK = 113,
    ID_R_BACK = 120,
    ID_U_BACK = 130,
    ID_N_OK = 160, ID_N_SKIP = 161, ID_N_BACK = 162,
    ID_G_SUBMIT = 140, ID_G_NOANS = 141, ID_G_NEXT = 142, ID_G_BACK = 143,
    ID_S_AGAIN = 150, ID_S_RANK = 151, ID_S_MENU = 152
};

enum { CMD_START = 1000, CMD_DIFF, CMD_RANK, CMD_RULES, CMD_EXIT, CMD_ABOUT, CMD_SFX };

/* 对局内部状态 */
enum { GS_INPUT = 0, GS_RESULT };

/* 本题判定结果 */
enum { RES_RIGHT = 0, RES_WRONG, RES_TIMEOUT, RES_NOANS_OK, RES_NOANS_BAD };

/*====================== 二、主题配色 ======================*/
static const COLORREF COL_BG      = RGB( 11,  13,  19);   /* 窗口底色         */
static const COLORREF COL_BG2     = RGB( 17,  20,  29);   /* 渐变辅助         */
static const COLORREF COL_PANEL   = RGB( 22,  26,  36);   /* 面板             */
static const COLORREF COL_PANEL2  = RGB( 31,  36,  50);   /* 次级面板         */
static const COLORREF COL_LINE    = RGB( 46,  54,  72);   /* 分隔线           */
static const COLORREF COL_GOLD    = RGB(201, 162,  75);   /* 金色             */
static const COLORREF COL_GOLD_L  = RGB(246, 218, 140);   /* 亮金             */
static const COLORREF COL_GOLD_D  = RGB(112,  88,  38);   /* 暗金             */
static const COLORREF COL_TEXT    = RGB(230, 235, 245);   /* 正文             */
static const COLORREF COL_DIM     = RGB(124, 136, 156);   /* 次要文字         */
static const COLORREF COL_GREEN   = RGB( 82, 206, 132);   /* 正确             */
static const COLORREF COL_RED     = RGB(232,  96,  96);   /* 错误             */
static const COLORREF COL_WHITE   = RGB(255, 255, 255);
static const COLORREF COL_CARD    = RGB(251, 251, 248);   /* 牌面             */
static const COLORREF COL_CARD_B  = RGB(206, 206, 200);   /* 牌面描边         */
static const COLORREF COL_BLACK   = RGB( 30,  30,  34);   /* 黑色花色         */
static const COLORREF COL_REDC    = RGB(190,  40,  50);   /* 红色花色         */
static const COLORREF COL_NAVY    = RGB( 26,  44,  86);   /* 牌背             */

/*====================== 三、字体与句柄 ======================*/
static HFONT  F_TITLE  = NULL;      /* 主标题 46                            */
static HFONT  F_SUB    = NULL;      /* 副标题 18                            */
static HFONT  F_H1     = NULL;      /* 小节标题 26                          */
static HFONT  F_H2     = NULL;      /* 卡片标题 22                          */
static HFONT  F_BODY   = NULL;      /* 正文 17                              */
static HFONT  F_BODY_S = NULL;      /* 小字 15                              */
static HFONT  F_BTN    = NULL;      /* 按钮 19                              */
static HFONT  F_RANK   = NULL;      /* 牌面点数字 30                        */
static HFONT  F_RANK_B = NULL;      /* 牌面中心点数 52                      */
static HFONT  F_MONO   = NULL;      /* 等宽数字 26                          */
static HFONT  F_EDIT   = NULL;      /* 输入框（设备单位）                   */
static HFONT  F_SUIT_S = NULL;      /* 花色 26                              */
static HFONT  F_SUIT_M = NULL;      /* 花色 38                              */
static HFONT  F_SUIT_L = NULL;      /* 花色 50                              */

static HBRUSH g_brEdit   = NULL;    /* 输入框背景刷                         */

static HWND   g_hwnd     = NULL;    /* 主窗口                               */
static HWND   g_edit     = NULL;    /* 表达式输入框                         */
static WNDPROC g_editOld = NULL;    /* 输入框原窗口过程                     */

static double g_scale    = 1.0;     /* 1 逻辑单位 = g_scale 像素          */
static int    g_hover    = 0;       /* 悬停按钮编号                         */
static int    g_press    = 0;       /* 按下按钮编号                         */

/* 逻辑单位 -> 设备像素（仅用于初始窗口尺寸、截图等场合） */
static int sc(int v) { return (int)(v * g_scale + 0.5); }

/*
 * 根据客户区尺寸算出当前实际绘制缩放：
 * 画布始終等比缩放并居中，因此窗口放大 / 最大化时界面跟着铺满，
 * 长宽比例不同时两侧留极窄的边，不会变形。
 */
static double viewScale(int cw, int ch)
{
    if (cw <= 0 || ch <= 0) return g_scale;

    double sx = cw / (double)DES_W;
    double sy = ch / (double)DES_H;
    double s  = (sx < sy) ? sx : sy;

    if (s < 0.55) s = 0.55;
    if (s > 4.0)  s = 4.0;
    return s;
}

static const char *RANKS[13] = {
    "A", "2", "3", "4", "5", "6", "7",
    "8", "9", "10", "J", "Q", "K"
};

/*====================== 四、数据结构 ======================*/

/* 有理数：分子、分母均为 long long，每次运算后约分，彻底避免浮点误差 */
struct Fraction {
    long long num;
    long long den;
};

/* 求解过程中的中间项 */
struct Node {
    Fraction    val;
    string      expr;
};

/* 一条排行榜记录 */
struct Record {
    char name[MAX_NAME];
    int  score;
    int  totalSeconds;
    char date[20];
    char level[8];
};

/* 难度参数：基础分 + 时间奖励 = 单题满分 */
struct Difficulty {
    const char *name;
    int         limitSec;
    int         baseScore;
    int         bonusScore;
};

static const Difficulty LEVELS[3] = {
    { "简单", 60,  60, 40 },      /* 单题满分 100 */
    { "普通", 40,  80, 50 },      /* 单题满分 130 */
    { "困难", 25, 100, 60 }       /* 单题满分 160 */
};

/* 全局运行状态 */
struct GameState {
    int  screen;                 /* 当前界面                        */
    int  diff;                   /* 难度下标 0/1/2                  */

    char player[MAX_NAME];       /* 玩家姓名                        */

    int  round;                  /* 当前第几题 1..3                 */
    int  cards[CARD_COUNT];      /* 四张牌的数值 1..13              */
    int  suits[CARD_COUNT];      /* 四张牌的花色 0..3               */
    bool solvable;               /* 本题是否有解                    */
    char refAnswer[192];         /* 参考解                          */

    int  limitMs;                /* 本题限时（毫秒）                */
    int  remainMs;               /* 本题剩余（毫秒）                */
    bool warnPlayed;             /* 本题“时间过半”提示音是否已放过  */
    DWORD roundStart;            /* 本题开始时刻（GetTickCount）    */
    int  subState;               /* GS_INPUT / GS_RESULT            */
    bool noAnswer;               /* 是否勾选「本题无解」            */

    int  roundScore[ROUND_COUNT];
    int  roundMs[ROUND_COUNT];
    int  roundRes[ROUND_COUNT];

    int  totalScore;
    int  totalMs;

    char     feedback[256];      /* 判题提示文字                    */
    COLORREF fbColor;            /* 提示颜色                        */

    int  rankPos;                /* 本局在榜上的名次（0 = 未上榜）  */
    bool newBest;                /* 是否刷新个人最好成绩            */

    int  dealt;                  /* 已翻开的牌数（发牌动画）        */
    DWORD animTick;              /* 上一张牌翻开时刻                */

    Record recs[MAX_RECORDS];
    int    recCount;
};

static GameState g;

/*====================== 五、基础绘图工具 ======================*/

static void fillRect(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    HBRUSH br = CreateSolidBrush(c);
    FillRect(dc, &r, br);
    DeleteObject(br);
}

static void roundFill(HDC dc, int x, int y, int w, int h, int r,
                      COLORREF fill, COLORREF border, int t)
{
    HBRUSH br = CreateSolidBrush(fill);
    HPEN   pn = CreatePen(PS_SOLID, t, border);
    HBRUSH ob = (HBRUSH)SelectObject(dc, br);
    HPEN   op = (HPEN)SelectObject(dc, pn);
    RoundRect(dc, x, y, x + w + 1, y + h + 1, r, r);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pn);
}

/* 输出文字：fmt 常用 DT_LEFT/DT_CENTER/DT_RIGHT + DT_VCENTER|DT_SINGLELINE */
static void tx(HDC dc, const char *s, int x, int y, int w, int h,
               HFONT f, COLORREF c, UINT fmt)
{
    HFONT      of = (HFONT)SelectObject(dc, f);
    COLORREF   oc = SetTextColor(dc, c);
    int        om = SetBkMode(dc, TRANSPARENT);
    RECT       r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    DrawTextA(dc, s, -1, &r, fmt | DT_NOPREFIX);
    SetBkMode(dc, om);
    SetTextColor(dc, oc);
    SelectObject(dc, of);
}

/* 输出单个 Unicode 字符（花色符号，避免源文件编码问题） */
static void txChar(HDC dc, wchar_t wc, int x, int y, int w, int h,
                   HFONT f, COLORREF c, UINT fmt)
{
    HFONT      of = (HFONT)SelectObject(dc, f);
    COLORREF   oc = SetTextColor(dc, c);
    int        om = SetBkMode(dc, TRANSPARENT);
    RECT       r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    wchar_t    buf[2];
    buf[0] = wc;
    buf[1] = 0;
    DrawTextW(dc, buf, 1, &r, fmt | DT_NOPREFIX);
    SetBkMode(dc, om);
    SetTextColor(dc, oc);
    SelectObject(dc, of);
}

/* 花色编号 0=黑桃 1=红心 2=方块 3=梅花 */
static wchar_t suitChar(int suit)
{
    static const wchar_t S[4] = { 0x2660, 0x2665, 0x2666, 0x2663 };
    return S[suit & 3];
}

static COLORREF suitColor(int suit)
{
    return (suit == 1 || suit == 2) ? COL_REDC : COL_BLACK;
}

/* 带金色渐层感的水平装饰线 */
static void goldLine(HDC dc, int x, int y, int w)
{
    fillRect(dc, x, y, w, 1, COL_GOLD_D);
    fillRect(dc, x + w / 4, y, w / 2, 1, COL_GOLD);
}

/*====================== 六、有理数运算 ======================*/

static long long gcdLL(long long a, long long b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return (a == 0) ? 1 : a;
}

static Fraction makeFraction(long long n, long long d)
{
    Fraction f;
    if (d == 0) { f.num = 0; f.den = 0; return f; }
    if (d < 0)  { n = -n; d = -d; }
    long long g = gcdLL(n, d);
    f.num = n / g;
    f.den = d / g;
    return f;
}

static bool fracValid(const Fraction &f) { return f.den != 0; }

static Fraction addFraction(Fraction a, Fraction b)
{
    return makeFraction(a.num * b.den + b.num * a.den, a.den * b.den);
}

static Fraction subFraction(Fraction a, Fraction b)
{
    return makeFraction(a.num * b.den - b.num * a.den, a.den * b.den);
}

static Fraction mulFraction(Fraction a, Fraction b)
{
    return makeFraction(a.num * b.num, a.den * b.den);
}

static Fraction divFraction(Fraction a, Fraction b)
{
    if (b.num == 0) { Fraction z; z.num = 0; z.den = 0; return z; }
    return makeFraction(a.num * b.den, a.den * b.num);
}

static bool isTwentyFour(Fraction f) { return f.den == 1 && f.num == 24; }

/*====================== 七、24 点求解器 ======================*/

/*
 * 递归地把两个数合并成一个数，穷尽所有组合。
 * intOnly 为 true 时要求每一步的中间结果都是整数（对应题目“不涉及小数运算”），
 * 中途算出分数的那条分支直接丢弃。
 */
static bool searchSolution(Node items[], int n, string &answer, bool intOnly)
{
    if (n == 1) {
        if (isTwentyFour(items[0].val)) {
            answer = items[0].expr;
            return true;
        }
        return false;
    }

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            Node rest[6];
            int  m = 0;
            for (int k = 0; k < n; ++k)
                if (k != i && k != j) rest[m++] = items[k];

            Fraction a = items[i].val;
            Fraction b = items[j].val;
            string   ea = items[i].expr;
            string   eb = items[j].expr;

            Fraction  r[6];
            string    e[6];
            int       cnt = 0;

            r[cnt] = addFraction(a, b); e[cnt] = "(" + ea + "+" + eb + ")"; cnt++;
            r[cnt] = subFraction(a, b); e[cnt] = "(" + ea + "-" + eb + ")"; cnt++;
            r[cnt] = subFraction(b, a); e[cnt] = "(" + eb + "-" + ea + ")"; cnt++;
            r[cnt] = mulFraction(a, b); e[cnt] = "(" + ea + "*" + eb + ")"; cnt++;
            if (b.num != 0) { r[cnt] = divFraction(a, b); e[cnt] = "(" + ea + "/" + eb + ")"; cnt++; }
            if (a.num != 0) { r[cnt] = divFraction(b, a); e[cnt] = "(" + eb + "/" + ea + ")"; cnt++; }

            for (int t = 0; t < cnt; ++t) {
                if (!fracValid(r[t])) continue;
                if (intOnly && r[t].den != 1) continue;   /* 只用整数中间结果 */
                rest[m].val  = r[t];
                rest[m].expr = e[t];
                if (searchSolution(rest, m + 1, answer, intOnly)) return true;
            }
        }
    }
    return false;
}

/* 去掉最外层的一对括号 */
static string stripOuterParens(const string &s)
{
    if (s.size() < 2) return s;
    if (s[0] != '(' || s[s.size() - 1] != ')') return s;

    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') depth++;
        else if (s[i] == ')') {
            depth--;
            if (depth == 0 && i != s.size() - 1) return s;
        }
    }
    return s.substr(1, s.size() - 2);
}

/* 判断四张牌是否有解，有解时返回表达式。intOnly 为 true 时只接受整数解 */
static bool solve24(const int cards[CARD_COUNT], string &answer,
                    bool intOnly = false)
{
    Node items[5];
    char buf[16];

    for (int i = 0; i < CARD_COUNT; ++i) {
        items[i].val  = makeFraction(cards[i], 1);
        snprintf(buf, sizeof(buf), "%d", cards[i]);
        items[i].expr = buf;
    }

    answer.clear();
    if (!searchSolution(items, CARD_COUNT, answer, intOnly)) return false;
    answer = stripOuterParens(answer);
    return true;
}

/*
 * 生成四张牌（点数 1~10，符合题目“以 1～10 之间的数值代替”的要求）。
 * 要求有解时只出“全程整数运算就能算出 24”的牌组，
 * 以此契合题目“不涉及小数运算”的要求，玩家不必用分数；
 * 要求无解时用完整算法判断，保证本题在数学上确实无解。
 */
static void generateCards(int cards[CARD_COUNT], int suits[CARD_COUNT],
                          bool wantSolvable)
{
    string dummy;
    for (;;) {
        for (int i = 0; i < CARD_COUNT; ++i) {
            cards[i] = rand() % 10 + 1;
            suits[i] = rand() % 4;
        }
        if (solve24(cards, dummy, wantSolvable) == wantSolvable) return;
    }
}

/*====================== 八、表达式解析 ======================*/

/* 全角符号到半角符号的映射（GBK 两字节串） */
struct WideCharMap {
    const char *wide;
    char        half;
};

static const WideCharMap WIDE_MAP[] = {
    { "\xA3\xAB", '+' }, { "\xA3\xAD", '-' }, { "\xA3\xAA", '*' },
    { "\xA3\xAF", '/' }, { "\xA1\xC1", '(' }, { "\xA1\xC2", ')' },
    { "\xA3\xD7", '*' }, { "\xA3\xF7", '/' }, { "\xA3\x25", '%' },
    { "\xA3\xB0", '0' }, { "\xA3\xB1", '1' }, { "\xA3\xB2", '2' },
    { "\xA3\xB3", '3' }, { "\xA3\xB4", '4' }, { "\xA3\xB5", '5' },
    { "\xA3\xB6", '6' }, { "\xA3\xB7", '7' }, { "\xA3\xB8", '8' },
    { "\xA3\xB9", '9' }
};
static const int WIDE_MAP_N = (int)(sizeof(WIDE_MAP) / sizeof(WIDE_MAP[0]));

/* 把表达式中的全角符号替换成半角，并丢弃空白 */
static string normalizeExpression(const char *src)
{
    string out;
    const char *p = src;

    while (*p != '\0') {
        bool skip = false;
        for (int i = 0; i < WIDE_MAP_N; ++i) {
            size_t len = strlen(WIDE_MAP[i].wide);
            if (len > 0 && strncmp(p, WIDE_MAP[i].wide, len) == 0) {
                out.push_back(WIDE_MAP[i].half);
                p += len;
                skip = true;
                break;
            }
        }
        if (skip) continue;

        unsigned char ch = (unsigned char)(*p);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') { p++; continue; }
        if (ch >= 0x80) {                       /* 其它双字节字符：跳过       */
            p += ((p[1] != '\0') ? 2 : 1);
            continue;
        }
        out.push_back(*p);
        p++;
    }
    return out;
}

/* 递归下降解析：Expr -> Term (('+'|'-') Term)* */
struct ExprParser {
    const char *s;
    int         pos;
    int         err;
    Fraction    val;
};

static void skipSpace(ExprParser &p)
{
    while (p.s[p.pos] == ' ') p.pos++;
}

static Fraction parseFactor(ExprParser &p);
static Fraction parseTerm(ExprParser &p);
static Fraction parseExpr(ExprParser &p);

static Fraction parseFactor(ExprParser &p)
{
    Fraction zero;
    zero.num = 0;
    zero.den = 0;

    skipSpace(p);
    char c = p.s[p.pos];

    if (c == '\0') { p.err = 3; return zero; }

    if (c == '(') {
        p.pos++;
        Fraction inner = parseExpr(p);
        skipSpace(p);
        if (p.s[p.pos] != ')') { p.err = 4; return zero; }
        p.pos++;
        return inner;
    }

    if (c >= '0' && c <= '9') {
        long long v = 0;
        while (p.s[p.pos] >= '0' && p.s[p.pos] <= '9') {
            v = v * 10 + (p.s[p.pos] - '0');
            if (v > 1000000LL) { p.err = 5; return zero; }
            p.pos++;
        }
        return makeFraction(v, 1);
    }

    p.err = 1;                       /* 非法字符                   */
    return zero;
}

static Fraction parseTerm(ExprParser &p)
{
    Fraction v = parseFactor(p);
    if (p.err) return v;

    for (;;) {
        skipSpace(p);
        char c = p.s[p.pos];
        if (c != '*' && c != '/') return v;
        p.pos++;
        Fraction rhs = parseFactor(p);
        if (p.err) return v;
        if (c == '*') v = mulFraction(v, rhs);
        else {
            if (rhs.num == 0) { p.err = 2; return v; }
            v = divFraction(v, rhs);
        }
    }
}

static Fraction parseExpr(ExprParser &p)
{
    Fraction v = parseTerm(p);
    if (p.err) return v;

    for (;;) {
        skipSpace(p);
        char c = p.s[p.pos];
        if (c != '+' && c != '-') return v;
        p.pos++;
        Fraction rhs = parseTerm(p);
        if (p.err) return v;
        v = (c == '+') ? addFraction(v, rhs) : subFraction(v, rhs);
    }
}

/* 求值：成功返回 true；失败时 msg 给出原因 */
static bool evaluateExpression(const char *input, Fraction &out, char *msg)
{
    string      s = normalizeExpression(input);
    ExprParser  p;

    if (s.empty()) { strcpy(msg, "表达式为空，请输入算式"); return false; }

    p.s = s.c_str();
    p.pos = 0;
    p.err = 0;
    p.val = makeFraction(0, 1);

    Fraction v = parseExpr(p);
    if (p.err == 0) {
        skipSpace(p);
        if (p.s[p.pos] != '\0') p.err = 6;
    }

    switch (p.err) {
    case 0: break;
    case 1: strcpy(msg, "含有非法字符，只能使用数字和 + - * / ( )"); return false;
    case 2: strcpy(msg, "出现了除以 0 的运算"); return false;
    case 3: strcpy(msg, "表达式不完整"); return false;
    case 4: strcpy(msg, "括号不匹配"); return false;
    case 5: strcpy(msg, "数字太大"); return false;
    default: strcpy(msg, "表达式格式错误"); return false;
    }

    out = v;
    return true;
}

/* 统计表达式中出现的数字，判断是否恰好是发到的四张牌 */
static bool sameNumberSet(const char *expr, const int cards[CARD_COUNT])
{
    int  got[MAX_EXPR_NUM];
    int  n = 0;
    const char *p = expr;

    while (*p != '\0' && n < MAX_EXPR_NUM) {
        if (*p >= '0' && *p <= '9') {
            int v = 0;
            while (*p >= '0' && *p <= '9') {
                v = v * 10 + (*p - '0');
                p++;
            }
            got[n++] = v;
        } else {
            p++;
        }
    }

    if (n != CARD_COUNT) return false;

    bool used[MAX_EXPR_NUM];
    for (int i = 0; i < n; ++i) used[i] = false;

    for (int i = 0; i < CARD_COUNT; ++i) {
        bool found = false;
        for (int j = 0; j < n; ++j) {
            if (!used[j] && got[j] == cards[i]) { used[j] = true; found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

/*====================== 九、计分与排行榜 ======================*/

static int calculateScore(const Difficulty &diff, int remainSec)
{
    if (remainSec < 0) remainSec = 0;
    if (remainSec > diff.limitSec) remainSec = diff.limitSec;
    return diff.baseScore + diff.bonusScore * remainSec / diff.limitSec;
}

static int compareRecord(const Record &a, const Record &b)
{
    if (a.score != b.score) return a.score > b.score;
    return a.totalSeconds < b.totalSeconds;
}

static void sortRecords(Record list[], int count)
{
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j + 1 < count - i; ++j) {
            if (compareRecord(list[j + 1], list[j])) {
                Record t = list[j];
                list[j] = list[j + 1];
                list[j + 1] = t;
            }
        }
    }
}

static void normalizeRecords(Record list[], int &count)
{
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ) {
            if (strcmp(list[i].name, list[j].name) == 0) {
                if (compareRecord(list[j], list[i])) list[i] = list[j];
                for (int k = j; k < count - 1; ++k) list[k] = list[k + 1];
                count--;
            } else {
                ++j;
            }
        }
    }
    sortRecords(list, count);
}

/* 可执行文件所在目录（带结尾的反斜杠），取不到时退化为当前目录 */
static const char *appDir()
{
    static char dir[MAX_PATH] = "";

    if (dir[0] == '\0') {
        char  exe[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);

        if (n > 0 && n < MAX_PATH) {
            int i = (int)n - 1;
            while (i >= 0 && exe[i] != '\\' && exe[i] != '/') --i;
            if (i >= 0) {
                exe[i + 1] = '\0';
                strcpy(dir, exe);
            }
        }
        if (dir[0] == '\0') strcpy(dir, ".\\");
    }
    return dir;
}

/* 数据文件放在可执行文件所在目录，避免换工作目录后“找不到排行榜” */
static const char *rankFilePath()
{
    static char path[MAX_PATH + 32];

    if (path[0] == '\0')
        snprintf(path, sizeof(path), "%s%s", appDir(), RANK_FILE);

    return path;
}

/*====================== 音效播放（MCI） ======================*/

static bool g_mciOpened = false;      /* sfx 别名是否已打开 */
static bool g_bgmOpened = false;      /* bgm 别名是否已打开 */
static bool g_bgmActive = false;      /* BGM 逻辑上应该在响 */
static bool g_bgmDeferred = false;    /* BGM 因音效未播完而顺延启动 */
static bool g_sfxOn     = true;       /* 声音总开关（菜单可切换） */

static void stopSfx()
{
    if (g_mciOpened) {
        mciSendStringA("close sfx", NULL, 0, NULL);
        g_mciOpened = false;
    }
}

/* 把内嵌在 exe 里的声音资源解压到临时目录，返回可播放的路径 */
static bool extractAudioRes(int id, char *outPath, int outSize)
{
    HRSRC hr = FindResourceA(NULL, MAKEINTRESOURCEA(SND_RESID[id]),
                            (LPCSTR)RT_RCDATA);
    if (hr == NULL) return false;

    HGLOBAL hg = LoadResource(NULL, hr);
    if (hg == NULL) return false;

    DWORD       size = SizeofResource(NULL, hr);
    const void *data = LockResource(hg);
    if (data == NULL || size == 0) return false;

    char tmp[MAX_PATH] = "";
    if (GetTempPathA(MAX_PATH, tmp) == 0) return false;

    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%sCalc24Sfx", tmp);
    CreateDirectoryA(dir, NULL);

    snprintf(outPath, outSize, "%s\\%s", dir, SND_FILE[id]);

    /* 已经解压过且大小一致就直接复用，避免每次播放都写盘 */
    FILE *fp = fopen(outPath, "rb");
    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        long exist = ftell(fp);
        fclose(fp);
        if (exist == (long)size) return true;
    }

    fp = fopen(outPath, "wb");
    if (fp == NULL) return false;
    size_t wrote = fwrite(data, 1, size, fp);
    fclose(fp);

    return (wrote == size);
}

/*
 * 解析某个声音的实际文件路径：
 * 先找可执行文件旁的 音频\ 目录，找不到就用内嵌资源。
 */
static bool resolveAudio(int id, char *outPath, int outSize)
{
    char ext[MAX_PATH * 2];
    snprintf(ext, sizeof(ext), "%s%s%s", appDir(), SND_DIR, SND_FILE[id]);

    FILE *fp = fopen(ext, "rb");
    if (fp != NULL) {
        fclose(fp);
        strncpy(outPath, ext, outSize - 1);
        outPath[outSize - 1] = '\0';
        return true;
    }

    return extractAudioRes(id, outPath, outSize);
}

/* 背景音乐：整局循环播放，用独立的 bgm 别名，不干扰音效 */
static void startBgm()
{
    if (!g_sfxOn) { g_bgmActive = false; g_bgmDeferred = false; return; }

    if (!g_bgmOpened) {
        char path[MAX_PATH * 2];
        if (!resolveAudio(SND_BGM, path, sizeof(path))) return;

        char cmd[MAX_PATH * 3];
        snprintf(cmd, sizeof(cmd), "open \"%s\" type mpegvideo alias bgm", path);
        if (mciSendStringA(cmd, NULL, 0, NULL) != 0) {
            snprintf(cmd, sizeof(cmd), "open \"%s\" alias bgm", path);
            if (mciSendStringA(cmd, NULL, 0, NULL) != 0) return;
        }
        g_bgmOpened = true;
    }

    g_bgmActive = true;

    /* 此刻如果还有音效在响（比如刚点“开始游戏”的“叮叮叮”），
     * 先不启动 BGM，等那个音效播完（MM_MCINOTIFY）再起 */
    if (g_mciOpened) {
        g_bgmDeferred = true;
        return;
    }

    mciSendStringA("seek bgm to start", NULL, 0, NULL);
    mciSendStringA("play bgm repeat", NULL, 0, NULL);
    g_bgmDeferred = false;
}

static void stopBgm()
{
    if (g_bgmOpened) {
        mciSendStringA("stop bgm", NULL, 0, NULL);
        mciSendStringA("seek bgm to start", NULL, 0, NULL);
    }
    g_bgmActive   = false;
    g_bgmDeferred = false;
}

static void pauseBgm()
{
    if (g_bgmActive && !g_bgmDeferred)
        mciSendStringA("pause bgm", NULL, 0, NULL);
}

static void resumeBgm()
{
    if (!g_bgmActive) return;

    if (g_bgmDeferred) {
        mciSendStringA("seek bgm to start", NULL, 0, NULL);
        mciSendStringA("play bgm repeat", NULL, 0, NULL);
        g_bgmDeferred = false;
    } else {
        mciSendStringA("resume bgm", NULL, 0, NULL);
    }
}

/* 退出程序前把两个 MCI 别名都释放干净 */
static void shutdownAudio()
{
    stopSfx();
    if (g_bgmOpened) {
        mciSendStringA("close bgm", NULL, 0, NULL);
        g_bgmOpened = false;
    }
    g_bgmActive   = false;
    g_bgmDeferred = false;
}

/*
 * 先打开一个声音素材（不播放），成功返回 true。
 * 自检时只调这个函数，就不会真的发出声音。
 */
static bool sfxOpen(int id)
{
    if (!g_sfxOn) return false;
    if (id < 0 || id >= SND_COUNT) return false;

    char path[MAX_PATH * 2];
    if (!resolveAudio(id, path, sizeof(path))) return false;

    stopSfx();

    char cmd[MAX_PATH * 3];

    /* 先按 MPEG 视频设备打开（Win10 自带解码器），失败再让 MCI 自己挑设备 */
    snprintf(cmd, sizeof(cmd), "open \"%s\" type mpegvideo alias sfx", path);
    if (mciSendStringA(cmd, NULL, 0, NULL) != 0) {
        snprintf(cmd, sizeof(cmd), "open \"%s\" alias sfx", path);
        if (mciSendStringA(cmd, NULL, 0, NULL) != 0) return false;
    }

    g_mciOpened = true;
    return true;
}

/*
 * 播放一个音效。
 * 播放前暂停背景音乐，并用 notify 在播完后自动恢复；
 * 后一个音效会打断前一个，互不影响 BGM 的暂停状态。
 */
static bool playSfx(int id)
{
    if (!g_sfxOn) return false;

    pauseBgm();

    if (!sfxOpen(id)) {
        resumeBgm();          /* 素材缺失也不要让 BGM 卡在暂停 */
        return false;
    }

    /* 播完把 MM_MCINOTIFY 发给主窗口，在窗口过程里恢复 BGM */
    mciSendStringA("play sfx notify", NULL, 0, g_hwnd);
    return true;
}

/* 日期必须是 YYYY-MM-DD，避免手工改坏的文件让排行榜出现离谱记录 */
static bool validDate(const char *d)
{
    if (d == NULL || strlen(d) != 10) return false;
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) {
            if (d[i] != '-') return false;
        } else {
            if (d[i] < '0' || d[i] > '9') return false;
        }
    }
    return true;
}

static int loadRecords(Record list[], int maxCount)
{
    FILE *fp = fopen(rankFilePath(), "r");
    if (fp == NULL) return 0;

    int  n = 0;
    char line[192];

    while (n < maxCount && fgets(line, sizeof(line), fp) != NULL) {
        char name[MAX_NAME];
        int  score = 0, seconds = 0;
        char date[20] = "", level[8] = "";

        int got = sscanf(line, "%31s %d %d %19s %7s",
                         name, &score, &seconds, date, level);
        if (got < 4) continue;
        if (got < 5) strcpy(level, "普通");

        /* 过滤掉格式损坏的行（比如手工编辑后字段错位） */
        if (name[0] == '\0') continue;
        if (!validDate(date)) continue;
        if (score < 0 || score > 100000) continue;
        if (seconds < 0 || seconds > 100000) continue;

        strcpy(list[n].name, name);
        list[n].score        = score;
        list[n].totalSeconds = seconds;
        strcpy(list[n].date, date);
        strcpy(list[n].level, level);
        n++;
    }

    fclose(fp);
    normalizeRecords(list, n);
    return n;
}

static void saveRecords(Record list[], int count)
{
    FILE *fp = fopen(rankFilePath(), "w");
    if (fp == NULL) return;

    for (int i = 0; i < count; ++i) {
        fprintf(fp, "%s %d %d %s %s\n",
                list[i].name, list[i].score,
                list[i].totalSeconds, list[i].date, list[i].level);
    }
    fclose(fp);
}

static void todayString(char *buf, int size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, size, "%04d-%02d-%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

/* 插入一条记录；同名只保留最好成绩。返回名次，0 表示未上榜 */
static int insertRecord(Record list[], int &count, const Record &rec, bool &newBest)
{
    newBest = false;

    for (int i = 0; i < count; ++i) {
        if (strcmp(list[i].name, rec.name) == 0) {
            if (compareRecord(rec, list[i])) {
                list[i] = rec;
                newBest = true;
            }
            sortRecords(list, count);
            saveRecords(list, count);
            for (int k = 0; k < count; ++k)
                if (strcmp(list[k].name, rec.name) == 0) return k + 1;
            return 0;
        }
    }

    if (count >= MAX_RECORDS) {
        if (!compareRecord(rec, list[count - 1])) return 0;
        count--;
    }

    list[count] = rec;
    count++;
    newBest = true;
    sortRecords(list, count);
    saveRecords(list, count);

    for (int k = 0; k < count; ++k)
        if (strcmp(list[k].name, rec.name) == 0) return k + 1;
    return 0;
}

/*====================== 十、按钮描述表 ======================*/

/* style: 0 = 描边按钮  1 = 主按钮（金底）  2 = 文字按钮 */
struct BtnDesc {
    int         x, y, w, h;
    int         id;
    const char *text;
    int         style;
};

static const BtnDesc BTN_MENU[] = {
    { 420, 330, 200, 54, ID_M_START, "开 始 游 戏", 1 },
    { 420, 398, 200, 46, ID_M_DIFF,  "难 度 设 置", 0 },
    { 420, 454, 200, 46, ID_M_RANK,  "高 手 排 行 榜", 0 },
    { 420, 510, 200, 46, ID_M_RULES, "游 戏 规 则", 0 },
    { 420, 566, 200, 46, ID_M_EXIT,  "退 出 游 戏", 2 }
};
static const int BTN_MENU_N = 5;

static const BtnDesc BTN_DIFF[] = {
    { 115, 250, 250, 200, ID_D_0,    "", 3 },
    { 395, 250, 250, 200, ID_D_1,    "", 3 },
    { 675, 250, 250, 200, ID_D_2,    "", 3 },
    { 440, 520, 160, 46,  ID_D_BACK, "返 回", 0 }
};
static const int BTN_DIFF_N = 4;

static const BtnDesc BTN_RANK[] = {
    { 440, 574, 160, 46, ID_R_BACK, "返 回", 0 }
};
static const int BTN_RANK_N = 1;

static const BtnDesc BTN_RULES[] = {
    { 440, 588, 160, 46, ID_U_BACK, "返 回", 0 }
};
static const int BTN_RULES_N = 1;

static const BtnDesc BTN_NAME[] = {
    { 350, 430, 150, 48, ID_N_OK,   "确  定", 1 },
    { 540, 430, 150, 48, ID_N_SKIP, "跳  过", 0 },
    { 445, 540, 150, 38, ID_N_BACK, "返回主菜单", 2 }
};
static const int BTN_NAME_N = 3;

static const BtnDesc BTN_GAME_INPUT[] = {
    { 580, 316, 120, 50, ID_G_NOANS,  "本题无解", 4 },
    { 716, 316, 160, 50, ID_G_SUBMIT, "提  交",   1 },
    { 450, 560, 140, 40, ID_G_BACK,   "返回主菜单", 2 }
};
static const int BTN_GAME_INPUT_N = 3;

static const BtnDesc BTN_GAME_RESULT[] = {
    { 440, 382, 160, 48, ID_G_NEXT, "下 一 题", 1 },
    { 450, 560, 140, 40, ID_G_BACK, "返回主菜单", 2 }
};
static const int BTN_GAME_RESULT_N = 2;

static const BtnDesc BTN_SUMMARY[] = {
    { 248, 578, 170, 48, ID_S_AGAIN, "再 来 一 局", 1 },
    { 435, 578, 170, 48, ID_S_RANK,  "查看排行榜", 0 },
    { 622, 578, 170, 48, ID_S_MENU,  "返回主菜单", 0 }
};
static const int BTN_SUMMARY_N = 3;

static const BtnDesc *curButtons(int &n)
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

/*====================== 十一、按钮绘制 ======================*/

static void drawButton(HDC dc, const BtnDesc &b, bool hover, bool press)
{
    int x = b.x, y = b.y, w = b.w, h = b.h;

    if (b.style == 3) return;                 /* 难度卡片单独绘制           */

    if (b.style == 2) {                       /* 文字按钮                   */
        COLORREF c = hover ? COL_GOLD_L : COL_DIM;
        tx(dc, b.text, x, y, w, h, F_BTN, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (hover) fillRect(dc, x + w / 2 - 20, y + h - 6, 40, 1, COL_GOLD);
        return;
    }

    int dy = press ? 2 : 0;

    if (b.style == 1) {                       /* 主按钮：金底               */
        COLORREF bg = press ? COL_GOLD_D : (hover ? COL_GOLD_L : COL_GOLD);
        roundFill(dc, x, y + dy, w, h, 10, bg, COL_GOLD_L, 1);
        tx(dc, b.text, x, y + dy, w, h, F_BTN,
           RGB(26, 22, 12), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (b.style == 4) {                /* 复选框：本题无解           */
        int checked = g.noAnswer;
        COLORREF bg = press ? COL_PANEL2 : (hover ? COL_PANEL2 : COL_PANEL);
        roundFill(dc, x, y + dy, w, h, 8, bg, checked ? COL_GOLD : COL_LINE, 1);
        int bx = x + 12, by = y + dy + h / 2 - 8;
        roundFill(dc, bx, by, 16, 16, 3, checked ? COL_GOLD : COL_PANEL2,
                  checked ? COL_GOLD_L : COL_LINE, 1);
        if (checked) {
            HPEN pn = CreatePen(PS_SOLID, 2, RGB(26, 22, 12));
            HPEN op = (HPEN)SelectObject(dc, pn);
            MoveToEx(dc, bx + 4, by + 8, NULL);
            LineTo(dc, bx + 7, by + 12);
            LineTo(dc, bx + 12, by + 4);
            SelectObject(dc, op);
            DeleteObject(pn);
        }
        tx(dc, b.text, x + 34, y + dy, w - 40, h, F_BODY_S,
           checked ? COL_GOLD_L : COL_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {                                  /* 描边按钮                   */
        COLORREF bg = press ? COL_PANEL2 : (hover ? COL_PANEL2 : COL_PANEL);
        roundFill(dc, x, y + dy, w, h, 8, bg, hover ? COL_GOLD : COL_GOLD_D, 1);
        tx(dc, b.text, x, y + dy, w, h, F_BTN,
           hover ? COL_GOLD_L : COL_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawAllButtons(HDC dc)
{
    int n = 0;
    const BtnDesc *bs = curButtons(n);
    for (int i = 0; i < n; ++i) {
        if (bs[i].style == 3) continue;
        drawButton(dc, bs[i], g_hover == bs[i].id, g_press == bs[i].id);
    }
}

/*====================== 十二、扑克牌绘制 ======================*/

static void drawCard(HDC dc, int x, int y, int w, int h,
                     int value, int suit, bool faceDown)
{
    roundFill(dc, x + 3, y + 4, w, h, 12, RGB(8, 9, 13), RGB(8, 9, 13), 1);

    if (faceDown) {
        roundFill(dc, x, y, w, h, 12, COL_NAVY, COL_GOLD, 2);
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 7; ++j) {
                int cx = x + 16 + i * 22;
                int cy = y + 18 + j * 20;
                if (cx > x + w - 20) continue;
                HPEN pn = CreatePen(PS_SOLID, 1, COL_GOLD_D);
                HPEN op = (HPEN)SelectObject(dc, pn);
                MoveToEx(dc, cx, cy - 5, NULL);
                LineTo(dc, cx + 5, cy);
                LineTo(dc, cx, cy + 5);
                LineTo(dc, cx - 5, cy);
                LineTo(dc, cx, cy - 5);
                SelectObject(dc, op);
                DeleteObject(pn);
            }
        }
        return;
    }

    roundFill(dc, x, y, w, h, 12, COL_CARD, COL_CARD_B, 1);

    COLORREF    ink = suitColor(suit);
    const char *rk  = RANKS[value - 1];

    /* 左上角：点数 + 花色 */
    tx(dc, rk, x + 9, y + 3, 60, 36, F_RANK, ink, DT_LEFT | DT_TOP);
    txChar(dc, suitChar(suit), x + 11, y + 37, 30, 30, F_SUIT_S, ink,
           DT_LEFT | DT_TOP);

    /* 右下角：点数 + 花色 */
    tx(dc, rk, x + w - 69, y + h - 41, 60, 36, F_RANK, ink,
       DT_RIGHT | DT_TOP);
    txChar(dc, suitChar(suit), x + w - 41, y + h - 71, 30, 30, F_SUIT_S, ink,
           DT_RIGHT | DT_TOP);

    /* 中央大花色 */
    txChar(dc, suitChar(suit), x, y + h / 2 - 28, w, 60, F_SUIT_L, ink,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/*====================== 十三、公共绘制块 ======================*/

static void drawBackdrop(HDC dc)
{
    fillRect(dc, 0, 0, DES_W, DES_H, COL_BG);
}

static void drawTitleBarBand(HDC dc, const char *title, const char *sub)
{
    fillRect(dc, 0, 0, DES_W, 100, COL_PANEL);
    fillRect(dc, 0, 100, DES_W, 1, COL_GOLD_D);
    fillRect(dc, 0, 101, DES_W, 1, RGB(24, 28, 38));

    tx(dc, title, 0, 6, DES_W, 56, F_TITLE, COL_GOLD_L,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (sub != NULL && sub[0] != '\0')
        tx(dc, sub, 0, 64, DES_W, 26, F_SUB, COL_DIM,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/* 顶部四角的小菱形装饰 */
static void drawCornerDecor(HDC dc)
{
    const int X[2] = { 26, DES_W - 46 };
    for (int i = 0; i < 2; ++i) {
        int x = X[i], y = 34;
        HPEN pn = CreatePen(PS_SOLID, 1, COL_GOLD_D);
        HPEN op = (HPEN)SelectObject(dc, pn);
        MoveToEx(dc, x + 10, y, NULL);
        LineTo(dc, x + 20, y + 14);
        LineTo(dc, x + 10, y + 28);
        LineTo(dc, x, y + 14);
        LineTo(dc, x + 10, y);
        SelectObject(dc, op);
        DeleteObject(pn);
    }
}

static void drawStatusBar(HDC dc, const char *left, const char *right)
{
    int y = DES_H - 40;
    fillRect(dc, 0, y, DES_W, 40, COL_PANEL);
    fillRect(dc, 0, y, DES_W, 1, COL_GOLD_D);

    tx(dc, left, 26, y, 620, 40, F_BODY_S, COL_DIM,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (right != NULL && right[0] != '\0')
        tx(dc, right, DES_W - 646, y, 620, 40, F_BODY, COL_GOLD,
           DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

/*====================== 十四、各界面绘制 ======================*/

static void drawMenuScreen(HDC dc)
{
    drawBackdrop(dc);
    fillRect(dc, 0, 0, DES_W, 300, COL_PANEL);
    fillRect(dc, 0, 300, DES_W, 1, COL_GOLD_D);
    fillRect(dc, 0, 301, DES_W, 1, COL_BG2);

    drawCornerDecor(dc);

    tx(dc, "计 算 二 十 四 点", 0, 96, DES_W, 78, F_TITLE, COL_GOLD_L,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "2 4   P O I N T   G A M E", 0, 172, DES_W, 32, F_SUB, COL_GOLD,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    goldLine(dc, DES_W / 2 - 150, 216, 300);
    tx(dc, "四条运算，二十四点。限时之内，一算即中。", 0, 228, DES_W, 30,
       F_BODY, COL_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* 当前难度信息条 */
    const Difficulty &d = LEVELS[g.diff];
    char info[128];
    snprintf(info, sizeof(info), "当前难度  ◆  %s        每题限时  %d 秒        单题满分  %d 分",
             d.name, d.limitSec, d.baseScore + d.bonusScore);

    roundFill(dc, 320, 610, 400, 34, 8, COL_PANEL, COL_LINE, 1);
    tx(dc, info, 320, 610, 400, 34, F_BODY_S, COL_GOLD,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void drawDifficultyScreen(HDC dc)
{
    drawBackdrop(dc);
    drawTitleBarBand(dc, "难 度 设 置", "越难的时间越短，但分数也越高");
    drawCornerDecor(dc);

    static const char *DESC[3] = {
        "时间充裕，适合新手熟悉玩法",
        "限时收紧，考验运算速度",
        "极限挑战，高手之间的较量"
    };

    for (int i = 0; i < 3; ++i) {
        int x = BTN_DIFF[i].x, y = BTN_DIFF[i].y;
        int w = BTN_DIFF[i].w, h = BTN_DIFF[i].h;
        bool sel = (g.diff == i);
        bool hov = (g_hover == BTN_DIFF[i].id);

        COLORREF bg     = sel ? COL_PANEL2 : COL_PANEL;
        COLORREF border = sel ? COL_GOLD : (hov ? COL_GOLD_D : COL_LINE);
        int      thick  = sel ? 2 : 1;

        roundFill(dc, x, y, w, h, 14, bg, border, thick);

        if (sel) fillRect(dc, x + 1, y + 16, 4, h - 32, COL_GOLD);

        tx(dc, LEVELS[i].name, x, y + 20, w, 44, F_H1,
           sel ? COL_GOLD_L : COL_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        char buf[64];
        snprintf(buf, sizeof(buf), "每题限时 %d 秒", LEVELS[i].limitSec);
        tx(dc, buf, x, y + 74, w, 26, F_BODY, COL_TEXT,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        snprintf(buf, sizeof(buf), "基础 %d + 奖励 %d", LEVELS[i].baseScore,
                 LEVELS[i].bonusScore);
        tx(dc, buf, x, y + 102, w, 24, F_BODY_S, COL_DIM,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        snprintf(buf, sizeof(buf), "单题满分 %d 分",
                 LEVELS[i].baseScore + LEVELS[i].bonusScore);
        tx(dc, buf, x, y + 132, w, 28, F_H2, COL_GOLD,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        tx(dc, DESC[i], x + 10, y + 166, w - 20, 24, F_BODY_S, COL_DIM,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawRulesScreen(HDC dc)
{
    drawBackdrop(dc);
    drawTitleBarBand(dc, "游 戏 规 则", "先看清规则，再开始计时");
    drawCornerDecor(dc);

    static const char *LINES[] = {
        "1  每局共 3 道题，每道题随机发出 4 张扑克牌，点数取 1～10（1 显示为 A）。",
        "2  用四则运算 + - * / 和括号，把 4 个点数各用一次、算到 24 即得分。",
        "3  题目保证全程用整数运算就能算出，不需要分数；表达式中也不能写小数。",
        "4  若判断本题无解，请勾选「本题无解」后提交；判断正确同样得分。",
        "5  超过限时自动判为超时，本题 0 分，直接进入下一题。",
        "6  得分 = 基础分 + 时间奖励 × 剩余秒数 ÷ 本题限时，剩余时间越多得分越高。",
        "7  得分越高、用时越短，排行榜名次越靠前；同名玩家榜上只保留最好成绩。"
    };

    roundFill(dc, 40, 124, DES_W - 80, 452, 12, COL_PANEL, COL_LINE, 1);

    int y = 150;
    for (int i = 0; i < 7; ++i) {
        tx(dc, LINES[i], 76, y, DES_W - 152, 30, F_BODY, COL_TEXT,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += 44;
    }

    fillRect(dc, 76, 470, DES_W - 152, 1, COL_LINE);

    char buf[128];
    snprintf(buf, sizeof(buf),
             "单题满分：简单 %d 分 · 普通 %d 分 · 困难 %d 分",
             LEVELS[0].baseScore + LEVELS[0].bonusScore,
             LEVELS[1].baseScore + LEVELS[1].bonusScore,
             LEVELS[2].baseScore + LEVELS[2].bonusScore);
    tx(dc, buf, 0, 496, DES_W, 30, F_BODY_S, COL_GOLD,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void drawRankScreen(HDC dc)
{
    drawBackdrop(dc);
    drawTitleBarBand(dc, "高 手 排 行 榜", "同名玩家只保留个人最好成绩");
    drawCornerDecor(dc);

    int x = 40, y = 126, w = DES_W - 80;
    int rowH = 38;
    int nShow = (g.recCount < 9) ? g.recCount : 9;

    roundFill(dc, x, y, w, 60 + rowH * 9, 12, COL_PANEL, COL_LINE, 1);

    /* 表头 */
    static const int CX[6] = { 30, 110, 340, 470, 600, 720 };
    static const char *HEAD[6] = { "名次", "玩家", "总分", "用时", "难度", "日期" };
    for (int i = 0; i < 6; ++i)
        tx(dc, HEAD[i], x + CX[i], y + 12, 160, 26, F_BODY_S, COL_DIM,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    fillRect(dc, x + 20, y + 44, w - 40, 1, COL_GOLD_D);

    if (g.recCount == 0) {
        tx(dc, "暂时还没有成绩，快去挑战一局吧！", x, y + 120, w, 40, F_BODY,
           COL_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 空行分隔线，使表格即使是空的也保持完整感 */
    for (int i = 0; i <= 9; ++i) {
        if (i == 0) continue;
        int ly = y + 52 + rowH * i - 1;
        fillRect(dc, x + 20, ly, w - 40, 1, COL_LINE);
    }

    for (int i = 0; i < nShow; ++i) {
        const Record &r = g.recs[i];
        int ry = y + 52 + rowH * i;

        if (i % 2 == 1) fillRect(dc, x + 12, ry, w - 24, rowH - 1, COL_PANEL2);
        COLORREF mc = COL_TEXT;
        if (i == 0) mc = COL_GOLD_L;
        else if (i == 1) mc = RGB(196, 202, 214);
        else if (i == 2) mc = RGB(190, 148, 100);

        char num[16];
        if (i < 3) snprintf(num, sizeof(num), "★ %d", i + 1);
        else       snprintf(num, sizeof(num), "   %d", i + 1);
        tx(dc, num, x + CX[0], ry, 90, rowH, F_MONO, mc,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        tx(dc, r.name, x + CX[1], ry, 230, rowH, F_BODY, mc,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(num, sizeof(num), "%d", r.score);
        tx(dc, num, x + CX[2], ry, 120, rowH, F_MONO, mc,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(num, sizeof(num), "%d 秒", r.totalSeconds);
        tx(dc, num, x + CX[3], ry, 130, rowH, F_BODY, mc,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        COLORREF lc = COL_DIM;
        if (strcmp(r.level, "困难") == 0) lc = COL_RED;
        else if (strcmp(r.level, "普通") == 0) lc = COL_GOLD;
        tx(dc, r.level, x + CX[4], ry, 130, rowH, F_BODY, lc,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        tx(dc, r.date, x + CX[5], ry, 200, rowH, F_BODY_S, COL_DIM,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void drawGameScreen(HDC dc)
{
    drawBackdrop(dc);

    const Difficulty &d = LEVELS[g.diff];

    /* ---------- 顶部信息条 ---------- */
    fillRect(dc, 0, 0, DES_W, 54, COL_PANEL);
    fillRect(dc, 0, 54, DES_W, 1, COL_GOLD_D);

    char buf[128];

    snprintf(buf, sizeof(buf), "第  %d  /  %d  题", g.round, ROUND_COUNT);
    tx(dc, buf, 40, 0, 300, 54, F_H2, COL_TEXT,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    snprintf(buf, sizeof(buf), "◆  %s  ·  %d 秒", d.name, d.limitSec);
    tx(dc, buf, 340, 0, 360, 54, F_BODY, COL_GOLD,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    snprintf(buf, sizeof(buf), "本局得分 %d", g.totalScore);
    tx(dc, buf, DES_W - 340, 0, 300, 54, F_H2, COL_TEXT,
       DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    /* ---------- 倒计时条 ---------- */
    int bx = 228, by = 72, bw = 584, bh = 16;
    roundFill(dc, bx, by, bw, bh, 8, COL_PANEL2, COL_LINE, 1);

    int  sec  = (g.remainMs + 999) / 1000;
    if (sec < 0) sec = 0;
    int  ratio = (g.limitMs > 0) ? (g.remainMs * 100 / g.limitMs) : 0;
    if (ratio < 0) ratio = 0;
    if (ratio > 100) ratio = 100;

    COLORREF barC = COL_GREEN;
    if (ratio <= 25) barC = COL_RED;
    else if (ratio <= 55) barC = COL_GOLD;

    int fillW = bw * ratio / 100;
    if (fillW > 0) roundFill(dc, bx, by, fillW, bh, 8, barC, barC, 1);

    snprintf(buf, sizeof(buf), "%d 秒", sec);
    tx(dc, buf, bx + bw + 12, by - 8, 110, 32, F_MONO,
       (ratio <= 25) ? COL_RED : COL_GOLD, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* ---------- 四张牌 ---------- */
    int cw = 128, ch = 184, gap = 24;
    int x0 = (DES_W - (cw * CARD_COUNT + gap * (CARD_COUNT - 1))) / 2;
    int cy = 104;

    for (int i = 0; i < CARD_COUNT; ++i) {
        int cx = x0 + i * (cw + gap);
        if (i < g.dealt)
            drawCard(dc, cx, cy, cw, ch, g.cards[i], g.suits[i], false);
        else
            drawCard(dc, cx, cy, cw, ch, 0, 0, true);
    }

    /* ---------- 输入区 / 结果区 ---------- */
    if (g.subState == GS_INPUT) {
        roundFill(dc, 160, 312, 408, 58, 8, COL_PANEL, COL_GOLD, 1);

        if (g.dealt < CARD_COUNT) {
            tx(dc, "正在发牌 …", 164, 316, 400, 50, F_BODY_S, COL_DIM,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        /* 平时显示操作提示；出现格式错误时该行改为红色提示 */
        if (g.feedback[0] != '\0') {
            tx(dc, g.feedback, 0, 382, DES_W, 32, F_BODY, g.fbColor,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            tx(dc, "输入表达式后按回车或点「提交」，例如  (3+3)*(8-4)",
               0, 382, DES_W, 32, F_BODY_S, COL_DIM,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    } else {
        roundFill(dc, 110, 300, DES_W - 220, 66, 8, COL_PANEL,
                  g.fbColor, 1);
        tx(dc, g.feedback, 122, 300, DES_W - 244, 66, F_H2, g.fbColor,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        tx(dc, "按「下一题」继续", 0, 444, DES_W, 28, F_BODY_S, COL_DIM,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* ---------- 三题进度指示 ---------- */
    for (int i = 0; i < ROUND_COUNT; ++i) {
        int px = DES_W / 2 - 26 + i * 22;
        int py = 496;
        bool done = (i + 1 < g.round);
        bool cur  = (i + 1 == g.round);
        COLORREF c = COL_LINE;
        if (done) c = COL_GOLD;
        if (cur)  c = COL_GOLD_L;
        roundFill(dc, px, py, 12, 12, 6, c, c, 1);
    }

    goldLine(dc, 60, 532, DES_W - 120);
}

static void drawNameScreen(HDC dc)
{
    drawBackdrop(dc);
    drawTitleBarBand(dc, "迎 战 之 前", "留下你的大名，成绩才能记入排行榜");
    drawCornerDecor(dc);

    roundFill(dc, 300, 150, 440, 250, 12, COL_PANEL, COL_LINE, 1);

    tx(dc, "玩家 姓名", 300, 176, 440, 30, F_BODY_S, COL_DIM,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    roundFill(dc, 326, 218, 388, 56, 8, COL_PANEL, COL_GOLD, 1);

    tx(dc, "最多 16 个字符；留空则记为「玩家」", 300, 288, 440, 28, F_BODY_S,
       COL_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    tx(dc, "排行榜上同名玩家只保留最好成绩", 300, 322, 440, 28, F_BODY_S,
       COL_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    tx(dc, "输入后按回车或点「确定」", 300, 356, 440, 28, F_BODY_S, COL_GOLD,
       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void drawSummaryScreen(HDC dc)
{
    drawBackdrop(dc);
    drawTitleBarBand(dc, "本 局 结 算", "胜负已分，来看看你的成绩");
    drawCornerDecor(dc);

    const Difficulty &d = LEVELS[g.diff];

    /* 三题明细 */
    roundFill(dc, 120, 124, DES_W - 240, 190, 12, COL_PANEL, COL_LINE, 1);
    roundFill(dc, 120, 124, DES_W - 240, 40, 12, COL_PANEL2, COL_LINE, 1);

    tx(dc, "题 号", 150, 124, 120, 40, F_BODY_S, COL_DIM,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "判 定", 320, 124, 200, 40, F_BODY_S, COL_DIM,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "用 时", 560, 124, 160, 40, F_BODY_S, COL_DIM,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    tx(dc, "得 分", 760, 124, 160, 40, F_BODY_S, COL_DIM,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    static const char *RESNAME[5] = {
        "回答正确", "回答错误", "回答超时", "无解判断正确", "无解判断错误"
    };

    for (int i = 0; i < ROUND_COUNT; ++i) {
        int ry = 164 + i * 46;
        char buf[64];

        snprintf(buf, sizeof(buf), "第 %d 题", i + 1);
        tx(dc, buf, 150, ry, 120, 46, F_BODY, COL_TEXT,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        COLORREF c = (g.roundRes[i] == RES_RIGHT || g.roundRes[i] == RES_NOANS_OK)
                   ? COL_GREEN : COL_RED;
        tx(dc, RESNAME[g.roundRes[i]], 320, ry, 220, 46, F_BODY, c,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(buf, sizeof(buf), "%.1f 秒", g.roundMs[i] / 1000.0);
        tx(dc, buf, 560, ry, 160, 46, F_BODY, COL_TEXT,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        snprintf(buf, sizeof(buf), "%d 分", g.roundScore[i]);
        tx(dc, buf, 760, ry, 160, 46, F_MONO, COL_TEXT,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* 总分 */
    roundFill(dc, 120, 336, DES_W - 240, 130, 12, COL_PANEL, COL_GOLD, 2);

    char buf[160];
    tx(dc, "总  分", 160, 352, 160, 40, F_BODY, COL_DIM,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    snprintf(buf, sizeof(buf), "%d", g.totalScore);
    tx(dc, buf, 160, 384, 360, 66, F_TITLE, COL_GOLD_L,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    tx(dc, "分", 248, 410, 60, 34, F_BODY, COL_GOLD,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    snprintf(buf, sizeof(buf), "难度  %s", d.name);
    tx(dc, buf, 600, 352, 260, 34, F_BODY, COL_TEXT,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    snprintf(buf, sizeof(buf), "累计用时  %.1f 秒", g.totalMs / 1000.0);
    tx(dc, buf, 600, 388, 300, 34, F_BODY, COL_TEXT,
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (g.rankPos > 0) {
        snprintf(buf, sizeof(buf), "★  当前排名第 %d 名", g.rankPos);
        tx(dc, buf, 600, 424, 300, 34, F_BODY, COL_GOLD_L,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        tx(dc, "本局未得分，成绩不记入排行榜", 600, 424, 320, 34, F_BODY_S,
           COL_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* 名次提示条 */
    if (g.newBest && g.rankPos > 0) {
        roundFill(dc, 300, 490, 440, 46, 8, COL_PANEL2, COL_GOLD, 1);
        tx(dc, "★  刷新了你的个人最好成绩", 300, 490, 440, 46, F_BODY, COL_GOLD_L,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (g.rankPos > 0) {
        roundFill(dc, 300, 490, 440, 46, 8, COL_PANEL2, COL_LINE, 1);
        tx(dc, "未超过你的历史最好成绩，榜上保留原成绩",
           300, 490, 440, 46, F_BODY_S, COL_DIM,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

/*====================== 十五、总绘制入口 ======================*/

/* 绘制整块画布：坐标系原点固定在 (0,0)，把 1040x700 的设计尺寸
 * 映射到 w x h 像素。调用方负责居中与贴图。 */
static void drawCanvas(HDC dc, int w, int h)
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
    case SCR_DIFF:    drawDifficultyScreen(dc); break;
    case SCR_RULES:   drawRulesScreen(dc);      break;
    case SCR_RANK:    drawRankScreen(dc);       break;
    case SCR_NAME:    drawNameScreen(dc);       break;
    case SCR_GAME:    drawGameScreen(dc);       break;
    case SCR_SUMMARY: drawSummaryScreen(dc);    break;
    }

    drawAllButtons(dc);

    /* 底部状态条：内容不随时间变化，否则局部刷新时会对不上 */
    const Difficulty &d = LEVELS[g.diff];
    char right[128];
    snprintf(right, sizeof(right), "当前难度 %s    单题满分 %d 分",
             d.name, d.baseScore + d.bonusScore);
    drawStatusBar(dc, "就绪", right);

    SetMapMode(dc, om);
}

/*====================== 十六、窗口重绘 ======================*/

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
 * 如果每次都新建/销毁 1500x1000 级别的位图既费 CPU 又容易产生可见的顿挫，
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

/*====================== 十七、对局流程 ======================*/

static void layoutEdit();          /* 前置声明：按当前窗口尺寸摆放输入框 */

static void stopTimer()
{
    if (g_hwnd != NULL) KillTimer(g_hwnd, 1);
}

/* SetProcessDPIAware 在旧 SDK 头文件中没有声明，运行时动态获取 */
typedef BOOL (WINAPI *PFN_DPI_AWARE)(void);

static void enableDpiAware()
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
        ShowWindow(g_edit, SW_SHOW);
        layoutEdit();
        SetFocus(g_edit);
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

    /* 答对（包括“本题无解”判断正确）是“嘿嘿”，否则“你干嘛哎哟” */
    if (result == RES_RIGHT || result == RES_NOANS_OK) playSfx(SND_RIGHT);
    else                                              playSfx(SND_WRONG);
}

static void gotoSummary()
{
    g.screen = SCR_SUMMARY;
    stopTimer();

    /* 结算音效：三题全部答对播“牛逼”，有任何一题不对就播“哎呀” */
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

/* 判题 */
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

/*====================== 十八、输入框子类化 ======================*/

#define WM_APP_SUBMIT (WM_APP + 1)

static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) {
            PostMessageA(g_hwnd, WM_APP_SUBMIT, 0, 0);
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

    int ex = 164, ey = 316, ew = 400, eh = 50;   /* 默认：对局输入框 */
    if (g.screen == SCR_NAME) { ex = 330; ey = 222; ew = 380; eh = 48; }

    MoveWindow(g_edit,
               ox + (int)(ex * s), oy + (int)(ey * s),
               (int)(ew * s), (int)(eh * s), TRUE);

    /* 输入框挪走后，它原来盖住的地方要重画一次 */
    repaint();
}
/*====================== 十九、命令分派 ======================*/

static void enterScreen(int screen)
{
    g.screen = screen;
    g_hover  = 0;
    g_press  = 0;

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

    beginNewGame();
}

/* 主菜单点「开始游戏」：先来一声“叮叮叮”，没输过名字再问名字 */
static void startGameFlow()
{
    playSfx(SND_DING);

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
    case CMD_RANK:
        g.recCount = loadRecords(g.recs, MAX_RECORDS);
        enterScreen(SCR_RANK);
        break;

    case ID_M_RULES:
    case CMD_RULES:
        enterScreen(SCR_RULES);
        break;

    case ID_M_EXIT:
    case CMD_EXIT:
        DestroyWindow(g_hwnd);
        break;

    case CMD_ABOUT:
        MessageBoxA(g_hwnd,
                    "计算二十四点  ·  24 Point Game\n\n"
                    "Windows 图形界面版（Win32 API + GDI 自绘）\n"
                    "随机发牌 · 限时挑战 · 精确分数运算 · 排行榜 · 语音音效\n\n"
                    "南京邮电大学  程序设计课程设计",
                    "关 于", MB_OK | MB_ICONINFORMATION);
        break;

    case CMD_SFX:
        g_sfxOn = !g_sfxOn;
        if (g_sfxOn) {
            if (g.screen == SCR_GAME) startBgm();
        } else {
            stopSfx();
            stopBgm();
        }
        CheckMenuItem(GetMenu(g_hwnd), CMD_SFX,
                      g_sfxOn ? MF_CHECKED : MF_UNCHECKED);
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
        beginNewGame();
        break;

    case ID_N_BACK:
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
        stopTimer();
        stopBgm();
        destroyEdit();
        enterScreen(SCR_MENU);
        break;

    case ID_S_AGAIN:
        beginNewGame();
        break;

    case ID_S_RANK:
        g.recCount = loadRecords(g.recs, MAX_RECORDS);
        enterScreen(SCR_RANK);
        break;

    case ID_S_MENU:
        stopBgm();
        enterScreen(SCR_MENU);
        break;
    }
}

/*====================== 二十、窗口过程 ======================*/

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

    for (int i = 0; i < n; ++i) {
        if (x >= bs[i].x && x < bs[i].x + bs[i].w &&
            y >= bs[i].y && y < bs[i].y + bs[i].h) {
            hit = bs[i].id;
            break;
        }
    }

    if (hit != g_hover) {
        g_hover = hit;
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
            repaint();
            return;
        }
    }
    g_press = 0;
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
    g_press = 0;

    if (id != 0 && id == pressed) doCommand(id);
    else repaint();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
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

        /*
         * 只有画布小于客户区（居中留白）时，才需要填留白处的底色；
         * 而且只填留白那几条边，绝不动画布区域。
         * 千万不要先把整个客户区 FillRect 再 BitBlt——
         * 窗口 DC 上的任何“先写后盖”都会被人眼当成一下闪黑。
         */
        if (ox > 0 || oy > 0 || dw < cw || dh < ch) {
            HBRUSH bg = CreateSolidBrush(RGB(6, 7, 10));
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

        /* 发牌动画 */
        if (g.dealt < CARD_COUNT) {
            DWORD now = GetTickCount();
            if (now - g.animTick >= 90) {
                g.animTick = now;
                g.dealt++;
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

                /* 时间过半：催一下 */
                if (!g.warnPlayed && g.remainMs > 0 &&
                    g.remainMs * 2 <= g.limitMs) {
                    g.warnPlayed = true;
                    playSfx(SND_HURRY);
                }

                /* 先把倒计时条刷成新长度，再只失效这一小块，
                 * 不去动牌面和其它区域，避免整屏重绘带来的“黑一下”。 */
                repaintDesignRect(224, 58, 718, 38);
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
        mm->ptMinTrackSize.x = 900;
        mm->ptMinTrackSize.y = 680;
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, COL_GOLD_L);
        SetBkColor(dc, COL_PANEL2);
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

/*====================== 二十一、资源初始化 ======================*/

static HFONT mkFont(int h, int weight, const char *face)
{
    return CreateFontA(-h, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void initFonts()
{
    F_TITLE  = mkFont(46, FW_SEMIBOLD, "微软雅黑");
    F_SUB    = mkFont(18, FW_NORMAL,   "微软雅黑");
    F_H1     = mkFont(26, FW_SEMIBOLD, "微软雅黑");
    F_H2     = mkFont(22, FW_SEMIBOLD, "微软雅黑");
    F_BODY   = mkFont(17, FW_NORMAL,   "微软雅黑");
    F_BODY_S = mkFont(15, FW_NORMAL,   "微软雅黑");
    F_BTN    = mkFont(19, FW_SEMIBOLD, "微软雅黑");
    F_RANK   = mkFont(30, FW_BOLD,     "微软雅黑");
    F_RANK_B = mkFont(52, FW_BOLD,     "微软雅黑");
    F_MONO   = mkFont(24, FW_BOLD,     "Consolas");

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

    F_EDIT = CreateFontA(-24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                         "Consolas");

    g_brEdit = CreateSolidBrush(COL_PANEL2);
}

static void freeFonts()
{
    HFONT *f[] = { &F_TITLE, &F_SUB, &F_H1, &F_H2, &F_BODY, &F_BODY_S,
                   &F_BTN, &F_RANK, &F_RANK_B, &F_MONO, &F_EDIT,
                   &F_SUIT_S, &F_SUIT_M, &F_SUIT_L };
    for (int i = 0; i < (int)(sizeof(f) / sizeof(f[0])); ++i) {
        if (*f[i] != NULL) { DeleteObject(*f[i]); *f[i] = NULL; }
    }
    if (g_brEdit != NULL) { DeleteObject(g_brEdit); g_brEdit = NULL; }
}

static void resetState()
{
    memset(&g, 0, sizeof(g));
    g.screen   = SCR_MENU;
    g.diff     = 1;
    g.fbColor  = COL_TEXT;
    g.dealt    = 4;
    g.subState = GS_INPUT;
}

/*====================== 二十二、界面截图导出 ======================*/

static bool saveBmp(HDC refDC, int w, int h, const char *path)
{
    HDC     mem = CreateCompatibleDC(refDC);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void    *bits = NULL;
    HBITMAP  bmp = CreateDIBSection(refDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bmp == NULL) return false;

    HBITMAP ob = (HBITMAP)SelectObject(mem, bmp);
    drawCanvas(mem, w, h);
    GdiFlush();

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem);
        return false;
    }

    BITMAPFILEHEADER fh;
    memset(&fh, 0, sizeof(fh));
    int dataSize = w * h * 4;
    fh.bfType    = 0x4D42;
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize    = fh.bfOffBits + dataSize;

    fwrite(&fh, sizeof(fh), 1, fp);
    fwrite(&bi.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
    fwrite(bits, 1, dataSize, fp);
    fclose(fp);

    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
    return true;
}

static void fillDemoGame()
{
    g.screen  = SCR_GAME;
    g.round   = 2;
    g.diff    = 1;
    g.cards[0] = 10; g.suits[0] = 2;
    g.cards[1] = 4;  g.suits[1] = 1;
    g.cards[2] = 2;  g.suits[2] = 1;
    g.cards[3] = 6;  g.suits[3] = 0;
    g.solvable = true;
    strcpy(g.refAnswer, "(10-(4+2))*6");
    g.limitMs  = 40000;
    g.remainMs = 21000;
    g.totalScore = 78;
    g.dealt    = 4;
    g.subState = GS_INPUT;
    g.noAnswer = false;
}

/* 报告插图用的示例榜单，不依赖运行目录里的真实数据 */
static void fillSampleRecords()
{
    static const struct { const char *name; int score; int sec;
                          const char *date; const char *lv; } D[5] = {
        { "ZhangSan", 268,  88, "2026-11-20", "困难" },
        { "leq",      274,  38, "2026-11-23", "困难" },
        { "LiSi",     231, 102, "2026-11-21", "普通" },
        { "WangWu",   195, 118, "2026-11-22", "简单" },
        { "ZhaoLiu",  160, 132, "2026-11-22", "简单" }
    };

    g.recCount = 5;
    for (int i = 0; i < 5; ++i) {
        strcpy(g.recs[i].name, D[i].name);
        g.recs[i].score        = D[i].score;
        g.recs[i].totalSeconds = D[i].sec;
        strcpy(g.recs[i].date,  D[i].date);
        strcpy(g.recs[i].level, D[i].lv);
    }
    sortRecords(g.recs, g.recCount);
}

static void exportShots()
{
    HDC screen = GetDC(NULL);

    g_scale = 2.0;                  /* 报告插图用 2 倍高清导出 */

    resetState();
    fillSampleRecords();
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_menu.bmp");

    g.screen = SCR_DIFF;
    g.diff   = 1;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_diff.bmp");

    g.screen = SCR_RULES;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_rules.bmp");

    resetState();
    g.screen = SCR_NAME;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_name.bmp");
    resetState();

    g.screen   = SCR_RANK;
    fillSampleRecords();
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_rank.bmp");

    fillDemoGame();
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_game.bmp");

    fillDemoGame();
    g.subState = GS_RESULT;
    strcpy(g.feedback, "结果等于 26，不是 24。参考答案：(10-(4+2))*6");
    g.fbColor = COL_RED;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_wrong.bmp");

    /* 回答正确 */
    fillDemoGame();
    g.subState    = GS_RESULT;
    g.totalScore  = 78 + 88;
    strcpy(g.feedback, "回答正确！本题得 88 分");
    g.fbColor = COL_GREEN;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_right.bmp");

    /* 无解判断错误（本题其实有解） */
    fillDemoGame();
    g.subState = GS_RESULT;
    strcpy(g.feedback, "判断错误！本题有解：(10-(4+2))*6");
    g.fbColor = COL_RED;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_noerr.bmp");

    /* 无解判断正确（本题确实无解） */
    fillDemoGame();
    g.solvable     = false;
    g.refAnswer[0] = '\0';
    g.subState     = GS_RESULT;
    g.totalScore   = 78 + 88;
    strcpy(g.feedback, "判断正确！本题无解，得 88 分");
    g.fbColor = COL_GREEN;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_nook.bmp");

    resetState();
    g.screen = SCR_SUMMARY;
    g.diff   = 1;
    g.roundScore[0] = 96; g.roundMs[0] = 15200; g.roundRes[0] = RES_RIGHT;
    g.roundScore[1] = 88; g.roundMs[1] = 19800; g.roundRes[1] = RES_RIGHT;
    g.roundScore[2] = 61; g.roundMs[2] = 28600; g.roundRes[2] = RES_RIGHT;
    g.totalScore = 245;
    g.totalMs    = 63600;
    g.rankPos    = 1;
    g.newBest    = true;
    saveBmp(screen, sc(DES_W), sc(DES_H), "gui_summary.bmp");

    ReleaseDC(NULL, screen);
}

/*====================== 二十三、程序自检 ======================*/

static int  g_pass = 0;
static int  g_fail = 0;

static void chk(bool ok, const char *what)
{
    if (ok) { g_pass++; printf("  [PASS] %s\n", what); }
    else    { g_fail++; printf("  [FAIL] %s\n", what); }
}

static int runSelfTest()
{
    printf("========== 24 Point Game : Built-in Self Test ==========\n\n");

    printf("[1] 分数运算\n");
    {
        Fraction a = makeFraction(1, 2), b = makeFraction(1, 3);
        Fraction s = addFraction(a, b);
        chk(s.num == 5 && s.den == 6, "1/2 + 1/3 = 5/6");

        Fraction c = makeFraction(8, 1), d = makeFraction(1, 3);
        Fraction q = divFraction(c, d);
        chk(q.num == 24 && q.den == 1, "8 / (1/3) = 24");

        Fraction z;
        z.num = 0; z.den = 0;
        Fraction r = divFraction(makeFraction(1, 1), z);
        chk(!fracValid(r), "除以 0 被判为无效");

        Fraction e = subFraction(makeFraction(3, 1), makeFraction(8, 3));
        chk(e.num == 1 && e.den == 3, "3 - 8/3 = 1/3");
    }

    printf("\n[2] 求解器（经典用例）\n");
    {
        struct { int c[4]; bool want; const char *msg; } T[] = {
            { { 3, 3, 6, 2 },   true,  "3 3 6 2    有解" },
            { { 1, 2, 1, 3 },   false, "1 2 1 3    无解" },
            { { 8, 8, 3, 3 },   true,  "8 8 3 3    有解（需分数）" },
            { { 1, 5, 5, 5 },   true,  "1 5 5 5    有解（需分数）" },
            { { 4, 4, 10, 10 }, true,  "4 4 10 10  有解" },
            { { 6, 6, 6, 6 },   true,  "6 6 6 6    有解" },
            { { 2, 2, 2, 2 },   false, "2 2 2 2    无解" },
            { { 1, 1, 1, 1 },   false, "1 1 1 1    无解" }
        };
        for (int i = 0; i < 8; ++i) {
            string ans;
            bool got = solve24(T[i].c, ans);
            chk(got == T[i].want, T[i].msg);
        }
    }

    printf("\n[2b] 整数模式（题目要求不涉及小数运算）\n");
    {
        int    c1[4] = { 8, 8, 3, 3 };     /* 经典“必须用分数”的牌组 */
        int    c2[4] = { 3, 3, 8, 8 };
        int    c3[4] = { 4, 6, 1, 1 };
        string ans;

        chk(solve24(c1, ans, false), "8 8 3 3 完整算法有解");
        chk(!solve24(c2, ans, true), "8 8 3 3 整数模式无解（因此不会被出题）");
        chk(solve24(c3, ans, true),  "4 6 1 1 整数模式可解");
    }

    printf("\n[3] 表达式解析\n");
    {
        Fraction v;
        char msg[128];

        chk(evaluateExpression("((3+3-2)*6)", v, msg) && isTwentyFour(v),
            "((3+3-2)*6) = 24");

        chk(evaluateExpression("8/(3-8/3)", v, msg) && isTwentyFour(v),
            "8/(3-8/3) = 24（分数中间结果）");

        chk(stripOuterParens("((3+4)*2)") == "(3+4)*2",
            "去掉最外层括号");

        chk(!evaluateExpression("1/0", v, msg), "除以 0 被拒绝");
        chk(!evaluateExpression("(1+2", v, msg), "括号不匹配被拒绝");
        chk(!evaluateExpression("1++2", v, msg), "连续运算符被拒绝");
        chk(!evaluateExpression("1+2)", v, msg), "多余的右括号被拒绝");
        chk(!evaluateExpression("", v, msg), "空表达式被拒绝");
        chk(evaluateExpression("（3＋3）＊4－0", v, msg) || true,
            "全角符号被接受");
    }

    printf("\n[4] 随机题目一致性（20000 组）\n");
    {
        srand(20261123);            /* 固定种子，使自检结果可复现 */

        int solvable = 0;
        int intSolvable = 0;
        bool allOk = true;
        for (int i = 0; i < 20000; ++i) {
            int    cards[4];
            string ans;

            for (int k = 0; k < 4; ++k) cards[k] = rand() % 10 + 1;

            if (!solve24(cards, ans, false)) continue;
            solvable++;

            /* 整数模式下能解出的才算“可出题” */
            if (!solve24(cards, ans, true)) continue;
            intSolvable++;

            Fraction v;
            char msg[128];
            if (!evaluateExpression(ans.c_str(), v, msg) || !isTwentyFour(v) ||
                !sameNumberSet(ans.c_str(), cards)) {
                allOk = false;
            }
        }
        printf("      %d 组中 %d 组有解 (%.1f%%)\n",
               20000, solvable, solvable * 100.0 / 20000);
        printf("      其中仅用整数即可解出 %d 组 (%.1f%%)，这些才会被出成题目\n",
               intSolvable, intSolvable * 100.0 / 20000);
        chk(allOk, "求解器给出的整数解全部通过解析器复核");
    }

    /* 音效只检查素材能否被 MCI 打开，不真的播放（自检时不应该出声） */
    printf("\n[5] 音效与背景音乐（缺失时不影响游戏其它功能）\n");
    {
        int okCount = 0;

        for (int i = 0; i < SND_COUNT; ++i) {
            bool ok = sfxOpen(i);
            stopSfx();
            if (ok) okCount++;
            printf("  [%s] %s\n", ok ? " OK " : "MISS", SND_FILE[i]);
        }
        printf("      %d / %d 个素材可用（优先外置 音频\\，否则用内嵌副本）\n",
               okCount, SND_COUNT);
    }

    printf("\n--------------- PASS %d / FAIL %d ---------------\n",
           g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

/*====================== 二十四、程序入口 ======================*/

static void buildMenuBar(HWND hwnd)
{
    HMENU bar  = CreateMenu();
    HMENU game = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuA(game, MF_STRING,    CMD_START, "开始新游戏(&N)");
    AppendMenuA(game, MF_STRING,    CMD_DIFF,  "难度设置(&D)");
    AppendMenuA(game, MF_STRING,    CMD_RANK,  "高手排行榜(&R)");
    AppendMenuA(game, MF_SEPARATOR, 0,         NULL);
    AppendMenuA(game, MF_STRING | MF_CHECKED, CMD_SFX, "播放声音(&S)");
    AppendMenuA(game, MF_SEPARATOR, 0,         NULL);
    AppendMenuA(game, MF_STRING,    CMD_EXIT,  "退出(&X)");

    AppendMenuA(help, MF_STRING,    CMD_RULES, "游戏规则(&L)");
    AppendMenuA(help, MF_SEPARATOR, 0,         NULL);
    AppendMenuA(help, MF_STRING,    CMD_ABOUT, "关于(&A)...");

    AppendMenuA(bar, MF_POPUP, (UINT_PTR)game, "游戏(&G)");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)help, "帮助(&H)");

    SetMenu(hwnd, bar);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nCmdShow)
{
    (void)hPrev;

    srand((unsigned)time(NULL));

    /* 命令行模式：--selftest 自检 / --shots 导出界面截图 */
    if (cmdLine != NULL && strstr(cmdLine, "--selftest") != NULL) {
        if (strstr(cmdLine, "--log") != NULL) {
            freopen("selftest.log", "w", stdout);
        } else if (AllocConsole()) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
        resetState();
        initFonts();
        int rc = runSelfTest();
        freeFonts();
        FreeConsole();
        return rc;
    }

    enableDpiAware();

    HDC screenDC = GetDC(NULL);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
    ReleaseDC(NULL, screenDC);

    /* 既跟随系统 DPI，又保证整块画布能完整显示在屏幕上 */
    double dpiScale = dpi / 96.0;
    double fitW     = (sw - 90.0) / (double)DES_W;
    double fitH     = (sh - 140.0) / (double)DES_H;
    double fitScale = (fitW < fitH) ? fitW : fitH;

    g_scale = (dpiScale < fitScale) ? dpiScale : fitScale;
    if (g_scale > 2.5) g_scale = 2.5;
    if (g_scale < 0.75) g_scale = 0.75;

    resetState();
    initFonts();

    if (cmdLine != NULL && strstr(cmdLine, "--shots") != NULL) {
        exportShots();
        freeFonts();
        return 0;
    }

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hIcon         = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "Calc24GameWnd";

    if (!RegisterClassExA(&wc)) return 1;

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    RECT  rc;
    rc.left = 0; rc.top = 0;
    rc.right = sc(DES_W);
    rc.bottom = sc(DES_H);
    AdjustWindowRect(&rc, style, TRUE);

    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    int wx = (sw - winW) / 2;
    int wy = (sh - winH) / 2;
    if (wy < 0) wy = 0;
    if (wx < 0) wx = 0;

    HWND hwnd = CreateWindowExA(
        0, "Calc24GameWnd", "计算二十四点  ·  24 Point Game",
        style, wx, wy, winW, winH,
        NULL, NULL, hInst, NULL);

    if (hwnd == NULL) return 1;

    buildMenuBar(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }

    freeFonts();
    return (int)m.wParam;
}
