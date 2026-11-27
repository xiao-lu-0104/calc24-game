/*==========================================================================
 *  核心逻辑：有理数运算 / 24 点求解器 / 表达式解析 / 计分与排行榜
 *  不依赖任何界面状态，可脱离 Win32 单独编译测试。
 *========================================================================*/

#include "calc24.h"

void fillRect(HDC dc, int x, int y, int w, int h, COLORREF c)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    HBRUSH br = CreateSolidBrush(c);
    FillRect(dc, &r, br);
    DeleteObject(br);
}

/* 输出文字：fmt 常用 DT_LEFT/DT_CENTER/DT_RIGHT + DT_VCENTER|DT_SINGLELINE */
void tx(HDC dc, const char *s, int x, int y, int w, int h,
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
void txChar(HDC dc, wchar_t wc, int x, int y, int w, int h,
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
wchar_t suitChar(int suit)
{
    static const wchar_t S[4] = { 0x2660, 0x2665, 0x2666, 0x2663 };
    return S[suit & 3];
}

COLORREF suitColor(int suit)
{
    return (suit == 1 || suit == 2) ? COL_REDC : COL_BLACK;
}

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

Fraction makeFraction(long long n, long long d)
{
    Fraction f;
    if (d == 0) { f.num = 0; f.den = 0; return f; }
    if (d < 0)  { n = -n; d = -d; }
    long long g = gcdLL(n, d);
    f.num = n / g;
    f.den = d / g;
    return f;
}

bool fracValid(const Fraction &f) { return f.den != 0; }

Fraction addFraction(Fraction a, Fraction b)
{
    return makeFraction(a.num * b.den + b.num * a.den, a.den * b.den);
}

Fraction subFraction(Fraction a, Fraction b)
{
    return makeFraction(a.num * b.den - b.num * a.den, a.den * b.den);
}

static Fraction mulFraction(Fraction a, Fraction b)
{
    return makeFraction(a.num * b.num, a.den * b.den);
}

Fraction divFraction(Fraction a, Fraction b)
{
    if (b.num == 0) { Fraction z; z.num = 0; z.den = 0; return z; }
    return makeFraction(a.num * b.den, a.den * b.num);
}

bool isTwentyFour(Fraction f) { return f.den == 1 && f.num == 24; }

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
string stripOuterParens(const string &s)
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
bool solve24(const int cards[CARD_COUNT], string &answer,
                    bool intOnly )
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
void generateCards(int cards[CARD_COUNT], int suits[CARD_COUNT],
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
bool evaluateExpression(const char *input, Fraction &out, char *msg)
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
bool sameNumberSet(const char *expr, const int cards[CARD_COUNT])
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

int calculateScore(const Difficulty &diff, int remainSec)
{
    if (remainSec < 0) remainSec = 0;
    if (remainSec > diff.limitSec) remainSec = diff.limitSec;
    return diff.baseScore + diff.bonusScore * remainSec / diff.limitSec;
}

int compareRecord(const Record &a, const Record &b)
{
    if (a.score != b.score) return a.score > b.score;
    return a.totalSeconds < b.totalSeconds;
}

void sortRecords(Record list[], int count)
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

void normalizeRecords(Record list[], int &count)
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
const char *appDir()
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
const char *rankFilePath()
{
    static char path[MAX_PATH + 32];

    if (path[0] == '\0')
        snprintf(path, sizeof(path), "%s%s", appDir(), RANK_FILE);

    return path;
}
