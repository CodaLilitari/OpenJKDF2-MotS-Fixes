#!/usr/bin/env python3
"""Build a portable local HTML report from a stopped agent session.

Python 3.11+ standard library only. Reports contain selected screenshots and
bounded log excerpts, which may include mod content. They do not copy game
containers, profiles, saves, environment variables, or complete launch commands.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import html
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import struct
import sys
import zlib

MAX_METADATA_BYTES = 2 * 1024 * 1024
MAX_LOG_BYTES = 256 * 1024
MAX_LOG_MATCHES = 64
MAX_CAPTURES = 32
MAX_STEPS = 257  # At most 256 recipe steps plus the runner's final failure evidence.
MAX_ASSETS = 4096
MAX_DIRECTORY_ENTRIES = 10000
MAX_WIDTH, MAX_HEIGHT = 4096, 2160
MAX_IMAGE_BYTES = MAX_WIDTH * MAX_HEIGHT * 3 + 4096
MAX_TOTAL_PIXELS = 64 * 1024 * 1024
FRAME = re.compile(r"(?:agent/)?frame-([1-9][0-9]{0,9})\.ppm\Z")
SHA256 = re.compile(r"[0-9a-fA-F]{64}\Z")
LOG_SOURCES = ("engine.stdout.log", "engine.stderr.log", "debug.log")
MATCHERS = (
    ("model", re.compile(r"rdModel3_LoadEntry|dflt\.3do|too many models", re.I)),
    ("archive", re.compile(r"^(?=.*(?:gob|goo))(?=.*(?:open|signature|version|invalid|bad|fail|error))", re.I)),
    ("material", re.compile(r"^(?=.*(?:rdMaterial|material|\.mat\b))(?=.*(?:magic|revision|header|payload|invalid|bad|fail|error))", re.I)),
    ("keyframe", re.compile(r"^(?=.*(?:keyframe|\.key\b))(?=.*(?:parse|error|fail|invalid))", re.I)),
    ("COG parser", re.compile(r"\bPARSER\b|^(?=.*(?:\bcog\b|syntax))(?=.*(?:error|fail))", re.I)),
    ("audio", re.compile(r"MIX_LoadAudio", re.I)),
    ("diagnostic text", re.compile(r"\b(?:error|warning|failed|failure|cannot|could not)\b", re.I)),
)
STATE_FIELDS = ("monotonicMs", "renderedFrame", "guiState", "menuActive", "levelActive", "worldLoaded",
                "multiplayer", "gameTimeMs", "paused", "windowSize", "drawableSize", "pointer",
                "heldKeys", "heldButtons", "world", "thingCapacity", "sectorCount")
PLAYER_FIELDS = ("thingId", "signature", "sector", "position", "velocity", "health", "headPYR")
SCENARIO_HASHES = ("scenarioSha256", "runnerSha256", "workspaceToolSha256", "assetManifestSha256", "binarySha256")


class ReportError(RuntimeError):
    pass


def _parts(relative):
    if not isinstance(relative, str) or not relative or "\\" in relative or any(ord(c) < 32 for c in relative):
        raise ReportError("Invalid relative file name")
    path = PurePosixPath(relative)
    if path.is_absolute() or any(p in {"", ".", ".."} for p in relative.split("/")):
        raise ReportError("File paths must remain inside the session")
    return path.parts


def _directory_at(parent_fd, parts):
    descriptor = os.dup(parent_fd)
    try:
        for name in parts:
            child = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def _directory(path):
    candidate = Path(path)
    if ".." in candidate.parts:
        raise ReportError("Directory paths must not contain parent traversal")
    candidate = candidate.absolute()
    root = os.open("/", os.O_RDONLY | os.O_DIRECTORY)
    try:
        return candidate, _directory_at(root, candidate.parts[1:])
    except OSError as exc:
        raise ReportError("Directory must exist and have no symlinked components") from exc
    finally:
        os.close(root)


def _read(root_fd, relative, limit, *, optional=False, prefix=False):
    parts = _parts(relative)
    parent = None
    descriptor = None
    try:
        parent = _directory_at(root_fd, parts[:-1])
        descriptor = os.open(parts[-1], os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=parent)
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode):
            raise ReportError(f"{relative} must be a regular file")
        if info.st_size > limit and not prefix:
            raise ReportError(f"{relative} exceeds the report size limit")
        with os.fdopen(descriptor, "rb") as stream:
            descriptor = None
            data = stream.read(limit + 1)
        if len(data) > limit and not prefix:
            raise ReportError(f"{relative} exceeds the report size limit")
        return data[:limit], info.st_size, len(data) > limit or info.st_size > limit
    except FileNotFoundError as exc:
        if optional:
            return None
        raise ReportError(f"Missing report input: {relative}") from exc
    except OSError as exc:
        raise ReportError(f"Cannot safely read {relative}: {exc.strerror}") from exc
    finally:
        if descriptor is not None:
            os.close(descriptor)
        if parent is not None:
            os.close(parent)


def _json(root_fd, name, *, optional=False):
    read = _read(root_fd, name, MAX_METADATA_BYTES, optional=optional)
    if read is None:
        return None
    try:
        def invalid_constant(value):
            raise ValueError(value)
        result = json.loads(read[0], parse_constant=invalid_constant)
    except (ValueError, UnicodeError, RecursionError) as exc:
        raise ReportError(f"Invalid JSON in {name}") from exc
    if not isinstance(result, dict) or type(result.get("schemaVersion")) is not int or result["schemaVersion"] != 1:
        raise ReportError(f"Unsupported or missing schemaVersion in {name}")
    return result


def _redactor(session, manifest, run):
    private = [(str(session), "[session]")]
    source = manifest.get("dataSource")
    if isinstance(source, str) and source.startswith("/"):
        private.append((source.rstrip("/"), "[source assets]"))
    command = run.get("command")
    if isinstance(command, list) and command and isinstance(command[0], str) and command[0].startswith("/"):
        private.append((command[0], "[binary]"))
    private.sort(key=lambda item: len(item[0]), reverse=True)

    def clean(value, limit=1024):
        if not isinstance(value, str):
            return ""
        value = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", value)
        value = "".join(c for c in value if ord(c) >= 32 or c in "\n\t")
        value = re.sub(r"\bfile:/+[^\s'\"<>]+", "[absolute path]", value, flags=re.I)
        for path, replacement in private:
            if path:
                value = value.replace(path, replacement)
        # Retain relative resource names and HTML-looking text for escaping;
        # redact other absolute paths even if the manifest did not name them.
        value = re.sub(r"(?<![\w:/<\]])/+[^\s'\"<>]+", "[absolute path]", value)
        value = re.sub(r"\b[A-Za-z]:[\\/][^\s'\"<>]+", "[absolute path]", value)
        value = re.sub(r"(?<!\w)\\\\[^\s'\"<>]+", "[absolute path]", value)
        return value if len(value) <= limit else value[:limit - 1] + "…"
    return clean


def _value(value, clean):
    if value is None or type(value) is bool:
        return value
    if type(value) is int:
        return value if abs(value) <= 2 ** 63 else None
    if type(value) is float:
        return value if math.isfinite(value) else None
    if isinstance(value, str):
        return clean(value)
    if isinstance(value, list):
        return [_value(item, clean) if not isinstance(item, (list, dict)) else None for item in value[:256]]
    return None


def _state(value, clean):
    if not isinstance(value, dict):
        return None
    result = {name: _value(value[name], clean) for name in STATE_FIELDS if name in value}
    player = value.get("player")
    result["player"] = ({name: _value(player[name], clean) for name in PLAYER_FIELDS if name in player}
                        if isinstance(player, dict) else None)
    return result


def _evidence(value, clean, depth=0):
    """Retain assertion operands without importing arbitrary nested metadata."""
    if depth > 4:
        return None
    if isinstance(value, dict):
        allowed = set(STATE_FIELDS + PLAYER_FIELDS + ("player", "min", "max", "notNull"))
        return {name: _evidence(item, clean, depth + 1) for name, item in value.items() if name in allowed}
    if isinstance(value, list):
        return [_evidence(item, clean, depth + 1) for item in value[:256]]
    return _value(value, clean)


def _run(value, clean):
    result = {}
    for name in ("status", "startedAt", "readyAt", "finishedAt", "stopReason", "error"):
        if isinstance(value.get(name), str):
            result[name] = clean(value[name])
    for name in ("exitCode", "forcedStop", "softwareGL", "nullAudio", "physicsOverrides", "deterministicSimulation"):
        if name in value and type(value[name]) in (int, bool):
            result[name] = value[name]
    digest = value.get("binarySha256")
    if isinstance(digest, str) and SHA256.fullmatch(digest):
        result["binarySha256"] = digest.lower()
    if isinstance(value.get("shutdownErrors"), list):
        result["shutdownErrors"] = [clean(item) for item in value["shutdownErrors"][:16] if isinstance(item, str)]
    return result


def _assets(manifest):
    files = manifest.get("files", [])
    if not isinstance(files, list) or len(files) > MAX_ASSETS:
        raise ReportError("Invalid or oversized asset manifest")
    game = manifest.get("game")
    if not isinstance(game, str) or game not in {"jk", "mots"}:
        raise ReportError("Asset manifest game must be jk or mots")
    result = {"game": game, "files": []}
    for entry in files:
        if not isinstance(entry, dict):
            raise ReportError("Invalid asset metadata")
        parts = _parts(entry.get("path"))
        if len(parts) < 2 or parts[0] not in {"resource", "episode", "music", "video", "startup"}:
            raise ReportError("Asset manifest contains a disallowed relative path")
        if type(entry.get("size")) is not int or not 0 <= entry["size"] <= 2 ** 63:
            raise ReportError("Invalid asset size")
        digest = entry.get("sha256")
        if not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise ReportError("Invalid asset SHA-256")
        result["files"].append({"path": "/".join(parts), "size": entry["size"], "sha256": digest.lower()})
    return result


def _capture(value):
    if not isinstance(value, dict) or not isinstance(value.get("file"), str):
        raise ReportError("Capture metadata must name a frame file")
    match = FRAME.fullmatch(value["file"])
    if not match or int(match[1]) > 2147483647:
        raise ReportError("Capture must be a contained agent/frame-N.ppm file")
    number = int(match[1])
    result = {"source": f"agent/frame-{number}.ppm", "file": f"captures/frame-{number}.png"}
    for name in ("width", "height", "renderedFrame", "monotonicMs"):
        if name in value:
            if type(value[name]) is not int or not 0 <= value[name] <= 2 ** 63:
                raise ReportError(f"Invalid capture {name}")
            result[name] = value[name]
    return result


def _scenario(value, clean, warnings):
    if value is None:
        return None, []
    steps = value.get("steps", [])
    if not isinstance(steps, list) or len(steps) > MAX_STEPS:
        raise ReportError("Invalid or oversized scenario step list")
    result = {name: clean(value[name]) for name in ("name", "status", "startedAt", "finishedAt", "error")
              if isinstance(value.get(name), str)}
    for name in SCENARIO_HASHES:
        if isinstance(value.get(name), str) and SHA256.fullmatch(value[name]):
            result[name] = value[name].lower()
    if isinstance(value.get("runtime"), dict):
        result["runtime"] = _run(value["runtime"], clean)
    for name in ("initialState", "finalState"):
        if name in value:
            result[name] = _state(value[name], clean)
    result["steps"] = []
    captures = {}
    omitted = 0
    for position, step in enumerate(steps):
        if not isinstance(step, dict):
            raise ReportError("Invalid scenario step")
        item = {name: clean(step[name]) for name in ("name", "op", "status", "error") if isinstance(step.get(name), str)}
        item["index"] = step["index"] if type(step.get("index")) is int else position
        if type(step.get("durationSeconds")) in (int, float):
            item["durationSeconds"] = _value(step["durationSeconds"], clean)
        if isinstance(step.get("requestIds"), list):
            ids = step["requestIds"]
            item["requestIds"] = [number for number in ids[:2048] if type(number) is int and 1 <= number <= 2147483647]
            if len(ids) > 2048:
                item["requestIdsOmitted"] = len(ids) - 2048
        if isinstance(step.get("mismatches"), list):
            item["mismatches"] = []
            for mismatch in step["mismatches"][:64]:
                if isinstance(mismatch, dict) and isinstance(mismatch.get("field"), str):
                    item["mismatches"].append({"field": clean(mismatch["field"]),
                        "expected": _evidence(mismatch.get("expected"), clean),
                        "actual": _evidence(mismatch.get("actual"), clean),
                        "missing": mismatch.get("missing") is True})
            if len(step["mismatches"]) > 64:
                item["mismatchesOmitted"] = len(step["mismatches"]) - 64
        if type(step.get("distance")) in (int, float):
            item["distance"] = _value(step["distance"], clean)
        if isinstance(step.get("distanceFrom"), dict):
            reference = step["distanceFrom"]
            item["distanceFrom"] = {name: _value(reference[name], clean) for name in ("checkpoint", "min", "max")
                                    if name in reference}
        if "state" in step:
            item["state"] = _state(step["state"], clean)
        if "capture" in step and step["capture"] is not None:
            capture = _capture(step["capture"])
            source = capture["source"]
            if source in captures:
                for field, field_value in capture.items():
                    if field in captures[source] and captures[source][field] != field_value:
                        raise ReportError("Conflicting metadata for a selected capture")
                captures[source].update(capture)
                item["capture"] = captures[source]
            elif len(captures) < MAX_CAPTURES:
                captures[source] = capture
                item["capture"] = capture
            else:
                omitted += 1
                item["capture"] = {"source": source, "omitted": True}
        result["steps"].append(item)
    if omitted:
        warnings.append(f"The 32-image limit omitted {omitted} later capture references.")
    return result, list(captures.values())


def _fallback_captures(root_fd, warnings):
    directory = None
    try:
        directory = _directory_at(root_fd, ("agent",))
        numbers = []
        with os.scandir(directory) as entries:
            for index, entry in enumerate(entries):
                if index >= MAX_DIRECTORY_ENTRIES:
                    raise ReportError("Agent directory exceeds the report listing limit")
                match = FRAME.fullmatch(entry.name)
                if match:
                    if not entry.is_file(follow_symlinks=False):
                        raise ReportError("Fallback capture must be a regular file without symlinks")
                    number = int(match[1])
                    if number <= 2147483647:
                        numbers.append(number)
        numbers.sort()
        if len(numbers) > MAX_CAPTURES:
            warnings.append(f"The 32-image limit omitted {len(numbers) - MAX_CAPTURES} later fallback captures.")
        return [_capture({"file": f"frame-{number}.ppm"}) for number in numbers[:MAX_CAPTURES]]
    except FileNotFoundError:
        return []
    except OSError as exc:
        raise ReportError("Cannot safely list the agent capture directory") from exc
    finally:
        if directory is not None:
            os.close(directory)


def _ppm(data):
    position = 0
    whitespace = b" \t\r\n\v\f"

    def token():
        nonlocal position
        while position < len(data):
            if position >= 4096:
                raise ReportError("PPM header exceeds its size limit")
            if data[position] in whitespace:
                position += 1
            elif data[position] == 35:
                end = data.find(b"\n", position, 4096)
                if end == -1:
                    raise ReportError("Unterminated PPM header comment")
                position = end + 1
            else:
                break
        start = position
        while position < len(data) and data[position] not in whitespace + b"#":
            position += 1
            if position >= 4096:
                raise ReportError("PPM header exceeds its size limit")
        if position == start:
            raise ReportError("Incomplete PPM header")
        return data[start:position]

    if token() != b"P6":
        raise ReportError("Capture must be a binary P6 RGB PPM")
    dimensions = (token(), token(), token())
    if not all(re.fullmatch(rb"[0-9]+", value) for value in dimensions):
        raise ReportError("Invalid PPM dimensions or sample range")
    width, height, maximum = (int(value) for value in dimensions)
    if not 1 <= width <= MAX_WIDTH or not 1 <= height <= MAX_HEIGHT or maximum != 255:
        raise ReportError("PPM dimensions or sample range exceed supported bounds")
    if position >= len(data) or data[position] not in whitespace:
        raise ReportError("PPM raster is missing its header separator")
    # Consume only the header terminator: the first RGB bytes may be whitespace
    # or '#'. Resolve CRLF using exact raster length, so a CR separator followed
    # by an LF-valued first pixel remains valid and byte-preserving.
    expected = width * height * 3
    position += 2 if data[position:position + 2] == b"\r\n" and len(data) - position - 2 == expected else 1
    pixels = data[position:]
    if len(pixels) != width * height * 3:
        raise ReportError("PPM pixel payload is incomplete or has trailing bytes")
    return width, height, pixels


def _png(width, height, pixels):
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)
    compressor = zlib.compressobj()
    compressed = bytearray()
    row_bytes = width * 3
    for offset in range(0, len(pixels), row_bytes):
        compressed.extend(compressor.compress(b"\0" + pixels[offset:offset + row_bytes]))
    compressed.extend(compressor.flush())
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", bytes(compressed)) + chunk(b"IEND", b""))


def _logs(root_fd, clean):
    result = []
    for source in LOG_SOURCES:
        read = _read(root_fd, source, MAX_LOG_BYTES, optional=True, prefix=True)
        if read is None:
            continue
        data, size, truncated = read
        lines = data.decode("utf-8", errors="replace").splitlines()
        matches, count = [], 0
        for number, line in enumerate(lines, 1):
            categories = [name for name, pattern in MATCHERS if pattern.search(line)]
            if categories:
                count += 1
                if len(matches) < MAX_LOG_MATCHES:
                    matches.append({"line": number, "categories": categories, "text": clean(line)})
        excerpt_indices = sorted(set(range(min(12, len(lines)))) | set(range(max(0, len(lines) - 24), len(lines))))
        result.append({"source": source, "sizeBytes": size, "bytesRead": len(data), "truncated": truncated,
                       "matchedLineCount": count, "matches": matches,
                       "excerpt": [{"line": index + 1, "text": clean(lines[index])} for index in excerpt_indices]})
    return result


def _write(root_fd, relative, contents):
    parts = _parts(relative)
    parent = _directory_at(root_fd, parts[:-1])
    try:
        descriptor = os.open(parts[-1], os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=parent)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(contents)
    finally:
        os.close(parent)


def _page(summary):
    escape = lambda value: html.escape(str(value), quote=True)
    pretty = lambda value: escape(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False))
    scenario, run = summary["scenario"] or {}, summary["run"]
    name = scenario.get("name", "Session report")
    status = summary["status"]
    tone = "good" if status in {"passed", "completed"} else "bad" if status == "failed" else "neutral"
    parts = ["<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">",
             f"<title>{escape(name)} · OpenJKDF2 report</title>", """<style>
:root{color-scheme:light;--ink:#162334;--muted:#526174;--line:#dce3eb;--paper:#fff;--wash:#f3f6fa}
*{box-sizing:border-box}body{margin:0;background:var(--wash);color:var(--ink);font:16px/1.55 system-ui,sans-serif}
main{max-width:1180px;margin:auto;padding:40px 24px 64px}header{padding-bottom:24px;border-bottom:1px solid var(--line)}
.eyebrow{font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:var(--muted)}h1{font-size:34px;line-height:1.15;margin:12px 0}h2{font-size:23px;margin:36px 0 14px}h3{font-size:17px}p{max-width:90ch}.muted,small{color:var(--muted)}
.badge{display:inline-block;border-radius:20px;padding:4px 13px;font-weight:650;background:#e7edf5}.good{background:#d9f1e3;color:#155b36}.bad{background:#fbe0df;color:#8b2420}
.card,details{background:var(--paper);border:1px solid var(--line);border-radius:10px;padding:16px;margin:12px 0}.notice{border-left:4px solid #a27720;padding:10px 16px;background:#fff6dc}
nav{display:flex;gap:18px;flex-wrap:wrap;margin-top:20px}a{color:#135b9b}code,pre{font-family:ui-monospace,monospace;font-size:12px}pre{white-space:pre-wrap;overflow-wrap:anywhere;max-height:430px;overflow:auto;background:#f5f7fa;padding:12px;border-radius:6px}
table{width:100%;border-collapse:collapse;background:var(--paper);font-size:13px}th,td{text-align:left;vertical-align:top;border-bottom:1px solid var(--line);padding:10px;overflow-wrap:anywhere}th{background:#eaf0f6}td code{word-break:break-all}.table-wrap{overflow-x:auto}
.gallery{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:18px}figure{margin:0;background:var(--paper);border:1px solid var(--line);border-radius:10px;overflow:hidden}figure img{display:block;width:100%;height:auto;background:#111}figcaption{padding:12px;font-size:13px}summary{cursor:pointer;font-weight:600}footer{border-top:1px solid var(--line);margin-top:32px;padding-top:16px;color:var(--muted);font-size:13px}
</style><main>""", f"<header><div class=\"eyebrow\">OpenJKDF2 · local mod test evidence</div><h1>{escape(name)}</h1>",
             f"<span class=\"badge {tone}\">{escape(status)}</span> <span class=\"muted\">Recorded scenario / runtime result</span>",
             "<nav><a href=\"#steps\">Steps</a><a href=\"#captures\">Captures</a><a href=\"#logs\">Log matches</a><a href=\"#provenance\">Provenance</a></nav></header>",
             "<p class=\"notice\">This local report contains selected screenshots and log excerpts that may include mod content. Review it before sharing. Absolute paths are redacted; game assets, profiles and saves are not bundled.</p>"]
    if scenario.get("error"):
        parts.append(f"<section class=\"card\"><h3>Scenario failure</h3><pre>{escape(scenario['error'])}</pre></section>")
    for warning in summary["warnings"]:
        parts.append(f"<p class=\"notice\">{escape(warning)}</p>")
    parts.append("<h2 id=\"steps\">Scenario steps</h2>")
    if scenario.get("steps"):
        parts.append("<div class=\"table-wrap\"><table><thead><tr><th>Step</th><th>Operation</th><th>Result</th><th>Seconds</th><th>Evidence</th></tr></thead><tbody>")
        for step in scenario["steps"]:
            evidence = ""
            if step.get("capture", {}).get("file"):
                evidence += f"<a href=\"{escape(step['capture']['file'])}\">PNG capture</a>"
            if step.get("capture", {}).get("omitted"):
                evidence += "Capture omitted by image limit"
            if step.get("error"):
                evidence += f"<pre>{escape(step['error'])}</pre>"
            if step.get("state") is not None:
                evidence += f"<details><summary>Checkpoint state</summary><pre>{pretty(step['state'])}</pre></details>"
            assertions = {name: step[name] for name in ("requestIds", "requestIdsOmitted", "mismatches",
                          "mismatchesOmitted", "distance", "distanceFrom") if name in step}
            if assertions:
                evidence += f"<details><summary>Assertion evidence and requests</summary><pre>{pretty(assertions)}</pre></details>"
            parts.append(f"<tr><td>{escape(step['index'])} · {escape(step.get('name', ''))}</td><td>{escape(step.get('op', ''))}</td><td>{escape(step.get('status', 'unknown'))}</td><td>{escape(step.get('durationSeconds', ''))}</td><td>{evidence}</td></tr>")
        parts.append("</tbody></table></div>")
    else:
        parts.append("<p class=\"muted\">No scenario steps were recorded. The report uses available runtime evidence.</p>")
    for field, title in (("initialState", "Initial state"), ("finalState", "Final state")):
        if scenario.get(field) is not None:
            parts.append(f"<details><summary>{title}</summary><pre>{pretty(scenario[field])}</pre></details>")
    parts.append("<h2 id=\"captures\">Selected captures</h2><div class=\"gallery\">")
    for capture in summary["captures"]:
        description = f"{capture['source']} · {capture['width']} × {capture['height']}"
        if "renderedFrame" in capture:
            description += f" · rendered frame {capture['renderedFrame']}"
        parts.append(f"<figure><a href=\"{escape(capture['file'])}\"><img src=\"{escape(capture['file'])}\" alt=\"{escape(description)}\" loading=\"lazy\"></a><figcaption>{escape(description)}</figcaption></figure>")
    parts.append("</div>")
    if not summary["captures"]:
        parts.append("<p class=\"muted\">No captures were selected.</p>")
    parts.append("<h2 id=\"logs\">Log matches</h2><p>These are observed text matches, not automatic failure verdicts or a resource-validation result. Missing optional music can produce audio errors in a successful run; some resource fallbacks produce no log message.</p>")
    for log in summary["logs"]:
        scope = f"Read the first {log['bytesRead']:,} of {log['sizeBytes']:,} bytes."
        if log["truncated"]:
            scope += " The remainder was not scanned."
        parts.append(f"<section class=\"card\"><h3>{escape(log['source'])}</h3><p class=\"muted\">{scope} {log['matchedLineCount']} matching lines in the scanned portion; showing {len(log['matches'])}.</p>")
        if log["matches"]:
            parts.append("<div class=\"table-wrap\"><table><thead><tr><th>Source line</th><th>Category</th><th>Observed text</th></tr></thead><tbody>")
            for match in log["matches"]:
                parts.append(f"<tr><td>{escape(log['source'])}:{match['line']}</td><td>{escape(', '.join(match['categories']))}</td><td><code>{escape(match['text'])}</code></td></tr>")
            parts.append("</tbody></table></div>")
        excerpt = "\n".join(f"{line['line']:>6} | {line['text']}" for line in log["excerpt"])
        parts.append(f"<details><summary>Bounded log excerpt</summary><pre>{escape(excerpt)}</pre></details></section>")
    if not summary["logs"]:
        parts.append("<p class=\"muted\">No supported log files were available.</p>")
    parts.append(f"<h2 id=\"provenance\">Run provenance</h2><pre>{pretty(run)}</pre>")
    if scenario.get("runtime"):
        parts.append(f"<details><summary>Runtime receipt recorded by the scenario</summary><pre>{pretty(scenario['runtime'])}</pre></details>")
    hashes = {key: scenario[key] for key in SCENARIO_HASHES if key in scenario}
    if hashes:
        parts.append(f"<pre>{pretty(hashes)}</pre>")
    parts.append(f"<details><summary>Asset manifest metadata · {len(summary['assets']['files'])} files · {escape(summary['assets']['game'])}</summary><p class=\"muted\">Hashes were recorded during staging. This report does not reread or verify the game containers.</p><div class=\"table-wrap\"><table><thead><tr><th>Relative asset path</th><th>Bytes</th><th>Recorded SHA-256</th></tr></thead><tbody>")
    for asset in summary["assets"]["files"]:
        parts.append(f"<tr><td>{escape(asset['path'])}</td><td>{asset['size']}</td><td><code>{escape(asset['sha256'])}</code></td></tr>")
    parts.append("</tbody></table></div></details><footer>All links are local. Machine-readable evidence: <a href=\"summary.json\">summary.json</a>. Screenshots preserve the RGB pixels of the selected completed frames.</footer></main></html>")
    return "\n".join(parts).encode("utf-8")


def build_report(session, output):
    """Write a new report directory and return only sanitized, relative metadata."""
    session_path, root_fd = _directory(session)
    output_path = Path(output)
    parent_fd = output_fd = None
    created = False
    try:
        if ".." in output_path.parts:
            raise ReportError("Output path must not contain parent traversal")
        output_path = output_path.absolute()
        if not output_path.name:
            raise ReportError("Output must name a new directory")
        _, parent_fd = _directory(output_path.parent)
        try:
            os.stat(output_path.name, dir_fd=parent_fd, follow_symlinks=False)
        except FileNotFoundError:
            pass
        else:
            raise ReportError("Report output already exists; choose a new directory")
        manifest = _json(root_fd, "session.json")
        raw_run = _json(root_fd, "run.json", optional=True) or {}
        raw_scenario = _json(root_fd, "scenario.json", optional=True)
        clean = _redactor(session_path, manifest, raw_run)
        warnings = []
        if not raw_run:
            warnings.append("No run receipt is available; engine shutdown status is unverified.")
        scenario, captures = _scenario(raw_scenario, clean, warnings)
        if raw_scenario is None:
            captures = _fallback_captures(root_fd, warnings)
        run = _run(raw_run, clean)
        status = (scenario or {}).get("status", run.get("status", "unknown"))
        if run.get("status") == "failed":
            status = "failed"
        summary = {"schemaVersion": 1, "createdAt": datetime.now(timezone.utc).isoformat(), "status": status,
                   "scenario": scenario, "run": run, "assets": _assets(manifest), "captures": captures,
                   "logs": _logs(root_fd, clean), "warnings": warnings}
        os.mkdir(output_path.name, mode=0o700, dir_fd=parent_fd)
        created = True
        output_fd = os.open(output_path.name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent_fd)
        if captures:
            os.mkdir("captures", mode=0o700, dir_fd=output_fd)
        total_pixels = 0
        for capture in captures:
            data = _read(root_fd, capture["source"], MAX_IMAGE_BYTES)[0]
            width, height, pixels = _ppm(data)
            for field, actual in (("width", width), ("height", height)):
                if field in capture and capture[field] != actual:
                    raise ReportError("Capture metadata dimensions do not match its PPM")
                capture[field] = actual
            total_pixels += width * height
            if total_pixels > MAX_TOTAL_PIXELS:
                raise ReportError("Selected captures exceed the report pixel budget")
            _write(output_fd, capture["file"], _png(width, height, pixels))
        _write(output_fd, "summary.json", (json.dumps(summary, indent=2, ensure_ascii=False, allow_nan=False) + "\n").encode("utf-8"))
        _write(output_fd, "index.html", _page(summary))
        return summary
    except BaseException as exc:
        if output_fd is not None:
            os.close(output_fd)
            output_fd = None
        if created:
            shutil.rmtree(output_path.name, dir_fd=parent_fd)
        if isinstance(exc, OSError):
            raise ReportError(f"Report filesystem operation failed: {exc.strerror}") from exc
        raise
    finally:
        if output_fd is not None:
            os.close(output_fd)
        if parent_fd is not None:
            os.close(parent_fd)
        os.close(root_fd)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", required=True, help="prepared local agent session")
    parser.add_argument("--output", required=True, help="new report directory with an existing real parent")
    args = parser.parse_args(argv)
    try:
        summary = build_report(args.session, args.output)
        print(json.dumps({"ok": True, "status": summary["status"], "captures": len(summary["captures"]),
                          "html": "index.html", "summary": "summary.json"}, indent=2), flush=True)
        return 0
    except (ReportError, OSError, ValueError) as exc:
        print(json.dumps({"ok": False, "error": str(exc)}), file=sys.stderr, flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
