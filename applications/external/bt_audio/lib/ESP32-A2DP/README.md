# A Simple Arduino Bluetooth Music Receiver and Sender for the ESP32

The ESP32 is a microcontroller that provides an API for Bluetooth A2DP which can be used to receive sound data e.g. from your Mobile Phone and makes it available via a callback method. The output is a PCM data stream, decoded from SBC format. The documentation can be found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html). 

![esp32](https://pschatzmann.github.io/ESP32-A2DP/img/esp32.jpeg)

I2S is an electrical serial bus interface standard used for connecting digital audio devices together. It is used to communicate PCM audio data between integrated circuits in an electronic device.

So we can just feed the input from Bluetooth to the I2S output: An example for this from Espressif can be found on [Github](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/classic_bt/a2dp_sink).

Unfortunately this example did not make me happy so I decided to convert it into a simple __Arduino Library__ that is very easy to use from an Arduino Software IDE.

## Supported Bluetooth Protocols

As the name of this library implies, it supports the A2DP [Bluetooth protocol](https://en.wikipedia.org/wiki/List_of_Bluetooth_profiles) which only provides audio streaming! 

It also supports Audio/Video Remote Control Profile (AVRCP) together with A2DP.

The Hands-Free Profile (HFP), Headset Profile (HSP) and standalone AVRCP without A2DP are __not__ supported!

## I2S API / Dependencies

Espressif is retiring the legacy I2S API: So with Arduino v3.0.0 (IDF v5) [my old I2S integration](https://github.com/pschatzmann/ESP32-A2DP/wiki/Legacy-I2S-API) will not be available any more. The legacy syntax is still working as long as you don't upgrade.  

In order to support a unique output API which is version independent, it is recommended to install and use the [AudioTools](https://github.com/pschatzmann/arduino-audio-tools) library. So the documentation and all the examples have been updated to use this new approach.

However you can also output to any other class which inherits from Arduino Print: e.g. the [Arduino ESP32 I2SClass](https://github.com/pschatzmann/ESP32-A2DP?tab=readme-ov-file#output-using-the-esp32-i2s-api) or you can use the [data callback](https://github.com/pschatzmann/ESP32-A2DP?tab=readme-ov-file#accessing-the-sink-data-stream-with-callbacks) described below. 


## A2DP Sink (Music Receiver)

This can be used e.g. to build your own Bluetooth Speaker.

### A Simple I2S Example (A2DS Sink) using default Pins
Here is the simplest example which just uses the proper default settings:

```cpp
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

void setup() {
    a2dp_sink.start("MyMusic");
}

void loop() {
}
```
This creates a new Bluetooth device with the name “MyMusic” and the output will be sent to the following default I2S pins which need to be connected to an external DAC:

- bck_io_num = 14
- ws_io_num = 15
- data_out_num = 22

Please note that these default pins have changed compared to the legacy API!

### Defining Pins

You can define your own pins easily before the ```start```.

```cpp
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

void setup() {
    auto cfg = i2s.defaultConfig();
    cfg.pin_bck = 14;
    cfg.pin_ws = 15;
    cfg.pin_data = 22;
    i2s.begin(cfg);

    a2dp_sink.start("MyMusic");
}

void loop() {
}
```

### Output Using the ESP32 I2S API

You can also use the Arduino ESP32 I2S API: You do not need to install any additional library for this. 

```cpp
#include "ESP_I2S.h"
#include "BluetoothA2DPSink.h"

const uint8_t I2S_SCK = 5;       /* Audio data bit clock */
const uint8_t I2S_WS = 25;       /* Audio data left and right clock */
const uint8_t I2S_SDOUT = 26;    /* ESP32 audio data output (to speakers) */
I2SClass i2s;

BluetoothA2DPSink a2dp_sink(i2s);

void setup() {
    i2s.setPins(I2S_SCK, I2S_WS, I2S_SDOUT);
    if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
      Serial.println("Failed to initialize I2S!");
      while (1); // do nothing
    }

    a2dp_sink.start("MyMusic");
}
void loop() {}
```
Please note, that this API also depends on the installed version: The example above is for ESP32 >= 3.0.0! 

### Output to the Internal DAC

You can also send the output directly to the internal DAC of the ESP32 by using the AnalogAudioStream from the AudioTools:

```cpp
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

AnalogAudioStream out;
BluetoothA2DPSink a2dp_sink(out);

void setup() {
    a2dp_sink.start("MyMusic");
}

void loop() {
}
```

The output goes now to the DAC pins GPIO25 (Channel 1) and GPIO26 (Channel 2).

### Accessing the Sink Data Stream with Callbacks

You can be notified when a packet is received. The API is using PCM data normally formatted as 44.1kHz sampling rate, two-channel 16-bit sample data. 


```cpp
// In the setup function:
a2dp_sink.set_on_data_received(data_received_callback);


// Then somewhere in your sketch:
void data_received_callback() {
  Serial.println("Data packet received");
}
```

Or you can access the packet:

```cpp
// In the setup function:
a2dp_sink.set_stream_reader(read_data_stream);

// Then somewhere in your sketch:
void read_data_stream(const uint8_t *data, uint32_t length)
{
  int16_t *samples = (int16_t*) data;
  uint32_t sample_count = length/2;
  // Do something with the data packet
}
```
In the ```a2dp_sink.set_stream_reader()``` method you can provide an optional parameter that defines if you want the output to I2S to be active or deactive - So you can use this method to e.g. to switch off I2S just by calling ```a2dp_sink.set_stream_reader(read_data_stream, false)```

### Support for Metadata

You can register a method which will be called when the system receives any AVRC metadata (`esp_avrc_md_attr_mask_t`). Here is an example
```cpp
void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
  Serial.printf("AVRC metadata rsp: attribute id 0x%x, %s\n", data1, data2);
}
a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
a2dp_sink.start("BT");
```
By default you should get the most important information, however you can adjust this by calling the ```set_avrc_metadata_attribute_mask``` method e.g if you just need the title and playing time you can call:
```cpp
set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_PLAYING_TIME);
```
before you start the A2DP sink. Note that data2 is actually a char* string, so even though `ESP_AVRC_MD_ATTR_PLAYING_TIME` is documented as the milliseconds of media duration you'll need to parse it before doing math on it. See the metadata example for more.

### Support for Notifications

Similarly to the `avrc_metadata_callback`, ESP IDF v4+ supports selected `esp_avrc_rn_param_t` callbacks like `set_avrc_rn_playstatus_callback`, `set_avrc_rn_play_pos_callback` and `set_avrc_rn_track_change_callback` which can be used to obtain `esp_avrc_playback_stat_t playback` playback status,`uint32_t play_pos` playback position and `uint8_t elm_id` track change flag respectively. See the playing_status_callbacks example for more details.

### Support for AVRC Commands

I have added the following AVRC commands, that you can use to 'control' your A2DP Source:

- play();
- pause();
- stop();
- next();
- previous();
- fast_forward();
- rewind();


## A2DP Source (Music Sender)

This can be used to feed e.g. your Bluetooth Speaker with your audio data.


### Sending Data from a A2DS Data Source with a Callback

We can also generate sound and send it e.g. to a Bluetooth Speaker.  

The supported audio codec in ESP32 A2DP is SBC: The API is using PCM data normally formatted as 44.1kHz sampling rate, two-channel 16-bit sample data. 

When you start the BluetoothA2DPSource, you need to pass the Bluetooth name that you want to connect to and a 'call back function' that provides the sound data:

```cpp
#include "BluetoothA2DPSource.h"

BluetoothA2DPSource a2dp_source;

// callback 
int32_t get_sound_data(uint8 *data, int32_t byteCount) {
    // generate your sound data 
    // return the effective length in bytes
    return byteCount;
}

void setup() {
  a2dp_source.set_data_callback(get_sound_data)
  a2dp_source.start("MyMusic");  
}

void loop() {}

```
Instead of the ```set_data_callback callback``` method you can also use ```set_data_callback_in_frames``` which uses frames instead of bytes. In Arduino you can also provide a Stream (e.g a File) as data source or a callback which provides streams.

In the examples you can find an implementation that generates sound with the help of the sin() function.

You can also indicate multiple alternative Bluetooth names. The system just connects to the first one which is available:

```cpp
void setup() {
  static std::vector<char*> bt_names = {"MyMusic","RadioPlayer","MusicPlayer"};
  a2dp_source.set_data_callback(get_sound_data)
  a2dp_source.start(bt_names); 
} 

```

Further information can be found in the [related class documentation](https://pschatzmann.github.io/ESP32-A2DP/html/class_bluetooth_a2_d_p_source.html)!


## Logging

This library uses the ESP32 logger that you can activate in Arduino in - Tools - Core Debug Log.

## Architecture / Dependencies 

The current code is purely dependent on the ESP-IDF (which is also provided by the Arduino ESP32 core). There are no other dependencies and this includes the Arduino API! 

Therefore we support:

- Arduino
- [PlatformIO](https://github.com/pschatzmann/ESP32-A2DP/wiki/PlatformIO)
- [Espressif IDF](https://github.com/pschatzmann/ESP32-A2DP/wiki/Espressif-IDF-as-a-Component)

This restriction limits however the provided examples. 

Before you clone the project, please read the following information which can be found in the [Wiki](https://github.com/pschatzmann/ESP32-A2DP/wiki/Design-Overview).

## Digital Sound Processing

You can use this library standalone, but it is part of my [audio-tools](https://github.com/pschatzmann/arduino-audio-tools) project. So you can easily enhance this functionality with sound effects, use filters or an equilizer, use alternative audio sinks or audio sources, do FFT etc. Here is a [simple example](https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-communication/a2dp/basic-a2dp-fft/basic-a2dp-fft.ino) how you can analyse the audio data with FFT.

## Documentation

- The [class documentation can be found here](https://pschatzmann.github.io/ESP32-A2DP/html/group__a2dp.html)
- You can also find further information in the [Wiki](https://github.com/pschatzmann/ESP32-A2DP/wiki)

## Support

I spent a lot of time to provide a comprehensive and complete documentation.
So please read the documentation first and check the issues and discussions before posting any new ones on Github.

Open __issues only for bugs__ and if it is not a bug, use a discussion: Provide enough information about 
- the selected scenario/sketch 
- what exactly you are trying to do
- your hardware
- your software versions
  - ESP32 version from the Board Manager
  - version of the ESP32-A2DP library

to enable others to understand and reproduce your issue.

Finally above all __don't__ send me any e-mails or post questions on my personal website! 

## Show and Tell

Get some inspiration [from projects that were using this library](https://github.com/pschatzmann/ESP32-A2DP/discussions/categories/show-and-tell) and share your projects with the community.

## Installation

For Arduino you can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with
```bash
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/ESP32-A2DP.git
git clone https://github.com/pschatzmann/arduino-audio-tools.git
```
For the provided examples, you will need to install the [audio-tools library](https://github.com/pschatzmann/arduino-audio-tools) as well. 

For other frameworks [see the Wiki](https://github.com/pschatzmann/ESP32-A2DP/wiki)

## Change History

The [Change History can be found in the Wiki](https://github.com/pschatzmann/ESP32-A2DP/wiki/Change-History)


## Sponsor Me

This software is totally free, but you can make me happy by rewarding me with a treat

- [Buy me a coffee](https://www.buymeacoffee.com/philschatzh)
- [Paypal me](https://paypal.me/pschatzmann?country.x=CH&locale.x=en_US)

