#include <cassert>
#include <cstddef>
#include <string>
#include <unordered_map>

#include "Main/jkHudInvDefaultBindings.h"
#include "Gameplay/sithInventoryKeybinds.h"
#include "Platform/GL/jkgm_cache_lookup.hpp"

namespace {

struct CacheEntry
{
    int marker;
};

void test_default_inventory_hotkeys()
{
    const int jk_base = jkHudInv_GetDefaultInventoryHotkeyBase(0);
    assert(jk_base == INPUT_FUNC_ACTIVATE16);
    assert(jk_base - 1 == INPUT_FUNC_ACTIVATE15);
    assert(jk_base - 2 == INPUT_FUNC_ACTIVATE14);

    const int mots_base = jkHudInv_GetDefaultInventoryHotkeyBase(1);
    assert(mots_base == INPUT_FUNC_ACTIVATE19);
    assert(mots_base - 1 == INPUT_FUNC_ACTIVATE18);
    assert(mots_base - 2 == INPUT_FUNC_ACTIVATE17);
}

void test_inventory_hotkey_capacity()
{
    assert(sithInventory_GetPowerKeybindCountForMode(0) == 20);
    assert(sithInventory_GetPowerKeybindCountForMode(1) == 32);
    assert(sithInventory_GetPowerKeybindCountForSerializedVersion(6) == 20);
    assert(sithInventory_GetPowerKeybindCountForSerializedVersion(0x7D6) == 32);
    assert(INPUT_FUNC_ACTIVATE0 + 31 == INPUT_FUNC_ACTIVATE31);

    // The Windows x64/QOL build keeps one endpoint/sentinel entry after all
    // 32 retail MotS activation slots.
    static_assert(SITHINVENTORY_NUM_POWERKEYBINDS == 33);
}

void test_jkgm_cache_lookup()
{
    using Cache = std::unordered_map<std::string, CacheEntry>;

    Cache first_wins;
    assert(jkgm_register_signature(
        first_wins, std::string("duplicate-signature"), CacheEntry{1}));
    assert(!jkgm_register_signature(
        first_wins, std::string("duplicate-signature"), CacheEntry{2}));
    assert(first_wins.size() == 1);
    assert(first_wins.find("duplicate-signature")->second.marker == 1);

    Cache paths{{"mat/example.mat0", {1}}};
    Cache signatures{{"0123456789abcdef", {2}}};

    CacheEntry* path_entry = jkgm_find_cache_entry(
        paths, std::string("mat/example.mat0"),
        signatures, std::string("missing-signature"));
    assert(path_entry == &paths.find("mat/example.mat0")->second);
    assert(path_entry->marker == 1);

    CacheEntry* signature_entry = jkgm_find_cache_entry(
        paths, std::string("mat/missing.mat0"),
        signatures, std::string("0123456789abcdef"));
    assert(signature_entry == &signatures.find("0123456789abcdef")->second);
    assert(signature_entry->marker == 2);

    // JkGfxMod's signature identity deliberately wins over a shared path.
    CacheEntry* preferred_entry = jkgm_find_cache_entry(
        paths, std::string("mat/example.mat0"),
        signatures, std::string("0123456789abcdef"));
    assert(preferred_entry == signature_entry);

    const std::size_t path_size = paths.size();
    const std::size_t signature_size = signatures.size();
    for (int i = 0; i < 3; ++i) {
        CacheEntry* missing_entry = jkgm_find_cache_entry(
            paths, std::string("mat/missing.mat0"),
            signatures, std::string("missing-signature"));
        assert(missing_entry == nullptr);
    }
    assert(paths.size() == path_size);
    assert(signatures.size() == signature_size);
}

} // namespace

int main()
{
    test_default_inventory_hotkeys();
    test_inventory_hotkey_capacity();
    test_jkgm_cache_lookup();
    return 0;
}
