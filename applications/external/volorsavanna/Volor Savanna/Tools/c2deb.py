# https://itsfoss.gitlab.io/post/how-to-create-a-simple-deb-debian-package/
import os

os.system("rm -rf VolorSavanna")
os.system("rm -rf volorsavanna")
os.system("git clone https://github.com/Invizabel/VolorSavanna")
os.system("gcc 'VolorSavanna/Volor Savanna/C/VolorSavanna.c' -o volorsavanna")
os.system("gcc -fPIC -shared -o libvolorsavanna.so 'VolorSavanna/Volor Savanna/C/VolorSavanna.c'")
os.system("chmod +x volorsavanna")
os.system("mkdir -p volor-savanna/DEBIAN")
os.system("mkdir -p volor-savanna/usr/local/bin")
os.system("mkdir -p volor-savanna/usr/local/lib")

with open("volor-savanna/DEBIAN/control", "w") as file:
    file.write("""Package: volorsavanna
Version: 1.0.0
Architecture: all
Maintainer: Michael Mueller
Description: Volor Savanna is a text adventure game!
""")

with open("volor-savanna/debian-binary", "w") as file:
    file.write("2.0")

os.system("cp volorsavanna volor-savanna/usr/local/bin/volorsavanna")
os.system("cp libvolorsavanna.so volor-savanna/usr/local/lib/libvolorsavanna.so")
os.system("dpkg-deb --build volor-savanna")
