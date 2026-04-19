import os
import subprocess
from contextlib import asynccontextmanager
from fastapi import FastAPI
from pydantic import BaseModel
from fastapi.responses import FileResponse

from fifo_client import run_request

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
RUNTIME_DIR = os.path.join(BASE_DIR, "server_runtime")
DAEMON_PATH = os.path.join(BASE_DIR, "daemon")
WORKER_PATH = os.path.join(BASE_DIR, "demo_worker")
PID_FILE = os.path.join(RUNTIME_DIR, "daemon.pid")

daemon_process = None

@asynccontextmanager
async def lifespan(app: FastAPI):
    global daemon_process

    os.makedirs(RUNTIME_DIR, exist_ok=True)

    # clean stale runtime bits
    for name in os.listdir(RUNTIME_DIR):
        if name == "REQUESTS" or name.endswith(".0") or name.endswith(".1") or name == "daemon.pid":
            try:
                os.unlink(os.path.join(RUNTIME_DIR, name))
            except OSError:
                pass

    daemon_process = subprocess.Popen(
        [DAEMON_PATH, RUNTIME_DIR, WORKER_PATH],
        cwd=BASE_DIR,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )

    yield

    if daemon_process is not None:
        daemon_process.terminate()
        try:
            daemon_process.wait(timeout=5)
        except Exception:
            daemon_process.kill()

app = FastAPI(lifespan=lifespan)

class RunRequest(BaseModel):
    command: str
    text: str = ""

@app.get("/api/health")
def health():
    return {
        "status": "ok",
        "daemon_pid_file_exists": os.path.exists(PID_FILE),
        "requests_fifo_exists": os.path.exists(os.path.join(RUNTIME_DIR, "REQUESTS")),
    }

@app.get("/")
def index():
    return FileResponse(os.path.join(BASE_DIR, "static/index.html"))

@app.post("/api/run")
def run(req: RunRequest):
    output = run_request([req.command, req.text])
    return {"status": "ok", "output": output}