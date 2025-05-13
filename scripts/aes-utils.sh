#!/bin/bash

function AES256_CFB128_encrypt() {
    raw_text=${1}
    hash_key=$(echo -n ${2} | sha256sum | sed 's/\s*-//g')
    hex_iv=$(echo -n ${3} | xxd -p)
    echo -n "${raw_text}" | openssl enc -aes-256-cfb -K ${hash_key} -iv ${hex_iv} | base64 > cipher-$(date +%s).txt
}

function AES256_CFB128_decrypt() {
    encrypted_text=${1}
    hash_key=$(echo -n ${2} | sha256sum | sed 's/\s*-//g')
    hex_iv=$(echo -n ${3} | xxd -p)
    echo -n "${encrypted_text}" | base64 -d | openssl enc -d -aes-256-cfb -K ${hash_key} -iv ${hex_iv} -out "decrypted-$(date +%s).json"
}

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
iv="helloHELLOhello1"

options=("encryption" "decryption")
select opt in "${options[@]}"
do
    case $opt in
        "encryption")
            AES256_CFB128_encrypt "${message}" "${password_key}" "${iv}"
            break;;
        "decryption")
            AES256_CFB128_decrypt "${message}" "${password_key}" "${iv}"
            break;;
        *) echo "Invalid option $REPLY"
            break;;
    esac
done

# -nopad
encrypt:
    echo -n "this is a top secret message" | openssl enc -nosalt -aes-256-cbc -e -K $(echo -n "my_encrypted_key" | sha256sum | cut -c1-64) -iv '5c1ed556267339a52e0f49a30f1ed082' -base64
decrypt:
    echo -n "<base64_encrypted>" | base64 --decode | openssl enc -nosalt -aes-256-cbc -d -K $(echo -n "my_encrypted_key" | sha256sum | cut -c1-64) -iv '5c1ed556267339a52e0f49a30f1ed082'