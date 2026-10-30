#!/usr/bin/env python3
"""Isolated Linux OpenJKDF2 sessions and the opt-in agent mailbox client.

Only Python's standard library is required. Game assets are always supplied
locally. See docs/AGENT_WORKSPACE.md for build and play examples.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import select
import shutil
import signal
import stat
import struct
import subprocess
import sys
import time
import uuid

PROTOCOL = "openjkdf2-agent-v1"
MAX_ID = 2147483647
MAX_REPLY = 1048576
MARKER = ".openjkdf2-agent-session"
KEYS = {chr(97 + n): 4 + n for n in range(26)}
KEYS.update({f"digit{n}": 29 + n for n in range(1, 10)})
KEYS.update({"digit0": 39, "return": 40, "enter": 40, "escape": 41, "esc": 41,
             "backspace": 42, "tab": 43, "space": 44, "minus": 45, "equals": 46,
             "grave": 53, "insert": 73, "home": 74, "pageup": 75, "delete": 76,
             "end": 77, "pagedown": 78, "right": 79, "left": 80, "down": 81,
             "up": 82, "lctrl": 224, "lshift": 225, "lalt": 226,
             "rctrl": 228, "rshift": 229, "ralt": 230})
KEYS.update({f"f{n}": 57 + n for n in range(1, 13)})


class AgentError(RuntimeError):
    pass


class AgentTimeout(AgentError):
    pass


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def atomic_json(path, value):
    path = Path(path)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(value, stream, indent=2, allow_nan=False)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def read_regular(path, limit=MAX_REPLY):
    """Never follow a mailbox symlink or block on a FIFO/device."""
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    except FileNotFoundError:
        return None
    except OSError as exc:
        raise AgentError(f"Cannot safely open {Path(path).name}: {exc.strerror}") from exc
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            raise AgentError(f"Invalid or oversized file: {Path(path).name}")
        value = stream.read(limit + 1)
    if len(value) > limit:
        raise AgentError(f"Oversized file: {Path(path).name}")
    return value


def read_json(path):
    value = read_regular(path)
    if value is None:
        return None
    try:
        parsed = json.loads(value)
    except (ValueError, UnicodeError) as exc:
        raise AgentError(f"Invalid JSON in {Path(path).name}") from exc
    if not isinstance(parsed, dict):
        raise AgentError(f"Expected JSON object in {Path(path).name}")
    return parsed


def private_directory(path):
    path = Path(path).absolute()
    try:
        info = path.lstat()
    except OSError as exc:
        raise AgentError(f"Missing private directory: {path}") from exc
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
        raise AgentError(f"Directory must be owned by this user and mode 0700: {path}")
    return path


def keycode(name):
    value = str(name).lower()
    if value in KEYS:
        return KEYS[value]
    if value.isascii() and value.isdecimal() and 1 <= int(value) <= 255:
        return int(value)
    raise AgentError(f"Unknown SDL scancode/key: {name}")


def validate_command(command):
    if not isinstance(command, str) or not command or any(ord(c) < 32 or ord(c) > 126 for c in command):
        raise AgentError("Command must be one printable ASCII line")
    if command in {"ping", "state", "release", "capture", "quit"}:
        return command
    if command.startswith("text "):
        if 1 <= len(command[5:]) <= 127 and command[5:].strip():
            return command
        raise AgentError("Text must contain 1..127 printable ASCII characters")
    parts = command.split(" ")
    if len(parts) == 3 and parts[0] in {"key", "mouse", "button"}:
        if not all(re.fullmatch(r"-?[0-9]+", v) for v in parts[1:]):
            raise AgentError("Input command arguments must be integers")
        a, b = map(int, parts[1:])
        valid = ((parts[0] == "key" and 1 <= a <= 255 and b in (0, 1))
                 or (parts[0] == "button" and 1 <= a <= 5 and b in (0, 1))
                 or (parts[0] == "mouse" and abs(a) <= 2048 and abs(b) <= 2048))
        if valid:
            return command
    raise AgentError("Unsupported command or arguments outside their allowed range")


def _child_named(directory, name):
    found = [p for p in directory.iterdir() if p.name.lower() == name.lower()]
    if len(found) > 1:
        raise AgentError(f"Ambiguous case variants for {name}")
    return found[0] if found else None


def _asset_tree(source, prefix):
    if not source.is_dir() or source.is_symlink():
        raise AgentError(f"Asset directory must be a real directory: {source}")
    source = source.resolve()
    files = []
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            raise AgentError(f"Symlinked asset is not accepted: {path}")
        if path.is_dir():
            continue
        if not path.is_file() or not path.resolve().is_relative_to(source):
            raise AgentError(f"Asset is not a regular contained file: {path}")
        files.append((path, Path(prefix) / path.relative_to(source)))
    return files


def prepare_session(data_root, session, *, game="jk", startup=None, episode_files=()):
    """Copy explicit asset trees into a new session; never import a profile."""
    data_root = Path(data_root).resolve(strict=True)
    session = Path(session).absolute()
    if session.exists() or session.is_symlink():
        raise AgentError("Session already exists; choose a new directory")
    if session.resolve().is_relative_to(data_root):
        raise AgentError("Session must be outside the source installation")
    if game not in {"jk", "mots"}:
        raise AgentError("Game must be jk or mots")
    files = []
    for name in ("resource", "episode", "music", "video"):
        source = _child_named(data_root, name)
        if source:
            files.extend(_asset_tree(source, name))
    names = {str(dst).lower() for _, dst in files}
    if game == "jk":
        required = {"resource/res2.gob", "episode/jk1.gob"}
        sound = {"resource/res1hi.gob", "resource/res1low.gob"}
    else:
        required = {"resource/jkmres.goo", "episode/jkm.goo"}
        sound = {"resource/jkmsndhi.goo", "resource/jkmsndlo.goo"}
    if not required.issubset(names) or not names.intersection(sound):
        raise AgentError(f"Source is missing required {game} resource/episode containers")
    marker_sources = [src for src, dst in files if str(dst).lower() == "resource/jk_.cd"]
    valid_magic = ({0x69973284, 0x699232C4, 0x69923384, 0x69923284}
                   if game == "jk" else {0x3B426929})
    marker_bytes = struct.pack("<I", 0x699232C4 if game == "jk" else 0x3B426929)
    if marker_sources:
        if marker_sources[0].stat().st_size != 4:
            raise AgentError("Install marker must contain exactly four bytes")
        marker_bytes = marker_sources[0].read_bytes()
        if struct.unpack("<I", marker_bytes)[0] not in valid_magic:
            raise AgentError("Install marker does not match the selected game")
    files = [(src, dst) for src, dst in files if str(dst).lower() != "resource/jk_.cd"]
    if startup is not None:
        files.extend(_asset_tree(Path(startup).resolve(strict=True), "startup"))
    for source in episode_files:
        source = Path(source).resolve(strict=True)
        if not source.is_file() or source.suffix.lower() not in {".gob", ".goo"}:
            raise AgentError("Additional episodes must be local GOB/GOO files")
        files.append((source, Path("episode") / source.name))
    destinations = [str(dst).lower() for _, dst in files]
    if len(destinations) != len(set(destinations)):
        raise AgentError("Asset destinations collide (case-insensitive)")
    total = sum(src.stat().st_size for src, _ in files)
    parent = session.parent
    if not parent.is_dir():
        raise AgentError("Create the session's parent directory first")
    if shutil.disk_usage(parent).free < total + 64 * 1024 * 1024:
        raise AgentError("Not enough disk space to copy assets and retain session evidence")
    session.mkdir(mode=0o700)
    try:
        (session / "agent").mkdir(mode=0o700)
        manifest = {"schemaVersion": 1, "createdAt": utc_now(), "game": game,
                    "dataSource": str(data_root), "assetBytes": total, "files": []}
        for source, relative in files:
            target = session / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            manifest["files"].append({"path": relative.as_posix(), "size": target.stat().st_size,
                                      "sha256": digest(target)})
        # Same local-install marker written by InstallHelper; not a game asset.
        (session / "resource/jk_.cd").write_bytes(marker_bytes)
        manifest["generatedInstallMarker"] = not bool(marker_sources)
        manifest["installMarkerSha256"] = hashlib.sha256(marker_bytes).hexdigest()
        (session / MARKER).write_text("openjkdf2-agent-session-v1\n", encoding="ascii")
        (session / MARKER).chmod(0o600)
        atomic_json(session / "session.json", manifest)
        return manifest
    except BaseException:
        shutil.rmtree(session)
        raise


class Client:
    def __init__(self, session, *, timeout=10.0):
        self.session = private_directory(session)
        self.directory = private_directory(self.session / "agent")
        if read_regular(self.session / MARKER, 128) != b"openjkdf2-agent-session-v1\n":
            raise AgentError("Session marker is missing or invalid")
        if not math.isfinite(timeout) or not 0 < timeout <= 120:
            raise AgentError("Timeout must be finite and in (0,120] seconds")
        self.timeout = timeout

    @contextmanager
    def _lock(self, deadline):
        try:
            fd = os.open(self.directory / "client.lock", os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        except OSError as exc:
            raise AgentError(f"Cannot safely open client lock: {exc.strerror}") from exc
        try:
            if not stat.S_ISREG(os.fstat(fd).st_mode):
                raise AgentError("Client lock is not a regular file")
            while True:
                try:
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    break
                except BlockingIOError:
                    if time.monotonic() >= deadline:
                        raise AgentTimeout("Another client still owns the command exchange")
                    time.sleep(.01)
            yield
        finally:
            os.close(fd)

    def _history_fd(self):
        try:
            fd = os.open(self.directory / "commands.jsonl", os.O_WRONLY | os.O_CREAT | os.O_APPEND
                         | os.O_NOFOLLOW | os.O_NONBLOCK, 0o600)
        except OSError as exc:
            raise AgentError(f"Cannot safely open command history: {exc.strerror}") from exc
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            os.close(fd)
            raise AgentError("Command history is not a regular file")
        return fd

    def _history(self, record):
        fd = self._history_fd()
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(json.dumps(record, allow_nan=False) + "\n")

    def request(self, command):
        command = validate_command(command)
        started = time.monotonic()
        with self._lock(started + self.timeout):
            mailbox = self.directory / "command.txt"
            if mailbox.exists() or mailbox.is_symlink():
                raise AgentError("An unconsumed command is pending; do not overwrite it")
            os.close(self._history_fd())  # Reject an unusable audit destination before sending input.
            previous = read_json(self.directory / "response.json") or {}
            sequence = read_json(self.directory / "sequence.json") or {}
            values = [previous.get("id", 0), sequence.get("id", 0)]
            if any(type(v) is not int or not 0 <= v <= MAX_ID for v in values):
                raise AgentError("Invalid command sequence metadata")
            request_id = max(values) + 1
            if request_id > MAX_ID:
                raise AgentError("Command IDs exhausted; start a new session")
            atomic_json(self.directory / "sequence.json", {"id": request_id})
            temporary = self.directory / f".command-{uuid.uuid4().hex}.tmp"
            fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            try:
                with os.fdopen(fd, "wb") as stream:
                    stream.write(f"{PROTOCOL}\n{request_id}\n{command}\n".encode("ascii"))
                    stream.flush()
                    os.fsync(stream.fileno())
                os.link(temporary, mailbox)  # Atomic publication, no overwrite.
            finally:
                temporary.unlink(missing_ok=True)
            record = {"id": request_id, "command": command, "issuedAt": utc_now()}
            original_error = None
            try:
                while time.monotonic() < started + self.timeout:
                    response = read_json(self.directory / "response.json")
                    if response and (type(response.get("id")) is not int or not 0 <= response["id"] <= MAX_ID):
                        raise AgentError("Receipt ID must be an integer in the supported range")
                    if response and response.get("id") == request_id:
                        if (type(response.get("schemaVersion")) is not int or response["schemaVersion"] != 1
                                or type(response.get("ok")) is not bool
                                or response.get("command") != command.split(" ", 1)[0]):
                            raise AgentError("Receipt does not match the requested protocol/command")
                        record["response"] = response
                        if not response["ok"]:
                            raise AgentError(str(response.get("error", "Engine rejected command")))
                        if not isinstance(response.get("result"), dict):
                            raise AgentError("Successful receipt is missing its result object")
                        return response
                    if response and type(response.get("id")) is int and response["id"] > request_id:
                        raise AgentError("Engine receipt skipped the requested ID")
                    time.sleep(.01)
                raise AgentTimeout(f"No receipt for command {request_id} within {self.timeout:g}s")
            except BaseException as exc:
                original_error = exc
                record["error"] = str(exc)
                raise
            finally:
                record["elapsedSeconds"] = round(time.monotonic() - started, 6)
                try:
                    self._history(record)
                except (AgentError, OSError) as history_error:
                    if original_error is not None:
                        original_error.add_note(f"Command history also failed: {history_error}")
                    else:
                        raise AgentError(f"Command completed in the engine, but history could not be written: {history_error}") from history_error

    def tap(self, key, seconds=.15):
        if not math.isfinite(seconds) or not 0 < seconds <= 1.5:
            raise AgentError("Tap duration must be in (0,1.5] seconds, below the input lease")
        code = keycode(key)
        down = self.request(f"key {code} 1")
        try:
            time.sleep(seconds)
        finally:
            up = self.request(f"key {code} 0")
        return {"down": down, "up": up}


def stop_child(child, *, grace=3.0):
    """Only for our own Popen children created with start_new_session=True."""
    # A dead group leader does not mean its descendants have stopped.
    deadline = time.monotonic() + grace
    try:
        os.killpg(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    while time.monotonic() < deadline:
        child.poll()
        try:
            os.killpg(child.pid, 0)
        except ProcessLookupError:
            break
        time.sleep(.01)
    try:
        os.killpg(child.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    child.wait(timeout=5)


def start_xvfb(session, width=1024, height=768, *, interrupted=()):
    executable = shutil.which("Xvfb")
    if not executable:
        raise AgentError("Xvfb is unavailable; use an existing display or install Xvfb")
    read_fd, write_fd = os.pipe()
    child = None
    try:
        with (session / "xvfb.log").open("xb") as log:
            child = subprocess.Popen([executable, "-displayfd", str(write_fd), "-screen", "0",
                                      f"{width}x{height}x24", "-nolisten", "tcp", "-noreset"],
                                     pass_fds=(write_fd,), stdout=log, stderr=log, start_new_session=True)
        os.close(write_fd)
        write_fd = -1
        deadline = time.monotonic() + 10
        while True:
            if interrupted:
                raise AgentError("Display startup interrupted by signal")
            if time.monotonic() >= deadline:
                raise AgentTimeout("Xvfb did not report a display within 10 seconds")
            ready, _, _ = select.select([read_fd], [], [], .05)
            if ready:
                break
        display = os.read(read_fd, 32).strip().decode("ascii")
        if not display.isdecimal() or child.poll() is not None:
            raise AgentError("Xvfb startup failed; inspect xvfb.log")
        return child, ":" + display
    except BaseException:
        if child:
            stop_child(child)
        raise
    finally:
        os.close(read_fd)
        if write_fd != -1:
            os.close(write_fd)


def run_session(args):
    session = private_directory(args.session)
    client = Client(session, timeout=1)
    binary = Path(args.binary).resolve(strict=True)
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise AgentError("Engine binary is not executable")
    if not 1 <= args.seconds <= 3600 or not math.isfinite(args.seconds):
        raise AgentError("Run duration must be 1..3600 seconds")
    for value in (args.episode, args.map):
        if not re.fullmatch(r"[A-Za-z0-9_.-]{1,100}", value):
            raise AgentError("Episode/map must be a simple engine name, not a path")
    manifest = read_json(session / "session.json")
    if manifest is None:
        raise AgentError("Missing prepared session manifest")
    # Exclusive marker also excludes two simultaneous launchers.
    with (session / "run.json").open("x", encoding="utf-8") as stream:
        stream.write("{}\n")
    command = [str(binary), "-agent", "-autostart", "-sp", "-episode", args.episode,
               "-map", args.map, "-windowGUI", "-debug", "log", "-verbose", "2"]
    if manifest["game"] == "mots":
        command.append("-motsCompat")
    if (session / "startup").is_dir():
        command.extend(["-path", "startup"])
    environment = os.environ.copy()
    environment.pop("OPENJKDF2_CAPTURE_REQUEST", None)
    environment["OPENJKDF2_AGENT_DIR"] = str(session / "agent")
    # Keep XDG writes local as well as the engine's CWD profile/save files.
    for name, relative in (("XDG_DATA_HOME", "xdg/data"), ("XDG_CONFIG_HOME", "xdg/config"),
                           ("XDG_CACHE_HOME", "xdg/cache"), ("XDG_STATE_HOME", "xdg/state"),
                           ("XDG_RUNTIME_DIR", "xdg/runtime")):
        directory = session / relative
        directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        environment[name] = str(directory)
    if args.software:
        environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
    if not args.audio:
        environment["SDL_AUDIO_DRIVER"] = "dummy"
        environment["ALSOFT_DRIVERS"] = "null"
    # Libraries beside a locally built engine are scoped to this child.
    environment["LD_LIBRARY_PATH"] = str(binary.parent) + (
        os.pathsep + environment["LD_LIBRARY_PATH"] if environment.get("LD_LIBRARY_PATH") else "")
    record = {"schemaVersion": 1, "startedAt": utc_now(), "binarySha256": digest(binary),
              "command": command, "softwareGL": args.software, "nullAudio": not args.audio,
              "physicsOverrides": False, "deterministicSimulation": False, "status": "starting"}
    child = display_child = None
    interrupted = []
    old_handlers = {}
    try:
        for sig in (signal.SIGINT, signal.SIGTERM):
            old_handlers[sig] = signal.signal(sig, lambda signum, frame: interrupted.append(signum))
        if args.xvfb or not environment.get("DISPLAY"):
            display_child, display = start_xvfb(session, args.width, args.height, interrupted=interrupted)
            environment["DISPLAY"] = display
            record["display"] = display
        with (session / "engine.stdout.log").open("xb") as stdout, (session / "engine.stderr.log").open("xb") as stderr:
            child = subprocess.Popen(command, cwd=session, env=environment, stdout=stdout, stderr=stderr,
                                     start_new_session=True)
        record["pid"] = child.pid
        atomic_json(session / "run.json", record)
        startup_deadline = time.monotonic() + 30
        while True:
            if child.poll() is not None:
                raise AgentError(f"Engine exited before ready (code {child.returncode}); inspect engine logs")
            if interrupted or time.monotonic() >= startup_deadline:
                raise AgentTimeout("Engine did not acknowledge agent mode during startup")
            try:
                client.request("ping")
                break
            except AgentError:
                time.sleep(.1)
        record.update(status="ready", readyAt=utc_now())
        atomic_json(session / "run.json", record)
        print(json.dumps({"event": "ready", "session": str(session), "pid": child.pid}), flush=True)
        finish = time.monotonic() + args.seconds
        while child.poll() is None and time.monotonic() < finish and not interrupted:
            time.sleep(.1)
        if child.poll() is None:
            record["stopReason"] = "signal" if interrupted else "duration"
            for command_text in ("release", "quit"):
                try:
                    Client(session, timeout=3).request(command_text)
                except AgentError as exc:
                    record.setdefault("shutdownErrors", []).append(str(exc))
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                record["forcedStop"] = True
        else:
            record["stopReason"] = "engine-exit"
        record["status"] = "completed"
    except BaseException as exc:
        record.update(status="failed", error=str(exc))
        raise
    finally:
        if child:
            stop_child(child)
            record["exitCode"] = child.returncode
            if record["status"] == "completed" and (record.get("forcedStop") or child.returncode != 0):
                record.update(status="failed", error=f"Engine did not finish cleanly: exit {child.returncode}")
        if display_child:
            stop_child(display_child)
        for sig, handler in old_handlers.items():
            signal.signal(sig, handler)
        record["finishedAt"] = utc_now()
        atomic_json(session / "run.json", record)
    if record.get("forcedStop") or record.get("exitCode") != 0:
        raise AgentError(f"Engine did not finish cleanly: exit {record.get('exitCode')}")
    return record


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    doctor = sub.add_parser("doctor", help="report local runtime/build prerequisites")
    doctor.add_argument("--binary")
    prepare = sub.add_parser("prepare", help="copy assets into a new isolated session")
    prepare.add_argument("--data-root", required=True)
    prepare.add_argument("--session", required=True)
    prepare.add_argument("--game", choices=("jk", "mots"), default="jk")
    prepare.add_argument("--startup")
    prepare.add_argument("--episode-file", action="append", default=[])
    run = sub.add_parser("run", help="supervise the engine and an optional private display")
    run.add_argument("--binary", required=True)
    run.add_argument("--session", required=True)
    run.add_argument("--episode", default="jk1")
    run.add_argument("--map", default="01narshadda.jkl")
    run.add_argument("--seconds", type=float, default=120)
    run.add_argument("--xvfb", action="store_true")
    run.add_argument("--software", action="store_true")
    run.add_argument("--audio", action="store_true", help="use available audio instead of null/dummy backends")
    run.add_argument("--width", type=int, default=1024)
    run.add_argument("--height", type=int, default=768)
    command = sub.add_parser("command", help="send one command and wait for its exact receipt")
    command.add_argument("--session", required=True)
    command.add_argument("--timeout", type=float, default=10)
    command.add_argument("command", nargs="+", help="e.g. state, capture, key 26 1, mouse 20 -10, quit")
    tap = sub.add_parser("tap", help="hold then release an ordinary key")
    tap.add_argument("--session", required=True)
    tap.add_argument("--timeout", type=float, default=10)
    tap.add_argument("--seconds", type=float, default=.15)
    tap.add_argument("key", help="named key (W, Up, Escape, F9) or numeric SDL scancode")
    args = parser.parse_args(argv)
    try:
        if args.action == "doctor":
            result = {"platform": sys.platform, "python": sys.version.split()[0],
                      "tools": {name: shutil.which(name) for name in ("cmake", "ninja", "gcc", "g++", "git", "Xvfb")},
                      "displayAvailable": bool(os.environ.get("DISPLAY"))}
            if args.binary:
                binary = Path(args.binary).resolve(strict=True)
                result["binary"] = {"path": str(binary), "executable": os.access(binary, os.X_OK),
                                    "sha256": digest(binary)}
        elif args.action == "prepare":
            result = prepare_session(args.data_root, args.session, game=args.game,
                                     startup=args.startup, episode_files=args.episode_file)
            result = {"session": str(Path(args.session).absolute()), "files": len(result["files"]),
                      "assetBytes": result["assetBytes"], "game": result["game"]}
        elif args.action == "run":
            if not 640 <= args.width <= 4096 or not 480 <= args.height <= 2160:
                raise AgentError("Display must be 640..4096 by 480..2160")
            result = run_session(args)
        elif args.action == "tap":
            result = Client(args.session, timeout=args.timeout).tap(args.key, args.seconds)
        else:
            result = Client(args.session, timeout=args.timeout).request(" ".join(args.command))
        print(json.dumps(result, indent=2, allow_nan=False), flush=True)
        return 0
    except (AgentError, OSError, ValueError) as exc:
        print(json.dumps({"ok": False, "error": str(exc)}), file=sys.stderr, flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
