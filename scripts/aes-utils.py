#! /usr/bin/python3

import sys
import base64
import hashlib
import os
import json
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

def path_exist(file_path):
    return os.path.isfile(file_path) or os.path.isdir(file_path)

def sha256_hash(data):
    # Create a new SHA-256 hash object
    sha256 = hashlib.sha256()
    # Convert the input data to bytes if it's not already
    if isinstance(data, str):
        data = data.encode()
    # Update the hash object with the data
    sha256.update(data)
    # Get the hexadecimal representation of the hash
    # hash_result = sha256.hexdigest()
    return sha256.digest

def decrypt_aes256_cfb128(encrypted_text, password, iv_text):
    hashObject = hashlib.sha256(password.encode())
    hashKey = hashObject.digest()
    ivBin = iv_text.encode()
    encryptedChars = base64.decodebytes(encrypted_text.encode())

    cipher = Cipher(algorithms.AES(hashKey), modes.CFB(ivBin), backend=default_backend())

    # Create a decryptor object
    decryptor = cipher.decryptor()

    # Decrypt the cipher text
    decrypted_data = decryptor.update(encryptedChars) + decryptor.finalize()

    return decrypted_data

def encrypt_aes_256_cfb128(text, password, iv):
    hashObject = hashlib.sha256(password.encode())
    hashKey = hashObject.digest()
    ivBin = iv.encode()
    cipher = Cipher(algorithms.AES(hashKey), modes.CFB(ivBin), backend=default_backend())
    encryptor = cipher.encryptor()
    encrypted_data = encryptor.update(text.encode()) + encryptor.finalize()
    return base64.encodebytes(encrypted_data)


if __name__ == "__main__":
    # password=""
    # iv=""
    # base64_encrypted_text = encrypt_aes_256_cfb128("this is a very long text", password, iv)
    # print(base64_encrypted_text)
    # exit(0)
    if len(sys.argv) < 2:
        print("Usage: python3 aes-utils.py <encrypted_file/plain_file>")
        exit(1)
    else:
        fileName = sys.argv[1]
        if not os.path.isfile(fileName):
            print("Invalid file")
            exit(1)
        if len(sys.argv) == 2:
            password = input("Input password: ")
            iv = input("Input init vector: ")
        elif len(sys.argv) == 4:
            password=sys.argv[2]
            iv=sys.argv[3]
        else:
            print("Usage: python3 aes-utils.py <encrypted_file>")
            exit(1)
    cipher=""
    with open(fileName, 'r', encoding='utf8') as cipherData:
        cipher=cipherData.read()
    decryptedText = decrypt_aes256_cfb128(encrypted_text = cipher, password=password, iv_text=iv)
    try:
        json_obj = json.loads(decryptedText)
        with open("decrypted_data.json", 'w') as outFile:
            outFile.write(json.dumps(json_obj, indent=4))
    except json.JSONDecoderError as e:
        with open("decrypted_data.txt", 'w') as outFile:
            outFile.write(json.dumps(decryptedText, indent=4))
    