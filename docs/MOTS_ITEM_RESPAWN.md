# MotS host pickup acknowledgment

The host-side `sithDSSThing_Take` path rejected a null taker even though MotS
pickup scripts normally call `TakeItem(item, -1)` and apply their effects locally.
The receive path already understood that sentinel. On the host, the item could
remain collision-disabled with no scheduled lifetime, so it never respawned.

The fix permits the missing actor only when MotS compatibility is active and
the transmitted actor ID is exactly `-1`. A missing actor in JK mode and an
unresolved non-sentinel actor ID remain rejected. Valid actor handling and client
forwarding are unchanged. The accepted host path acknowledges the take and calls
the existing `sithItem_SetItemTaken` lifecycle logic, including native MotS
respawn timing.

This was found during World Between Worlds Canyon Oasis testing. WBW separately
needed to expand the converted map's COG-instance capacity and adapt Force pickup
semantics. A converter change cannot fix this engine host path.

A checked-in regression harness compiles the actual `sithDSSThing_Take` and
`sithItem_SetItemTaken` functions with controlled dependencies. It checks MotS
null-taker acknowledgment and respawn scheduling, JK null rejection, unknown GUID
rejection, valid actors, client forwarding and item-state guards. Reverting only
the new condition fails the original MotS regression assertion. This unit harness
does not establish peer synchronization; full game results are recorded separately.

Run the asset-free regression with a native GCC or Clang compiler:

```sh
python3 src/Tests/run_takeitem_test.py --cc gcc --sanitize
python3 src/Tests/run_pov_lighting_test.py --cc gcc --sanitize
```

The second test protects a related startup fix discovered while validating the
new build. The world alpha pass can leave architectural RGB lighting active;
first-person weapon and saber models instead need scalar 3DO lighting. Their
active draw branch now selects mono explicitly before model submission and cache
flushes. The test executes the complete `jkPlayer_DrawPov` function in accelerated
and legacy configurations, checks draw/cache observations, and rejects the old
RGB leak. It uses synthetic camera/render dependencies, not a GPU image test.

Both commands passed again with AddressSanitizer and UndefinedBehaviorSanitizer
before publication. The existing `run_mots_lighting_test.py --cc gcc --sanitize`
also passed its 823 production-lighting checks, 193 placed-thing checks with
`JKM_LIGHTING`, 169 without it, and all seven old-behavior controls. The two
production files exactly match the LF-normalized hashes recorded for the tested
Windows executable below. Both new regression commands are included in the
existing Windows x64 retail-fidelity workflow; local results do not claim a
GitHub Actions run.

The 2026-09-09 Canyon integration fixture then observed all six consumed Force
items naturally respawning during a 134-second MotS multiplayer host run. It
used staged player positions with real collision and production pickup scripts.
The executable SHA-256 was
`fd8ddfc23f09540068318bedcc3f08e5ce6b12ea5ee04ed3cf45fd8ed46b7873`;
the source WBW conversion SHA-256 was
`cd5656b2c60beb50ae4d9a368405661af9d4a16c4d47f15eff0dde228e51be53`.
This establishes the host's native timed item lifecycle in that fixture, not
joined-peer synchronization or a played player-death cycle.

The change is in this fork. Upstream authors may reuse it under the existing
project license; please credit the fork's contributors and Coda Lilitari for the
investigation and fix, without implying a separate account or organization.
