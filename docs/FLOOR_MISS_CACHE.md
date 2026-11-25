# Exact floor-query miss cache

On desktop builds with quality-of-life improvements, set `OPENJKDF2_FLOOR_MISS_CACHE=1` before opening a level to enable this experimental collision optimization. The default uses the original collision scan. The option applies at level open. A failed allocation also uses the original scan.

Physics floor checks can submit identical downward rays several times during one displayed frame, including successive multiplayer simulation substeps. After a target passes the original sphere test and the ray has been transformed into model coordinates, the cache can reuse a previously observed mesh miss. Eligibility requires a zero-radius downward floor query against an uncontrolled LEVELGEO COG Thing with face collision and no moving path. A recorded key contains the exact binary32 local origin, direction, distance, radius and search flags. Nonfinite inputs take the original path.

A polygon miss can still write the caller’s distance when its infinite plane intersects the ray. Those queries and all contacts execute the original mesh scan every time. Only a zero return that left all three outputs untouched can populate the cache. A matching key therefore returns zero with distance, normal and face left as supplied by the caller.

The table has 2,048 slots and four keys per slot. It is allocated only for an enabled level and released when the level closes. A new simulation update, model initialization, geometry replacement, normal recalculation or geometry release invalidates earlier entries. Any future API that edits mesh positions, face normals or position indices must also advance the geometry revision. The optimization changes neither simulation cadence nor geometry. Its benefit depends on the level, the frequency of identical floor rays and the platform.

Run the synthetic host regression from the source root with `python3 -B src/Tests/run_floor_miss_cache_test.py`. It checks a miss that leaves outputs untouched, a polygon miss that overwrites distance, a hit after same-update mesh replacement, adjacent floating-point keys and excluded query types.
