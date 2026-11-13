#ifndef OPENHW_ONLINE_CLIENT_H
#define OPENHW_ONLINE_CLIENT_H
#include "OnlineProtocol.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void hwOnlineConnect(const char *name);
void hwOnlineDisconnect(void);
int hwOnlineConnected(void);
int hwOnlineSend(unsigned type,const void *data,size_t length);
int hwOnlineReceive(unsigned *type,void *data,size_t *length);
void hwOnlineStatus(char *text,size_t capacity);
void hwOnlineFingerprint(unsigned char output[32]);
void hwOnlineGamePump(void);
void hwOnlineGameLoaded(void);
void hwOnlineGameEnded(void);
int hwOnlineLobbySnapshot(OHLobby *snapshot);
void hwOnlineLobbyStatus(char *text,size_t capacity);
void hwOnlineCloseMenu(void);
int hwOnlineProbeRequested(int argc,char **argv);
int hwOnlineProbeMain(void);
#ifdef __cplusplus
}
#endif
#endif
