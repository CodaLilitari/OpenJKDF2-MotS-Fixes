# Linux Agent Workspace Implementation Plan

> For agentic workers: use the development execution skills and preserve the
> independent native-hook, tooling and baseline-build ownership boundaries.

**Goal:** Deliver a branch on both fork remotes with opt-in Linux play/test hooks and a usable
isolated launcher, without altering game simulation behavior.

**Architecture:** A disabled-by-default native adapter bridges a private file
mailbox to ordinary input, read-only state, and completed-frame capture. A
standard-library Python tool stages assets and supervises sessions.

**Tech stack:** C/C++, existing SDL3/OpenGL, Python 3, CMake, Xvfb/Mesa.

**Spec:** `docs/superpowers/specs/2026-10-30-agent-workspace-design.md`.

## Global constraints

- Base SHA: `ea715f21d8bd54ec2f2c5cc55ad710d851b8e442`.
- Build option `OPENJKDF2_AGENT_HOOKS` defaults OFF; launch requires `-agent`.
- Do not edit physics, collision, COG, game-time, weapons, serialization or network protocols.
- No licensed assets, local paths, credentials or runtime binaries in published artifacts.
- Input lease: 2000 ms; mailbox: 4096 bytes; IDs: 1..2147483647.

## Review focus

- Partial/oversized or stale commands must not become new input.
- Controller disappearance must release held keys and buttons.
- Menu and level transitions must not expose freed world/player objects.
- Capture must retain GL state and show the real current view.
- Source assets and unrelated processes must remain untouched on failures.

## Task 1: Opt-in native adapter

Files: new `stdAgent`/protocol modules and native tests; minimal integration in
`Window.c`, `Main.c`, SDL input polling, and menu shutdown; CMake feature gate.

Produces: the mailbox and reply contract in the design document.

- [x] Write and run failing parser/lease/inactive-contract tests.
- [x] Implement bounded parsing, private file access, replies and input expiry.
- [x] Integrate ordinary input, validated state, frame capture and shutdown.
- [x] Run native tests with sanitizers and build with hooks enabled.

## Task 2: Isolated workspace and Python client

Files: `scripts/agent_workspace.py`, `src/Tests/test_agent_workspace.py`.

Consumes: the Task 1 wire protocol. Produces: `Client.request(command)`,
`Client.tap(key, seconds)`, and prepare/run/command/tap/doctor CLI commands.

- [x] Write and observe failures for staging isolation and request validation.
- [x] Implement new-session staging and source-asset checks.
- [x] Test actual file exchange with a small protocol peer, stale replies,
      timeout and lock contention; implement matched receipts and command logs.
- [x] Implement supervised engine/Xvfb launch with bounded shutdown and logs.
- [x] Test real child cleanup and error propagation; run the complete tool suite.

## Task 3: Native verification and review

Files: `docs/AGENT_WORKSPACE.md`, source-only regression integration and
`docs/AGENT_WORKSPACE_VALIDATION.md`.

- [x] Preserve and run unchanged baseline 14-group gate and native build.
- [x] Run the existing gate plus new native/tool tests on the branch.
- [x] Exercise an actual game session: state, view, normal input, release,
      menu interaction, input expiry and clean shutdown; cover bad requests in
      the native protocol and client suites.
- [x] Independently review changes and the preserved engine boundary; fix
      material findings with focused regression tests.
- [x] Record exact validation scope and commit source.
- [ ] Publish the new branch to the user's GitHub fork and GitLab mirror using
      their CLIs; verify both remote branch SHAs. No main-branch update.
