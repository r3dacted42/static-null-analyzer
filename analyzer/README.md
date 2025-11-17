```bash
cmake -B build -S .
cmake --build build
```

```bash
clang -fsyntax-only -Xclang -ast-list source.cpp
```

```bash
clang -fsyntax-only -Xclang -ast-dump=json -Xclang -ast-dump-filter=function source.cpp
```

```bash
clang -fsyntax-only -Xclang -ast-dump=json -Xclang -ast-dump-filter=check tests/00.cpp > tests/00.json
```

```bash
jq '.inner[] | select(.kind == "FunctionDecl" and .name == "func")' ast.json > filtered_ast.json
```
