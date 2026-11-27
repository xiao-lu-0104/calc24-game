/*==========================================================================
 *  计算二十四点  ——  公共头文件
 *
 *  只声明被两个以上模块使用的常量、类型、全局状态与函数；
 *  各 .cpp 内部的辅助函数仍保留 static。
 *========================================================================*/

#ifndef CALC24_H
#define CALC24_H

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
static const char *SND_FILE[SND_COUNT] __attribute__((unused)) = {
    "阿姨你快一点啦阿姨.mp3",
    "嘿嘿.mp3",
    "你干嘛哎哟.mp3",
    "牛逼啊这个人 有操作的呀.mp3",
    "哎呀.mp3",
    "背景音乐.mp3",
    "叮叮叮.mp3"
};

static const int SND_RESID[SND_COUNT] __attribute__((unused)) = { 102, 103, 104, 105, 106, 101, 107 };

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
extern HFONT     F_HERO;   /* 大标题 40（中文）                    */
extern HFONT     F_H1;   /* 卡片标题 32                          */
extern HFONT     F_H2;   /* 小标题 26                            */
extern HFONT     F_H3;   /* 按钮 / 强调 20                       */
extern HFONT     F_BODY;   /* 正文 16                              */
extern HFONT     F_SMALL;   /* 小字 12                              */
extern HFONT     F_TINY;   /* 微字 10                              */
extern HFONT     F_CAP;   /* 拉丁小标题 10（Bahnschrift）         */
extern HFONT     F_NUM;   /* 数字 22                              */
extern HFONT     F_NUM_M;   /* 数字 26                              */
extern HFONT     F_NUM_L;   /* 数字 38                              */
extern HFONT     F_NUM_XL;   /* 数字 64                              */
extern HFONT     F_NUM_XXL;   /* 数字 104                             */
extern HFONT     F_CARD_NUM;   /* 牌面点数 76                          */
extern HFONT     F_VERT;   /* 竖排标题 48                          */
extern HFONT     F_WATER;   /* 背景水印 250                         */
extern HFONT     F_MONO;   /* 等宽数字 24                          */
extern HFONT     F_EDIT;   /* 输入框（设备单位）                   */
extern HFONT     F_SUIT_S;   /* 花色 26                              */
extern HFONT     F_SUIT_M;   /* 花色 38                              */
extern HFONT     F_SUIT_L;   /* 花色 50                              */

extern HBRUSH    g_brEdit;   /* edit field background brush */

extern HWND      g_hwnd;   /* 主窗口                               */
extern HWND      g_edit;   /* 表达式输入框                         */
extern WNDPROC   g_editOld;   /* 输入框原窗口过程                     */

extern double    g_scale;   /* 1 逻辑单位 = g_scale 像素          */
extern int       g_hover;   /* 悬停按钮编号                         */
extern int       g_press;   /* 按下按钮编号                         */
extern int       g_hoverSlot;   /* 悬停按钮在列表中的下标（区分重复 ID）*/
extern int       g_pressSlot;   /* 按下按钮在列表中的下标               */
extern bool      g_nameFromMenu;   /* 姓名界面是“档案改名”而非开局     */

/* 逻辑单位 -> 设备像素（仅用于初始窗口尺寸、截图等场合） */

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

extern GameState g;


/*====================== 跨模块使用的数据类型 ======================*/

struct BtnDesc {
    int         x, y, w, h;
    int         id;
    int         style;
    const char *text;
};

/*====================== 缩放工具（定义在 app.cpp） ======================*/

int sc(int v);                   /* 逻辑单位 -> 设备像素            */
double viewScale(int cw, int ch);/* 按客户区尺寸算等比缩放系数     */

/*====================== 跨模块全局变量 ======================*/


/*====================== 跨模块函数 ======================*/

extern bool g_sfxOn;                    /* 音效总开关（定义在 audio.cpp）  */
const char *appDir();                   /* exe 所在目录（含尾部反斜杠）    */
const char *rankFilePath();             /* rank.txt 的完整路径             */
const BtnDesc *curButtons(int &n);      /* 当前界面的按钮表                */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);


Fraction addFraction(Fraction a, Fraction b);
void arcPoly(HDC dc, int cx, int cy, int r, double a0, double a1, COLORREF c, int t);
void barcode(HDC dc, int x, int y, int w, int h, COLORREF c);
int bestScore();
int calculateScore(const Difficulty &diff, int remainSec);
int compareRecord(const Record &a, const Record &b);
void cutBorder(HDC dc, int x, int y, int w, int h, int cut, COLORREF c, int t);
void cutFill(HDC dc, int x, int y, int w, int h, int cut, COLORREF c);
void diamond(HDC dc, int cx, int cy, int r, COLORREF c, bool solid);
Fraction divFraction(Fraction a, Fraction b);
void drawBackdrop(HDC dc);
void drawCanvas(HDC dc, int w, int h);
void drawFooter(HDC dc, const char *left, const char *right);
void drawIcon(HDC dc, int kind, int cx, int cy, int s, COLORREF c);
void drawPlayingCard(HDC dc, int x, int y, int w, int h, int value, int suit, bool faceDown, int idx);
void emblem(HDC dc, int cx, int cy, int r, COLORREF col, COLORREF dark);
void enableDpiAware();
bool evaluateExpression(const char *input, Fraction &out, char *msg);
void exportShots();
void fillRect(HDC dc, int x, int y, int w, int h, COLORREF c);
bool fracValid(const Fraction &f);
void freeBackground();
void freeFonts();
void generateCards(int cards[CARD_COUNT], int suits[CARD_COUNT], bool wantSolvable);
void hline(HDC dc, int x, int y, int w, COLORREF c, int t);
void initFonts();
int insertRecord(Record list[], int &count, const Record &rec, bool &newBest);
bool isTwentyFour(Fraction f);
int loadRecords(Record list[], int maxCount);
Fraction makeFraction(long long n, long long d);
void normalizeRecords(Record list[], int &count);
void paperCard(HDC dc, int x, int y, int w, int h, int cut, COLORREF fill, COLORREF border, int t, bool shadow);
bool playSfx(int id);
int playerLevel();
void polySolid(HDC dc, const POINT *p, int n, COLORREF c);
void resetState();
void resumeBgm();
int runSelfTest();
bool sameNumberSet(const char *expr, const int cards[CARD_COUNT]);
bool sfxOpen(int id);
void shutdownAudio();
bool solve24(const int cards[CARD_COUNT], string &answer, bool intOnly = false);
void sortRecords(Record list[], int count);
void startBgm();
void stopBgm();
void stopSfx();
string stripOuterParens(const string &s);
Fraction subFraction(Fraction a, Fraction b);
wchar_t suitChar(int suit);
COLORREF suitColor(int suit);
void todayString(char *buf, int size);
void tx(HDC dc, const char *s, int x, int y, int w, int h, HFONT f, COLORREF c, UINT fmt);
void txChar(HDC dc, wchar_t wc, int x, int y, int w, int h, HFONT f, COLORREF c, UINT fmt);
void vline(HDC dc, int x, int y, int h, COLORREF c, int t);

#endif  /* CALC24_H */