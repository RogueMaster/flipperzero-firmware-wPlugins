## Volor Savanna is a text adventure game

#### COMPLETE:
* C
* Flipper Zero
* Python
* Web

#### TODO:
* Go
* TI 84

#### To play:
* Python (Windows): pip install VolorSavanna && python3 -m VolorSavanna
* Python (Linux): python3 -m venv env && source env/bin/activate && pip install VolorSavanna && python3 -m VolorSavanna

#### To compile to DLL:
* gcc -fPIC -shared -o VolorSavanna.so VolorSavanna.c

#### To use DLL (Python) (Non-Debian Linux):

```
import ctypes
import os
current_directory = os.getcwd()
game = ctypes.CDLL(f"{current_directory}/VolorSavanna.so")
print(game.main())
```

#### To use DLL (Python) (Debian Linux):
* git clone https://github.com/Invizabel/VolorSavanna && cd VolorSavanna/Volor\ Savanna/Tools && python3 c2deb.py && sudo dpkg -i volor-savanna.deb
* Then:
```
import ctypes
import os
from ctypes.util import find_library

lib = find_library("volorsavanna")
game = ctypes.CDLL(lib)
print(game.main())
```

* To compile DLL to debian package (in the Tools folder): python3 c2deb.py
