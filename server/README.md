# server

- serves the static frontend and provides endpoint for using the analyzer pipeline

## endpoints

- GET `/` - frontend

- POST `/api/analyze`  
  expects:  
  ```json
  { 
    "code": string, // c++ program containing function definition 
    "func": string  // name of function to analyze
  }
  ```
  returns:  
  ```json
  {
    "dot": string, // DOT representation of CFG
    "issues": array // array of issues flagged by the analyzer
  }
  ```

## how to run

```bash
# make a venv and activate it
python3 -m venv .venv
source ./.venv/bin/activate

# install requirements
pip install -r requirements.txt

# build the analyzer
cd ../analyzer 
make 
cd ../server

# make sure the analyzer is in PATH
export PATH=path/to/static-null-analyzer/analyzer:$PATH

# run dev server
fastapi dev server.py
```
