#ifndef ED_INPUT_H
#define ED_INPUT_H
#include <stdint.h>

typedef struct {
    uint32_t lastPressMs;
    int wasDown;
    int pending;
} EdDoubleTap;

static inline void edResetDoubleTap(EdDoubleTap *tap,int down) {
    tap->lastPressMs=0;tap->wasDown=down!=0;tap->pending=0;
}

/* Count distinct presses, never held-key repeat. Unsigned subtraction also
   handles the monotonic millisecond clock wrapping around. */
static inline int edDoubleTap(EdDoubleTap *tap,int down,uint32_t now,uint32_t intervalMs) {
    int pressed=down&&!tap->wasDown;
    tap->wasDown=down!=0;
    if(tap->pending && (uint32_t)(now-tap->lastPressMs)>intervalMs)tap->pending=0;
    if(!pressed)return 0;
    if(tap->pending){tap->pending=0;return 1;}
    tap->lastPressMs=now;tap->pending=1;return 0;
}
#endif
