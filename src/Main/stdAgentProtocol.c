#include "Main/stdAgentProtocol.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int integer(const char** cursor, int minimum, int maximum, char separator, int* value)
{
    const char* start = *cursor;
    if (*start == '-') ++start;
    if (*start < '0' || *start > '9') return 0;
    errno = 0;
    char* end;
    long parsed = strtol(*cursor, &end, 10);
    if (errno || *end != separator || parsed < minimum || parsed > maximum) return 0;
    *value = (int)parsed;
    *cursor = separator ? end + 1 : end;
    return 1;
}

int stdAgent_Parse(const char* data, size_t size, stdAgentCommand* command)
{
    static const char prefix[] = "openjkdf2-agent-v1\n";
    memset(command, 0, sizeof(*command));
    if (!data || size < sizeof(prefix) || size > 4096 || memchr(data, 0, size)
        || memcmp(data, prefix, sizeof(prefix)-1) || data[size-1] != '\n') return 0;
    char text[4097];
    memcpy(text, data, size);
    text[size-1] = 0;
    const char* cursor = text + sizeof(prefix)-1;
    if (!integer(&cursor, 1, INT_MAX, '\n', &command->id) || strchr(cursor, '\n') || strchr(cursor, '\r')) return 0;
    for (int verb = STDAGENT_PING; verb <= STDAGENT_TEXT; ++verb) {
        const char* name = stdAgent_VerbName((stdAgentVerb)verb);
        size_t length = strlen(name);
        if (strncmp(cursor, name, length) || (cursor[length] && cursor[length] != ' ')) continue;
        command->verb = (stdAgentVerb)verb;
        cursor += length;
        if (verb == STDAGENT_KEY || verb == STDAGENT_MOUSE || verb == STDAGENT_BUTTON) {
            if (*cursor++ != ' ') return 0;
            int minimum = verb == STDAGENT_MOUSE ? -2048 : 1;
            int maximum = verb == STDAGENT_MOUSE ? 2048 : (verb == STDAGENT_KEY ? 255 : 5);
            return integer(&cursor, minimum, maximum, ' ', &command->a)
                && integer(&cursor, verb == STDAGENT_MOUSE ? -2048 : 0,
                           verb == STDAGENT_MOUSE ? 2048 : 1, 0, &command->b);
        }
        if (verb == STDAGENT_TEXT) {
            if (*cursor++ != ' ') return 0;
            size_t length = strlen(cursor);
            if (!length || length >= sizeof(command->text)) return 0;
            for (size_t i = 0; i < length; ++i)
                if ((unsigned char)cursor[i] < 32 || (unsigned char)cursor[i] > 126) return 0;
            memcpy(command->text, cursor, length+1);
            return 1;
        }
        return !*cursor;
    }
    return 0;
}
const char* stdAgent_VerbName(stdAgentVerb verb)
{
    static const char* names[] = {"invalid", "ping", "state", "key", "mouse", "button", "release", "capture", "quit", "text"};
    return verb >= STDAGENT_INVALID && verb <= STDAGENT_TEXT ? names[verb] : names[0];
}
int stdAgent_ApplyInput(stdAgentInput* input, const stdAgentCommand* command, uint64_t nowMs)
{
    if (command->verb == STDAGENT_RELEASE) { stdAgent_ClearInput(input); return 1; }
    if (command->verb == STDAGENT_KEY) {
        if (command->a < 1 || command->a > 255 || command->b < 0 || command->b > 1) return 0;
        input->keys[command->a] = (unsigned char)command->b;
    } else if (command->verb == STDAGENT_BUTTON) {
        if (command->a < 1 || command->a > 5 || command->b < 0 || command->b > 1) return 0;
        input->buttons[command->a] = (unsigned char)command->b;
    } else if (command->verb != STDAGENT_MOUSE && command->verb != STDAGENT_TEXT) return 0;
    input->leaseUntilMs = nowMs <= UINT64_MAX-2000 ? nowMs+2000 : UINT64_MAX;
    return 1;
}
int stdAgent_InputExpired(const stdAgentInput* input, uint64_t nowMs)
{ return input->leaseUntilMs && nowMs >= input->leaseUntilMs; }
void stdAgent_ClearInput(stdAgentInput* input)
{ memset(input, 0, sizeof(*input)); }
