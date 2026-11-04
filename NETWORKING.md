# GeoSith multiplayer

Use the same GeoSith release and matching Jedi Knight Resource archives and loose asset overrides on every
computer. GeoSith checks the network protocol, JK/MOTS mode and resource archive/asset
contents before admitting a player. Older GeoSith/OpenJKDF2 releases cannot join
this protocol. The editor sends the level and placed COG sources automatically;
Jedi Knight textures, models, sounds and other game assets are not transferred.

## Joining

In Join Game, type `LAN` to search the local network, or enter the host address:
`203.0.113.10`, `host.example:27020`, `[IPv6-address]:27020`, or a VPN address.
`127.0.0.1` connects only to another instance on your own computer.
Select the listed game and enter its password if required. Passwords are checked
by the host and are never included in discovery responses. Discovery can take up
to 12 seconds per resolved address, with a bounded DNS lookup and cancellation.
Status text distinguishes invalid addresses, DNS failure, unavailable hosts,
incompatible versions/resources, bad passwords and synchronization progress.

## Hosting over the internet

The host's game UDP port defaults to 27020 and can be changed in the host menu.
Allow GeoSith.exe through Windows Firewall on the network profile you are using.
GeoSith attempts UPnP router mapping in the background on Windows. It does not
replace an existing mapping belonging to another program/computer, and removes
mappings it created when the session closes. Set GEOSITH_DISABLE_UPNP=1 before
launching if you prefer to manage router mappings yourself.

Pause > Network Details shows the current connection/router status and lets the
host retry automatic mapping or disable editing/undo by connected clients.
A successful router mapping is not proof of internet reachability. Ask a friend
on another internet connection to try the public IP and selected UDP port.

If automatic mapping is unavailable, forward that UDP port on your router to the
hosting computer's LAN address. Share your public IP and port with your friend.
Do not share 127.0.0.1 or a private LAN address with someone outside that network.
LAN discovery uses UDP 27019; do not forward that discovery port to the internet.

Carrier-grade NAT, double NAT and restricted networks may still prevent direct
hosting. This build does not include a public matchmaking/relay service. Use an
existing shared VPN and connect to the host's VPN address, or use a host with a
publicly reachable address. A relay cannot be provided just by shipping the GNS
DLL; it needs a deployed service. No account or external service is configured
by GeoSith automatically.

## Synchronization and recovery

Network processing continues while material/object browsers and attribute dialogs
are open. A remote document change cancels an open draft dialog before stale
attributes can be applied. Joining uses an acknowledged snapshot and catches up
to newer revisions after welcome, so continuous editing cannot indefinitely move
the join target. Level transfers have progress messages and bounded validation
recovery. Client edits are immediate previews; the host validates them, and a
rejected or superseded edit is reverted to the authoritative document.

Editor documents use explicit little-endian field encoding. Initial/recovery
transfers send a complete document; later transfers use acknowledged byte deltas
when smaller. Unchanged catalogs and COG text therefore compress away. Editor
bulk transfers use a lower-priority transport lane, while control messages have
priority and position updates use sequenced unreliable delivery to avoid stale queues
without accepting old positions after newer ones.

## Diagnostics and tests

Add -verboseNetworking to Launch GeoSith.cmd for detailed transport diagnostics.
Normal errors are also displayed without that flag. An occupied/unavailable game
port fails hosting instead of presenting a nonfunctional server.

The automated three-process test exercises hostname discovery, authentication,
rejected passwords, occupied ports, the actual synchronization menu through
join/welcome, level replication, predicted
editing, shared undo/redo, stale edits, application-message loss/duplication and
map edits. The test configures 25 ms send/receive latency, 2% packet loss, and
an 8 KB transport send buffer to exercise backpressure. Native tests cover IPv4/IPv6 parsing, checked packet reads, portable
serialization, truncation rejection and delta encoding. Real WAN, router UPnP and
Windows Firewall behavior must also be tested on the actual host network.
