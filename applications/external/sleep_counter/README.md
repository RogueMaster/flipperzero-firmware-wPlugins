# Sleep Counter Flipper App
This is a flipper app that counts how much you move in your sleep.
Shoutout to MatthewKuKanich https://github.com/MatthewKuKanich/flipper-radar he is the one who made the original app for this Radar module.
I took his code and just heavily modified it to be a sleep counter so that I can set this up on my flipper, place it near my bed, and when I go to sleep have it count how much I move around.
The fap application is in the dist folder.


### For the 3D-Print Files
Tolerance is super tight, but the chip fits fairly perfectly in the holder for me.
The RCWL-0516 holder fits perfectly into the radar Dish, although it is a bit of a friction fit.
If you have problems, I'd recommend sizing the holder up by 1% and the Radar Dish up by 2% in your slicer.
Additionally, after printing this out, I covered the radar dish in foil up to around halfway, and it allowed me to properly make this directional. While still receiving a sensitive enough range.



### Some Quick Notes
When you plug in the RCWA module, it will plug into the first 3 GPIO pins.(5v, A7, A6)
Part of why I use this module is that it lets you know the movement through only the red LED on the flipper, making this quiet and incognito
The foil wrapping is difficult to do properly, and I have not found a way around this, so let me know if you do!
