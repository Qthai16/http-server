#! /bin/bash

alias xclip-cp="xclip -selection c"
# alias count-files='find . -type f 2>/dev/null | wc -l'
alias robo3t='/home/thai/Downloads/robo3T/robo3t-1.4.4-linux-x86_64-e6ac9ec/bin/robo3t'
alias largest-files='sudo find . -type f -exec du -ah {} + | grep -v "/$" | sort -rh | head -20'
alias postman="/home/thai/Downloads/postman/Postman/Postman"

squash-mount() {
	sudo mount "$1" "/mnt/squash-mount" -t squashfs -o loop
}

count-file() { 
	sh -c "find \"$1\" -type f 2>/dev/null | wc -l"
}

squash-umount() {
	sudo umount "/home/thai/Desktop/compressDB"
}

tar-gzip() {
	# $1: tar.gz file name
	# $2: path
	tar czf "$1" "$2"
}

tar-unzip() {
	# $1: tar.gz file name
	# $2: extracted path
	tar xvf "$1" -C $2
}

function split_colon_string() {
    text=$1
    local -n array=$2

    while IFS=':' read -ra items; do
        for (( i = 0; i < ${#items[@]}; ++i )); do
            item="${items[i]}"
            while [[ "${item: -1}" == '\'  ]]; do # Last character is '\'
                item="${item::-1}:" # Replace '\' by ':'
                let ++i
                if [[ $i < ${#items[@]} ]]; then
                    item+="${items[i]}" # Concate next string
                fi
            done
            array["${#array[@]}"]="$item" # Push back to result list
        done
    done <<< "$text"
}
