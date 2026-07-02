#!/usr/bin/env python
"""
This script bridges data between a serial port and a TCP connection. It reads data from
a specified serial port and sends it to a TCP server, as well as reads data from the TCP
server and writes it to the serial port. Command line arguments allow dynamic configuration
of the serial port, TCP server.
"""

import serial
import socket
import threading
import logging
import time
import argparse


def serial_to_tcp(ser, sock, stop_event):
    """Continuously read data from the serial port and send it to the TCP server."""
    while not stop_event.is_set():
        try:
            # Read available bytes; wait at least one byte if none is immediately available
            data = ser.read(ser.in_waiting or 1)
            if data:
                sock.sendall(data)
        except Exception as e:
            logging.error("Error sending serial data to TCP: %s", e)
            stop_event.set()
            break
        # Sleep briefly to reduce CPU usage
        time.sleep(0.01)


def tcp_to_serial(ser, sock, stop_event):
    """Continuously read data from the TCP connection and write it to the serial port."""
    while not stop_event.is_set():
        try:
            data = sock.recv(1024)
            if not data:
                logging.info("TCP connection closed by remote server.")
                stop_event.set()
                break
            ser.write(data)
        except Exception as e:
            logging.error("Error writing TCP data to serial: %s", e)
            stop_event.set()
            break
        time.sleep(0.01)


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Bridge data between a serial port and a TCP connection."
    )
    parser.add_argument(
        "SERIAL_PORT", help="Serial port to read from (e.g. /dev/ttyUSB0 or COM3)"
    )
    parser.add_argument(
        "TCP_SERVER", help="TCP server hostname or IP (e.g. telehack.com)"
    )
    parser.add_argument(
        "--port", type=int, default=23, help="TCP port number (default: 23)"
    )
    parser.add_argument(
        "--baudrate",
        type=int,
        default=19200,
        help="Baudrate for the serial port (default: 19200)",
    )
    return parser.parse_args()


def main():
    args = parse_arguments()

    # Setup basic logging
    logging.basicConfig(
        level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s"
    )

    stop_event = threading.Event()
    ser = None
    sock = None

    try:
        # Open the serial port
        ser = serial.Serial(args.SERIAL_PORT, args.baudrate, timeout=0)
        logging.info(
            "Opened serial port %s at %d baud.", args.SERIAL_PORT, args.baudrate
        )

        # Create a TCP socket and connect to the remote server
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((args.TCP_SERVER, args.port))
        logging.info("Connected to TCP server %s:%d.", args.TCP_SERVER, args.port)

        # Create and start threads for bidirectional data transfer
        t_serial = threading.Thread(
            target=serial_to_tcp, args=(ser, sock, stop_event), daemon=True
        )
        t_tcp = threading.Thread(
            target=tcp_to_serial, args=(ser, sock, stop_event), daemon=True
        )
        t_serial.start()
        t_tcp.start()

        # Wait until stop_event is set (by threads or KeyboardInterrupt)
        while not stop_event.is_set():
            time.sleep(0.5)

    except KeyboardInterrupt:
        logging.info("Interrupted by user.")
    except Exception as e:
        logging.error("Error: %s", e)
    finally:
        stop_event.set()  # signal threads to stop

        # Wait for threads to finish cleanly
        if "t_serial" in locals():
            t_serial.join(timeout=2)
        if "t_tcp" in locals():
            t_tcp.join(timeout=2)

        # Attempt to shut down and close the TCP socket
        if sock:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            sock.close()
        # Close the serial connection
        if ser:
            ser.close()

        logging.info("Closed connections. Exiting.")


if __name__ == "__main__":
    main()
