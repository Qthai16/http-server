#!/bin/bash

if [[ $# -eq 0 || -z $1 ]];then
    printf "Read from standard input: "
    # ignore option -r
    read message
elif [ ! -z $1 ];then
    message=$(cat $1)
else
    echo "Usage: echo -n <your encrypted message> | ./decrypted.sh\
    OR ./decrypted.sh <ecrypted_file>"
fi

password_key="this_is_not_a_password"
iv="hello"
hash_key=$(echo -n ${password_key} | sha256sum | sed 's/\s*-//g')
hex_iv=$(echo -n ${iv} | xxd -p)
# $(echo -n "this_is_not_a_password" | openssl dgst -sha256 -hex | sed 's/^.* //')

echo -n "${message}" | base64 -d | openssl enc -d -aes-256-cfb -K ${hash_key} -iv ${hex_iv} -out "decrypted-$(date +%s).json"