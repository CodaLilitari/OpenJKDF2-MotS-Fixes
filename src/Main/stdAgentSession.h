#ifndef OPENJKDF2_STDAGENTSESSION_H
#define OPENJKDF2_STDAGENTSESSION_H
#include "Main/stdAgentProtocol.h"

typedef struct stdAgentSession { int directoryFd, lastId, lockFd; } stdAgentSession;

int stdAgent_OpenSession(stdAgentSession* session, const char* path);
int stdAgent_StartupPathValid(const char* name);
void stdAgent_CloseSession(stdAgentSession* session);
/* 0: empty mailbox; 1: valid new command; -1: rejected; -2: unsafe mailbox. */
int stdAgent_Receive(stdAgentSession* session, stdAgentCommand* command, const char** error);
int stdAgent_Respond(stdAgentSession* session, const stdAgentCommand* command, const char* resultJson, const char* error);
int stdAgent_WriteFrame(stdAgentSession* session, int id, const unsigned char* pixels, int width, int height);
#endif
