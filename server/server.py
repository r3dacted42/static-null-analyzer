import subprocess
import tempfile
import os
import shutil
from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import FileResponse, HTMLResponse, Response
from pydantic import BaseModel
from fastapi.staticfiles import StaticFiles
from starlette.background import BackgroundTask

# --- Configuration ---
# Define paths for tools, assuming they are in the system PATH
CLANG_CMD = "clang++"
JQ_CMD = "jq"
ANALYZER_CMD = "analyzer"
DOT_CMD = "dot"
FRONTEND_FILE = "./public/index.html"

app = FastAPI()

class CodeInput(BaseModel):
    code: str
    func: str

@app.get("/", response_class=HTMLResponse)
async def get_frontend():
    if not os.path.exists(FRONTEND_FILE):
        raise HTTPException(status_code=404, detail="index.html not found.")
    with open(FRONTEND_FILE, "r") as f:
        return HTMLResponse(content=f.read())

def cleanup_dir(path: str):
    if os.path.exists(path):
        shutil.rmtree(path)

@app.post("/api/analyze")
async def analyze_code(req: CodeInput):
    """
    Receives C++ code, runs the full analysis pipeline,
    and returns the resulting CFG as a PNG image.
    """
    code = req.code
    func = req.func

    temp_dir = tempfile.mkdtemp()
    cleanup = BackgroundTask(cleanup_dir, temp_dir)
    
    code_filename = os.path.join(temp_dir, "user_code.cpp")
    ast_filename = os.path.join(temp_dir, "ast.json")
    filtered_ast_filename = os.path.join(temp_dir, "filtered_ast.json")
    svg_filename = os.path.join(temp_dir, "graph.svg")

    try:
        with open(code_filename, "w") as f:
            f.write(code)

        clang_cmd = [
            CLANG_CMD, "-std=c++17", "-fsyntax-only", "-Xclang", "-ast-dump=json",
            "-Xclang", f'-ast-dump-filter={func}', code_filename
        ]
        with open(ast_filename, "w") as f_out:
            subprocess.run(
                clang_cmd, stdout=f_out, stderr=subprocess.PIPE, check=True
            )

        jq_cmd = [
            JQ_CMD,
            f'select(.kind == "FunctionDecl" and .loc.file == "{code_filename}")',
            ast_filename
        ]
        with open(filtered_ast_filename, "w") as f_out:
            subprocess.run(jq_cmd, stdout=f_out, stderr=subprocess.PIPE, check=True)

        analyzer_cmd = [ANALYZER_CMD, filtered_ast_filename]
        analyzer_result = subprocess.run(
            analyzer_cmd, capture_output=True, text=True, check=True
        )
        dot_string = analyzer_result.stdout

        dot_cmd = [DOT_CMD, "-Tsvg", "-o", svg_filename]
        subprocess.run(
            dot_cmd, input=dot_string, text=True, stderr=subprocess.PIPE, check=True
        )
        return FileResponse(
            svg_filename, 
            media_type="image/svg+xml",
            background=cleanup
        )

    except subprocess.CalledProcessError as e:
        # Handle errors from any of the commands
        raise HTTPException(status_code=500, detail={
            "error": "Analysis failed",
            "command": " ".join(e.cmd),
            "stderr": e.stderr.decode("utf-8") if e.stderr else "No stderr"
        })
    except Exception as e:
        raise HTTPException(status_code=500, detail={"error": str(e)})


app.mount("/", StaticFiles(directory="public"), name="public")
