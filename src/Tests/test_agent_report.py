#!/usr/bin/env python3
"""Behavioral report tests with synthetic assets, captures, and diagnostics."""
import hashlib
from html.parser import HTMLParser
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/agent_report.py"


class Tags(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tags = []

    def handle_starttag(self, tag, attributes):
        self.tags.append((tag, dict(attributes)))


def decode_png(path):
    """Independently check PNG framing, CRCs, and the exported RGB rows."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError("not a PNG")
    position, compressed, dimensions = 8, bytearray(), None
    seen_end = False
    while position < len(data):
        length = struct.unpack_from(">I", data, position)[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + length]
        checksum = struct.unpack_from(">I", data, position + 8 + length)[0]
        if checksum != zlib.crc32(kind + payload) & 0xffffffff:
            raise AssertionError("bad PNG CRC")
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if (depth, color, compression, filtering, interlace) != (8, 2, 0, 0, 0):
                raise AssertionError("not ordinary 8-bit RGB PNG")
            dimensions = width, height
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            seen_end = True
        position += length + 12
    if not seen_end or dimensions is None or position != len(data):
        raise AssertionError("incomplete PNG")
    width, height = dimensions
    raster = zlib.decompress(compressed)
    if len(raster) != height * (1 + width * 3):
        raise AssertionError("wrong raster length")
    pixels = bytearray()
    for row in range(height):
        offset = row * (1 + width * 3)
        if raster[offset] != 0:
            raise AssertionError("expected unfiltered report rows")
        pixels.extend(raster[offset + 1:offset + 1 + width * 3])
    return dimensions, bytes(pixels)


class ReportTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(SCRIPT.is_file(), "agent report generator is not implemented")
        spec = importlib.util.spec_from_file_location("agent_report", SCRIPT)
        self.tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.tool)
        self.temp = tempfile.TemporaryDirectory(prefix="openjk-report-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.session = self.make_session("session")
        self.output = self.root / "report"

    def write_json(self, path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def make_session(self, name):
        session = self.root / name
        (session / "agent").mkdir(parents=True, mode=0o700)
        session.chmod(0o700)
        self.write_json(session / "session.json", {
            "schemaVersion": 1, "game": "jk", "dataSource": str(self.root / "private-install"),
            "files": [{"path": "resource/res2.gob", "size": 123, "sha256": "a" * 64}],
        })
        self.write_json(session / "run.json", {
            "schemaVersion": 1, "status": "completed", "exitCode": 0,
            "binarySha256": "b" * 64, "startedAt": "2026-10-30T12:00:00Z",
            "finishedAt": "2026-10-30T12:00:02Z", "stopReason": "engine-exit",
        })
        return session

    def scenario(self, *, status="passed", name="Mod smoke test", steps=None, **extra):
        value = {"schemaVersion": 1, "name": name, "status": status,
                 "startedAt": "2026-10-30T12:00:00Z", "finishedAt": "2026-10-30T12:00:02Z",
                 "steps": steps or []}
        value.update(extra)
        self.write_json(self.session / "scenario.json", value)

    def capture(self, number=1, pixels=b"\x10\x20\x30", width=1, height=1):
        path = self.session / "agent" / f"frame-{number}.ppm"
        path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)
        return path

    def selected(self, file="frame-1.ppm", **capture_fields):
        self.scenario(steps=[{"index": 0, "name": "Spawn", "op": "checkpoint", "status": "passed",
                              "durationSeconds": .1, "capture": {"file": file, **capture_fields}}])

    def test_selected_capture_becomes_valid_png_with_exact_rgb_row_order(self):
        pixels = b"\n#\x00\xff\x10\x20\x01\x02\x03\x80\x90\xa0"
        self.capture(2, pixels, 2, 2)
        self.capture(1)  # Unselected evidence must not be copied.
        self.selected("agent/frame-2.ppm", width=2, height=2, renderedFrame=42, monotonicMs=1234)
        summary = self.tool.build_report(self.session, self.output)
        self.assertEqual(summary["status"], "passed")
        self.assertEqual(summary, json.loads((self.output / "summary.json").read_text()))
        self.assertEqual({p.relative_to(self.output).as_posix() for p in self.output.rglob("*") if p.is_file()},
                         {"index.html", "summary.json", "captures/frame-2.png"})
        self.assertEqual(decode_png(self.output / "captures/frame-2.png"), ((2, 2), pixels))
        self.assertEqual(summary["captures"][0]["renderedFrame"], 42)
        self.assertIn('src="captures/frame-2.png"', (self.output / "index.html").read_text())

    def test_failed_scenario_and_checkpoint_state_remain_visible(self):
        self.scenario(status="failed", error="Expected the test room; reached another sector",
                      finalState={"world": "test.jkl", "player": {"position": [1, 2, 3], "sector": 4}},
                      steps=[{"index": 0, "name": "Check sector", "op": "assert", "status": "failed",
                              "durationSeconds": .03, "error": "sector mismatch"}])
        summary = self.tool.build_report(self.session, self.output)
        self.assertEqual(summary["status"], "failed")
        self.assertEqual(summary["scenario"]["finalState"]["player"]["position"], [1, 2, 3])
        page = (self.output / "index.html").read_text()
        self.assertIn("Expected the test room", page)
        self.assertIn("sector mismatch", page)

    def test_assertion_evidence_and_runner_hashes_survive_without_private_extra_fields(self):
        hashes = {name: "c" * 64 for name in ("scenarioSha256", "runnerSha256", "workspaceToolSha256",
                                              "assetManifestSha256", "binarySha256")}
        self.scenario(status="failed", **hashes,
                      runtime={"status": "failed", "exitCode": 17, "command": "PRIVATE_COMMAND_SENTINEL"},
                      steps=[{"index": 0, "name": "Health and movement", "op": "assert", "status": "failed",
                              "requestIds": [1, 2], "distance": 3.5,
                              "distanceFrom": {"checkpoint": "start", "min": .5, "max": 2},
                              "mismatches": [{"field": "player.health", "expected": {"min": 50, "max": 100},
                                              "actual": 25, "missing": False, "environment": "ENVIRONMENT_SENTINEL"}]}])
        summary = self.tool.build_report(self.session, self.output)
        step = summary["scenario"]["steps"][0]
        self.assertEqual(step.get("requestIds"), [1, 2])
        self.assertEqual(step["distance"], 3.5)
        self.assertEqual(step["distanceFrom"], {"checkpoint": "start", "min": .5, "max": 2})
        self.assertEqual(step["mismatches"], [{"field": "player.health", "expected": {"min": 50, "max": 100},
                                               "actual": 25, "missing": False}])
        self.assertEqual(summary["scenario"]["runtime"], {"status": "failed", "exitCode": 17})
        for name, value in hashes.items():
            self.assertEqual(summary["scenario"][name], value)
        contents = (self.output / "index.html").read_text() + (self.output / "summary.json").read_text()
        self.assertNotIn("PRIVATE_COMMAND_SENTINEL", contents)
        self.assertNotIn("ENVIRONMENT_SENTINEL", contents)
        self.assertIn("Assertion evidence", contents)

    def test_maximum_recipe_can_include_the_extra_failure_evidence_step(self):
        steps = [{"index": number, "name": "Check", "op": "assert", "status": "passed"}
                 for number in range(256)]
        steps.append({"index": 256, "name": "Failure evidence", "op": "checkpoint", "status": "passed",
                      "state": {"world": "fixture.jkl", "player": None}})
        self.scenario(status="failed", steps=steps)
        try:
            summary = self.tool.build_report(self.session, self.output)
        except self.tool.ReportError as exc:
            self.fail(f"A bounded final evidence step must be reportable: {exc}")
        self.assertEqual(len(summary["scenario"]["steps"]), 257)

    def test_log_matches_do_not_turn_a_passed_scenario_into_failure(self):
        self.scenario()
        (self.session / "engine.stderr.log").write_text("Starting\nERROR MIX_LoadAudio: missing optional music\n")
        summary = self.tool.build_report(self.session, self.output)
        self.assertEqual(summary["status"], "passed")
        log = summary["logs"][0]
        self.assertEqual(log["source"], "engine.stderr.log")
        self.assertEqual(log["matches"][0]["line"], 2)
        self.assertIn("audio", log["matches"][0]["categories"])
        self.assertIn("Log matches", (self.output / "index.html").read_text())

    def test_html_escapes_names_errors_and_log_text(self):
        malicious = '<script>alert("test")</script><img src=x onerror=alert(1)>'
        self.scenario(name=malicious, status="failed", error=malicious,
                      steps=[{"index": 0, "name": malicious, "op": "assert", "status": "failed",
                              "durationSeconds": 0, "error": malicious}])
        (self.session / "debug.log").write_text("PARSER error: " + malicious + "\n")
        self.tool.build_report(self.session, self.output)
        page = (self.output / "index.html").read_text()
        parser = Tags()
        parser.feed(page)
        self.assertFalse(any(tag in {"script", "img", "svg", "iframe"} for tag, _ in parser.tags))
        self.assertIn("&lt;script&gt;", page)
        self.assertNotIn(malicious, page)

    def test_report_omits_asset_bytes_profiles_environment_commands_and_private_paths(self):
        (self.session / "resource").mkdir()
        (self.session / "player").mkdir()
        (self.session / "resource/res2.gob").write_bytes(b"LICENSED_ASSET_SENTINEL")
        (self.session / "player/private.plr").write_bytes(b"PROFILE_SENTINEL")
        (self.session / "player/save.jks").write_bytes(b"SAVE_SENTINEL")
        run = json.loads((self.session / "run.json").read_text())
        run.update(command=["/home/private/bin/game", "FULL_COMMAND_SENTINEL"],
                   environment={"TOKEN": "ENVIRONMENT_SENTINEL"})
        self.write_json(self.session / "run.json", run)
        private_install = str(self.root / "private-install")
        self.scenario(status="failed", error=f"Load failed at {self.session}/player/save.jks and {private_install}/episode/a.gob",
                      finalState={"world": "/home/private/maps/test.jkl", "environment": "ENVIRONMENT_SENTINEL",
                                  "command": "FULL_COMMAND_SENTINEL", "player": None})
        (self.session / "engine.stdout.log").write_text(f"GOB open failure {private_install}/episode/a.gob\n")
        summary = self.tool.build_report(self.session, self.output)
        contents = b"\n".join(p.read_bytes() for p in self.output.rglob("*") if p.is_file())
        for secret in (b"LICENSED_ASSET_SENTINEL", b"PROFILE_SENTINEL", b"SAVE_SENTINEL", b"ENVIRONMENT_SENTINEL",
                       b"FULL_COMMAND_SENTINEL", str(self.session).encode(), private_install.encode(), b"/home/private"):
            self.assertNotIn(secret, contents)
        self.assertEqual(summary["assets"]["files"], [{"path": "resource/res2.gob", "size": 123, "sha256": "a" * 64}])

    def test_absolute_file_uris_and_double_slash_paths_are_redacted(self):
        self.scenario(error="Failed at file:///home/private-user/secret%20map/texture.mat")
        (self.session / "debug.log").write_text(
            "warning file:///home/private-user/secret%20map/texture.mat\n"
            "warning //home/private-user/secret/texture.mat\n")
        self.tool.build_report(self.session, self.output)
        contents = (self.output / "index.html").read_text() + (self.output / "summary.json").read_text()
        self.assertNotIn("private-user", contents)
        self.assertNotIn("secret%20map", contents)

    def test_capture_paths_cannot_escape_or_select_arbitrary_files(self):
        for number, value in enumerate(("../private.ppm", "/tmp/private.ppm", "agent/../frame-1.ppm",
                                        "agent/nested/frame-1.ppm", "player/save.jks", "frame-1.png")):
            with self.subTest(value=value):
                self.selected(value)
                output = self.root / f"bad-path-{number}"
                with self.assertRaises(self.tool.ReportError):
                    self.tool.build_report(self.session, output)
                self.assertFalse(output.exists())

    def test_symlinked_metadata_capture_and_logs_are_rejected(self):
        outside = self.root / "outside"
        outside.write_bytes(b"external must remain untouched")
        for number, name in enumerate(("scenario.json", "agent/frame-1.ppm", "engine.stderr.log")):
            with self.subTest(name=name):
                session = self.make_session(f"symlink-{number}")
                if name == "agent/frame-1.ppm":
                    self.write_json(session / "scenario.json", {"schemaVersion": 1, "name": "x", "status": "passed",
                        "steps": [{"index": 0, "name": "x", "op": "checkpoint", "status": "passed",
                                   "capture": {"file": "frame-1.ppm"}}]})
                (session / name).symlink_to(outside)
                output = self.root / f"symlink-report-{number}"
                with self.assertRaises(self.tool.ReportError):
                    self.tool.build_report(session, output)
                self.assertFalse(output.exists())
        self.assertEqual(outside.read_bytes(), b"external must remain untouched")

    def test_symlinked_directory_components_are_rejected(self):
        outside = self.root / "external-agent"
        outside.mkdir()
        (outside / "frame-1.ppm").write_bytes(b"P6\n1 1\n255\nabc")
        (self.session / "agent").rmdir()
        (self.session / "agent").symlink_to(outside, target_is_directory=True)
        self.selected()
        with self.assertRaises(self.tool.ReportError):
            self.tool.build_report(self.session, self.output)
        self.assertFalse(self.output.exists())
        alias = self.root / "session-alias"
        alias.symlink_to(self.session, target_is_directory=True)
        with self.assertRaises(self.tool.ReportError):
            self.tool.build_report(alias, self.output)
        parent_alias = self.root / "output-alias"
        parent_alias.symlink_to(outside, target_is_directory=True)
        with self.assertRaises(self.tool.ReportError):
            self.tool.build_report(self.session, parent_alias / "report")
        self.assertFalse((outside / "report").exists())

    def test_fifo_capture_is_rejected_without_blocking_or_partial_output(self):
        self.selected()
        os.mkfifo(self.session / "agent/frame-1.ppm")
        result = subprocess.run([sys.executable, "-B", str(SCRIPT), "--session", str(self.session),
                                 "--output", str(self.output)], capture_output=True, text=True, timeout=2)
        self.assertEqual(result.returncode, 2)
        self.assertFalse(self.output.exists())

    def test_strict_ppm_rejects_wrong_format_bounds_and_incomplete_pixels(self):
        cases = (b"P3\n1 1\n255\n1 2 3", b"P6\n1 1\n65535\nabcdef", b"P6\n0 1\n255\n",
                 b"P6\n999999 999999\n255\n", b"P6\n1 1\n255\nab", b"P6\n1 1\n255\nabcd")
        self.selected()
        for number, contents in enumerate(cases):
            with self.subTest(contents=contents):
                (self.session / "agent/frame-1.ppm").write_bytes(contents)
                output = self.root / f"invalid-ppm-{number}"
                with self.assertRaises(self.tool.ReportError):
                    self.tool.build_report(self.session, output)
                self.assertFalse(output.exists())

    def test_cr_header_separator_does_not_consume_a_leading_lf_pixel(self):
        self.selected()
        (self.session / "agent/frame-1.ppm").write_bytes(b"P6\n1 1\n255\r\n\x02\x03")
        try:
            self.tool.build_report(self.session, self.output)
        except self.tool.ReportError as exc:
            self.fail(f"A valid CR separator and LF pixel must retain its raster: {exc}")
        self.assertEqual(decode_png(self.output / "captures/frame-1.png"), ((1, 1), b"\n\x02\x03"))

    def test_native_diagnostic_word_order_retains_useful_categories(self):
        lines = ["Error: Bad signature in header of gob file.", "Error: Bad version 9 for gob file",
                 "Parse error while reading keyframes, line 8.", "Too many models already loaded!",
                 "rdMaterial: short or invalid pixel payload"]
        (self.session / "debug.log").write_text("\n".join(lines))
        summary = self.tool.build_report(self.session, self.output)
        observed = summary["logs"][0]["matches"]
        self.assertEqual(len(observed), 5)
        for match, category in zip(observed, ("archive", "archive", "keyframe", "model", "material")):
            self.assertIn(category, match["categories"])

    def test_long_repeated_resource_words_do_not_cause_unbounded_log_matching(self):
        (self.session / "debug.log").write_text("gob material keyframe syntax " * 10000)
        try:
            result = subprocess.run([sys.executable, "-B", str(SCRIPT), "--session", str(self.session),
                                     "--output", str(self.output)], capture_output=True, text=True, timeout=2)
        except subprocess.TimeoutExpired:
            self.fail("A bounded log prefix caused excessive matching time")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_large_logs_have_bounded_excerpts_and_truthful_line_references(self):
        self.scenario()
        text = "normal startup\n" + "PARSER syntax error: fixture\n" * 20000 + "UNREAD_TAIL_SENTINEL\n"
        (self.session / "debug.log").write_text(text)
        (self.session / "engine.stderr.log").write_text("warning " + "x" * 1000000)
        summary = self.tool.build_report(self.session, self.output)
        debug = next(log for log in summary["logs"] if log["source"] == "debug.log")
        self.assertTrue(debug["truncated"])
        self.assertLessEqual(debug["bytesRead"], 262144)
        self.assertEqual(debug["matches"][0]["line"], 2)
        self.assertLessEqual(len(debug["matches"]), 64)
        self.assertGreater(debug["matchedLineCount"], len(debug["matches"]))
        for log in summary["logs"]:
            self.assertTrue(all(len(line["text"]) <= 1024 for line in log["excerpt"]))
        combined = (self.output / "summary.json").read_text() + (self.output / "index.html").read_text()
        self.assertNotIn("UNREAD_TAIL_SENTINEL", combined)
        self.assertLess(len(combined), 200000)

    def test_without_scenario_fallback_selects_at_most_32_frames_in_numeric_order(self):
        for number in range(1, 36):
            self.capture(number)
        summary = self.tool.build_report(self.session, self.output)
        self.assertEqual(summary["status"], "completed")
        self.assertEqual(len(summary["captures"]), 32)
        self.assertEqual(summary["captures"][0]["source"], "agent/frame-1.ppm")
        self.assertEqual(summary["captures"][-1]["source"], "agent/frame-32.ppm")
        self.assertTrue(summary["warnings"])

    def test_existing_output_is_never_overwritten(self):
        self.output.mkdir()
        (self.output / "keep").write_text("untouched")
        with self.assertRaises(self.tool.ReportError):
            self.tool.build_report(self.session, self.output)
        self.assertEqual((self.output / "keep").read_text(), "untouched")

    def test_oversized_metadata_is_rejected_before_output_creation(self):
        (self.session / "scenario.json").write_bytes(b" " * (2097152 + 1))
        with self.assertRaises(self.tool.ReportError):
            self.tool.build_report(self.session, self.output)
        self.assertFalse(self.output.exists())

    def test_invalid_manifest_game_has_a_structured_cli_error(self):
        manifest = json.loads((self.session / "session.json").read_text())
        manifest["game"] = {}
        self.write_json(self.session / "session.json", manifest)
        result = subprocess.run([sys.executable, "-B", str(SCRIPT), "--session", str(self.session),
                                 "--output", str(self.output)], capture_output=True, text=True, timeout=2)
        self.assertEqual(result.returncode, 2)
        self.assertFalse(json.loads(result.stderr)["ok"])
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
