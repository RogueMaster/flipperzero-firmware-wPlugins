Use flipper as Openbench Logic Sniffer (ols) logic analyzer in PulseView

Install PulseView - https://www.sigrok.org/wiki/Downloads
Launch PulseView and connect to channels C0, C1, C3, B2, B3, A4, A6, A7  
Try to start pulseview from the terminal
if you encounter problems with the configuration pulseview via gui

pulseview -d ols:conn=/dev/ttyACM1

original readme:
Then start PulseView and add a new "Openbench Logic Sniffer (ols)" and select the second flipper serial port.

When arming, you can now look at the trace in PulseView.

Changes:
- all 8 channels supported Channel 0 is C0, Channel 1 is C1, etc Channel 7 is A7
- fixed sampling rate not supported (yet?)
- if a trigger level is defined, no matter which one, the signals are captured as soon this signal changes
- maximum capture rate unclear. didnt make any tests. guess in the 100kHz range
- sample count capped to 16384 for now. didnt check what is possible using malloc()
- only ONE SHOT currently supported. unknown reason. you have to close and reopen the capture window in PulseView (probably bug in PulseView?)

Source: https://github.com/g3gg0/flipper-logic_analyzer