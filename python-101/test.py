import base64
import sys

def my_base64_decode(base64Str):
    return base64.b64decode(base64Str)

if __name__=="__main__":
    base64Str = sys.argv[1]
    print("decoded:", my_base64_decode(base64Str))