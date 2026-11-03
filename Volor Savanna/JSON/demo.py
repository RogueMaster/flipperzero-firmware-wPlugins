import json

with open("VolorSavanna.json", "r") as file:
    game = json.loads(file.read())

user_name = input(game["x"]["user_name"].replace("\\n", "\n"))

temp = "character"
choice = input(game["x"][temp].replace("\\n", "\n"))
for key, value in game["y"].items():
    for i in value:
        if isinstance(i, list):
            if temp == i[0] and int(choice) == int(i[1]):
                temp = key.replace("run", "1")

while True:
    choice = input(game["x"][temp].replace("\\n", "\n").replace("\"", "").replace(" + ", "").replace("user_name", user_name))
    for key, value in game["y"].items():
        if isinstance(value[0], str):
            if temp == value[0] and int(choice) == int(value[1]):
                temp = key.replace("run", "1")
                break