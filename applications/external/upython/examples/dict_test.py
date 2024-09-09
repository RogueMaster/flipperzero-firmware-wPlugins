d = dict()

for i in range(1,100):
  d[str(i)] = i

for k,v in d.items():
  val = '{0} = {1}'.format(k, v)

  print(val)
