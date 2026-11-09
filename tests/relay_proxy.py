"""Loopback-only UDP impairment proxy for the imported relay integration test."""
import argparse
import heapq
import random
import select
import socket
import time

parser = argparse.ArgumentParser()
parser.add_argument('--port', type=int, default=27940)
parser.add_argument('--target-port', type=int, default=27939)
parser.add_argument('--loss', type=float, default=0.03)
args = parser.parse_args()
rng = random.Random(20261105)
listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
listener.bind(('127.0.0.1', args.port))
listener.setblocking(False)
peers = {}
clients = {}
pending = []
serial = 0
dropped = 0
last_report = time.monotonic()

def queue(sock, address, packet):
    global serial, dropped
    # Impair data, including individual fragments and ACKs. Leave the relay's
    # session management intact so the test specifically exercises game traffic.
    data = len(packet) >= 8 and packet[4] == 64
    if data and rng.random() < args.loss:
        dropped += 1
        return
    serial += 1
    heapq.heappush(pending, (time.monotonic() + (rng.random() * 0.035 if data else 0), serial, sock, address, packet))

print('Relay UDP proxy ready', flush=True)
while True:
    readable, _, _ = select.select([listener, *clients], [], [], 0.005)
    for sock in readable:
        packet, address = sock.recvfrom(65536)
        if sock is listener:
            if address not in peers:
                if len(peers) >= 64:
                    continue
                upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                upstream.bind(('127.0.0.1', 0))
                upstream.setblocking(False)
                peers[address] = upstream
                clients[upstream] = address
            queue(peers[address], ('127.0.0.1', args.target_port), packet)
        elif address == ('127.0.0.1', args.target_port):
            queue(listener, clients[sock], packet)
    now = time.monotonic()
    if now - last_report >= 5:
        print(f"queued={serial} dropped={dropped}", flush=True)
        last_report = now
    while pending and pending[0][0] <= now:
        _, _, sock, address, packet = heapq.heappop(pending)
        sock.sendto(packet, address)
