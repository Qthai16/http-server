#! /bin/bash

./wrk -t12 -c400 -d30s http://127.0.0.1:11225/
wait
ab -c 100 -n 50000 -k http://127.0.0.1:11225/

