#ifndef JK_EDITOR_RESOURCES_H
#define JK_EDITOR_RESOURCES_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void jkEditor_ResourcesLoad(void);
void *jkEditor_ResourceOpen(const char *path,const char *mode);
void jkEditor_ResourceClose(void *file);
size_t jkEditor_ResourceRead(void *file,void *out,size_t size);
const char *jkEditor_ResourceGets(void *file,char *out,size_t size);
int jkEditor_ResourceSeek(void *file,int offset,int whence);
int jkEditor_ResourceTell(void *file);
int jkEditor_ResourceSize(void *file);
int jkEditor_ResourceEof(void *file);
#ifdef __cplusplus
}
#endif
#endif
