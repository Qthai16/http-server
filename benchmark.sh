#! /bin/bash

./wrk -c400 -d15s -t12 http://127.0.0.1:11225/
wait
./wrk -c10000 -d15s -t10 -s post.lua http://127.0.0.1:11225/form
wait
ab -c 1000 -n 50000 -k http://127.0.0.1:11225/
wait
ab -c 1000 -n 100000 -p CMakeLists.txt -k http://127.0.0.1:11225/form
