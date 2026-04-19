import os
import sys
import time
import uuid
import fcntl
import select

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SERVER_DIR = os.path.join(BASE_DIR, "server_runtime")
REQUESTS_FIFO = os.path.join(SERVER_DIR, "REQUESTS")

def encode_args(args):
    parts = []
    for arg in args:
        parts.append(f"{len(arg)} {arg}")
    return " ".join(parts) + "\n"

def make_client_id():
    return "web_" + uuid.uuid4().hex[:8]

def run_request(args, timeout=15.0):  # was 5.0
    client_id = make_client_id()

    fifo_in  = os.path.join(SERVER_DIR, f"{client_id}.0")
    fifo_out = os.path.join(SERVER_DIR, f"{client_id}.1")

    os.mkfifo(fifo_in)
    os.mkfifo(fifo_out)

    fd_in = None
    fd_out = None
    try:
        # Preload args before notifying the daemon to avoid startup races.
        fd_in = os.open(fifo_in, os.O_RDWR | os.O_NONBLOCK)
        os.write(fd_in, encode_args(args).encode())

        fd_out = os.open(fifo_out, os.O_RDONLY | os.O_NONBLOCK)

        with open(REQUESTS_FIFO, "w") as req:
            req.write(client_id + "\n")
            req.flush()

        flags = fcntl.fcntl(fd_out, fcntl.F_GETFL)
        fcntl.fcntl(fd_out, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

        deadline = time.monotonic() + timeout
        chunks = []
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timed out waiting for worker output")
            ready, _, _ = select.select([fd_out], [], [], remaining)
            if not ready:
                raise TimeoutError("timed out waiting for worker output")
            chunk = os.read(fd_out, 4096)
            if not chunk:
                break
            chunks.append(chunk)

        return b"".join(chunks).decode()

    finally:
        if fd_in is not None:
            try:
                os.close(fd_in)
            except OSError:
                pass
        if fd_out is not None:
            try:
                os.close(fd_out)
            except OSError:
                pass
        for path in (fifo_in, fifo_out):
            try:
                os.unlink(path)
            except OSError:
                pass


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: python fifo_client.py <command> [arg]")
        sys.exit(1)

    command = sys.argv[1]
    arg = sys.argv[2] if len(sys.argv) > 2 else ""

    result = run_request([command, arg])
    print(result)