import socket
import sys


HOST = "127.0.0.1"  # Keep this lab client on the local machine.
DEFAULT_PORT = 8080


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_PORT

    print(f"Connecting to {HOST}:{port}. Enter HELLO, TIME, or QUIT.")
    while True:
        try:
            command = input("> ").strip().upper()
        except EOFError:
            break

        if not command:
            continue

        try:
            # Each command uses a new connection because the server closes after one request.
            with socket.create_connection((HOST, port), timeout=5) as sock:
                sock.sendall((command + "\n").encode("utf-8"))

                response = bytearray()
                while True:
                    data = sock.recv(1024)
                    if not data:
                        break
                    response.extend(data)

            print(response.decode("utf-8", errors="replace").strip())
        except OSError as error:
            print(f"Connection error: {error}")
            break

        if command == "QUIT":
            break


if __name__ == "__main__":
    main()