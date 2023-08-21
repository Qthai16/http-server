#! /bin/bash

# 1-Install doxygen
sudo apt-get install doxygen
 
# 2-Install drawing tool
sudo apt-get install graphviz
 
# 3-cd to source directory
cd client_scan_process
 
# 4-Gen doxygen config
# note: client_scan_process is an example, you can change it to your service/module
doxygen -g client_scan_process.cfg

# 5- run doxygen
doxygen client_scan_process.cfg
# 6-open document
firefox html/index.html &
