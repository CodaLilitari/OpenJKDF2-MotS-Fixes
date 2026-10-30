#!/usr/bin/env python3
"""Scenario behavior against a real file-protocol peer; no game data required."""
from contextlib import redirect_stdout
import importlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

PEER = r'''
import json,os,pathlib,sys,time
directory=pathlib.Path(os.environ['OPENJKDF2_AGENT_DIR'])
state={'world':'fixture.jkl','worldLoaded':True,'levelActive':False,
       'menuActive':True,'multiplayer':False,'paused':False,
       'heldKeys':[],'heldButtons':[],'renderedFrame':1,'player':None}
events=[]
deadline=time.monotonic()+20
while time.monotonic()<deadline:
    mailbox=directory/'command.txt'
    if not mailbox.exists(): time.sleep(.002); continue
    wire=mailbox.read_text().splitlines(); mailbox.unlink()
    request_id=int(wire[1]); command=wire[2]; parts=command.split(' '); verb=parts[0]
    events.append(command)
    result={}
    if verb=='key':
        key,down=map(int,parts[1:])
        state['heldKeys']=[key] if down else []
        if key==40 and down:
            state.update(levelActive=True,menuActive=False)
            state['player']={'position':[-1e308,0.,0.] if EXTREME_POSITIONS else [0.,0.,0.],'health':100,'sector':0}
        if key==82 and down and state['player']:
            state['player']['position'][0]=1e308 if EXTREME_POSITIONS else state['player']['position'][0]+.1
    elif verb=='state':
        state['renderedFrame']+=1; result=state.copy()
    elif verb=='capture':
        name=f'frame-{request_id}.ppm'
        (directory/name).write_bytes(b'P6\n2 1\n255\n'+bytes([255,0,0,0,255,0]))
        result={'file':name,'width':2,'height':1,'renderedFrame':state['renderedFrame'],'monotonicMs':123}
    elif verb in ('release','quit'):
        if SIGNAL_ON_RELEASE and verb=='release':
            os.kill(os.getppid(),15)
            time.sleep(.05)
        state.update(heldKeys=[],heldButtons=[])
    reply={'schemaVersion':1,'id':request_id,'command':verb,'ok':True,'result':result}
    temp=directory/'peer.tmp'; temp.write_text(json.dumps(reply)); temp.replace(directory/'response.json')
    if verb=='quit':
        pathlib.Path('peer-events.json').write_text(json.dumps(events))
        sys.exit(EXIT_CODE)
sys.exit(99)
'''


class ScenarioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tool = importlib.import_module("agent_scenario")

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="openjk-scenario-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = self.root / "data"
        (self.data / "resource").mkdir(parents=True)
        (self.data / "episode").mkdir()
        for relative in ("resource/res2.gob", "resource/res1hi.gob", "episode/jk1.gob"):
            (self.data / relative).write_bytes(b"synthetic fixture, not licensed content")
        self.session = self.root / "session"

    def recipe(self):
        return {"schemaVersion": 1, "name": "Synthetic movement",
                "launch": {"game": "jk", "episode": "jk1", "map": "fixture.jkl"},
                "steps": [
                    {"op": "wait", "expect": {"menuActive": True}, "timeout": 1},
                    {"op": "tap", "key": "Enter", "seconds": .01},
                    {"op": "wait", "expect": {"levelActive": True, "player": {"notNull": True}}, "timeout": 1},
                    {"op": "checkpoint", "name": "start"},
                    {"op": "tap", "key": "Up", "seconds": .01},
                    {"op": "assert", "expect": {"world": "fixture.jkl", "player.health": {"min": 1}},
                     "distanceFrom": {"checkpoint": "start", "min": .05, "max": .2}},
                    {"op": "checkpoint", "name": "finish"}]}

    def args(self, recipe=None, exit_code=0, *, signal_on_release=False, extreme_positions=False):
        path = self.root / "recipe.json"
        path.write_text(json.dumps(recipe or self.recipe()))
        engine = self.root / "peer-engine"
        engine.write_text(f"#!{sys.executable}\nEXIT_CODE={exit_code}\nSIGNAL_ON_RELEASE={signal_on_release}\nEXTREME_POSITIONS={extreme_positions}\n" + PEER)
        engine.chmod(0o700)
        return SimpleNamespace(scenario=path, session=self.session, data_root=self.data,
                               binary=engine, startup=None, episode_file=[], seconds=5,
                               xvfb=False, software=False, audio=False, width=1024,
                               height=768, no_report=True)

    def run_fixture(self, args):
        with mock.patch.dict(os.environ, {"DISPLAY": ":unused-by-peer"}), redirect_stdout(io.StringIO()):
            result = self.tool.run_scenario(args)
        runtime = json.loads((self.session / "run.json").read_text())
        with self.assertRaises(ProcessLookupError):
            os.kill(runtime["pid"], 0)
        return result, runtime

    def test_validation_rejects_execution_and_unbounded_or_mistyped_steps(self):
        invalid = [
            {"op": "shell", "value": "echo unintended"},
            {"op": []},
            {"op": "command", "value": "quit"},
            {"op": "command", "value": "console godmode"},
            {"op": "tap", "key": "Up", "seconds": True},
            {"op": "tap", "key": "Up", "seconds": 2},
            {"op": "wait", "expect": {}, "timeout": 1},
            {"op": "wait", "expect": {"world": "x"}, "timeout": float('inf')},
            {"op": "wait", "expect": {"world": "x"}, "retryTap": {"key": "Enter", "every": .01}},
            {"op": "wait", "expect": {"world": "x"}, "retryTap": {"key": "Enter", "seconds": 2}},
            {"op": "wait", "expect": {"world": "x"}, "retryTap": {"key": "Enter", "shell": "unused"}},
            {"op": "sleep", "seconds": 31},
            {"op": "assert", "expect": {"player.health": {"min": float('nan')}}},
            {"op": "assert", "distanceFrom": {"checkpoint": "future", "max": 1}},
            {"op": "assert", "distanceFrom": {"checkpoint": [], "max": 1}},
            {"op": "assert", "expect": {"player.health": {"min": 10 ** 1000}}},
            {"op": "checkpoint", "name": "x", "typo": True},
        ]
        for step in invalid:
            with self.subTest(step=step), self.assertRaises(self.tool.AgentError):
                value = self.recipe(); value['steps'] = [step]
                self.tool.validate_scenario(value)
        value = self.recipe(); value['launch']['map'] = '../escape.jkl'
        with self.assertRaises(self.tool.AgentError): self.tool.validate_scenario(value)
        value = self.recipe(); value['schemaVersion'] = True
        with self.assertRaises(self.tool.AgentError): self.tool.validate_scenario(value)

    def test_checkpoint_names_and_capture_budget_are_validated(self):
        value = self.recipe(); value['steps'].append({'op': 'checkpoint', 'name': 'start'})
        with self.assertRaises(self.tool.AgentError): self.tool.validate_scenario(value)
        value['steps'] = [{'op': 'checkpoint', 'name': f'frame-{i}'} for i in range(32)]
        with self.assertRaises(self.tool.AgentError): self.tool.validate_scenario(value)

    def test_state_comparisons_distinguish_missing_null_bool_and_nonfinite(self):
        self.assertFalse(self.tool.mismatches({'player': None}, {'player': None}))
        self.assertTrue(self.tool.mismatches({}, {'player': None}))
        self.assertTrue(self.tool.mismatches({'levelActive': 1}, {'levelActive': True}))
        self.assertTrue(self.tool.mismatches({'health': True}, {'health': {'min': 0}}))
        self.assertTrue(self.tool.mismatches({'health': float('nan')}, {'health': {'min': 0}}))
        self.assertFalse(self.tool.mismatches({'player': {'position': [0, 2, 3]}},
                                              {'player.position.1': {'min': 1, 'max': 3}}))

    def test_real_protocol_scenario_checks_movement_and_clean_shutdown(self):
        result, runtime = self.run_fixture(self.args())
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(runtime['status'], 'completed')
        self.assertEqual(runtime['exitCode'], 0)
        self.assertTrue(all(s['status'] == 'passed' for s in result['steps']))
        checkpoints = [s for s in result['steps'] if s['op'] == 'checkpoint']
        self.assertEqual(len(checkpoints), 2)
        self.assertEqual(checkpoints[-1]['state']['player']['position'], [.1, 0, 0])
        self.assertTrue((self.session / 'agent' / checkpoints[-1]['capture']['file']).is_file())
        events = json.loads((self.session / 'peer-events.json').read_text())
        self.assertEqual(events[-2:], ['release', 'quit'])
        self.assertEqual((self.data / 'episode/jk1.gob').read_bytes(), b'synthetic fixture, not licensed content')
        self.assertEqual(json.loads((self.session / 'scenario.json').read_text())['status'], 'passed')

    def test_failed_assertion_retains_state_capture_and_still_quits(self):
        recipe = self.recipe()
        recipe['steps'][5]['expect']['player.health'] = {'min': 101}
        result, runtime = self.run_fixture(self.args(recipe))
        self.assertEqual(result['status'], 'failed')
        self.assertEqual(runtime['status'], 'failed')
        self.assertEqual(runtime['exitCode'], 0)
        self.assertIn('player.health', result['error'])
        self.assertTrue(any(s['status'] == 'failed' and s.get('state') for s in result['steps']))
        self.assertTrue(any(s.get('capture') for s in result['steps'] if s['op'] == 'failure-capture'))
        self.assertEqual(json.loads((self.session / 'peer-events.json').read_text())[-2:], ['release', 'quit'])

    def test_wait_timeout_is_failure_despite_clean_engine_exit(self):
        recipe = self.recipe(); recipe['steps'] = [{'op': 'wait', 'expect': {'levelActive': True}, 'timeout': .08}]
        result, runtime = self.run_fixture(self.args(recipe))
        self.assertEqual(result['status'], 'failed')
        self.assertEqual(runtime['exitCode'], 0)
        self.assertIn('timed out', result['error'])
        self.assertFalse(result['steps'][0]['state']['levelActive'])

    def test_engine_bad_exit_cannot_pass_after_successful_assertions(self):
        result, runtime = self.run_fixture(self.args(exit_code=17))
        self.assertEqual(result['status'], 'failed')
        self.assertEqual(runtime['exitCode'], 17)
        self.assertIn('17', result['error'])

    def test_invalid_recipe_does_not_create_or_launch_session(self):
        recipe = self.recipe(); recipe['steps'][0]['timeout'] = -1
        with self.assertRaises(self.tool.AgentError): self.tool.run_scenario(self.args(recipe))
        self.assertFalse(self.session.exists())

    def test_signal_during_shutdown_cannot_report_success(self):
        result, runtime = self.run_fixture(self.args(signal_on_release=True))
        self.assertEqual(result['status'], 'failed')
        self.assertEqual(runtime['status'], 'failed')
        self.assertEqual(runtime['exitCode'], 0)
        self.assertEqual(runtime['stopReason'], 'signal')
        self.assertIn('interrupted', result['error'])

    def test_overflow_distance_has_a_durable_failed_receipt(self):
        result, runtime = self.run_fixture(self.args(extreme_positions=True))
        self.assertEqual(result['status'], 'failed')
        self.assertEqual(runtime['exitCode'], 0)
        self.assertIn('finite', result['error'])
        self.assertEqual(json.loads((self.session / 'scenario.json').read_text())['status'], 'failed')

    def test_relative_session_path_can_generate_report(self):
        args = self.args()
        args.session = Path(os.path.relpath(self.session))
        args.no_report = False
        result, runtime = self.run_fixture(args)
        self.assertEqual(result['status'], 'passed', result.get('reportError'))
        self.assertTrue((self.session / 'report/index.html').is_file())
        self.assertEqual(runtime['exitCode'], 0)

    def test_wait_retries_explicit_key_until_state_matches(self):
        recipe = self.recipe()
        recipe['steps'] = [
            {'op': 'wait', 'name': 'enter-when-ready', 'timeout': 1,
             'expect': {'levelActive': True},
             'retryTap': {'key': 'Enter', 'seconds': .01, 'every': .2}},
            {'op': 'checkpoint', 'name': 'entered'}
        ]
        result, runtime = self.run_fixture(self.args(recipe))
        self.assertEqual(result['status'], 'passed')
        events = json.loads((self.session / 'peer-events.json').read_text())
        self.assertEqual(events.count('key 40 1'), 1)
        self.assertEqual(events.count('key 40 0'), 1)
        self.assertTrue(result['finalState']['levelActive'])

    def test_signal_during_display_cleanup_cannot_report_success(self):
        args = self.args()
        args.xvfb = True
        fakebin = self.root / 'bin'
        fakebin.mkdir()
        fake = fakebin / 'Xvfb'
        pidfile = self.root / 'display.pid'
        fake.write_text(
            f'#!{sys.executable}\nimport os,signal,sys,time,pathlib\n'
            f'pathlib.Path({str(pidfile)!r}).write_text(str(os.getpid()))\n'
            'def stop(signum,frame):\n'
            ' os.kill(os.getppid(),signal.SIGTERM)\n'
            ' raise SystemExit(0)\n'
            'signal.signal(signal.SIGTERM,stop)\n'
            'fd=int(sys.argv[sys.argv.index("-displayfd")+1])\n'
            'os.write(fd,b"73\\n"); os.close(fd)\n'
            'while True: time.sleep(.05)\n')
        fake.chmod(0o700)
        with mock.patch.dict(os.environ, {'PATH': str(fakebin) + os.pathsep + os.environ['PATH']}):
            result, runtime = self.run_fixture(args)
        self.assertEqual(result['status'], 'failed')
        self.assertEqual(runtime['status'], 'failed')
        self.assertIn('interrupted', result['error'])
        with self.assertRaises(ProcessLookupError):
            os.kill(int(pidfile.read_text()), 0)


if __name__ == '__main__':
    unittest.main()
