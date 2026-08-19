#ifndef OPENJKDF2_SITHINVENTORY_KEYBINDS_H
#define OPENJKDF2_SITHINVENTORY_KEYBINDS_H

#include "engine_config.h"

// The original JK input table has 20 activation slots; MotS expanded the same
// table to 32.  Keep the mode decision asset-free because input/profile setup
// can run before the inventory description has been loaded.
static inline int sithInventory_GetPowerKeybindCountForMode(int bMotsCompat)
{
    return bMotsCompat
        ? SITHINVENTORY_POWERKEYBIND_COUNT_MOTS
        : SITHINVENTORY_POWERKEYBIND_COUNT_JK;
}

// Save and DSS payloads carry their own compatibility version.  Never infer
// their serialized width from Main_bMotsCompat: loading a file temporarily
// changes sithComm_version to the version stored in that file.
static inline int sithInventory_GetPowerKeybindCountForSerializedVersion(
    int serializedVersion)
{
    return sithInventory_GetPowerKeybindCountForMode(
        serializedVersion == 0x7D6);
}

#endif // OPENJKDF2_SITHINVENTORY_KEYBINDS_H
