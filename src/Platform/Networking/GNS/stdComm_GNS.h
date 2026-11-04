#ifndef _PLATFORM_NETWORKING_GNS_STDCOMM_GNS_H
#define _PLATFORM_NETWORKING_GNS_STDCOMM_GNS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PLATFORM_GNS

void stdComm_GNS_Startup();
void stdComm_GNS_Shutdown();
void stdComm_GNS_GetStatus(char *text,int size);
int stdComm_GNS_IsConnecting(void);
void stdComm_GNS_AllowEdits(int allow);
int stdComm_GNS_EditsAllowed(void);
void stdComm_GNS_RetryRouter(void);
int stdComm_GNS_TestDiscovery(void);
int stdComm_GNS_TestListener(void);

#endif // PLATFORM_GNS

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_NETWORKING_GNS_STDCOMM_GNS_H
