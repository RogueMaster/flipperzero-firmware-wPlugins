
/**
 * UartHelper is a utility class that helps with reading lines of data from a UART.
 * It uses a stream buffer to receive data from the UART ISR, and a worker thread
 * to dequeue data from the stream buffer and process it.  The worker thread uses
 * a ring buffer to hold data until a delimiter is found, at which point the line
 * is extracted and the process_line callback is invoked.
 * 
 * @author CodeAllNight
*/

#ifndef UART_HELPER_H
#define UART_HELPER_H

#include <furi.h>
#include "ring_buffer.h"

/**
 * WorkerEventFlags are used to signal the worker thread to exit or to process data.
 * Each flag is a bit in a 32-bit integer, so we can use the FuriThreadFlags API to
 * wait for either flag to be set.
*/
typedef enum {
    WorkerEventDataWaiting = 1 << 0, // bit flag 0 - data is waiting to be processed
    WorkerEventExiting = 1 << 1, // bit flag 1 - worker thread is exiting
} WorkerEventFlags;

/**
 * Callback function for processing a line of data.
 * 
 * @param line The line of data to process.
*/
typedef void (*ProcessLine)(FuriString* line, void* context);

/**
 * UartHelper is a utility class that helps with reading lines of data from a UART.
*/
typedef struct {
    // UART bus & channel to use
    FuriHalBus uart_bus;
    FuriHalSerialHandle* serial_handle;
    bool uart_init_by_app;

    // Stream buffer to hold incoming data (worker will dequeue and process)
    FuriStreamBuffer* rx_stream;

    // Worker thread that dequeues data from the stream buffer and processes it
    FuriThread* worker_thread;

    FuriThreadId transmitter_thread_id;

    // Buffer to hold data until a delimiter is found
    RingBuffer* ring_buffer;

    void* context;

} UartHelper;

void uart_helper_set_transmitter_thread_id(void* context, FuriThreadId thread_id);

/**
 * Allocates a new UartHelper.  The UartHelper will be initialized with a baud rate of 115200.
 * Log messages will be disabled since they also use the UART.
 * 
 * IMPORTANT -- FURI_LOG_x calls will not work!
 * 
 * @return A new UartHelper.
*/
UartHelper* uart_helper_alloc();

/**
 * Sets the delimiter to use when parsing data.  The default delimeter is '\n' and
 * the default value for include_delimiter is false.
 * 
 * @param helper            The UartHelper.
 * @param delimiter         The delimiter to use.
 * @param include_delimiter If true, the delimiter will be included in the line of data passed to the callback function.
*/
void uart_helper_set_delimiter(UartHelper* helper, char delimiter, bool include_delimiter);

/**
 * Sets the baud rate for the UART.  The default is 115200.
 * 
 * @param helper    The UartHelper.
 * @param baud_rate The baud rate.
*/
void uart_helper_set_baud_rate(UartHelper* helper, uint32_t baud_rate);

/**
 * Sets the read text in text variable.
 */
bool uart_helper_read(UartHelper* helper, FuriString* text);

/**
 * Sends data over the UART TX pin.
*/
void uart_helper_send(UartHelper* helper, const char* data, size_t length, bool wait_response);

/**
 * Sends a string over the UART TX pin.
*/
void uart_helper_send_string(UartHelper* helper, FuriString* string);

/**
 * Frees the UartHelper & enables log messages.
*/
void uart_helper_free(UartHelper* helper);

#endif
