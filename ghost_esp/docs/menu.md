# Ghost ESP App Menu Guide

## Overview
This guide explains how to add new menu items and commands to the Ghost ESP application. The app uses a structured approach for menu organization, command execution, and UI feedback.

## Menu Command Structure
```c
typedef struct {
   const char* label;           // Display label in menu 
   const char* command;         // UART command to send
   const char* capture_prefix;  // Prefix for capture files (NULL if none)
   const char* file_ext;        // File extension for captures (NULL if none)
   const char* folder;          // Folder for captures (NULL if none)
   bool needs_input;           // Whether command requires text input
   const char* input_text;     // Text to show in input box (NULL if none)
   bool needs_confirmation;    // Whether command needs confirmation
   const char* confirm_header; // Confirmation dialog header
   const char* confirm_text;   // Confirmation dialog text
} MenuCommand;
```

## Adding New Menu Commands

### 1. Define the Command
Add your command to the appropriate menu array:
```c
static const MenuCommand wifi_commands[] = {
   // Basic command without any special features
   {"My Command", "mycmd\n", NULL, NULL, NULL, false, NULL, false, NULL, NULL},

   // Command that captures data
   {"Capture Data", "capture\n", "capture_prefix", "pcap", GHOST_ESP_APP_FOLDER_PCAPS, 
    false, NULL, false, NULL, NULL},

   // Command that needs user input
   {"Custom Command", "cmd", NULL, NULL, NULL, true, "Enter value:", 
    false, NULL, NULL},

   // Command with confirmation dialog
   {"Dangerous Command", "danger\n", NULL, NULL, NULL, false, NULL, true,
    "Warning", "This action cannot be undone.\nAre you sure?"},
};
```

### 2. Menu Types and Features

#### Basic Command
```c
{"Label", "command\n", NULL, NULL, NULL, false, NULL, false, NULL, NULL}
```

#### Command with File Capture
```c
{"Label", "command\n", "prefix", "ext", FOLDER, false, NULL, false, NULL, NULL}
```

#### Command with User Input
```c
{"Label", "command", NULL, NULL, NULL, true, "Input prompt", false, NULL, NULL}
```

#### Command with Confirmation
```c
{"Label", "command\n", NULL, NULL, NULL, false, NULL, true,
"Header", "Confirmation message"}
```

## Command Execution Flow

### Regular Commands
1. Command validation
2. ESP connection check
3. Send command via UART
4. Process response

### Input Commands
1. Show text input dialog
2. Wait for user input
3. Send command + input
4. Process response

### Confirmation Commands
1. Show confirmation dialog
2. Wait for user response
3. Execute on confirm
4. Return to menu on cancel

### Capture Commands
1. Execute command
2. Create capture file
3. Save data to file
4. Close file on completion

## Example Implementations

### WiFi Command Group
```c
static const MenuCommand wifi_commands[] = {
   // Scanning Operations
   {"Scan WiFi APs", "scanap\n", NULL, NULL, NULL, false, NULL, false, NULL, NULL},
   {"Stop Scan", "stopscan\n", NULL, NULL, NULL, false, NULL, false, NULL, NULL},

   // Capture Operations
   {"Sniff Packets", "capture\n", "packet_capture", "pcap", 
    GHOST_ESP_APP_FOLDER_PCAPS, false, NULL, false, NULL, NULL},

   // Interactive Operations
   {"Connect WiFi", "connect", NULL, NULL, NULL, true, "SSID Password", 
    false, NULL, NULL},

   // Dangerous Operations
   {"Deauth", "attack -d\n", NULL, NULL, NULL, false, NULL, true,
    "Warning", "This will disconnect devices.\nContinue?"},
};
```

### BLE Command Group
```c
static const MenuCommand ble_commands[] = {
   {"Find Flippers", "blescan -f\n", NULL, NULL, NULL, false, NULL, false, NULL, NULL},
   {"Stop Scan", "blescan -s\n", NULL, NULL, NULL, false, NULL, false, NULL, NULL},
};
```

## Best Practices

1. Command String Format
  - Add newline for automatic commands: `command\n`
  - Omit newline for input commands: `command`

2. Input Commands
  - Clear input prompts
  - Show expected format
  - Validate input where possible

3. Confirmation Dialogs
  - Clear warning messages
  - Explain consequences
  - Allow easy cancellation

4. Capture Files
  - Descriptive prefixes
  - Appropriate extensions
  - Organized folders

5. Error Handling
  - Check ESP connection
  - Validate inputs
  - Handle failures gracefully

6. UI Feedback
  - Show operation status
  - Clear error messages
  - Return to appropriate menu

## Common Patterns

### Command with Input and Confirmation
```c
{"Dangerous Input", "danger", NULL, NULL, NULL, true, "Enter target:", true,
"Warning", "This will affect target.\nContinue?"}
```

### Capture with Auto-naming
```c
{"Capture Data", "capture\n", "auto_capture", "bin", GHOST_ESP_APP_FOLDER_PCAPS,
false, NULL, false, NULL, NULL}
```

### Toggle Command
```c
{"Toggle Feature", "toggle\n", NULL, NULL, NULL, false, NULL, true,
"Toggle", "Feature will be switched.\nContinue?"}
```