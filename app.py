from fastapi import FastAPI
from pydantic import BaseModel
from fifo_client import run_request

app = FastAPI()

# request model
class RunRequest(BaseModel):
    command: str
    text: str = ""

# health check
@app.get("/api/health")
def health():
    return {"status": "ok"}

# info endpoint
@app.get("/api/info")
def info():
    return {
        "commands": ["echo", "reverse", "count"]
    }

# main endpoint
@app.post("/api/run")
def run(req: RunRequest):
    try:
        output = run_request([req.command, req.text])
        return {
            "status": "ok",
            "output": output
        }
    except Exception as e:
        return {
            "status": "error",
            "error": str(e)
        }