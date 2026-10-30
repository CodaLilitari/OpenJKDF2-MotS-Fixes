# Mod scenario and report increment

The user invited further additions that help mod development. Build on the
published agent workspace at `8a633de0258565f9f750bcbd571f439ce08d06d9`, on the
same branch and both existing fork remotes. This increment changes Python
tooling, examples, tests, and documentation only.

## Design

A versioned, data-only JSON recipe declares its game/episode/map and a bounded
sequence of ordinary input, state waits, assertions, and named checkpoints.
Assertions compare observed fields, numeric bounds, or displacement from a
previous checkpoint. Recipes cannot execute Python, shell, or console code.
They do not set gameplay state or advance a synthetic simulation clock.

Reuse `prepare_session` and the existing process/display owner. Add an optional
controller callback to `run_session`, run in its main thread after endpoint
readiness. On controller completion or failure, use the same release/quit and
owned-child cleanup. A successful recipe requires passing assertions and a
clean engine exit. Keep a failed step, last observed state, and a failure
capture when the remaining budget and engine allow it.

The report combines a sanitized JSON summary, HTML, byte-preserving PNG copies
of selected captures, asset/binary hashes, and bounded observed log matches.
It does not include original assets, saves, profiles, environment variables,
or absolute source paths. Reports remain local until explicitly shared.
Silence in a log does not certify all resources: some missing materials fall
back silently, and normal absent music can emit error messages.

## Ownership and checks

- Root owns `agent_scenario.py`, its tests, the optional launcher callback,
  sample recipes, documentation, and host-gate integration.
- Independent agent owns `agent_report.py` and report tests.
- Native review confirms the existing observations and diagnostics available;
  there are no native edits in this increment.
- Write failing tests first for schema limits, actual protocol exchange,
  wait/assertion failures, cleanup, report validity, and evidence boundaries.
- Run the existing workspace tests plus new tests and the complete host gate.
- Run the shipped smoke recipe against the already verified native executable
  and local original JK data; also run an intentional failing assertion to
  verify failure evidence and orderly cleanup.
- Independently review the implementation, fix concrete findings, then commit
  and push the same branch through the existing GitHub/GitLab CLI logins.

No archive extraction, general dependency linter, deterministic replay, COG
inspection, inventory telemetry, or simulation changes are included.
