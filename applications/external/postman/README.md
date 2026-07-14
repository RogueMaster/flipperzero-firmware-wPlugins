
# ![Flipper Postman Logo](./flipper-postman.png) Flipper Postman

![Static Badge](https://img.shields.io/badge/Flipper%20Zero%20Firmware-1.0.1-orange)


Flipper Postman is a companion app for [Flipper Postman Board Software for ESP32S2 (Flipper Dev Board)](https://github.com/MassivDash/flipper-postman-esp32s2), it provides convenient Ui for all of the custom board actions. 

---
## Features

### Wifi Manager 
- List all available networks
- Save network connection details to csv file 
- Quick connect to wifi networks saved in the csv file

### GET requests 
- Make simple get requests 
- Display or save the results to file 
- Choose between single call or streaming mode for bigger response data
- JSON response formatting
- Save simple get requests to csv file
- Load saved requests from csv file

### POST requests
- Make simple post requests
- Display or save the results to file
- Choose between single call or streaming mode for bigger response data
- JSON response formatting
- Save simple post requests to csv file
- Load saved requests from csv file

### Download files 
- Download files from the url 
- Saves files to apps_data/postmanflipx folder
- Slow but stable buffer download (tested on files up to 100MB)

### Custom HTTP Request builder (in development)
- Build custom HTTP requests with custom headers and body
- Send custom requests and display the response or stream response data to file.
- GET, POST, PUT, DELETE, PATCH methods supported
- Add custom headers
- Add custom body
- Save custom requests to csv file
- Load saved custom requests from csv file

### UDP Server (in development)
- Start UDP server on the device
- Send and receive UDP packets
- Display received packets
- Send packets to IP address and port


### NOTES: 
- Esp32s2 has limited capabilities for handling large payloads, only smaller sites will work with the "Request" mode, for bigger sites use "Stream" mode.
- Save to file is a direct stream to the file, if every other method fails, try to save the response to file and read it from there.
- Check more info about the board at [Flipper Postman Board Software for ESP32S2 (Flipper Dev Board)](https://github.com/MassivDash/flipper-postman-esp32s2)


Author: SpaceGhost [@spaceout.pl](https://spaceout.pl)