import argparse
import socket
import sys
import time


HOST = "127.0.0.1"  # This script cannot be pointed at another host.
MAX_CONNECTIONS = 10
MESSAGE = b"HELLO\n"


def main():
    parser = argparse.ArgumentParser(description="Slow-client localhost learning demo")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--connections", type=int, default=1)
    parser.add_argument("--delay", type=float, default=2.0,
                        help="seconds between bytes (0.1 to 30)")
    args = parser.parse_args()

    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if not 1 <= args.connections <= MAX_CONNECTIONS:
        parser.error(f"--connections must be between 1 and {MAX_CONNECTIONS}")
    if not 0.1 <= args.delay <= 30:
        parser.error("--delay must be between 0.1 and 30 seconds")

    sockets = []
    try:
        # Open only the requested small number of loopback connections.
        for number in range(args.connections):
            sock = socket.create_connection((HOST, args.port), timeout=5)
            sockets.append(sock)
            print(f"Opened local connection {number + 1}/{args.connections}")

        print(f"Sending one byte every {args.delay:g} seconds on the first connection.")
        for byte in MESSAGE:
            sockets[0].sendall(bytes([byte]))
            print(f"Sent {bytes([byte])!r}")
            if byte != MESSAGE[-1]:
                time.sleep(args.delay)

        response = bytearray()
        while True:
            data = sockets[0].recv(1024)
            if not data:
                break
            response.extend(data)
        print("Server response:", response.decode("utf-8", errors="replace").strip())
    except (OSError, BrokenPipeError) as error:
        print(f"Connection ended: {error}")
    finally:
        if len(sockets) > 1:
            try:
                input("Other local connections are still open. Press Enter to close them. ")
            except EOFError:
                pass
        for sock in sockets:
            sock.close()


if __name__ == "__main__":
    main()