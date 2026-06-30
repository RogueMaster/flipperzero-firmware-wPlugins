# flipperscope

To compile and install

```
cd flipperzero-firmware/applications_user
git clone https://github.com/anfractuosity/flipperscope.git
cd ..
./fbt DEBUG=1 VERBOSE=1 fap_scope
./fbt launch_app APPSRC=applications_user/flipperscope
```

Alternatively the binary can now be installed from https://lab.flipper.net/apps/flipperscope or the Flipper Mobile App.

Provide signal to **pin 16/PC0**, with a voltage ranging from 0V to 2.5V and ground to **pin 18/GND**.

Press the 'ok' button (button in the centre of joypad) to pause/unpause the waveform display.

[Demo](https://www.youtube.com/watch?v=tu2X1WwADF4) showing three different waveform types from a signal generator.

Also see [Derek Jamison's demonstration](https://www.youtube.com/watch?v=iC5fBGwCPHw&t=374s) of this app as well as other interesting projects.

## Measurements

* Measures frequency of waveform in hertz
* Measures voltage: min, max, Vpp
* FFT option provides simple spectrum analyser

![Signal Generator](photos/sig.jpg)

![Flipper Zero running flipperscope](photos/freq.jpg)

![Rigol](photos/rigol.jpg)

![Flipper Zero running flipperscope - showing voltage measurements](photos/volt.jpg)

In the following photo attached a MAX9814 electret microphone module to the flipper zero and used the spectrum
analyser functionality, with an FFT window size of 1024 and played a 3kHz sine wave tone from a computer.

![Flipper Zero running flipperscope - showing spectrum analyser](photos/fft.jpg)

## Processing captures

You can use the following simple Python script, for processing the captured waveforms, from flipperscope.

```
import matplotlib.pyplot as plt
import numpy as np
import struct

fig, ax = plt.subplots()
data = open("Sine.dat", "rb").read()
y = [(float(x[0]) / 2500) * 2.5 for x in struct.iter_unpack("<H", data)]
x = np.arange(len(y))

ax.plot(x, y, label='Voltage')
ax.set_xlabel('Sample')
ax.set_ylabel('Voltage / V')
ax.set_title('ADC Voltage')
ax.set_ylim([0, 2.5])
ax.legend()
plt.show()
```

![Captured waveform](photos/sine.png)

## To Do

* Customisable input pin
* Trigger type mode

## Inspiration

* For GUI menu system, used code principles from weather station and signal generator apps
* STM32 DMA example
* VREFBUF information - https://community.st.com/s/question/0D53W00001awIlMSAU/enable-and-use-vrefbuf-for-adc-measures
* Relocating vector table - https://community.nxp.com/t5/i-MX-Processors/Relocate-vector-table-to-ITCM/m-p/1302304
* Uses FFT algorithm from - https://www.algorithm-archive.org/contents/cooley_tukey/cooley_tukey.html
