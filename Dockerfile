# Web playground for hosting (Render). Builds the compiler for Linux, then runs server.py.
FROM python:3.12-slim
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev && rm -rf /var/lib/apt/lists/*
ENV PYTHONUNBUFFERED=1
WORKDIR /app
COPY 202307020045_CompilerDesignMiniProject/ .
# kept as minicc.exe (a valid Linux file name) so server.py finds it unchanged
RUN gcc -std=c99 -O2 -o minicc.exe src/*.c
CMD ["python", "server.py"]
