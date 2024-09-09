from random import randint, seed

seed(None)

for limit in range(2,100):
  value = randint(2, limit)
  print(value)
