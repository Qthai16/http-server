#! /bin/bash

curl -v --data-binary "@wrk" -H 'Content-Disposition: attachment; filename="wrk"' http://localhost:11225/form
curl -v -T reports.zip -H 'filename: reports.zip' http://localhost:11225/file

./wrk -c400 -d5s -t12 http://localhost:11225/
wait
./wrk -c400 -d5s -t12 -s post.lua http://localhost:11225/form
wait
# ab -c 1000 -n 50000 -k http://localhost:11225/
# wait
# ab -c 1000 -n 100000 -p CMakeLists.txt -k http://localhost:11225/form
