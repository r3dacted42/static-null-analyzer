import subprocess
import tempfile
import os
import shutil
from fastapi import FastAPI, Request, HTTPException, BackgroundTasks
from fastapi.responses import FileResponse, HTMLResponse, Response
from pydantic import BaseModel
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
import json

CLANG_CMD = "clang++"
JQ_CMD = "jq"
ANALYZER_CMD = "analyzer"

app = FastAPI()

origins = [
    "http://127.0.0.1:5173",
    "http://127.0.0.1:8000",
    "http://127.0.0.1:8180",
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class CodeInput(BaseModel):
    code: str
    func: str

class AnalysisResult(BaseModel):
    dot: str
    issues: list

def cleanup_dir(path: str):
    if os.path.exists(path):
        shutil.rmtree(path)

@app.post("/api/analyze")
async def analyze_code(req: CodeInput, tasks: BackgroundTasks):
    code = req.code
    func = req.func

    temp_dir = tempfile.mkdtemp()
    tasks.add_task(cleanup_dir, temp_dir)
    
    code_filename = os.path.join(temp_dir, "user_code.cpp")
    ast_filename = os.path.join(temp_dir, "ast.json")
    filtered_ast_filename = os.path.join(temp_dir, "filtered_ast.json")
    dot_filename = os.path.join(temp_dir, "cfg.dot")
    issues_filename = os.path.join(temp_dir, "issues.json")

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

        analyzer_cmd = [ANALYZER_CMD, filtered_ast_filename, dot_filename, issues_filename]
        analyzer_result = subprocess.run(
            analyzer_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
        )

        res = {}
        with open(issues_filename, "r") as f:
            res["issues"] = json.load(f)
        with open(dot_filename, "r") as f:
            res["dot"] = f.read()
        return res

    except subprocess.CalledProcessError as e:
        raise HTTPException(status_code=500, detail={
            "error": "analysis failed",
            "stderr": e.stderr.decode("utf-8") if e.stderr else "no stderr"
        })
    except Exception as e:
        raise HTTPException(status_code=500, detail={"error": str(e)})


app.mount("/", StaticFiles(directory="public", html=True), name="public")
