# analyzer

- works on the Clang AST: exported to JSON and filtered for a only a particular function definition
- creates CFG by recursive descent on the AST nodes and stores pointer-related data
- runs a simple data flow analysis on the CFG to track states of pointers
- flags any potential null pointer dereferences on the output CFG (graphviz dot format) and as a JSON of issues

## output format

- `graph.dot`: CFG in graphviz DOT format
- `issues.json`:
  ```json
  [ // list of issues
    {
      "start_offset": number,
      "end_offset": number,
      "message": string // contains var name
    }
  ]
  ```

## how to run

for analyzing a function named `func` defined in file "input.cpp"

```bash
# export clang AST as JSON
clang -fsyntax-only -Xclang -ast-dump=json -Xclang -ast-dump-filter=func input.cpp > ast.json

# secondary filtering (for files with similar function names)
jq '.inner[] | select(.kind == "FunctionDecl" and .name == "func")' ast.json > filt_ast.json

# build analyzer
make

# run analyzer: input_json output_dot output_issue_json
./analyzer filt_ast.json graph.dot issues.json
```
