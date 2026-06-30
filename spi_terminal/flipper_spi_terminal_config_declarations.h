#ifndef SPI_CONFIG_ADDED_INCLUDES
#include <furi_hal_spi_types.h>
#include "toolbox/value_index_ex.h"
#define SPI_CONFIG_ADDED_INCLUDES
#endif

#ifndef UNWRAP_ARGS
// Removes brackets from a list of values
#define UNWRAP_ARGS(...) __VA_ARGS__
#endif

#ifndef ADD_CONFIG_ENTRY
// This is just a dummy function to enable some fancy syntax highlighting and autocomplete.
// It will only be used at edit time.
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    void(dummy_config_##name##_preview_func)() {                                                    \
        const char* labelstr = label;                                                               \
        UNUSED(labelstr);                                                                           \
        const char* helpstr = helpText;                                                             \
        UNUSED(helpstr);                                                                            \
        const type def = defaultValue;                                                              \
        const type vals[valuesCount] = {UNWRAP_ARGS values};                                        \
        size_t index = (valueIndexFunc)(def, vals, valuesCount);                                    \
        UNUSED(index);                                                                              \
        const char* const strs[valuesCount] = {UNWRAP_ARGS strings};                                \
        UNUSED(strs);                                                                               \
    }
#endif

#ifndef FORMAT_VALUE_DESCRIPTION
#define FORMAT_VALUE_DESCRIPTION(header, text) "=== " header " ===\n" text "\n"
#endif

#ifndef FORMAT_DESCRIPTION_MIN
#define FORMAT_DESCRIPTION_MIN(general_info, default_value_str) \
    general_info "\n\nDefault: " default_value_str
#endif

#ifndef FORMAT_DESCRIPTION
#define FORMAT_DESCRIPTION(general_info, default_value_str, value_descriptions) \
    general_info "\n\n" UNWRAP_ARGS value_descriptions "\nDefault: " default_value_str
#endif

ADD_CONFIG_ENTRY(
    "Display mode",
    FORMAT_DESCRIPTION(
        "Changes, how the received data is displayed.",
        "Auto",
        (FORMAT_VALUE_DESCRIPTION("Auto", "Use ASCII, C escape sequence or Hex")
             FORMAT_VALUE_DESCRIPTION(
                 "Text",
                 "Use ASCII for everything (Non printable chars are replaced by a space ' ')")
                 FORMAT_VALUE_DESCRIPTION("Hex", "Use Hex for everything")
                     FORMAT_VALUE_DESCRIPTION("Binary", "Use binary for everything"))),
    display_mode,
    TerminalDisplayMode,
    TerminalDisplayModeAuto,
    value_index_display_mode,
    display_mode,
    4,
    (TerminalDisplayModeAuto,
     TerminalDisplayModeText,
     TerminalDisplayModeHex,
     TerminalDisplayModeBinary),
    ("Auto", "Text", "Hex", "Binary"))

ADD_CONFIG_ENTRY(
    "Terminal Buffer behaviour",
    FORMAT_DESCRIPTION(
        "Configures, how the Terminal Screen buffer behaves, if you leave/enter the Terminal View.",
        "Clear",
        (FORMAT_VALUE_DESCRIPTION("Clear", "Terminal Buffer will be cleared")
             FORMAT_VALUE_DESCRIPTION("Keep", "The Buffer will keep it's content"))),
    terminal_buffer_behaviour,
    TerminalBufferBehaviour,
    TerminalBufferBehaviourClear,
    value_index_buffer_behaviour,
    terminal_buffer_behaviour,
    2,
    (TerminalBufferBehaviourClear, TerminalBufferBehaviourKeep),
    ("Clear", "Keep"))

ADD_CONFIG_ENTRY(
    "DMA RX Buffer size",
    FORMAT_DESCRIPTION(
        "Sets the buffer size for a SPI read request. A higher value results in a less frequent update. A higher value will result in a faster data rate."
        "SPI Terminal utilizes the inbuild DMA controller for a event/interrupt based receive and transmit system. This removes the overhead of a polling based system.",
        "1",
        ("1-256 byte")),
    rx_dma_buffer_size,
    size_t,
    1,
    value_index_size_t,
    rx_dma_buffer_size,
    9,
    (1, 2, 4, 8, 16, 32, 64, 128, 256),
    ("1", "2", "4", "8", "16", "32", "64", "128", "256"))

ADD_CONFIG_ENTRY(
    "Mode",
    FORMAT_DESCRIPTION(
        "Configures the SPI-Mode.",
        "Slave",
        (FORMAT_VALUE_DESCRIPTION("Master", "Flipper is controlling the SPI clock")
             FORMAT_VALUE_DESCRIPTION("Slave", "A external device is controlling the SPI clock"))),
    spi_mode,
    uint32_t,
    LL_SPI_MODE_SLAVE,
    value_index_uint32,
    spi.Mode,
    2,
    (LL_SPI_MODE_MASTER, LL_SPI_MODE_SLAVE),
    ("Master", "Slave"))

ADD_CONFIG_ENTRY(
    "Direction",
    FORMAT_DESCRIPTION(
        "Configures the transfer direction.",
        "Full Duplex",
        (FORMAT_VALUE_DESCRIPTION(
            "Full Duplex",
            "Data is transmitted in both directions at the same time (RX and TX). Both data lines are used simultaneously.")
             FORMAT_VALUE_DESCRIPTION(
                 "Simplex RX",
                 "Only receive data. Can be used to 'sniff' the communication between devices.")
                 FORMAT_VALUE_DESCRIPTION(
                     "Half Duplex RX/TX",
                     "Only send data. In this mode, RX and TX modes are altered after every transfer. This alows the use of only one data line"))),
    spi_direction,
    uint32_t,
    LL_SPI_FULL_DUPLEX,
    value_index_uint32,
    spi.TransferDirection,
    4,
    (LL_SPI_FULL_DUPLEX, LL_SPI_SIMPLEX_RX, LL_SPI_HALF_DUPLEX_RX, LL_SPI_HALF_DUPLEX_TX),
    ("Full Duplex", "Simplex RX", "Half Duplex RX", "Half Duplex TX"))

ADD_CONFIG_ENTRY(
    "Data Width",
    FORMAT_DESCRIPTION_MIN("Sets the data width of a single transfer.", "8 bit"),
    spi_data_width,
    uint32_t,
    LL_SPI_DATAWIDTH_8BIT,
    value_index_uint32,
    spi.DataWidth,
    13,
    (LL_SPI_DATAWIDTH_4BIT,
     LL_SPI_DATAWIDTH_5BIT,
     LL_SPI_DATAWIDTH_6BIT,
     LL_SPI_DATAWIDTH_7BIT,
     LL_SPI_DATAWIDTH_8BIT,
     LL_SPI_DATAWIDTH_9BIT,
     LL_SPI_DATAWIDTH_10BIT,
     LL_SPI_DATAWIDTH_11BIT,
     LL_SPI_DATAWIDTH_12BIT,
     LL_SPI_DATAWIDTH_13BIT,
     LL_SPI_DATAWIDTH_14BIT,
     LL_SPI_DATAWIDTH_15BIT,
     LL_SPI_DATAWIDTH_16BIT),
    ("4 bit",
     "5 bit",
     "6 bit",
     "7 bit",
     "8 bit",
     "9 bit",
     "10 bit",
     "11 bit",
     "12 bit",
     "13 bit",
     "14 bit",
     "15 bit",
     "16 bit"))

ADD_CONFIG_ENTRY(
    "Clock polarity",
    FORMAT_DESCRIPTION(
        "Configures the polarity of the SPI clock.",
        "High",
        (FORMAT_VALUE_DESCRIPTION("Low", "Clock line is 0 (low, off) on idle")
             FORMAT_VALUE_DESCRIPTION("High", "Clock line is 1 (high, on) on idle"))),
    spi_clock_polarity,
    uint32_t,
    LL_SPI_POLARITY_LOW,
    value_index_uint32,
    spi.ClockPolarity,
    2,
    (LL_SPI_POLARITY_LOW, LL_SPI_POLARITY_HIGH),
    ("Low", "Heigh"))

ADD_CONFIG_ENTRY(
    "Clock Phase",
    FORMAT_DESCRIPTION(
        "Defines, on which clock edge the data is sampled.",
        "1-Edge",
        (FORMAT_VALUE_DESCRIPTION(
            "1-Edge",
            "Trigger on the first clock polarity change (CP: High, CLK: ...1 1010 1010 1... => Trigger on the transition from 1 to 0)")
             FORMAT_VALUE_DESCRIPTION(
                 "2-Edge",
                 "Trigger on the second clock polarity change (CP: High, CLK: ...1 1010 1010 1... => Trigger on the transition from 0 to 1)"))),
    spi_clock_phase,
    uint32_t,
    LL_SPI_PHASE_1EDGE,
    value_index_uint32,
    spi.ClockPhase,
    2,
    (LL_SPI_PHASE_1EDGE, LL_SPI_PHASE_2EDGE),
    ("1-Edge", "2-Edge"))

ADD_CONFIG_ENTRY(
    "Non Slave Select",
    FORMAT_DESCRIPTION(
        "Configures the slave select behavior on the CS pin",
        "Soft",
        (FORMAT_VALUE_DESCRIPTION(
            "Soft",
            "NSS managed by the software. In this mode, Flipper SPI Terminal is not using any Slave Select mechanisms and is always sending or receiving data.")
             FORMAT_VALUE_DESCRIPTION(
                 "Hard Input",
                 "If Flipper Zero is acting as a Slave, CS-Pin is used as standard chip select. If Flipper Zero is configured as Master, this pin can be used for a 'Multi Master' configuration.")
                 FORMAT_VALUE_DESCRIPTION(
                     "Hard Output:",
                     " (Only used in Master-Mode) CS is driven low as soon as the Terminal Screen is entered. Due to a Flipper Firmware limitation, the pin might send a pulse to the other devices."))),
    spi_nss,
    uint32_t,
    LL_SPI_NSS_SOFT,
    value_index_uint32,
    spi.NSS,
    3,
    (LL_SPI_NSS_SOFT, LL_SPI_NSS_HARD_INPUT, LL_SPI_NSS_HARD_OUTPUT),
    ("Soft", "Hard Input", "Hard Output"))

ADD_CONFIG_ENTRY(
    "Baudrate prescaler",
    FORMAT_DESCRIPTION_MIN(
        "Sets, by how many cycles, the SPI clock is divided. Flippers SPI clock is running at 32MHz. A prescaler of Div 2 will result int a baudrate of around 16 Mbit.",
        "Div 32"),
    spi_baud_rate,
    uint32_t,
    LL_SPI_BAUDRATEPRESCALER_DIV32,
    value_index_uint32,
    spi.BaudRate,
    8,
    (LL_SPI_BAUDRATEPRESCALER_DIV2,
     LL_SPI_BAUDRATEPRESCALER_DIV4,
     LL_SPI_BAUDRATEPRESCALER_DIV8,
     LL_SPI_BAUDRATEPRESCALER_DIV16,
     LL_SPI_BAUDRATEPRESCALER_DIV32,
     LL_SPI_BAUDRATEPRESCALER_DIV64,
     LL_SPI_BAUDRATEPRESCALER_DIV128,
     LL_SPI_BAUDRATEPRESCALER_DIV256),
    ("Div 2", "Div 4", "Div 8", "Div 16", "Div 32", "Div 64", "Div 128", "Div 256"))

ADD_CONFIG_ENTRY(
    "Bit-order",
    FORMAT_DESCRIPTION(
        "Sets, in which order bytes are received.",
        "MSB",
        ("Most significant bit or Least significant bit")),
    spi_bit_order,
    uint32_t,
    LL_SPI_MSB_FIRST,
    value_index_uint32,
    spi.BitOrder,
    2,
    (LL_SPI_MSB_FIRST, LL_SPI_LSB_FIRST),
    ("MSB", "LSB"))

ADD_CONFIG_ENTRY(
    "CRC",
    FORMAT_DESCRIPTION_MIN("Enables or disables the CRC calculation.", "Disabled"),
    spi_crc_calculation,
    uint32_t,
    LL_SPI_CRCCALCULATION_DISABLE,
    value_index_uint32,
    spi.CRCCalculation,
    2,
    (LL_SPI_CRCCALCULATION_DISABLE, LL_SPI_CRCCALCULATION_ENABLE),
    ("Disabled", "Enabled"))

ADD_CONFIG_ENTRY(
    "CRC Poly",
    FORMAT_DESCRIPTION_MIN("Sets the CRC Polynomial value.", "/"),
    spi_crc_poly,
    uint32_t,
    7,
    value_index_uint32,
    spi.CRCPoly,
    10,
    (2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
    ("2", "3", "4", "5", "6", "7", "8", "9", "10", "11"))
