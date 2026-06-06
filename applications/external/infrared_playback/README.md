This is similar to the playback feature of the standard IR app on the Flipper.
The main difference is:
* The names of the recorded remotes are lazy loaded, allowing larger files to be loaded without crashing the flipper
    * Though it is still limited to 1024 to allow for fast updates when using a large file
* Cleaning up raw payloads to prevent using data that makes no sense
    * Raw files can contain data for a time so long that it seems to freeze the flipper.
    * Caps the data at 1 second (value of 1000000)
* Note: Data that cannot be parsed (eg having negative data) does not get loaded
