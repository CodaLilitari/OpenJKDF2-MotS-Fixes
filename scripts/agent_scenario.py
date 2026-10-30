#!/usr/bin/env python3
"""Run bounded JSON mod smoke tests through ordinary OpenJKDF2 agent controls."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sys
import time
from types import SimpleNamespace

import agent_workspace as workspace
from agent_workspace import (AgentError, AgentTimeout, Client, atomic_json, digest,
                             keycode, prepare_session, read_json, read_regular,
                             run_session, utc_now, validate_command)

MISSING = object()
NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,63}")
FIELD = re.compile(r"[A-Za-z][A-Za-z0-9]*(?:\.(?:[A-Za-z][A-Za-z0-9]*|[0-9]+))*")


def number(value):
    try:
        return type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        return False


def keys(value, allowed, label):
    if not isinstance(value, dict) or set(value) - set(allowed):
        raise AgentError(f"{label} must be an object with only these fields: {', '.join(allowed)}")


def duration(value, maximum, label, *, allow_zero=False):
    if not number(value) or value > maximum or (value < 0 if allow_zero else value <= 0):
        raise AgentError(f"{label} must be a finite number {'from 0' if allow_zero else 'above 0'} to {maximum}")


def literal(value, depth=0):
    if depth > 4:
        return False
    if value is None or type(value) is bool or isinstance(value, str):
        return not isinstance(value, str) or len(value) <= 1024
    if number(value):
        return True
    return isinstance(value, list) and len(value) <= 256 and all(literal(v, depth + 1) for v in value)


def validate_expect(expect):
    if not isinstance(expect, dict) or not 1 <= len(expect) <= 32:
        raise AgentError("expect must contain 1..32 field comparisons")
    for field, expected in expect.items():
        if not isinstance(field, str) or len(field) > 128 or not FIELD.fullmatch(field):
            raise AgentError("Expected field names use dot-separated object keys or array indices")
        if isinstance(expected, dict):
            if expected == {"notNull": True} and type(expected['notNull']) is bool:
                continue
            if not expected or set(expected) - {"min", "max"} or not all(number(v) for v in expected.values()):
                raise AgentError(f"Invalid comparison for {field}; use min/max or notNull: true")
            if expected.get('min', -math.inf) > expected.get('max', math.inf):
                raise AgentError(f"Comparison bounds are reversed for {field}")
        elif not literal(expected):
            raise AgentError(f"Invalid literal comparison for {field}")


def validate_scenario(value):
    keys(value, ('schemaVersion', 'name', 'launch', 'steps'), 'Scenario')
    if type(value.get('schemaVersion')) is not int or value['schemaVersion'] != 1:
        raise AgentError("Scenario schemaVersion must be integer 1")
    if not isinstance(value.get('name'), str) or not 1 <= len(value['name']) <= 120 or any(ord(c) < 32 for c in value['name']):
        raise AgentError("Scenario name must contain 1..120 printable characters")
    launch = value.get('launch')
    keys(launch, ('game', 'episode', 'map'), 'launch')
    if launch.get('game') not in ('jk', 'mots'):
        raise AgentError("launch.game must be jk or mots")
    for field in ('episode', 'map'):
        if not isinstance(launch.get(field), str) or not re.fullmatch(r'[A-Za-z0-9_.-]{1,100}', launch[field]):
            raise AgentError(f"launch.{field} must be a simple engine name, not a path")
    steps = value.get('steps')
    if not isinstance(steps, list) or not 1 <= len(steps) <= 256:
        raise AgentError("Scenario must contain 1..256 steps")
    result = copy.deepcopy(value)
    names, checkpoints = set(), set()
    captures = 0
    fields = {'wait': ('expect', 'timeout', 'stableFor', 'retryTap'), 'assert': ('expect', 'distanceFrom'),
              'checkpoint': ('capture',), 'tap': ('key', 'seconds'), 'command': ('value',),
              'sleep': ('seconds',)}
    for index, step in enumerate(result['steps'], 1):
        if not isinstance(step, dict) or not isinstance(step.get('op'), str) or step['op'] not in fields:
            raise AgentError(f"Step {index} uses an unsupported operation")
        op = step['op']
        keys(step, ('op', 'name', *fields[op]), f'Step {index}')
        name = step.setdefault('name', f'step-{index:03d}-{op}')
        if not isinstance(name, str) or not NAME.fullmatch(name) or name in names:
            raise AgentError(f"Step {index} needs a unique simple name of up to 64 characters")
        names.add(name)
        if op in ('wait', 'assert'):
            if 'expect' in step:
                validate_expect(step['expect'])
            elif op == 'wait' or 'distanceFrom' not in step:
                raise AgentError(f"Step {name} needs an expectation")
        if op == 'wait':
            duration(step.setdefault('timeout', 30), 120, 'Wait timeout')
            duration(step.setdefault('stableFor', 0), step['timeout'], 'stableFor', allow_zero=True)
            if 'retryTap' in step:
                retry = step['retryTap']
                keys(retry, ('key', 'seconds', 'every'), 'retryTap')
                keycode(retry.get('key', ''))
                duration(retry.setdefault('seconds', .1), 1.5, 'Retry tap seconds')
                duration(retry.setdefault('every', 1), 30, 'Retry interval')
                if retry['every'] < max(.2, retry['seconds']):
                    raise AgentError('Retry interval must be at least 0.2 seconds and no shorter than its tap')
        elif op == 'assert' and 'distanceFrom' in step:
            reference = step['distanceFrom']
            keys(reference, ('checkpoint', 'min', 'max'), 'distanceFrom')
            if not isinstance(reference.get('checkpoint'), str) or reference['checkpoint'] not in checkpoints:
                raise AgentError("distanceFrom must name an earlier checkpoint")
            bounds = {k: v for k, v in reference.items() if k != 'checkpoint'}
            if not bounds or not all(number(v) and v >= 0 for v in bounds.values()) or bounds.get('min', 0) > bounds.get('max', math.inf):
                raise AgentError("distanceFrom requires nonnegative, ordered min/max bounds")
        elif op == 'checkpoint':
            if type(step.setdefault('capture', True)) is not bool:
                raise AgentError("checkpoint.capture must be boolean")
            captures += step['capture']
            checkpoints.add(name)
        elif op == 'tap':
            keycode(step.get('key', ''))
            duration(step.setdefault('seconds', .15), 1.5, 'Tap seconds')
        elif op == 'sleep':
            duration(step.get('seconds'), 30, 'Sleep seconds')
        elif op == 'command':
            command = validate_command(step.get('value'))
            if command.split(' ', 1)[0] not in {'key', 'mouse', 'button', 'text', 'release'}:
                raise AgentError("command steps accept ordinary input or release; the runner owns quit and observation")
    if captures > 31:
        raise AgentError("At most 31 capture checkpoints are allowed; one report slot is reserved for failure evidence")
    return result


def lookup(state, field):
    value = state
    for part in field.split('.'):
        if isinstance(value, dict):
            value = value.get(part, MISSING)
        elif isinstance(value, list) and part.isdecimal() and int(part) < len(value):
            value = value[int(part)]
        else:
            return MISSING
    return value


def equal(actual, expected):
    if actual is MISSING:
        return False
    if number(expected):
        return number(actual) and actual == expected
    if isinstance(expected, list):
        return isinstance(actual, list) and len(actual) == len(expected) and all(equal(a, e) for a, e in zip(actual, expected))
    return type(actual) is type(expected) and actual == expected


def mismatches(state, expect):
    failures = []
    for field, expected in expect.items():
        actual = lookup(state, field)
        if isinstance(expected, dict):
            if 'notNull' in expected:
                matches = actual is not MISSING and actual is not None
            else:
                matches = (number(actual) and actual >= expected.get('min', -math.inf)
                           and actual <= expected.get('max', math.inf))
        else:
            matches = equal(actual, expected)
        if not matches:
            failures.append({'field': field, 'expected': expected,
                             'actual': None if actual is MISSING else actual,
                             'missing': actual is MISSING})
    return failures


class ScenarioRunner:
    def __init__(self, spec, result, session, deadline, interrupted):
        self.spec, self.result, self.session = spec, result, Path(session)
        self.deadline, self.interrupted = deadline, interrupted
        self.checkpoints = {}
        self.last_state = None
        self.row = None

    def remaining(self, deadline=None):
        if self.interrupted:
            raise AgentError("Scenario interrupted by signal")
        remaining = min(self.deadline, deadline or self.deadline) - time.monotonic()
        if remaining <= 0:
            raise AgentTimeout("Scenario or step timed out")
        return remaining

    def request(self, command, deadline=None):
        response = Client(self.session, timeout=min(10, self.remaining(deadline))).request(command)
        self.row.setdefault('requestIds', []).append(response['id'])
        return response['result']

    def state(self, deadline=None):
        state = self.request('state', deadline)
        try:
            json.dumps(state, allow_nan=False)
        except ValueError as exc:
            raise AgentError("Engine state contains a non-finite value") from exc
        self.last_state = self.row['state'] = state
        self.result.setdefault('initialState', state)
        self.result['finalState'] = state
        return state

    def capture(self):
        capture = self.request('capture')
        name = capture.get('file')
        width, height = capture.get('width'), capture.get('height')
        if (not isinstance(name, str) or not re.fullmatch(r'frame-[1-9][0-9]*\.ppm', name)
                or type(width) is not int or type(height) is not int
                or not 1 <= width <= 4096 or not 1 <= height <= 2160):
            raise AgentError("Invalid capture receipt")
        data = read_regular(self.session / 'agent' / name, 32 * 1024 * 1024)
        header = f'P6\n{width} {height}\n255\n'.encode('ascii')
        if data is None or not data.startswith(header) or len(data) != len(header) + width * height * 3:
            raise AgentError("Capture receipt does not identify a complete native RGB image")
        self.row['capture'] = dict(capture, sha256=hashlib.sha256(data).hexdigest())

    def sleep(self, seconds, deadline=None):
        finish = time.monotonic() + seconds
        while time.monotonic() < finish:
            time.sleep(min(.05, max(0, finish - time.monotonic()), self.remaining(deadline)))

    def tap(self, key, seconds, deadline=None):
        if self.remaining(deadline) <= seconds:
            raise AgentTimeout("Not enough execution budget for this key tap")
        code = keycode(key)
        self.request(f'key {code} 1', deadline)
        original_error = None
        try:
            self.sleep(seconds, deadline)
        except BaseException as exc:
            original_error = exc
            raise
        finally:
            try:
                self.request(f'key {code} 0', deadline)
            except (Exception, KeyboardInterrupt) as release_error:
                if original_error is None:
                    raise
                original_error.add_note(f'Key-up also failed: {release_error}')

    def execute(self, step):
        op = step['op']
        self.remaining()
        if op == 'command':
            self.request(step['value'])
        elif op == 'tap':
            self.tap(step['key'], step['seconds'])
        elif op == 'sleep':
            self.sleep(step['seconds'])
        elif op == 'checkpoint':
            self.checkpoints[step['name']] = self.state()
            if step['capture']:
                self.capture()
        elif op == 'wait':
            finish = min(self.deadline, time.monotonic() + step['timeout'])
            since = None
            next_retry = 0
            while time.monotonic() < finish:
                self.row['mismatches'] = mismatches(self.state(finish), step['expect'])
                now = time.monotonic()
                if not self.row['mismatches']:
                    since = now if since is None else since
                    if now - since >= step['stableFor']:
                        return
                else:
                    since = None
                    retry = step.get('retryTap')
                    if retry and now >= next_retry and finish - now > retry['seconds']:
                        self.tap(retry['key'], retry['seconds'], finish)
                        next_retry = time.monotonic() + retry['every']
                self.sleep(min(.1, max(0, finish - time.monotonic())), finish)
            fields = ', '.join(step['expect'])
            raise AgentTimeout(f'Wait timed out for {fields}')
        elif op == 'assert':
            state = self.state()
            failures = mismatches(state, step.get('expect', {}))
            self.row['mismatches'] = failures
            if 'distanceFrom' in step:
                reference = step['distanceFrom']
                points = [lookup(s, 'player.position') for s in (state, self.checkpoints[reference['checkpoint']])]
                if any(not isinstance(p, list) or len(p) != 3 or not all(number(v) for v in p) for p in points):
                    raise AgentError('distanceFrom requires a valid finite player position at both checkpoints')
                distance = math.dist(*points)
                if not number(distance):
                    raise AgentError('distanceFrom calculation must be finite')
                self.row['distance'] = distance
                self.row['distanceFrom'] = reference
                if not reference.get('min', 0) <= distance <= reference.get('max', math.inf):
                    failures.append({'field': 'player.position distance', 'expected': reference, 'actual': distance})
            if failures:
                raise AgentError('Assertion failed: ' + ', '.join(f['field'] for f in failures))

    def run(self):
        for step, row in zip(self.spec['steps'], self.result['steps']):
            self.row = row
            started = time.monotonic()
            row['status'] = 'running'
            atomic_json(self.session / 'scenario.json', self.result)
            try:
                self.execute(step)
                row['status'] = 'passed'
            except (Exception, KeyboardInterrupt) as exc:
                row.update(status='failed', error=str(exc))
                failure = {'index': len(self.result['steps']) + 1, 'name': 'failure-evidence',
                           'op': 'failure-capture', 'status': 'unavailable'}
                if self.last_state is not None:
                    failure['state'] = self.last_state
                self.result['steps'].append(failure)
                self.row = failure
                try:
                    self.capture()
                    failure['status'] = 'passed'
                except (Exception, KeyboardInterrupt) as capture_error:
                    failure['error'] = str(capture_error)
                raise
            finally:
                row['durationSeconds'] = round(time.monotonic() - started, 6)
                atomic_json(self.session / 'scenario.json', self.result)


def reject_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise AgentError(f'Duplicate JSON key: {key}')
        result[key] = value
    return result


def run_scenario(args):
    raw = read_regular(args.scenario)
    if raw is None:
        raise AgentError('Scenario file does not exist')
    try:
        spec = validate_scenario(json.loads(raw, object_pairs_hook=reject_duplicates))
    except (ValueError, UnicodeError, RecursionError) as exc:
        raise AgentError(f'Invalid scenario JSON: {exc}') from exc
    duration(args.seconds, 3600, 'Execution seconds')
    if args.seconds < 1 or not 640 <= args.width <= 4096 or not 480 <= args.height <= 2160:
        raise AgentError('Execution budget must be at least 1 second; display must be 640..4096 by 480..2160')
    binary = Path(args.binary).resolve(strict=True)
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise AgentError('Engine binary is not executable')
    prepare_session(args.data_root, args.session, game=spec['launch']['game'],
                    startup=args.startup, episode_files=args.episode_file)
    session = Path(args.session).resolve(strict=True)
    result = {'schemaVersion': 1, 'name': spec['name'], 'status': 'running', 'startedAt': utc_now(),
              'scenarioSha256': hashlib.sha256(raw).hexdigest(), 'runnerSha256': digest(__file__),
              'workspaceToolSha256': digest(workspace.__file__), 'assetManifestSha256': digest(session / 'session.json'),
              'physicsOverrides': False, 'deterministicSimulation': False,
              'steps': [{'index': i, 'name': s['name'], 'op': s['op'], 'status': 'not_run'}
                        for i, s in enumerate(spec['steps'], 1)]}
    atomic_json(session / 'scenario-input.json', spec)
    atomic_json(session / 'scenario.json', result)
    launch = SimpleNamespace(**{**vars(args), 'session': session,
                               'episode': spec['launch']['episode'], 'map': spec['launch']['map']})
    try:
        runtime = run_session(launch, controller=lambda path, deadline, interrupted:
                              ScenarioRunner(spec, result, path, deadline, interrupted).run())
        if runtime['status'] != 'completed' or runtime.get('exitCode') != 0:
            raise AgentError('Scenario requires a clean engine exit')
        result['status'] = 'passed'
    except (Exception, KeyboardInterrupt) as exc:
        result.update(status='failed', error=f'{type(exc).__name__}: {exc}')
    finally:
        runtime = read_json(session / 'run.json') or {}
        result['runtime'] = {k: runtime[k] for k in ('status', 'exitCode', 'stopReason', 'forcedStop', 'shutdownErrors', 'error') if k in runtime}
        result['binarySha256'] = runtime.get('binarySha256')
        result['finishedAt'] = utc_now()
        atomic_json(session / 'scenario.json', result)
    if not args.no_report:
        try:
            from agent_report import build_report
            build_report(session, session / 'report')
            result['report'] = 'report/index.html'
        except (Exception, KeyboardInterrupt) as exc:
            result.update(status='failed', reportError=str(exc))
        atomic_json(session / 'scenario.json', result)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('scenario', 'binary', 'data-root', 'session'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--startup')
    parser.add_argument('--episode-file', action='append', default=[])
    parser.add_argument('--seconds', type=float, default=180)
    parser.add_argument('--xvfb', action='store_true')
    parser.add_argument('--software', action='store_true')
    parser.add_argument('--audio', action='store_true')
    parser.add_argument('--width', type=int, default=1024)
    parser.add_argument('--height', type=int, default=768)
    parser.add_argument('--no-report', action='store_true', help='retain JSON/logs without generating HTML/PNG evidence')
    args = parser.parse_args(argv)
    try:
        result = run_scenario(args)
        print(json.dumps(result, indent=2, allow_nan=False))
        return 0 if result['status'] == 'passed' else 2
    except (AgentError, OSError, ValueError) as exc:
        print(json.dumps({'status': 'failed', 'error': str(exc)}), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
