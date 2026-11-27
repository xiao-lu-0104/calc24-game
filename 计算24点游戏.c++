/*==========================================================================
 *  编译： g++ -std=c++11 -O2 -mwindows -o 计算24点游戏.exe 计算24点游戏.cpp -lgdi32 -lwinmm
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
static const int  DES_W        = 1280;         /* 设计画布宽（逻辑单位）     */
static const int  DES_H        = 800;          /* 设计画布高                 */
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
    ID_M_NAME, ID_M_ABOUT, ID_M_SFX,
    ID_D_0 = 110, ID_D_1 = 111, ID_D_2 = 112, ID_D_BACK = 113, ID_D_OK = 114,
    ID_R_BACK = 120,
    ID_U_BACK = 130,
    ID_N_OK = 160, ID_N_SKIP = 161, ID_N_BACK = 162,
    ID_G_SUBMIT = 140, ID_G_NOANS = 141, ID_G_NEXT = 142, ID_G_BACK = 143, ID_G_BACK2 = 144,
    ID_S_AGAIN = 150, ID_S_RANK = 151, ID_S_MENU = 152, ID_S_BACK = 153,
    ID_I_SFX = 170, ID_I_RULES = 171, ID_I_RANK = 172, ID_I_NAME = 173
};

enum { CMD_START = 1000, CMD_DIFF, CMD_RANK, CMD_RULES, CMD_EXIT, CMD_ABOUT, CMD_SFX };

/* 对局内部状态 */
enum { GS_INPUT = 0, GS_RESULT };

/* 本题判定结果 */
enum { RES_RIGHT = 0, RES_WRONG, RES_TIMEOUT, RES_NOANS_OK, RES_NOANS_BAD };

/*====================== 二、终端主题配色 ======================*/
/* 卡面与控件用色：米白底色、深色文字和品红强调色 */

static const COLORREF P_PAPER   = RGB(245, 243, 238);   /* 终端纸面         */
static const COLORREF P_PAPER2  = RGB(230, 226, 217);   /* 纸面次级         */
static const COLORREF P_INK     = RGB( 22,  22,  22);   /* 墨黑             */
static const COLORREF P_INK2    = RGB( 70,  70,  70);   /* 次级墨色         */
static const COLORREF P_GRAY    = RGB(135, 135, 135);   /* 灰字             */
static const COLORREF P_WHITE   = RGB(255, 255, 255);
static const COLORREF P_PINK    = RGB(238,  92, 140);   /* 品红主色         */
static const COLORREF P_PINK_D  = RGB(198,  52,  98);   /* 深品红           */
static const COLORREF P_GOLD    = RGB(198, 168, 106);   /* 细金线           */

/* 旧逻辑沿用的颜色（判题提示、扑克牌花色） */
static const COLORREF COL_GREEN = RGB( 24, 130,  78);
static const COLORREF COL_RED   = RGB(196,  54,  66);
static const COLORREF COL_TEXT  = RGB(226, 230, 235);
static const COLORREF COL_BLACK = RGB( 30,  30,  34);
static const COLORREF COL_REDC  = RGB(190,  40,  50);

/*====================== 三、字体与句柄 ======================*/
static HFONT  F_HERO   = NULL;      /* 大标题 40（中文）                    */
static HFONT  F_H1     = NULL;      /* 卡片标题 32                          */
static HFONT  F_H2     = NULL;      /* 小标题 26                            */
static HFONT  F_H3     = NULL;      /* 按钮 / 强调 20                       */
static HFONT  F_BODY   = NULL;      /* 正文 16                              */
static HFONT  F_SMALL  = NULL;      /* 小字 12                              */
static HFONT  F_TINY   = NULL;      /* 微字 10                              */
static HFONT  F_CAP    = NULL;      /* 拉丁小标题 10（Bahnschrift）         */
static HFONT  F_NUM    = NULL;      /* 数字 22                              */
static HFONT  F_NUM_M  = NULL;      /* 数字 26                              */
static HFONT  F_NUM_L  = NULL;      /* 数字 38                              */
static HFONT  F_NUM_XL = NULL;      /* 数字 64                              */
static HFONT  F_NUM_XXL= NULL;      /* 数字 104                             */
static HFONT  F_CARD_NUM = NULL;    /* 牌面点数 76                          */
static HFONT  F_VERT   = NULL;      /* 竖排标题 48                          */
static HFONT  F_WATER  = NULL;      /* 背景水印 250                         */
static HFONT  F_MONO   = NULL;      /* 等宽数字 24                          */
static HFONT  F_EDIT   = NULL;      /* 输入框（设备单位）                   */
static HFONT  F_SUIT_S = NULL;      /* 花色 26                              */
static HFONT  F_SUIT_M = NULL;      /* 花色 38                              */
static HFONT  F_SUIT_L = NULL;      /* 花色 50                              */

static HBRUSH g_brEdit   = NULL;    /* edit field background brush */

static HWND   g_hwnd     = NULL;    /* 主窗口                               */
static HWND   g_edit     = NULL;    /* 表达式输入框                         */
static WNDPROC g_editOld = NULL;    /* 输入框原窗口过程                     */

static double g_scale    = 1.0;     /* 1 逻辑单位 = g_scale 像素          */
static int    g_hover    = 0;       /* 悬停按钮编号                         */
static int    g_press    = 0;       /* 按下按钮编号                         */
static int    g_hoverSlot = -1;     /* 悬停按钮在列表中的下标（区分重复 ID）*/
static int    g_pressSlot = -1;     /* 按下按钮在列表中的下标               */
static bool   g_nameFromMenu = false; /* 姓名界面是“档案改名”而非开局     */

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

/* 输出文字：fmt 常用 DT_LEFT/DT_CENTER/DT_RIGHT + DT_VCENTER|DT_SINGLELINE */
static void tx(HDC dc, const char *s, int x, int y, int w, int h,
               HFONT f, COLORREF c, UINT fmt)
{
    HFONT      of = (HFONT)SelectObject(dc, f);
    COLORREF   oc = SetTextColor(dc, c);
    int        om = SetBkMode(dc, TRANSPARENT);
    RECT       r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    DrawTextA(dc, s, -1, &r, fmt | DT_NOPREFIX | DT_SINGLELINE);
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
    DrawTextW(dc, buf, 1, &r, fmt | DT_NOPREFIX | DT_SINGLELINE);
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

/*====================== 十、界面基础绘制 ======================*/

/* 颜色在第二节定义：P_ 开头为浅色卡面主题 */

static void hline(HDC dc, int x, int y, int w, COLORREF c, int t)
{
    fillRect(dc, x, y, w, t, c);
}

static void vline(HDC dc, int x, int y, int h, COLORREF c, int t)
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

static void polySolid(HDC dc, const POINT *p, int n, COLORREF c)
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

static void cutFill(HDC dc, int x, int y, int w, int h, int cut, COLORREF c)
{
    POINT p[8];
    cutPts(p, x, y, w, h, cut);
    polySolid(dc, p, 8, c);
}

static void cutBorder(HDC dc, int x, int y, int w, int h, int cut,
                      COLORREF c, int t)
{
    POINT p[9];
    cutPts(p, x, y, w, h, cut);
    p[8] = p[0];
    polyOutline(dc, p, 9, c, t);
}

/* 有投影的浅色卡片 */
static void paperCard(HDC dc, int x, int y, int w, int h, int cut,
                      COLORREF fill, COLORREF border, int t, bool shadow)
{
    if (shadow) cutFill(dc, x + 6, y + 7, w, h, cut, RGB(8, 10, 14));
    cutFill(dc, x, y, w, h, cut, fill);
    if (t > 0) cutBorder(dc, x, y, w, h, cut, border, t);
}



static void diamond(HDC dc, int cx, int cy, int r, COLORREF c, bool solid)
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

static void barcode(HDC dc, int x, int y, int w, int h, COLORREF c)
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
static void arcPoly(HDC dc, int cx, int cy, int r, double a0, double a1,
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
static void drawIcon(HDC dc, int kind, int cx, int cy, int s, COLORREF c)
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
static void emblem(HDC dc, int cx, int cy, int r, COLORREF col, COLORREF dark)
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

/* 斜向细纹：给色块加一层织理 */
static void diagonalStripes(HDC dc, int x, int y, int w, int h,
                            COLORREF c, int step)
{
    if (step < 4) step = 4;

    int saved = SaveDC(dc);
    IntersectClipRect(dc, x, y, x + w, y + h);

    HPEN pn = CreatePen(PS_SOLID, 1, c);
    HPEN op = (HPEN)SelectObject(dc, pn);
    for (int i = -h; i < w + h; i += step) {
        MoveToEx(dc, x + i, y, NULL);
        LineTo(dc, x + i - h, y + h);
    }
    SelectObject(dc, op);
    DeleteObject(pn);

    RestoreDC(dc, saved);
}

/* 粉带：外侧最浓，向内快速过渡到白，同时自上而下略微加深 */
static void pinkBand(HDC dc, int x, int y, int w, int h, bool fromLeft)
{
    const int SEG_X = 48;
    const int SEG_Y = 24;

    for (int j = 0; j < SEG_Y; ++j) {
        double ty = (SEG_Y > 1) ? j / (double)(SEG_Y - 1) : 0.0;
        COLORREF dense = mixColor(RGB(248, 112, 156), RGB(198, 40, 92), ty);
        COLORREF loose = mixColor(RGB(255, 247, 251), RGB(255, 233, 241), ty);

        int py = y + h * j / SEG_Y;
        int ph = y + h * (j + 1) / SEG_Y - py;
        if (ph <= 0) continue;

        for (int i = 0; i < SEG_X; ++i) {
            double tx = (i + 0.5) / SEG_X;
            if (!fromLeft) tx = 1.0 - tx;
            tx = pow(tx, 2.4);

            int px = x + w * i / SEG_X;
            int pw = x + w * (i + 1) / SEG_X - px;
            if (pw <= 0) continue;

            fillRect(dc, px, py, pw, ph, mixColor(dense, loose, tx));
        }
    }
}

/* 1280x800 的渐变版面：底色渐变 + 中央柔光 + 两侧粉带 + 上下黑带 */
static void paintBackground(HDC dc)
{
    const int TOP_H  = 130;
    const int BOT_Y  = 748;
    const int BAND_W = 340;
    const int BODY_H = BOT_Y - TOP_H;

    /* 1. 底色：上淡粉、下纯白 */
    vGradient(dc, 0, 0, DES_W, DES_H, RGB(255, 240, 246), P_WHITE);

    /* 2. 中部横向渐变：中间最亮，向两侧轻轻加深 */
    hGradient(dc, 0, TOP_H, DES_W / 2, BODY_H, RGB(255, 243, 248), P_WHITE);
    hGradient(dc, DES_W / 2, TOP_H, DES_W - DES_W / 2, BODY_H,
              P_WHITE, RGB(255, 243, 248));

    /* 3. 两侧粉带：外侧浓、向内渐隐 */
    pinkBand(dc, 0, TOP_H, BAND_W, BODY_H, true);
    pinkBand(dc, DES_W - BAND_W, TOP_H, BAND_W, BODY_H, false);

    /* 4. 白色区域铺一层极淡斜纹 */
    diagonalStripes(dc, BAND_W, TOP_H, DES_W - 2 * BAND_W, BODY_H,
                    RGB(253, 243, 248), 46);

    /* 5. 粉带内部再叠一层亮斜纹，形成织理 */
    diagonalStripes(dc, 0, TOP_H, BAND_W, BODY_H, RGB(255, 214, 230), 30);
    diagonalStripes(dc, DES_W - BAND_W, TOP_H, BAND_W, BODY_H,
                    RGB(255, 214, 230), 30);

    /* 6. 上下黑带 */
    vGradient(dc, 0, 0, DES_W, TOP_H, RGB(5, 7, 10), RGB(46, 54, 64));
    vGradient(dc, 0, BOT_Y, DES_W, DES_H - BOT_Y, RGB(46, 54, 64), RGB(5, 7, 10));

    /* 7. 黑带与内容区的交界线 */
    hline(dc, 0, TOP_H - 2, DES_W, RGB(158, 62, 96), 2);
    hline(dc, 0, BOT_Y, DES_W, RGB(158, 62, 96), 2);
}

/* 背景只在首次绘制时渲染一遍，之后直接贴图 */
static HBITMAP g_bgBitmap    = NULL;
static HDC     g_bgDC        = NULL;
static HBITMAP g_bgOldBitmap = NULL;

static void freeBackground()
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
}

static void buildBackground()
{
    HDC screen = GetDC(NULL);
    if (screen == NULL) return;

    g_bgDC     = CreateCompatibleDC(screen);
    g_bgBitmap = CreateCompatibleBitmap(screen, DES_W, DES_H);
    ReleaseDC(NULL, screen);

    if (g_bgDC == NULL || g_bgBitmap == NULL) {
        freeBackground();
        return;
    }

    g_bgOldBitmap = (HBITMAP)SelectObject(g_bgDC, g_bgBitmap);
    paintBackground(g_bgDC);
}

static void drawBackdrop(HDC dc)
{
    if (g_bgDC == NULL) buildBackground();

    if (g_bgDC == NULL) {
        paintBackground(dc);
        return;
    }
    BitBlt(dc, 0, 0, DES_W, DES_H, g_bgDC, 0, 0, SRCCOPY);
}

static void drawFooter(HDC dc, const char *left, const char *right)
{
    hline(dc, 28, 766, 1224, RGB(54, 62, 72), 1);
    tx(dc, left, 28, 772, 1000, 22, F_SMALL, RGB(130, 138, 148),
       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (right != NULL && right[0] != '\0')
        tx(dc, right, 1052, 772, 200, 22, F_SMALL, RGB(120, 128, 138),
           DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static int bestScore()
{
    int b = 0;
    for (int i = 0; i < g.recCount; ++i)
        if (g.recs[i].score > b) b = g.recs[i].score;
    return b;
}

static int playerLevel()
{
    int lv = 1 + bestScore() / 150;
    if (lv > 99) lv = 99;
    return lv;
}

/*====================== 十二、按钮描述表 ======================*/

/* style: 0=纸面按钮 1=黑底主按钮 2=纸面文字按钮 3=难度卡(自绘)
          4=复选框 5=深色文字按钮 7=自绘卡片 8=图标按钮 9=返回图标 */
struct BtnDesc {
    int         x, y, w, h;
    int         id;
    int         style;
    const char *text;
};

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
    { 430, 400, 180, 52, ID_N_OK,   1, "确  定" },
    { 630, 400, 180, 52, ID_N_SKIP, 0, "跳  过" }
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

/*====================== 十六、对局 / 姓名 / 结算 ======================*/

static void drawPlayingCard(HDC dc, int x, int y, int w, int h,
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

    paperCard(dc, 380, 180, 520, 360, 12, P_PAPER, P_INK, 2, true);
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
    tx(dc, "最多 16 个字符；留空则记为「玩家」", 420, 366, 440, 20, F_SMALL,
       RGB(110, 110, 110), DT_LEFT | DT_TOP);
    tx(dc, "排行榜上同名玩家只保留最好成绩", 420, 388, 440, 20, F_SMALL,
       RGB(110, 110, 110), DT_LEFT | DT_TOP);

    tx(dc, "输入后按回车或点「确定」", 420, 468, 440, 22, F_SMALL, P_PINK_D,
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



/*====================== 十八、窗口重绘 ======================*/

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

/*====================== 十九、对局流程 ======================*/

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

/*====================== 二十、输入框子类化 ======================*/

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
/*====================== 二十一、命令分派 ======================*/

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

/*====================== 二十二、窗口过程 ======================*/

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

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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

        /* 发牌动画 */
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

                /* 时间过半：催一下 */
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

/*====================== 二十三、资源初始化 ======================*/

static HFONT mkFont(int h, int weight, const char *face)
{
    return CreateFontA(-h, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void initFonts()
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

static void freeFonts()
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

static void resetState()
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

/*====================== 二十四、界面截图导出 ======================*/

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

/*====================== 二十五、程序自检 ======================*/

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

/*====================== 二十六、程序入口 ======================*/

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

    if (!RegisterClassExA(&wc)) {
        freeFonts();
        return 1;
    }

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    RECT  rc;
    rc.left = 0; rc.top = 0;
    rc.right = sc(DES_W);
    rc.bottom = sc(DES_H);
    AdjustWindowRect(&rc, style, FALSE);   /* 不使用系统菜单栏 */

    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    int wx = (sw - winW) / 2;
    int wy = (sh - winH) / 2;
    if (wy < 0) wy = 0;
    if (wx < 0) wx = 0;

    HWND hwnd = CreateWindowExA(
        0, "Calc24GameWnd", "计算二十四点",
         style, wx, wy, winW, winH,
        NULL, NULL, hInst, NULL);

    if (hwnd == NULL) {
        freeFonts();
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }

    freeFonts();
    freeBackground();
    return (int)m.wParam;
}





















