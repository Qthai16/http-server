#! /bin/bash

InFile=$1

if [[ $# -eq 0 || -z ${InFile} ]];then
	echo "Usage: <script_name> input_file"	
fi

openssl enc -aes-256-cfb -K $(echo -n "this_is_not_a_password" | openssl dgst -sha256 -hex | sed 's/^.* //') -iv $(echo -n "hello" | xxd -p) -in ${InFile} | base64 -w 0 > cipher-text.txt
