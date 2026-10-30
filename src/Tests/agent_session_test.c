#define _POSIX_C_SOURCE 200809L
#include "Main/stdAgentSession.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_file(const char* path, const char* data)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600); assert(fd >= 0);
    assert(write(fd, data, strlen(data)) == (ssize_t)strlen(data)); assert(!close(fd));
}

int main(void)
{
    char path[] = "/tmp/openjk-agent-session-XXXXXX";
    assert(mkdtemp(path)); assert(!chdir(path));
    assert(!mkdir("channel", 0700)); assert(!mkdir("resource", 0700));
    write_file(".openjkdf2-agent-session", "test\n"); write_file("resource/jk_.cd", "test");
    assert(!mkdir("startup", 0755));
    assert(stdAgent_StartupPathValid("startup"));
    assert(!stdAgent_StartupPathValid("../startup"));
    assert(!stdAgent_StartupPathValid("/tmp"));
    assert(!chmod("startup", 0777)); assert(!stdAgent_StartupPathValid("startup"));
    assert(!rmdir("startup")); assert(!symlink("resource", "startup"));
    assert(!stdAgent_StartupPathValid("startup")); assert(!unlink("startup"));
    char channel[256]; assert(snprintf(channel, sizeof(channel), "%s/channel", path) > 0);
    stdAgentSession session;
    assert(!stdAgent_OpenSession(&session, NULL));
    assert(!stdAgent_OpenSession(&session, "channel"));
    assert(!symlink("channel", "channel-alias"));
    char alias[256]; assert(snprintf(alias, sizeof(alias), "%s/channel-alias", path) > 0);
    assert(!stdAgent_OpenSession(&session, alias)); assert(!unlink("channel-alias"));
    assert(stdAgent_OpenSession(&session, channel));
    stdAgentSession competing;
    assert(!stdAgent_OpenSession(&competing, channel));
    stdAgentCommand command; const char* error;
    assert(stdAgent_Receive(&session, &command, &error) == 0);
    write_file("channel/command.txt", "openjkdf2-agent-v1\n7\nkey 26 1\n");
    assert(stdAgent_Receive(&session, &command, &error) == 1 && command.id == 7);
    assert(access("channel/command.txt", F_OK) != 0);
    assert(stdAgent_Respond(&session, &command, "{\"accepted\":true}", NULL));
    FILE* receipt = fopen("channel/response.json", "r"); assert(receipt);
    char json[512]; size_t length = fread(json, 1, sizeof(json)-1, receipt); json[length] = 0; fclose(receipt);
    assert(strstr(json, "\"id\":7") && strstr(json, "\"ok\":true") && strstr(json, "\"command\":\"key\""));
    write_file("channel/command.txt", "openjkdf2-agent-v1\n7\nquit\n");
    assert(stdAgent_Receive(&session, &command, &error) == -1 && error);
    write_file("outside.txt", "openjkdf2-agent-v1\n8\nquit\n");
    assert(!unlink("channel/response.json")); assert(!symlink("../outside.txt", "channel/response.json"));
    assert(!stdAgent_Respond(&session, &command, "{}", NULL));
    assert(!unlink("channel/response.json"));
    assert(stdAgent_Respond(&session, &command, "{}", NULL));
    assert(!symlink("../outside.txt", "channel/command.txt"));
    assert(stdAgent_Receive(&session, &command, &error) < 0);
    assert(!access("outside.txt", F_OK)); assert(!unlink("channel/command.txt"));
    assert(!mkfifo("channel/command.txt", 0600));
    assert(stdAgent_Receive(&session, &command, &error) < 0); assert(!unlink("channel/command.txt"));
    write_file("channel/command.txt", "openjkdf2-agent-v1\n8\nping\n");
    assert(!chmod("channel/command.txt", 0644));
    assert(stdAgent_Receive(&session, &command, &error) < 0); assert(!unlink("channel/command.txt"));
    write_file("channel/publish.tmp", "openjkdf2-agent-v1\n8\nping\n");
    assert(!link("channel/publish.tmp", "channel/command.txt"));
    assert(stdAgent_Receive(&session, &command, &error) == 0);
    assert(!unlink("channel/publish.tmp"));
    assert(stdAgent_Receive(&session, &command, &error) == 1 && command.id == 8);
    char oversized[4098]; memset(oversized, 'x', sizeof(oversized)-1); oversized[sizeof(oversized)-1] = 0;
    write_file("channel/command.txt", oversized);
    assert(stdAgent_Receive(&session, &command, &error) == -1);
    assert(access("channel/command.txt", F_OK) != 0);
    unsigned char pixels[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
    assert(stdAgent_WriteFrame(&session, 8, pixels, 2, 2));
    assert(!stdAgent_WriteFrame(&session, 8, pixels, 2, 2));
    FILE* frame = fopen("channel/frame-8.ppm", "rb"); assert(frame);
    assert(fread(json, 1, 11, frame) == 11); assert(!memcmp(json, "P6\n2 2\n255\n", 11));
    assert(fgetc(frame) == 7); fclose(frame);
    stdAgent_CloseSession(&session);
    assert(stdAgent_OpenSession(&competing, channel));
    stdAgent_CloseSession(&competing);
    assert(!chmod("channel", 0755)); assert(!stdAgent_OpenSession(&session, channel));
    assert(!chmod("channel", 0700)); assert(!unlink(".openjkdf2-agent-session"));
    assert(!stdAgent_OpenSession(&session, channel));
    assert(!unlink("resource/jk_.cd")); assert(!rmdir("resource"));
    assert(!unlink("channel/response.json")); assert(!unlink("channel/frame-8.ppm")); assert(!unlink("channel/engine.lock"));
    assert(!rmdir("channel")); assert(!unlink("outside.txt"));
    assert(!chdir("/tmp")); assert(!rmdir(path));
    puts("agent session tests passed (private directory, stale IDs, symlinks, FIFO, permissions, receipts, PPM)");
    return 0;
}
