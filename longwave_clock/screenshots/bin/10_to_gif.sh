#!/bin/bash

res="$(cd "$(dirname $0)"; pwd;)/../res"
t=$(mktemp -d);
mkdir $t/1 $t/2 $t/3 $t/4
echo "Working in $t";

n=0;
for i in $(ls *.png | sort); do 
  convert $i -alpha remove +repage -crop 512x256+1120+626 +repage $t/1/$i;
  convert $t/1/$i -resize 250x125 $t/2/$i;
  composite -geometry +195+420 $t/2/$i $res/gpio_simple.png $t/3/$i
  composite -geometry +415+0 $res/wave_$((n%3+1)).png $t/3/$i $t/4/$i
  n=$(($n+1));
done;

convert -delay 100 -loop 0 -layers Optimize $t/4/*.png animation.gif
# Open in GIMP
# 1. Image>Mode>Indexed>Generate optimum palette
# 2. Filters>Animation>Optimize (for GIF)
# 3. File>Export As...
