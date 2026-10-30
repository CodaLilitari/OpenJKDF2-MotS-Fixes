#include "Main/stdAgentProtocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int parse(const char* text, stdAgentCommand* command)
{ return stdAgent_Parse(text, strlen(text), command); }

int main(int argc, char** argv)
{
    (void)argv;
    stdAgentCommand command;
    if (argc == 1) {
        assert(parse("openjkdf2-agent-v1\n1\nkey 26 1\n", &command));
        assert(command.id == 1 && command.verb == STDAGENT_KEY && command.a == 26 && command.b == 1);
        assert(parse("openjkdf2-agent-v1\n2147483647\nmouse -2048 2048\n", &command));
        assert(command.id == 2147483647 && command.a == -2048 && command.b == 2048);
        assert(parse("openjkdf2-agent-v1\n2\ntext Agent name 12\n", &command));
        assert(!strcmp(command.text, "Agent name 12"));
        const char* invalid[] = {
            "openjkdf2-agent-v2\n1\nping\n", "openjkdf2-agent-v1\n0\nping\n",
            "openjkdf2-agent-v1\n2147483648\nping\n", "openjkdf2-agent-v1\n-1\nping\n",
            "openjkdf2-agent-v1\n+1\nping\n", "openjkdf2-agent-v1\n1x\nping\n",
            "openjkdf2-agent-v1\n1\nkey 0 1\n", "openjkdf2-agent-v1\n1\nkey 256 1\n",
            "openjkdf2-agent-v1\n1\nkey 26 2\n", "openjkdf2-agent-v1\n1\nkey 26 1 junk\n",
            "openjkdf2-agent-v1\n1\nbutton 6 1\n", "openjkdf2-agent-v1\n1\nmouse 2049 0\n",
            "openjkdf2-agent-v1\n1\nmouse 1.0 0\n", "openjkdf2-agent-v1\n1\ncapture /tmp/path\n",
            "openjkdf2-agent-v1\n1\nquit\nping\n", "openjkdf2-agent-v1\n1\nping",
            "openjkdf2-agent-v1\n1\ntext \n", "openjkdf2-agent-v1\n1\ntext hello\tthere\n"
        };
        for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
            assert(!parse(invalid[i], &command));
        const char embedded[] = "openjkdf2-agent-v1\n1\nping\n\0ignored";
        assert(!stdAgent_Parse(embedded, sizeof(embedded)-1, &command));
        char huge[4097]; memset(huge, 'x', sizeof(huge));
        assert(!stdAgent_Parse(huge, sizeof(huge), &command));
        puts("agent parser tests passed");
    } else {
        stdAgentInput input = {0};
        command = (stdAgentCommand){ .id = 1, .verb = STDAGENT_KEY, .a = 26, .b = 1 };
        assert(stdAgent_ApplyInput(&input, &command, 100));
        assert(input.keys[26] && !stdAgent_InputExpired(&input, 2099));
        assert(stdAgent_InputExpired(&input, 2100));
        command.verb = STDAGENT_STATE;
        assert(!stdAgent_ApplyInput(&input, &command, 2000));
        assert(stdAgent_InputExpired(&input, 2100));
        command.verb = STDAGENT_BUTTON; command.a = 3; command.b = 1;
        assert(stdAgent_ApplyInput(&input, &command, 2100));
        assert(input.buttons[3] && !stdAgent_InputExpired(&input, 4099));
        assert(stdAgent_InputExpired(&input, 4100));
        stdAgent_ClearInput(&input);
        assert(!input.keys[26] && !input.buttons[3] && !stdAgent_InputExpired(&input, UINT64_MAX));
        puts("agent input lease tests passed");
    }
    return 0;
}
