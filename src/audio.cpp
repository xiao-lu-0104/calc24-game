/*==========================================================================
 *  音频播放（MCI）：内嵌 mp3 解包、音效与背景音乐互斥协调。
 *========================================================================*/

#include "calc24.h"

static bool g_mciOpened = false;      /* sfx 别名是否已打开 */
static bool g_bgmOpened = false;      /* bgm 别名是否已打开 */
static bool g_bgmActive = false;      /* BGM 逻辑上应该在响 */
static bool g_bgmDeferred = false;    /* BGM 因音效未播完而顺延启动 */
bool g_sfxOn     = true;       /* 声音总开关（菜单可切换） */

void stopSfx()
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
void startBgm()
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

void stopBgm()
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

void resumeBgm()
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
void shutdownAudio()
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
bool sfxOpen(int id)
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
bool playSfx(int id)
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

int loadRecords(Record list[], int maxCount)
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

void todayString(char *buf, int size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, size, "%04d-%02d-%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

/* 插入一条记录；同名只保留最好成绩。返回名次，0 表示未上榜 */
int insertRecord(Record list[], int &count, const Record &rec, bool &newBest)
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
