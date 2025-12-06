# static null analyzer

program analysis 2025 winter final project  

uses a simple data flow analysis algorithm to find potential null pointer dereferences in a subset of c++ code  

algorithm reference: https://www.cs.cmu.edu/~aldrich/courses/17-355-17sp/notes/03-dataflow-frameworks.pdf  
deployment (gcloud run): https://static-null-analyzer-50533719506.asia-south2.run.app  
docker image tag: `docker.io/r3dacted42/static-null-analyzer:latest`

for more information about individual components, check out their READMEs

<img src="https://github.com/r3dacted42/static-null-analyzer/blob/master/screenshot.png?raw=true">

## containerization

- analyzer (C++ app) 
  - base image: `gcc:latest`
  - install deps
  - build analyzer binary
- client (Vue SPA) 
  - base image: `node:lts`
  - install deps
  - copy api url to ENV
  - build static frontend `dist` directory
- server (FastAPI app)
  - base image: `python:3.10-slim`
  - install deps
  - copy `analyzer` to path
  - copy sever script
  - copy frontend `dist` as `public`
- expose port `8080`
- run `uvicorn`
