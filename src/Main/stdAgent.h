#ifndef OPENJKDF2_STDAGENT_H
#define OPENJKDF2_STDAGENT_H

#ifdef OPENJKDF2_AGENT_HOOKS
#include <stdint.h>
#define STDAGENT_KEYBOARD_ID 0x7ffffffeu
int stdAgent_Init(int argc, char** argv);
int stdAgent_IsActive(void);
int stdAgent_IsQuitRequested(void);
void stdAgent_Shutdown(void);
void stdAgent_Poll(void);
void stdAgent_BeforeSwap(void);
int stdAgent_KeyDown(int scancode, int physicalDown);
uint32_t stdAgent_MouseButtons(uint32_t physicalButtons);
#endif

#endif
