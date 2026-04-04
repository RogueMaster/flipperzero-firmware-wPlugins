[← Back to documentation index](README.md)

# File Manager Guide

The File Manager starts an HTTP web server on the Flipper, allowing you to browse, download, upload, and delete files on the microSD card from any web browser on the same LAN.

## How It Works

1. Flipper obtains an IP address via DHCP
2. An HTTP server starts on **port 80**
3. A random authentication token is generated
4. The URL with token is displayed on the Flipper screen
5. Open the URL in any browser to manage files

## Authentication

Each session generates a random 4-character hexadecimal token (e.g., `a3f1`). The URL displayed on the Flipper screen includes this token:

```
http://192.168.1.42/?t=a3f1
```

**All HTTP requests must include the `?t=XXXX` parameter.** Requests without a valid token receive `403 Forbidden`. The token is automatically included in all links within the web UI, so you only need to enter it once when opening the initial URL.

The token is only visible on the Flipper's physical screen -- it is not discoverable over the network. This prevents unauthorized access from other devices on the LAN.

## Usage

1. Open **Tools → File Manager** on the Flipper
2. Wait for DHCP to assign an IP (shown on screen)
3. Note the URL with token displayed on the Flipper screen
4. Open the URL in a web browser on any device on the same network
5. Browse, upload, download, or delete files
6. Press **Back** on the Flipper to stop the server

## Operations

### Browsing

Navigate directories by clicking folder names. The current path is shown at the top. Click the parent directory link (`..`) to go up.

Each entry shows:
- File/folder name
- File size (for files)
- Action links (download, delete)

### Downloading Files

Click the **download** link next to any file. The browser will download the file with its original filename.

Large files are streamed in chunks -- the Flipper reads and sends data in blocks to stay within memory limits.

### Uploading Files

Each directory page has an **Upload** form at the bottom. Select a file and click Upload. The file is uploaded via `multipart/form-data` POST request.

**Limitations:**
- Upload speed is limited by SPI throughput (~300-500 KB/s)
- Very large files may take significant time
- Only one file can be uploaded at a time

### Creating Folders

Enter a folder name in the **Create Folder** form and submit. The folder is created in the current directory.

### Deleting

Click **Delete** next to any file or folder. A confirmation dialog appears before deletion. Folders must be empty to be deleted.

## Web UI

The web interface uses a dark theme with responsive layout:

- Works on desktop and mobile browsers
- No JavaScript framework dependencies -- pure HTML
- Clean, minimal design optimized for readability

## Browser Compatibility

| Browser | Support |
|---------|---------|
| Chrome / Chromium | Full |
| Firefox | Full |
| Safari (macOS/iOS) | Full |
| Edge | Full |
| Android Browser | Full |

## Technical Details

### TCP Send Layer

The W5500's `send()` function is non-blocking and may not send all data immediately. The File Manager implements a reliable TCP send layer with `SEND_OK` polling:

1. Write data to the W5500 TX buffer
2. Issue SEND command
3. Poll for SEND_OK status
4. Repeat until all data is sent

This ensures reliable delivery of HTTP responses, especially for large file downloads.

### Connection Handling

- Single client at a time (one TCP socket)
- Keep-alive connections are supported for faster sequential requests
- Socket is force-closed on exit to prevent Flipper freeze (the W5500 `send()` has an internal `while(1)` with no timeout)

### Security

See [Security](security.md) for detailed information about:
- Path traversal protection
- XSS prevention
- Upload filename sanitization
- Auth token system
