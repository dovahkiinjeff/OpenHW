#ifndef OPENHW_DEDICATED_SERVER_H
#define OPENHW_DEDICATED_SERVER_H
#ifdef __cplusplus
extern "C" {
#endif
extern int hwDedicatedMode;
int hwDedicatedRequested(int argc, char **argv);
int hwDedicatedMain(int argc, char **argv);
char *utyDedicatedSystemsInit(void);
#ifdef __cplusplus
}
#endif
#endif
