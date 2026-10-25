# Mod integration checks informed by SM Sith Lord

This is an original validation workflow for our OpenJKDF2-MotS-Fixes fork,
informed by **SM Sith Lord's COG Factory**. His project implementations, art,
documentation and research retain their authorship. The linked pages are source
evidence and inspiration; their reported results are not tests of our engine.

## Budget the complete startup configuration

[P68 Survival Arena](https://smsithlord.com/openjkdf2libretro/cogfactory/gen1/p68-survival-arena.html)
reports an inventory failure caused by inadequate static-world COG capacity.
That page is an obsolete first-generation experiment. Its durable lesson is to
account for every producer of instances before diagnosing gameplay behavior.

Our published engine source at
[`74e4657c`](https://gitlab.com/codalilitari/OpenJKDF2-MotS-Fixes/-/tree/74e4657c4f1751d09af9ad9e056acb8f41997acb)
independently establishes the relevant accounting: `sithCog_ReadCogsListText`
allocates `World cogs` slots; `sithCog_Load` checks `sizeCogs` and increments
`numCogs` after a successful instance load; `jkHudInv_ItemDatLoad` calls that
loader for each inventory `cog=` binding. Sharing a script filename does not
mean sharing an instance. Script-table capacity is a separate budget.

Use the original [COG Workshop capacity preflight](https://gitlab.com/codalilitari/jk-mots-cog-workshop/-/blob/main/docs/STATIC_CAPACITY.md)
on the selected `static.jkl` and `items.dat`, reserving additional space for
other known instance producers. Keep the JSON and exact input hashes with the
package. A capacity fit assumes successful resource loading; it does not prove
resource availability, gameplay correctness or a target's complete limits.

## Treat whole tables and profile initialization as one contract

[P69 Mod Menu](https://smsithlord.com/openjkdf2libretro/cogfactory/gen1/p69-mod-menu.html)
and [P53 Campaign](https://smsithlord.com/openjkdf2libretro/cogfactory/gen1/p53-campaign.html)
motivate explicit review of `jkl/static.jkl`, `misc/items.dat` and
`ui/jkstrings.uni`. Two archives providing different rows at the same virtual
path do not create an automatic row merge.

Our current `Main.c` already loads the static world and initializes inventory
before creating the autostart player profile. This study does not add another
version of that existing fix. Keep direct-boot and menu-start profile tests
distinct, and record the exact resource providers, saved bindings and engine
revision. Use [WBW's archive audit](https://gitlab.com/codalilitari/World-Between-Worlds/-/blob/codex/massassi-top50/tools/archive_audit.py)
to expose table replacement and placement questions before launching the game.
It does not simulate mount precedence or validate table contents.

## Make acceptance tests observe the claimed behavior

These are procedures for future bounded game tests, not new execution results:

| Claim | Required observation | Discriminating control |
| --- | --- | --- |
| A switch is usable | Approach it and press the actual bound use input; observe target selection, dispatch and state change | An out-of-reach or blocked target must not activate |
| A temporary menu survives load correctly | Record object identities before save and after load; distinguish persisted objects from deliberate reconstruction | Repeated open/close and load must not accumulate duplicates or stale controls |
| A multiplayer feature synchronizes | Record host and joined-peer state independently, including late join and repeated events | Confirm the peer joined before the intended failure is induced; an early launch failure does not validate the negative control |
| Lighting and motion both work | Use reproducible camera/light conditions and sample moving geometry in the actual renderer | A passing motion trace must not substitute for an observed lighting result |

The test-design lessons come respectively from SM Sith Lord's
[P54 Holodeck](https://smsithlord.com/openjkdf2libretro/cogfactory/gen1/p54-holodeck.html),
[P69 Mod Menu](https://smsithlord.com/openjkdf2libretro/cogfactory/gen1/p69-mod-menu.html),
[P73 Network COGs](https://smsithlord.com/openjkdf2libretro/cogfactory/gen2/p73-network-cogs.html),
and [P77 Airena](https://smsithlord.com/openjkdf2libretro/cogfactory/gen2/p77-airena.html).

This update is source inspection and validation guidance. It introduces no
engine binary or gameplay change, and establishes no new retail, savegame,
visual or connected-client result. JK 1.01, MotS, enhanced forks and libretro
builds retain separate compatibility claims.
