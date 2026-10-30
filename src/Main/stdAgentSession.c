#define _POSIX_C_SOURCE 200809L
#include "Main/stdAgentSession.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

static int private_regular(const struct stat* info)
{
    return S_ISREG(info->st_mode) && info->st_uid == geteuid()
        && !(info->st_mode & 0077) && info->st_nlink == 1;
}

int stdAgent_StartupPathValid(const char* name)
{
    if (!name || strcmp(name, "startup")) return 0;
    int fd = open("startup", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return 0;
    struct stat info;
    int valid = !fstat(fd, &info) && S_ISDIR(info.st_mode) && info.st_uid == geteuid() && !(info.st_mode & 0022);
    close(fd);
    return valid;
}

int stdAgent_OpenSession(stdAgentSession* session, const char* path)
{
    session->directoryFd = -1; session->lastId = 0; session->lockFd = -1;
    if (!path || path[0] != '/' || strlen(path) > 4095) return 0;
    struct stat info;
    if (stat(".", &info) || info.st_uid != geteuid() || (info.st_mode & 0077)) return 0;
    int marker = open(".openjkdf2-agent-session", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (marker < 0) return 0;
    int valid = !fstat(marker, &info) && private_regular(&info);
    close(marker);
    if (!valid) return 0;
    int assets = open("resource/jk_.cd", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (assets < 0) return 0;
    valid = !fstat(assets, &info) && S_ISREG(info.st_mode);
    close(assets);
    if (!valid) return 0;
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return 0;
    if (fstat(fd, &info) || !S_ISDIR(info.st_mode) || info.st_uid != geteuid() || (info.st_mode & 0077)) {
        close(fd); return 0;
    }
    int lock = openat(fd, "engine.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
    if (lock < 0) { close(fd); return 0; }
    if (fstat(lock, &info) || !private_regular(&info) || flock(lock, LOCK_EX | LOCK_NB)) {
        close(lock); close(fd); return 0;
    }
    session->directoryFd = fd;
    session->lockFd = lock;
    return 1;
}
void stdAgent_CloseSession(stdAgentSession* session)
{
    if (session->lockFd >= 0) close(session->lockFd);
    if (session->directoryFd >= 0) close(session->directoryFd);
    session->directoryFd = -1;
    session->lockFd = -1;
}
int stdAgent_Receive(stdAgentSession* session, stdAgentCommand* command, const char** error)
{
    memset(command, 0, sizeof(*command)); *error = NULL;
    int fd = openat(session->directoryFd, "command.txt", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) return 0;
        *error = "unsafe or unreadable command mailbox"; return -2;
    }
    struct stat opened, current;
    /* The client publishes with link(temp, command) then unlinks temp. Do not
       consume the brief two-link intermediate state or turn it into an error. */
    if (!fstat(fd, &opened) && S_ISREG(opened.st_mode) && opened.st_uid == geteuid()
        && !(opened.st_mode & 0077) && opened.st_nlink == 2) { close(fd); return 0; }
    if (fstat(fd, &opened) || !private_regular(&opened)) {
        close(fd); *error = "command must be a private owned regular file with one link"; return -2;
    }
    char data[4097];
    ssize_t size = read(fd, data, sizeof(data));
    int readError = size < 0;
    close(fd);
    if (fstatat(session->directoryFd, "command.txt", &current, AT_SYMLINK_NOFOLLOW)
        || opened.st_dev != current.st_dev || opened.st_ino != current.st_ino) {
        *error = "mailbox changed during read"; return -2;
    }
    if (unlinkat(session->directoryFd, "command.txt", 0)) {
        *error = "cannot consume command mailbox"; return -2;
    }
    if (readError || size != opened.st_size || size > 4096) {
        *error = "command exceeds 4096 bytes or could not be read completely"; return -1;
    }
    int parsed = stdAgent_Parse(data, (size_t)size, command);
    if (command->id && command->id <= session->lastId) {
        *error = "command id must increase strictly"; return -1;
    }
    if (command->id > session->lastId) session->lastId = command->id;
    if (!parsed) { *error = "invalid command syntax or argument range"; return -1; }
    return 1;
}

static int json_string(FILE* file, const char* value)
{
    if (fputc('"', file) == EOF) return 0;
    for (const unsigned char* p = (const unsigned char*)value; *p; ++p) {
        if (*p < 32 || *p >= 127 || *p == '"' || *p == '\\') {
            if (fprintf(file, "\\u%04x", *p) < 0) return 0;
        } else if (fputc(*p, file) == EOF) return 0;
    }
    return fputc('"', file) != EOF;
}

int stdAgent_Respond(stdAgentSession* session, const stdAgentCommand* command, const char* resultJson, const char* error)
{
    struct stat info;
    if (!fstatat(session->directoryFd, "response.json", &info, AT_SYMLINK_NOFOLLOW)) {
        if (!private_regular(&info)) return 0;
    } else if (errno != ENOENT) return 0;
    int fd = openat(session->directoryFd, ".response.tmp", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return 0;
    FILE* file = fdopen(fd, "w");
    if (!file) { close(fd); unlinkat(session->directoryFd, ".response.tmp", 0); return 0; }
    int ok = fprintf(file, "{\"schemaVersion\":1,\"id\":%d,\"ok\":%s,\"command\":\"%s\",",
        command->id, error ? "false" : "true", stdAgent_VerbName(command->verb)) >= 0;
    if (error) ok = ok && fputs("\"error\":", file) >= 0 && json_string(file, error);
    else ok = ok && fprintf(file, "\"result\":%s", resultJson ? resultJson : "{}") >= 0;
    ok = ok && fputs("}\n", file) >= 0;
    if (fclose(file)) ok = 0;
    if (ok && renameat(session->directoryFd, ".response.tmp", session->directoryFd, "response.json")) ok = 0;
    if (!ok) unlinkat(session->directoryFd, ".response.tmp", 0);
    return ok;
}
int stdAgent_WriteFrame(stdAgentSession* session, int id, const unsigned char* pixels, int width, int height)
{
    if (!pixels || id < 1 || width < 1 || height < 1 || width > 4096 || height > 2160) return 0;
    char name[48]; snprintf(name, sizeof(name), "frame-%d.ppm", id);
    int fd = openat(session->directoryFd, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return 0;
    FILE* file = fdopen(fd, "wb");
    if (!file) { close(fd); unlinkat(session->directoryFd, name, 0); return 0; }
    int ok = fprintf(file, "P6\n%d %d\n255\n", width, height) > 0;
    for (int y = height-1; ok && y >= 0; --y)
        ok = fwrite(pixels + (size_t)y * (size_t)width * 3, 3, (size_t)width, file) == (size_t)width;
    if (fclose(file)) ok = 0;
    if (!ok) unlinkat(session->directoryFd, name, 0);
    return ok;
}
