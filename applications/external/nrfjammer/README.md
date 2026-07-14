# For educational purposes only, inside a confined environment (e.g. faraday cage). JAMMING IS ILLEGAL!

# Flipper Zero nRF24 Jammer
Simple Flipper Zero nRF24 jammer for the 2.4Ghz spectrum I use to study wireless protocol hardening. Turn your Flipper Zero into a bluetooth or wifi jammer. Works with the [latest Unleashed firmware](https://github.com/Eng1n33r/flipperzero-firmware). See the release on how to install the external app on the sdcard. For building instructions please refer the [official FAP guide](https://github.com/Eng1n33r/flipperzero-firmware/blob/dev/documentation/AppsOnSDCard.md). Don't forget to enable 5V on GPIO!

For custom channels navigate to custom by pressing UP and then LEFT to select a .txt file in format ch1,ch2,ch3 and so on. 
Example:
1,2,3,4,5,6 will emit on channels 1 to 6

Demo:
https://bsky.app/profile/hookgab.bsky.social/post/3lcgnsx7wfk2x

Example of the spectrum at 2.440 Ghz:
![spectrum](img/spectrum.png)

# FAQ
It's crashing, what do I do?
* It's a known issue, stop bashing buttons randomly while the nRF is engaged. For anything else file a ticket with PRECISE steps to reproduce
  
It's not working very well.
* Set Log Level to None and disable any debugging features as they eat precious CPU cycles.
  
It's not doing anything!!11!1
* Not all devices are created equal. Newer Bluetooth versions have more robust channel hopping algorithms or noise filtering so this is less effective. Since for my use case (studying RF interference) having the device centimeters away from the target is more than enough, I do not plan to support anything else that might lead people into legal trouble.

# JAMMING IS ILLEGAL. ONLY USE THIS FOR EDUCATIONAL PURPOSES INSIDE A WELL SHIELDED ENVIRONMENT.
