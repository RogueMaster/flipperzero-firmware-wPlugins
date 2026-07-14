# Why?
Has ever happened to you when you are setting up your new smartphone and you are asked for the wifi password to feel the extreme urge to start cursing every known divinity? 

Clippy solves this problem by turning your Flipper Zero into a cross-device shared clipboard.

# How does it work?
Pretty simple, Clippy has 2 modes:
- copy
  - turns Flipper Zero into a mass storage device
  - you'll find a file named clippy.txt at the root of the filesystem
  - you can store a list of strings there along with a description
- paste
  - turns Flipper Zero into a usb/bt keyboard
  - you can choose from clipboard items built from the list of strings gathered from clippy.txt
  - selecting one paste the string to the connected device

# Installation

Download the [latest release](https://github.com/mtormento/clippy/releases/latest)
of the *fap* file and put it into the `apps` folder on your SD card of your Flipper Zero. 

# How do I build this thing?
```shell
cd applications_user
git clone https://github.com/mtormento/clippy.git

cd ..

# Build the application
./fbt fap_clippy

# Build and launch the application
./fbt launch APPSRC=clippy
```
