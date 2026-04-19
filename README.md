# Daemon IPC API

A C-based daemon that executes commands using FIFO-based interprocess communication, exposed through a FastAPI web API.

## 🚀 Live Demo

👉 https://daemon-ipc-api.onrender.com

The deployed version runs the same system architecture, with the daemon spawning worker processes and managing communication through named pipes.

To use:
in command type: echo, reverse or count
in input: type anything
    - hit run
---

## 🧠 Overview

This project implements a multi-layer system:

- **C daemon**
  - Long running process (server-like) that handles client requests by
    - Spawning worker processes and
    - Communicating w/ them via named pipes (FIFOs)

- **Worker program**
  - Executes commands (`echo`, `reverse`, `count`)
  - Streams results back through IPC

- **Python client**
  - Manages FIFO creation and communication
  - Bridges API requests to the daemon

- **FastAPI service**
  - Exposes HTTP endpoints
  - Starts and manages the daemon
  - Provides a web-accessible interface

---


## Running the Full System Locally

### Prerequisites

- macOS or Linux
- Python 3
- GCC (or compatible C compiler)

---

### 1. Clone the repository

git clone https://github.com/Mason9013/daemon-IPC-api.git
now open cloned repository

in terminal (in root of project):
2. run: pip install -r requirements.txt
3. run: make
4. run: ./scripts/start.sh
    - you should see: Starting daemon...
    - note: terminal will hang; daemon is running in background
    - you will need to open a new terminal (and go to project   folder) for step 5

5. start API server, run: python -m uvicorn app:app --reload

6. open API in your browser with this link: http://127.0.0.1:8000/docs

7. test command: "command" available commands: reverse, count, echo. text: "string" can be any letters
    - go to: POST and hit Try it out, output will be long but will contain the count, reverse or echo'd input

8. stop the daemon !IMPORTANT!. run: ./scripts/stop.sh in project root directory or in terminal where you launched the daemon (after hitting ctrl+c)

- note opening: https://daemon-ipc-api.onrender.com/docs will expose the API therefore opening the same site as running locally