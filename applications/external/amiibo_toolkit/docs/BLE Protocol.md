# BLE Protocol for AmiTool

This document describes the Bluetooth Low Energy (BLE) protocol used by AmiTool to communicate with a host device.

## BLE Services and Characteristics

Once enabled BLE in the AmiTool app, it will start to advertise as a BLE peripheral, with name "AmiTool YourFlipperName". This will disable the Flipper's own BLE functions, because the firmware profile is replaced by the AmiTool app one. The official firmware does not support adding to the firmware profile to keep both functions working at the same time. To maintain compatibility with the official firmware, AmiTool replaces the firmware profile with its own, which includes the necessary services and characteristics for communication.

The two characteristics used by AmiTool are:

- 19ed82ae-ed21-4c9d-4145-228e61fe0000: This characteristic is used for receiving data from the Flipper device. Supports Notify and Indicate.
- 19ed82ae-ed21-4c9d-4145-228e62fe0000: This characteristic is used for sending data to the Flipper device. Supports Write and Write Without Response.

## Data Format

The protocol is binary. The format of each message is as follows, where the message are hex and size in bytes:

| Function | Sent                 | Size | Response    | Size | Description                                     |
| -------- | -------------------- | ---- | ----------- | ---- | ----------------------------------------------- |
| Generate | A2 B0 + Character ID | 10   | B0 A2       | 2    | Generate a new character with the specified ID. |
| Get UID  | A2 B1                | 2    | B1 A2 + UID | 9    | Get the UID of the tag currently in memory      |
| Get UID  | A2 B1                | 2    | B1 A2 00    | 3    | No tag in memory                                |
