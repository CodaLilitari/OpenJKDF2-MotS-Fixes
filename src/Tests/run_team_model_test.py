#!/usr/bin/env python3
"""Test extracted production JK/MotS team-model packet handlers without assets.

Usage: python3 src/Tests/run_team_model_test.py --cc gcc --ubsan
Independent expectations cover the upstream five-row mapping and ensure
lookup misses skip model changes without rejecting otherwise valid team updates.
