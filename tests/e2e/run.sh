#!/bin/bash
# usage: run.sh <build-dir> <port>
B=$1; P=${2:-9721}; D=/tmp/ciallo-e2e-$P
export WINEDEBUG=-all
rm -rf $D; mkdir -p $D
wine $B/fanlx_app.exe --data-dir "Z:$D" --init-demo >/dev/null 2>&1
wine $B/fanlx_app.exe --data-dir "Z:$D" --im-server --im-host 127.0.0.1 --im-port $P --im-sqlite > $D/server.log 2>&1 &
SP=$!
for i in $(seq 1 60); do (echo > /dev/tcp/127.0.0.1/$P) 2>/dev/null && break; sleep 0.5; done
cd /workspace/e2e-ciallo && python3 -u scenarios.py $P; R=$?
kill $SP; exit $R
