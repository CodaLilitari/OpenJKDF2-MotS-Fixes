#ifndef OPENJKDF2_JKHUDINV_DEFAULT_BINDINGS_H
#define OPENJKDF2_JKHUDINV_DEFAULT_BINDINGS_H

#include "types_enums.h"

// MotS inserts three hotkeyable Force powers before Field Light.  Returning
// the retail F2 slot here keeps F2-F4 contiguous and makes both modes testable
// without loading game data or creating a player profile.
static inline int jkHudInv_GetDefaultInventoryHotkeyBase(int bMotsCompat)
{
    return bMotsCompat ? INPUT_FUNC_ACTIVATE19 : INPUT_FUNC_ACTIVATE16;
}

#endif // OPENJKDF2_JKHUDINV_DEFAULT_BINDINGS_H
