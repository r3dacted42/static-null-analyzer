import subprocess
import tempfile
import os
from flask import Flask, request, jsonify, send_file, abort

app = Flask(__name__)

@app.route("/")
def hello():
    return "C++ Analyzer Server is running!"

@app.route("/analyze", methods=["POST"])
def analyze_code():
    if not request.json or "code" not in request.json:
        abort(400, description="Request must be JSON with a 'code' key.")

    code = request.json["code"]
    try:
        # 1. Create a temp file for the user's C++ code
        with tempfile.NamedTemporaryFile(suffix=".cpp", delete=False) as code_file:
            code_file.write(code.encode("utf-8"))
            code_filename = code_file.name
        
        # 2. Create temp files for the outputs
        ast_file = tempfile.NamedTemporaryFile(suffix=".json", delete=False)
        filtered_ast_file = tempfile.NamedTemporaryFile(suffix=".json", delete=False)
        dot_file = tempfile.NamedTemporaryFile(suffix=".dot", delete=False)
        png_file = tempfile.NamedTemporaryFile(suffix=".png", delete=False)

        # 3. Run Clang to generate AST JSON
        # Note: We must use clang++ for C++ code
        clang_cmd = [
            "clang++", "-std=c++17", "-Xclang", "-ast-dump=json",
            code_filename
        ]
        with open(ast_file.name, "w") as f_out:
            subprocess.run(clang_cmd, stdout=f_out, stderr=subprocess.PIPE, check=True)

        # 4. Run jq to filter the AST (as we discussed)
        # This selects only FunctionDecls from the user's file
        jq_cmd = [
            "jq",
            f'.inner[] | select(.kind == "FunctionDecl" and .loc.file == "{code_filename}")',
            ast_file.name
        ]
        with open(filtered_ast_file.name, "w") as f_out:
            subprocess.run(jq_cmd, stdout=f_out, stderr=subprocess.PIPE, check=True)

        # 5. Run your C++ analyzer on the filtered JSON
        # Your analyzer prints the DOT string to stdout
        analyzer_cmd = ["analyzer", filtered_ast_file.name]
        analyzer_result = subprocess.run(
            analyzer_cmd, capture_output=True, text=True, check=True
        )
        dot_string = analyzer_result.stdout

        # 6. Save the DOT string to its temp file
        with open(dot_file.name, "w") as f:
            f.write(dot_string)

        # 7. Run Graphviz 'dot' to create the PNG
        dot_cmd = ["dot", "-Tpng", dot_file.name, "-o", png_file.name]
        subprocess.run(dot_cmd, stderr=subprocess.PIPE, check=True)

        # 8. Send the PNG image file back to the client
        return send_file(png_file.name, mimetype="image/png")

    except subprocess.CalledProcessError as e:
        # Handle errors from any of the commands
        return jsonify({
            "error": "Analysis failed",
            "command": " ".join(e.cmd),
            "stderr": e.stderr.decode("utf-8") if e.stderr else "No stderr"
        }), 500
    except Exception as e:
        return jsonify({"error": str(e)}), 500
    finally:
        # Clean up all temp files
        for f in [code_file, ast_file, filtered_ast_file, dot_file, png_file]:
            if os.path.exists(f.name):
                os.remove(f.name)

if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=8080)