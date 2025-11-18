
FROM gcc:latest AS builder
RUN apt-get update && apt-get install -y make
WORKDIR /app
COPY analyzer/Makefile ./
COPY analyzer/src ./src
RUN make all

FROM python:3.10-slim
RUN apt-get update && apt-get install -y clang jq graphviz \
    --no-install-recommends && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY server/requirements.txt .
RUN pip install -r requirements.txt

COPY --from=builder /app/analyzer /usr/local/bin/analyzer
COPY server/server.py .
COPY server/public ./public

EXPOSE 8080
CMD ["uvicorn", "server:app", "--host", "0.0.0.0", "--port", "8080"]