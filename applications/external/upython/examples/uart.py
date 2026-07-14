import time
import flipperzero as f0


def read_from_uart():
    with f0.uart_open(f0.UART_MODE_USART, 115200) as uart:
        for _ in range(1000):
            raw_data = uart.read()

            if len(raw_data) > 0:
                data = raw_data.decode()

                print(data)

            time.sleep_ms(10)


read_from_uart()
