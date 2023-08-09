#! /usr/bin/python3

import os
import sys
import numbers
import json

def my_average(*args):
    return sum(args)/len(args)

def abitraryDict(**kwargs):
    for item in kwargs:
        print("{}: {}".format(item, kwargs[item]))

def decryptAES256_CFB128_cipher(encryptedText, password, iv):
    # sha256 password key and save as list of char
    # use iv as list of char
    # base64 decoded encrypted text
    pass

def isFileExist(filepath):
    if not os.path.isfile(filepath):
        # print("ERROR: ", filepath, " does not exist!")
        return False
        # exit(ERROR_INVALID_ARGUMENT)
    else: return True
        # print(filepath, "exists")

class SimpleDataClass():
    def __init__(self, atr1, atr2, atr3) -> None:
        self.atr1 = atr1
        self.atr2 = atr2
        self.atr3 = atr3

    def print(self):
        print("{} {} {}".format(self.atr1, self.atr2, self.atr3))

class NotSoSimpleClass(SimpleDataClass):
    # def __init__(self, atr1, atr2, atr3) -> None:
    #     SimpleDataClass(atr1, atr2, atr3)

    arg4 = "this belong to NotSoSimpleClass"

    def print(self):
        print("{} {} {} {}".format(self.atr1, self.atr2, self.atr3, NotSoSimpleClass.arg4))

if __name__ == "__main__":
    simpleObj = SimpleDataClass(1, "hello", {"abc":1, "def": 2})
    simpleObj.print()
    exit(0)
    filePath = sys.argv[1]
    if isFileExist(filePath):
        print(filePath, "exists")
    else: print("{} does not exist!".format(filePath))
    # newStr = "this is a string separate by space"
    tokens = "this is a string separate by space".split(' ')
    print("tokens: {t}, size: {len}".format(t=tokens, len=len(tokens)))
    print(f"tokens: {tokens}, size: {len(tokens)}")
    simpleList = ["aflsaj", "aurowieru", "12312"]
    simpleDict = {
        "abc": 1,
        "nested": {
            "abc1": "123",
            "def1": 5,
            "74": True
        },
        "ghj": [1, 2, 3, 5]
    }
    for key,value in simpleDict.items():
        print("{}: {}".format(key, value))
    simpleTuple=("af;kslfd",1, True, 2, 3, 4)
    total=0.0
    for item in simpleTuple:
        if (isinstance(item, numbers.Number)):
            total+=item
    print(total)
    print(simpleDict["nested"]["abc1"])
    simpleSet = set()
    simpleSet.add(1)
    print(simpleSet)

    print("average: {}".format(my_average(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)))
    abitraryDict(fruit="apple", tool="hammer")

    # with open("test-file.txt", 'r') as testFile:
    #     print(testFile.read())
    # for item in ([1, 2, 3], "string", 97498234):
    #     print(item)
    moreSimpleTuple = [(1,2), (3,4), (5,6), (7,8)]
    # someVar = 2
    # print("someVar is equal to 2") if someVar == 2 else pass
    for (first, second) in moreSimpleTuple:
        print(f"{first}: {second}")
    for num in range(1,5):
        print("this is amazing, time: {}".format(num))
    for ind, letter in enumerate("aflkajsdfklajsdflkds"):
        print(letter)
    # for item in zip({"key1": 1, "key2":2}, {"abc": 1, "xyz":2}):
    for item in zip([1,2,3], [4,5,6, 7]):
        print(item)
    print(123 in ["dlkajfsdl", "aflkas"])
    age = None
    while True:
        try:
            age = input("Hey, enter your age: ")
            if (isinstance(int(age), numbers.Number)):
                break
        except ValueError as err:
            print("Hey, you enter bullsh*t!!!")
    print("you are {} years old".format(age))
    dummyString3434="the quick brown fox jumps over the lazy dog"
    strList = [ letter for letter in dummyString3434 ]
    numberList = [ num for num in range(2,20,2)]
    # list comprehensions
    # for item in simpleList:
