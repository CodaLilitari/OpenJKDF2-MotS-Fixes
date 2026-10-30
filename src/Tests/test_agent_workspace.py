#!/usr/bin/env python3
"""Behavioral tests of the real workspace tool; no licensed assets needed."""
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from contextlib import redirect_stdout
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/agent_workspace.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("agent_workspace", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class WorkspaceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not SCRIPT.exists():
            raise AssertionError("the agent workspace tool is not implemented")
        cls.tool = load_tool()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="openjk-agent-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = self.root / "game"
        (self.data / "resource").mkdir(parents=True)
        (self.data / "episode").mkdir()
        (self.data / "resource/res2.gob").write_bytes(b"synthetic common resources")
        (self.data / "resource/res1hi.gob").write_bytes(b"synthetic audio")
        (self.data / "episode/jk1.gob").write_bytes(b"synthetic episode")
        (self.data / "player").mkdir()
        (self.data / "player/private.plr").write_bytes(b"never import this profile")
        (self.data / "config.json").write_text('{"private":true}')
        self.session = self.root / "session"

    def prepare(self):
        return self.tool.prepare_session(self.data, self.session)

    def peer(self, responder):
        errors = []
        def run():
            try:
                mailbox = self.session / "agent/command.txt"
                deadline = time.monotonic() + 2
                while not mailbox.exists():
                    if time.monotonic() >= deadline:
                        raise AssertionError("no command reached the peer")
                    time.sleep(.002)
                wire = mailbox.read_bytes()
                mailbox.unlink()
                responder(wire)
            except BaseException as exc:
                errors.append(exc)
        thread = threading.Thread(target=run, daemon=True)
        thread.start()
        def join():
            thread.join(3)
            self.assertFalse(thread.is_alive(), "protocol peer did not finish")
            if errors:
                raise errors[0]
        self.addCleanup(join)

    def reply(self, value):
        path = self.session / "agent/reply.tmp"
        path.write_text(json.dumps(value))
        path.replace(self.session / "agent/response.json")

    def test_prepare_copies_assets_without_profile_or_shared_writes(self):
        manifest = self.prepare()
        self.assertEqual(manifest["schemaVersion"], 1)
        self.assertTrue((self.session / ".openjkdf2-agent-session").is_file())
        self.assertEqual(stat.S_IMODE((self.session / ".openjkdf2-agent-session").stat().st_mode), 0o600)
        self.assertEqual(stat.S_IMODE(self.session.stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE((self.session / "agent").stat().st_mode), 0o700)
        self.assertEqual((self.session / "resource/res2.gob").read_bytes(), b"synthetic common resources")
        self.assertFalse((self.session / "player").exists())
        self.assertFalse((self.session / "config.json").exists())
        (self.session / "resource/res2.gob").write_bytes(b"session only")
        self.assertEqual((self.data / "resource/res2.gob").read_bytes(), b"synthetic common resources")
        self.assertEqual((self.session / "resource/jk_.cd").read_bytes(), b"\xc4\x32\x92\x69")
        self.assertFalse((self.data / "resource/jk_.cd").exists())

    def test_existing_session_is_never_overwritten(self):
        self.session.mkdir()
        (self.session / "keep").write_text("untouched")
        with self.assertRaises((self.tool.AgentError, FileExistsError)):
            self.prepare()
        self.assertEqual((self.session / "keep").read_text(), "untouched")

    def test_missing_resources_fail_before_creating_session(self):
        (self.data / "resource/res2.gob").unlink()
        with self.assertRaises(self.tool.AgentError):
            self.prepare()
        self.assertFalse(self.session.exists())

    def test_wrong_game_install_marker_is_rejected(self):
        (self.data / "resource/jk_.cd").write_bytes(b"\x29\x69\x42\x3b")
        with self.assertRaises(self.tool.AgentError):
            self.prepare()
        self.assertFalse(self.session.exists())

    def test_symlink_escape_in_assets_is_rejected(self):
        outside = self.root / "outside.txt"
        outside.write_text("not game data")
        (self.data / "resource/escape").symlink_to(outside)
        with self.assertRaises(self.tool.AgentError):
            self.prepare()
        self.assertFalse(self.session.exists())

    def test_session_inside_data_is_rejected(self):
        with self.assertRaises(self.tool.AgentError):
            self.tool.prepare_session(self.data, self.data / "resource/session")
        self.assertFalse((self.data / "resource/session").exists())

    def test_commands_reject_protocol_injection_and_bad_ranges(self):
        for command in ["key 26 1\nquit", "key 0 1", "key 256 1", "key 26 2",
                        "mouse 2049 0", "button 6 1", "quit extra", "state\x00",
                        "text ", "text " + "x" * 128, "console godmode"]:
            with self.subTest(command=command):
                with self.assertRaises(self.tool.AgentError):
                    self.tool.validate_command(command)
        for command in ["ping", "state", "key 26 1", "mouse -20 15",
                        "button 1 0", "release", "capture", "quit", "text Agent 1"]:
            self.assertEqual(self.tool.validate_command(command), command)

    def test_named_keys_follow_sdl_scancodes(self):
        self.assertEqual(self.tool.keycode("W"), 26)
        self.assertEqual(self.tool.keycode("Up"), 82)
        self.assertEqual(self.tool.keycode("F9"), 66)
        self.assertEqual(self.tool.keycode("Escape"), 41)
        self.assertEqual(self.tool.keycode("4"), 4)
        self.assertEqual(self.tool.keycode("Digit4"), 33)
        with self.assertRaises(self.tool.AgentError):
            self.tool.keycode("not-a-key")

    def test_request_uses_exact_wire_and_matched_receipt(self):
        self.prepare()
        def respond(wire):
            self.assertEqual(wire, b"openjkdf2-agent-v1\n1\nstate\n")
            self.reply({"schemaVersion": 1, "id": 1, "command": "state", "ok": True,
                        "result": {"world": "fixture.jkl"}})
        self.peer(respond)
        response = self.tool.Client(self.session, timeout=1).request("state")
        self.assertEqual(response["result"]["world"], "fixture.jkl")
        history = (self.session / "agent/commands.jsonl").read_text().splitlines()
        self.assertEqual(json.loads(history[-1])["response"]["id"], 1)

    def test_stale_receipt_does_not_acknowledge_new_command(self):
        self.prepare()
        self.reply({"schemaVersion": 1, "id": 7, "command": "ping", "ok": True,
                    "result": {"stale": True}})
        def respond(wire):
            self.assertEqual(wire, b"openjkdf2-agent-v1\n8\nping\n")
            time.sleep(.02)
            self.reply({"schemaVersion": 1, "id": 8, "command": "ping", "ok": True,
                        "result": {"fresh": True}})
        self.peer(respond)
        response = self.tool.Client(self.session, timeout=1).request("ping")
        self.assertEqual(response["result"], {"fresh": True})

    def test_timeout_preserves_unconsumed_command(self):
        self.prepare()
        client = self.tool.Client(self.session, timeout=.04)
        with self.assertRaises(self.tool.AgentTimeout):
            client.request("key 26 1")
        self.assertEqual((self.session / "agent/command.txt").read_bytes(),
                         b"openjkdf2-agent-v1\n1\nkey 26 1\n")
        with self.assertRaises(self.tool.AgentError):
            client.request("quit")

    def test_engine_rejection_is_not_reported_as_success(self):
        self.prepare()
        self.peer(lambda wire: self.reply({"schemaVersion": 1, "id": 1, "command": "capture",
                                           "ok": False, "error": "no rendered frame"}))
        with self.assertRaisesRegex(self.tool.AgentError, "no rendered frame"):
            self.tool.Client(self.session, timeout=1).request("capture")

    def test_reply_symlink_is_never_followed(self):
        self.prepare()
        external = self.root / "external.json"
        external.write_text('{"id":100}')
        (self.session / "agent/response.json").symlink_to(external)
        with self.assertRaises(self.tool.AgentError):
            self.tool.Client(self.session, timeout=.04).request("ping")
        self.assertFalse((self.session / "agent/command.txt").exists())

    def test_boolean_receipt_cannot_acknowledge_integer_command(self):
        self.prepare()
        self.peer(lambda wire: self.reply({"schemaVersion": True, "id": True, "command": "state",
                                           "ok": True, "result": {}}))
        with self.assertRaises(self.tool.AgentError):
            self.tool.Client(self.session, timeout=.1).request("state")

    def test_history_fifo_cannot_block_request_timeout(self):
        self.prepare()
        os.mkfifo(self.session / "agent/commands.jsonl", 0o600)
        try:
            completed = subprocess.run([sys.executable, str(SCRIPT), "command", "--session",
                                        str(self.session), "--timeout", ".05", "ping"],
                                       capture_output=True, text=True, timeout=1)
        except subprocess.TimeoutExpired:
            self.fail("history FIFO bypassed the request timeout")
        self.assertEqual(completed.returncode, 2)
        self.assertFalse((self.session / "agent/command.txt").exists())

    def test_history_failure_preserves_original_command_timeout(self):
        self.prepare()
        def respond(wire):
            history = self.session / "agent/commands.jsonl"
            history.unlink(missing_ok=True)
            history.mkdir()
        self.peer(respond)
        with self.assertRaises(self.tool.AgentTimeout):
            self.tool.Client(self.session, timeout=.1).request("state")

    def test_history_failure_after_receipt_reports_already_completed_command(self):
        self.prepare()
        def respond(wire):
            history = self.session / "agent/commands.jsonl"
            history.unlink(missing_ok=True)
            history.mkdir()
            self.reply({"schemaVersion": 1, "id": 1, "command": "ping", "ok": True, "result": {}})
        self.peer(respond)
        with self.assertRaisesRegex(self.tool.AgentError, "completed"):
            self.tool.Client(self.session, timeout=1).request("ping")

    def test_second_writer_cannot_enter_locked_exchange(self):
        import fcntl
        self.prepare()
        lock = open(self.session / "agent/client.lock", "w")
        self.addCleanup(lock.close)
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with self.assertRaises(self.tool.AgentTimeout):
            self.tool.Client(self.session, timeout=.04).request("ping")
        self.assertFalse((self.session / "agent/command.txt").exists())

    def test_stop_child_reaps_owned_process(self):
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"],
                                 start_new_session=True)
        self.addCleanup(lambda: child.poll() is None and child.kill())
        self.tool.stop_child(child, grace=.1)
        self.assertIsNotNone(child.poll())

    def test_crashed_engine_has_failed_durable_receipt(self):
        self.prepare()
        engine = self.root / "engine"
        engine.write_text(f"#!{sys.executable}\nimport json,pathlib,time,os\n"
                          "pathlib.Path('observed-env.json').write_text(json.dumps({'runtime':os.environ.get('XDG_RUNTIME_DIR')}))\n"
                          "p=pathlib.Path('agent/command.txt')\n"
                          "while not p.exists(): time.sleep(.005)\n"
                          "request_id=int(p.read_text().splitlines()[1]); p.unlink()\n"
                          "reply={'schemaVersion':1,'id':request_id,'command':'ping','ok':True,'result':{}}\n"
                          "tmp=pathlib.Path('agent/response.tmp'); tmp.write_text(json.dumps(reply))\n"
                          "tmp.replace('agent/response.json')\n"
                          "time.sleep(.15)\nraise SystemExit(17)\n")
        engine.chmod(0o700)
        args = SimpleNamespace(session=self.session, binary=engine, seconds=1,
                               episode="jk1", map="01narshadda.jkl", software=False,
                               audio=False, xvfb=False, width=1024, height=768)
        old_display = os.environ.get("DISPLAY")
        os.environ["DISPLAY"] = ":unused-for-test"
        try:
            with redirect_stdout(io.StringIO()), self.assertRaises(self.tool.AgentError):
                self.tool.run_session(args)
        finally:
            if old_display is None:
                os.environ.pop("DISPLAY", None)
            else:
                os.environ["DISPLAY"] = old_display
        record = json.loads((self.session / "run.json").read_text())
        self.assertEqual(record["status"], "failed")
        self.assertEqual(record["exitCode"], 17)
        self.assertIn("17", record["error"])
        observed = json.loads((self.session / "observed-env.json").read_text())
        self.assertEqual(observed["runtime"], str(self.session / "xdg/runtime"))
        self.assertEqual(stat.S_IMODE((self.session / "xdg/runtime").stat().st_mode), 0o700)

    def test_stop_child_terminates_descendants_after_leader_exits(self):
        import signal
        pidfile = self.root / "descendant.pid"
        code = ("import os,signal,time,pathlib\n"
                "if os.fork()==0:\n"
                " signal.signal(signal.SIGTERM,signal.SIG_IGN)\n"
                f" pathlib.Path({str(pidfile)!r}).write_text(str(os.getpid()))\n"
                " time.sleep(30)\n"
                "else:\n time.sleep(30)\n")
        child = subprocess.Popen([sys.executable, "-c", code], start_new_session=True)
        def cleanup():
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.wait(timeout=3)
        self.addCleanup(cleanup)
        deadline = time.monotonic() + 2
        while not pidfile.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        self.assertTrue(pidfile.exists())
        descendant = int(pidfile.read_text())
        self.tool.stop_child(child, grace=.05)
        status = Path(f"/proc/{descendant}/stat")
        deadline = time.monotonic() + 1
        while status.exists() and status.read_text().split(")", 1)[1].strip().split()[0] != "Z":
            if time.monotonic() >= deadline:
                self.fail("owned descendant survived cleanup")
            time.sleep(.01)

    def test_signal_during_display_startup_stops_owned_child(self):
        import signal
        self.prepare()
        fakebin = self.root / "bin"
        fakebin.mkdir()
        pidfile = self.root / "xvfb.pid"
        fake = fakebin / "Xvfb"
        fake.write_text(f"#!{sys.executable}\nimport os,time,pathlib\n"
                        f"pathlib.Path({str(pidfile)!r}).write_text(str(os.getpid()))\n"
                        "time.sleep(30)\n")
        fake.chmod(0o700)
        env = dict(os.environ, PATH=str(fakebin) + os.pathsep + os.environ["PATH"])
        log = open(self.root / "launcher.log", "w")
        self.addCleanup(log.close)
        launcher = subprocess.Popen([sys.executable, str(SCRIPT), "run", "--binary", "/bin/true",
                                     "--session", str(self.session), "--xvfb"], env=env,
                                    stdout=log, stderr=log, start_new_session=True)
        def cleanup():
            if launcher.poll() is None:
                launcher.kill()
            launcher.wait(timeout=3)
            if pidfile.exists():
                try:
                    os.killpg(int(pidfile.read_text()), signal.SIGKILL)
                except ProcessLookupError:
                    pass
        self.addCleanup(cleanup)
        deadline = time.monotonic() + 3
        while not pidfile.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        self.assertTrue(pidfile.exists())
        launcher.terminate()
        launcher.wait(timeout=3)
        pid = int(pidfile.read_text())
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)
        record = json.loads((self.session / "run.json").read_text())
        self.assertEqual(record["status"], "failed")
        self.assertIn("finishedAt", record)


if __name__ == "__main__":
    unittest.main()
