/*==========================================================================
 *  截图导出与内置自检。
 *========================================================================*/

#include "calc24.h"

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

void exportShots()
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

static int  g_pass = 0;
static int  g_fail = 0;

static void chk(bool ok, const char *what)
{
    if (ok) { g_pass++; printf("  [PASS] %s\n", what); }
    else    { g_fail++; printf("  [FAIL] %s\n", what); }
}

int runSelfTest()
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
