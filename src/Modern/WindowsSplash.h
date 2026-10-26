#ifndef HW_MODERN_WINDOWS_SPLASH_H
#define HW_MODERN_WINDOWS_SPLASH_H

#ifdef __cplusplus
extern "C" {
#endif

void hwWindowsSplashStart(void);
void hwWindowsSplashUpdate(const char *stage);
void hwWindowsSplashFinish(void);
void hwWindowsSplashSetVisible(int visible);

enum { HW_SPLASH_HISTORY_LINES = 4, HW_SPLASH_TEXT_SIZE = 256 };
typedef struct HWWindowsSplashSnapshot
{
    unsigned int completed;
    unsigned int skipped;
    unsigned int total;
    unsigned int percentHundredths;
    unsigned int failed;
    unsigned int active;
    unsigned long long revision;
    double startedSeconds;
    double taskStartedSeconds;
    char task[HW_SPLASH_TEXT_SIZE];
    char detail[HW_SPLASH_TEXT_SIZE];
    char history[HW_SPLASH_HISTORY_LINES][HW_SPLASH_TEXT_SIZE];
} HWWindowsSplashSnapshot;

/* Actual checklist completion, never an estimate of remaining time.
   These functions do no work after startup has finished. */
int hwWindowsSplashIsActive(void);
int hwWindowsSplashTaskIsRunning(const char *id);
void hwWindowsSplashTaskBegin(const char *id, const char *label);
void hwWindowsSplashTaskComplete(const char *id, const char *detail);
void hwWindowsSplashTaskSkip(const char *id, const char *reason);
void hwWindowsSplashSkipPending(const char *prefix, const char *reason);
void hwWindowsSplashFail(const char *reason);
void hwWindowsSplashGetSnapshot(HWWindowsSplashSnapshot *snapshot);

/* Splash lifecycle / detail adapters; the UI owner calls these. */
void hwWindowsSplashProgressReset(void);
void hwWindowsSplashProgressStop(void);
void hwWindowsSplashProgressDetail(const char *detail);

#ifdef __cplusplus
}
#endif

#endif
