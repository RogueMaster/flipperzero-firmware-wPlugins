# Flipper Zero HC-SR501 PIR Proximity Alarm App

Simple proximity alarm app for Flipper Zero using HC-SR501 PIR sensor.  
Triggers vibration, LED alerts, and screen notifications. Features 3-second alarm duration with blinking feedback.

## Wiring Diagram

| HC-SR501 Pin | Flipper Zero Pin | Description |
|--------------|-----------------|-------------|
| **SIG (OUT)** | **GPIO C0 (Pin 16)** | Motion detection signal |
| **VCC**       | **5V (Pin 1)**       | Power supply (5V) |
| **GND**       | **GND (Pin 8)**      | Ground |

## Features

- **Motion Detection**: HC-SR501 PIR sensor detects human motion within ~7m range
- **Multi-Alert System**: Vibration motor + LED blinking + screen notification
- **Auto-Reset**: Returns to idle state after alarm duration

## App States

**Idle State**  
- No motion detected  
- *(Idle state, no motion visual representation removed due to Markdown restrictions)*

**Alarm State (Active)**  
- Motion detected, triggers vibration, LED, and screen notification  
- *(Alarm state visual representation removed due to Markdown restrictions)*

## Implementation Guide

**Required Components:**

- Flipper Zero (firmware supporting GPIO input)
- HC-SR501 PIR Motion Sensor
- Jumper wires (female-to-male recommended)
- Optional: 100nF capacitor across VCC-GND for sensor stability

**Pin Configuration:**

```lua
-- GPIO Pin Definitions
local PIR_PIN  = gpio.c0     -- Pin 16 / GPIO C0 (PIR Input)
local POWER_SUPPLY  = gpio.gpioa7 -- Pin 1  / GPIO 5V 
local GND  = gpio.gpiob3 -- Pin 8/18 / GPIO  GND
