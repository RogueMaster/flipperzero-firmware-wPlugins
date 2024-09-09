class Greeter:
  def __init__(self, name: str):
    self._name = name

  def __str__(self):
    return 'hello {name}!'.format(name=self._name)

  def __repr__(self):
    return self.__str__()

world_greeter = Greeter('world')

print(world_greeter)
