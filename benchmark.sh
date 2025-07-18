#! /bin/bash
# curl -v --data-binary "@wrk" -H 'Content-Disposition: attachment; filename="wrk"' http://localhost:11225/file

./wrk -c100 -d5s -t1 http://localhost:11225/
wait
./wrk -c2000 -d5s -t8 http://localhost:11225/
wait
./wrk -c5000 -d5s -t8 -s post.lua http://localhost:11225/file
wait
