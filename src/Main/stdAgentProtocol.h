#ifndef OPENJKDF2_STDAGENTPROTOCOL_H
#define OPENJKDF2_STDAGENTPROTOCOL_H

#include <stddef.h>
#include <stdint.h>

typedef enum stdAgentVerb {
    STDAGENT_INVALID, STDAGENT_PING, STDAGENT_STATE, STDAGENT_KEY,
    STDAGENT_MOUSE, STDAGENT_BUTTON, STDAGENT_RELEASE, STDAGENT_CAPTURE,
    STDAGENT_QUIT, STDAGENT_TEXT
} stdAgentVerb;

typedef struct stdAgentCommand {
    int id;
    stdAgentVerb verb;
    int a, b;
    char text[128];
} stdAgentCommand;

typedef struct stdAgentInput {
    unsigned char keys[256];
    unsigned char buttons[6];
    uint64_t leaseUntilMs;
} stdAgentInput;

int stdAgent_Parse(const char* data, size_t size, stdAgentCommand* command);
const char* stdAgent_VerbName(stdAgentVerb verb);
int stdAgent_ApplyInput(stdAgentInput* input, const stdAgentCommand* command, uint64_t nowMs);
int stdAgent_InputExpired(const stdAgentInput* input, uint64_t nowMs);
void stdAgent_ClearInput(stdAgentInput* input);

#endif
