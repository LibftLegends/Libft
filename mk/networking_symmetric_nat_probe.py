#!/usr/bin/env python3

import socket
import sys
import time


def run_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.bind(("0.0.0.0", 49322))
    server.settimeout(20.0)
    handled = 0
    while handled < 2:
        payload, source = server.recvfrom(128)
        if payload not in (b"destination-a", b"destination-b"):
            raise RuntimeError("unexpected symmetric NAT probe payload")
        server.sendto(str(source[1]).encode("ascii"), source)
        handled += 1
    server.close()


def run_client():
    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client.bind(("10.89.3.2", 40000))
    client.settimeout(10.0)
    client.sendto(b"destination-a", ("10.90.3.2", 49322))
    client.sendto(b"destination-b", ("10.90.3.3", 49322))
    observed_ports = []
    while len(observed_ports) < 2:
        payload, source = client.recvfrom(128)
        if source[0] != "10.90.3.2" and source[0] != "10.90.3.3":
            raise RuntimeError("symmetric NAT reply came from an unexpected peer")
        observed_ports.append(int(payload.decode("ascii")))
    client.close()
    if observed_ports[0] == observed_ports[1]:
        raise RuntimeError("destination-dependent NAT mappings used one source port")


if len(sys.argv) != 2:
    raise RuntimeError("expected server or client role")
if sys.argv[1] == "server":
    run_server()
elif sys.argv[1] == "client":
    run_client()
else:
    raise RuntimeError("unknown symmetric NAT probe role")
