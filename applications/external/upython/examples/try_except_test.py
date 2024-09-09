try:
  raise Exception('something went wrong')
except Exception as e:
  print(e)