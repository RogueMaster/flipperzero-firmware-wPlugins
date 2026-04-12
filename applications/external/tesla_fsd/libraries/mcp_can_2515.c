#include "mcp_can_2515.h"

// To read a register with SPI protocol
static bool read_register(FuriHalSpiBusHandle* spi, uint8_t address, uint8_t* data) {
    bool ret = true;
    uint8_t instruction[] = {INSTRUCTION_READ, address};
    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, data, sizeof(data), TIMEOUT_SPI);

    furi_hal_spi_release(spi);
    return ret;
}

// To read a register with SPI Protocol
static bool set_register(FuriHalSpiBusHandle* spi, uint8_t address, uint8_t data) {
    bool ret = true;
    uint8_t instruction[] = {INSTRUCTION_WRITE, address, data};
    furi_hal_spi_acquire(spi);
    ret = furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);
    furi_hal_spi_release(spi);
    return ret;
}

// To modify the value of one bit from a register
static bool
    modify_register(FuriHalSpiBusHandle* spi, uint8_t address, uint8_t mask, uint8_t data) {
    uint8_t instruction[] = {INSTRUCTION_BITMOD, address, mask, data};
    bool ret = true;
    furi_hal_spi_acquire(spi);
    ret = furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);
    furi_hal_spi_release(spi);
    return ret;
}

// To reset the MCP2515
bool mcp_reset(FuriHalSpiBusHandle* spi) {
    uint8_t buff[1] = {INSTRUCTION_RESET};
    bool ret = true;
    furi_hal_spi_acquire(spi);
    ret = furi_hal_spi_bus_tx(spi, buff, sizeof(buff), TIMEOUT_SPI);
    furi_hal_spi_release(spi);
    return ret;
}

// To get the MCP2515 status
bool mcp_get_status(FuriHalSpiBusHandle* spi, uint8_t* data) {
    uint8_t buff[1] = {INSTRUCTION_READ_STATUS};
    bool ret = true;
    furi_hal_spi_acquire(spi);
    ret =
        (furi_hal_spi_bus_tx(spi, buff, sizeof(buff), TIMEOUT_SPI) &&
         furi_hal_spi_bus_rx(spi, data, sizeof(data), TIMEOUT_SPI));
    furi_hal_spi_release(spi);
    return ret;
}

// This function works to get the Can Id from the buffer
void read_Id(FuriHalSpiBusHandle* spi, uint8_t addr, uint32_t* id, uint8_t* ext) {
    uint8_t tbufdata[4] = {0, 0, 0, 0};
    *ext = 0;
    *id = 0;

    read_register(spi, addr, tbufdata);

    *id = (tbufdata[MCP_SIDH] << 3) + (tbufdata[MCP_SIDL] >> 5);

    if((tbufdata[MCP_SIDL] & MCP_TXB_EXIDE_M) == MCP_TXB_EXIDE_M) {
        *id = (*id << 2) + (tbufdata[MCP_SIDL] & 0x03);
        *id = (*id << 8) + tbufdata[MCP_EID8];
        *id = (*id << 8) + tbufdata[MCP_EID0];
        *ext = 1;
    }
}

// get actual mode of the MCP2515
uint8_t get_mode(FuriHalSpiBusHandle* spi) {
    uint8_t data = 0;

    uint8_t instruction[] = {INSTRUCTION_READ, MCP_CANSTAT};
    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, &data, 1, TIMEOUT_SPI);

    furi_hal_spi_release(spi);

    return data & CANSTAT_OPM;
}

// compare if the chip is the same mode
bool is_mode(MCP2515* mcp_can, MCP_MODE mode) {
    uint16_t data = get_mode(mcp_can->spi);
    if(data == mode) return true;
    return false;
}

// To set a new mode
bool set_new_mode(MCP2515* mcp_can, MCP_MODE new_mode) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;

    uint8_t read_status = 0;
    bool ret = false;

    if(get_mode(spi) == new_mode) return false;

    if((get_mode(spi) == MCP_SLEEP) && new_mode != MCP_SLEEP) {
        uint8_t wake_up_enabled = 0;
        read_register(spi, MCP_CANINTE, &wake_up_enabled);
        wake_up_enabled &= MCP_WAKIF;

        if(!wake_up_enabled) {
            modify_register(spi, MCP_CANINTE, MCP_WAKIF, MCP_WAKIF);
        }

        if(get_mode(spi) != MCP_LISTENONLY) {
            return false;
        }

        if(!wake_up_enabled) {
            modify_register(spi, MCP_CANINTE, MCP_WAKIF, 1);
        }

        modify_register(spi, MCP_CANINTF, MCP_WAKIF, 0);
    }

    uint32_t time_out = furi_get_tick();

    time_out = furi_get_tick();

    do {
        modify_register(spi, MCP_CANCTRL, CANCTRL_REQOP, MODE_CONFIG);
        read_register(spi, MCP_CANSTAT, &read_status);

        read_status &= CANSTAT_OPM;
        if(read_status == MODE_CONFIG) ret = true;

        furi_delay_us(1);

    } while((ret != true) && ((furi_get_tick() - time_out) < 50));

    time_out = furi_get_tick();

    do {
        modify_register(spi, MCP_CANCTRL, CANCTRL_REQOP, new_mode);
        read_register(spi, MCP_CANSTAT, &read_status);

        read_status &= CANSTAT_OPM;
        if(read_status == new_mode) return true;

        furi_delay_us(1);

    } while((furi_get_tick() - time_out) < 50);

    return false;
}

// To set Config mode
bool set_config_mode(MCP2515* mcp_can) {
    bool ret = true;
    ret = set_new_mode(mcp_can, MODE_CONFIG);

    return ret;
}

// To set Normal Mode
bool set_normal_mode(MCP2515* mcp_can) {
    bool ret = true;
    ret = set_new_mode(mcp_can, MCP_NORMAL);
    return ret;
}

// To set ListenOnly Mode
bool set_listen_only_mode(MCP2515* mcp_can) {
    bool ret = true;
    ret = set_new_mode(mcp_can, MCP_LISTENONLY);
    return ret;
}

// To set Sleep Mode
bool set_sleep_mode(MCP2515* mcp_can) {
    bool ret = true;
    ret = set_new_mode(mcp_can, MCP_SLEEP);
    return ret;
}

// To set Loop Back Mode
bool set_loop_back_mode(MCP2515* mcp_can) {
    bool ret = true;
    ret = set_new_mode(mcp_can, MCP_LOOPBACK);
    return ret;
}

// To write the mask-filters for the chip
void write_mf(FuriHalSpiBusHandle* spi, uint8_t address, uint8_t ext, uint32_t id) {
    uint16_t canId = (uint16_t)(id & 0x0FFFF);
    uint8_t bufData[4];

    if(ext) {
        bufData[MCP_EID0] = (uint8_t)(canId & 0xFF);
        bufData[MCP_EID8] = (uint8_t)(canId >> 8);
        canId = (uint16_t)(id >> 16);
        bufData[MCP_SIDL] = (uint8_t)(canId & 0x03);
        bufData[MCP_SIDL] += (uint8_t)((canId & 0x1C) << 3);
        bufData[MCP_SIDL] |= MCP_TXB_EXIDE_M;
        bufData[MCP_SIDH] = (uint8_t)(canId >> 5);
    } else {
        bufData[MCP_SIDL] = (uint8_t)((canId & 0x07) << 5);
        bufData[MCP_SIDH] = (uint8_t)(canId >> 3);
        bufData[MCP_EID0] = 0;
        bufData[MCP_EID8] = 0;
    }

    uint8_t instruction[] = {INSTRUCTION_WRITE, address};

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);

    furi_hal_spi_bus_tx(spi, bufData, 4, TIMEOUT_SPI);
    furi_hal_spi_release(spi);
}

// Init can buffers
void init_can_buffer(FuriHalSpiBusHandle* spi) {
    uint8_t a1 = 0, a2 = 0, a3 = 0;

    uint8_t std = 0;
    uint8_t ext = 1;

    uint32_t ulMask = 0x00, ulFilt = 0x00;

    write_mf(spi, MCP_RXM0SIDH, ext, ulMask);

    write_mf(spi, MCP_RXM1SIDH, ext, ulMask);

    write_mf(spi, MCP_RXF0SIDH, ext, ulFilt);

    write_mf(spi, MCP_RXF1SIDH, std, ulFilt);

    write_mf(spi, MCP_RXF2SIDH, ext, ulFilt);

    write_mf(spi, MCP_RXF3SIDH, std, ulFilt);

    write_mf(spi, MCP_RXF4SIDH, ext, ulFilt);

    write_mf(spi, MCP_RXF5SIDH, std, ulFilt);

    a1 = MCP_TXB0CTRL;
    a2 = MCP_TXB1CTRL;
    a3 = MCP_TXB2CTRL;

    for(int i = 0; i < 14; i++) {
        set_register(spi, a1, 0);
        set_register(spi, a2, 0);
        set_register(spi, a3, 0);
        a1++;
        a2++;
        a3++;
    }
}

// This function works to set Registers to initialize the MCP2515
void set_registers_init(FuriHalSpiBusHandle* spi) {
    set_register(spi, MCP_CANINTE, MCP_RX0IF | MCP_RX1IF);

    set_register(spi, MCP_BFPCTRL, MCP_BxBFS_MASK | MCP_BxBFE_MASK);

    set_register(spi, MCP_TXRTSCTRL, 0x00);

    set_register(spi, MCP_RXB0CTRL, MCP_RXB_BUKT_MASK);
    set_register(spi, MCP_RXB1CTRL, 0);

    // Part added
    /*modify_register(
        spi, MCP_RXB0CTRL, MCP_RXB_RX_MASK | MCP_RXB_BUKT_MASK, MCP_RXB_RX_ANY | MCP_RXB_BUKT_MASK);

    modify_register(spi, MCP_RXB1CTRL, MCP_RXB_RX_MASK, MCP_RXB_RX_ANY);*/
}

// This function Works to set the Clock and Bitrate of the MCP2515
void mcp_set_bitrate(FuriHalSpiBusHandle* spi, MCP_BITRATE bitrate, MCP_CLOCK clk) {
    uint8_t cfg1 = 0, cfg2 = 0, cfg3 = 0;

    switch(clk) {
    case MCP_8MHZ:
        switch(bitrate) {
        case MCP_125KBPS:
            cfg1 = MCP_8MHz_125kBPS_CFG1;
            cfg2 = MCP_8MHz_125kBPS_CFG2;
            cfg3 = MCP_8MHz_125kBPS_CFG3;
            break;
        case MCP_250KBPS:
            cfg1 = MCP_8MHz_250kBPS_CFG1;
            cfg2 = MCP_8MHz_250kBPS_CFG2;
            cfg3 = MCP_8MHz_250kBPS_CFG3;
            break;
        case MCP_500KBPS:
            cfg1 = MCP_8MHz_500kBPS_CFG1;
            cfg2 = MCP_8MHz_500kBPS_CFG2;
            cfg3 = MCP_8MHz_500kBPS_CFG3;
            break;
        case MCP_1000KBPS:
            cfg1 = MCP_8MHz_1000kBPS_CFG1;
            cfg2 = MCP_8MHz_1000kBPS_CFG2;
            cfg3 = MCP_8MHz_1000kBPS_CFG3;
            break;
        }
        break;
    case MCP_16MHZ:
        switch(bitrate) {
        case MCP_125KBPS:
            cfg1 = MCP_16MHz_125kBPS_CFG1;
            cfg2 = MCP_16MHz_125kBPS_CFG2;
            cfg3 = MCP_16MHz_125kBPS_CFG3;
            break;
        case MCP_250KBPS:
            cfg1 = MCP_16MHz_250kBPS_CFG1;
            cfg2 = MCP_16MHz_250kBPS_CFG2;
            cfg3 = MCP_16MHz_250kBPS_CFG3;
            break;
        case MCP_500KBPS:
            cfg1 = MCP_16MHz_500kBPS_CFG1;
            cfg2 = MCP_16MHz_500kBPS_CFG2;
            cfg3 = MCP_16MHz_500kBPS_CFG3;
            break;
        case MCP_1000KBPS:
            cfg1 = MCP_16MHz_1000kBPS_CFG1;
            cfg2 = MCP_16MHz_1000kBPS_CFG2;
            cfg3 = MCP_16MHz_1000kBPS_CFG3;
            break;
        }
        break;

    case MCP_20MHZ:
        switch(bitrate) {
        case MCP_125KBPS:
            cfg1 = MCP_20MHz_125kBPS_CFG1;
            cfg2 = MCP_20MHz_125kBPS_CFG2;
            cfg3 = MCP_20MHz_125kBPS_CFG3;
            break;
        case MCP_250KBPS:
            cfg1 = MCP_20MHz_250kBPS_CFG1;
            cfg2 = MCP_20MHz_250kBPS_CFG2;
            cfg3 = MCP_20MHz_250kBPS_CFG3;
            break;
        case MCP_500KBPS:
            cfg1 = MCP_20MHz_500kBPS_CFG1;
            cfg2 = MCP_20MHz_500kBPS_CFG2;
            cfg3 = MCP_20MHz_500kBPS_CFG3;
            break;
        case MCP_1000KBPS:
            cfg1 = MCP_20MHz_1000kBPS_CFG1;
            cfg2 = MCP_20MHz_1000kBPS_CFG2;
            cfg3 = MCP_20MHz_1000kBPS_CFG3;
            break;
        }
        break;
    }

    set_register(spi, MCP_CNF1, cfg1);
    set_register(spi, MCP_CNF2, cfg2);
    set_register(spi, MCP_CNF3, cfg3);
}

// To set a Mask
void init_mask(MCP2515* mcp_can, uint8_t num_mask, uint32_t mask) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;

    uint8_t ext = 0;

    set_config_mode(mcp_can);

    if(num_mask > 1) return;

    if(mask > 0x7FF) ext = 1;

    if(num_mask == 0) {
        write_mf(spi, MCP_RXM0SIDH, ext, mask);
    }

    if(num_mask == 1) {
        write_mf(spi, MCP_RXM1SIDH, ext, mask);
    }

    set_new_mode(mcp_can, mcp_can->mode);
}

// To set a Filter
void init_filter(MCP2515* mcp_can, uint8_t num_filter, uint32_t filter) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;

    uint8_t ext = 0;

    set_config_mode(mcp_can);

    if(num_filter > 6) return;

    if(filter > 0x7FF) ext = 1;

    switch(num_filter) {
    case 0:
        write_mf(spi, MCP_RXF0SIDH, ext, filter);
        break;
    case 1:
        write_mf(spi, MCP_RXF1SIDH, ext, filter);
        break;

    case 2:
        write_mf(spi, MCP_RXF2SIDH, ext, filter);
        break;

    case 3:
        write_mf(spi, MCP_RXF3SIDH, ext, filter);
        break;

    case 4:
        write_mf(spi, MCP_RXF4SIDH, ext, filter);
        break;

    case 5:
        write_mf(spi, MCP_RXF5SIDH, ext, filter);
        break;

    default:
        break;
    }

    set_new_mode(mcp_can, mcp_can->mode);
}

// This function works to know if there is any message waiting
uint8_t read_rx_tx_status(FuriHalSpiBusHandle* spi) {
    uint8_t ret = 0;

    mcp_get_status(spi, &ret);

    ret &= (MCP_STAT_TXIF_MASK | MCP_STAT_RXIF_MASK);

    ret = (ret & MCP_STAT_TX0IF ? MCP_TX0IF : 0) | (ret & MCP_STAT_TX0IF ? MCP_TX1IF : 0) |
          (ret & MCP_STAT_TX0IF ? MCP_TX2IF : 0) | (ret & MCP_STAT_RXIF_MASK);

    return ret;
}

// The function to read the message
void read_frame(FuriHalSpiBusHandle* spi, CANFRAME* frame, uint8_t read_instruction) {
    uint8_t data[4];
    uint8_t data_ctrl = 0;

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, &read_instruction, 1, TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, data, sizeof(data), TIMEOUT);

    uint32_t id = (data[MCP_SIDH] << 3) + (data[MCP_SIDL] >> 5);
    uint8_t ext = 0;

    if((data[MCP_SIDL] & MCP_TXB_EXIDE_M) == MCP_TXB_EXIDE_M) {
        id = (id << 2) + (data[MCP_SIDL] & 0x03);
        id = (id << 8) + data[MCP_EID8];
        id = (id << 8) + data[MCP_EID0];
        ext = 1;
    }

    frame->canId = id;
    frame->ext = ext;

    furi_hal_spi_bus_rx(spi, &data_ctrl, 1, TIMEOUT);

    frame->data_lenght = data_ctrl & MCP_DLC_MASK;
    frame->req = (data_ctrl & MCP_RTR_MASK) ? 1 : 0;

    for(uint8_t i = 0; i < frame->data_lenght; i++) {
        furi_hal_spi_bus_rx(spi, &frame->buffer[i], 1, TIMEOUT);
    }

    furi_hal_spi_release(spi);
}

// This function Works to get the Can message
ERROR_CAN read_can_message(MCP2515* mcp_can, CANFRAME* frame) {
    ERROR_CAN ret = ERROR_OK;
    FuriHalSpiBusHandle* spi = mcp_can->spi;

    uint8_t status = 0;

    mcp_get_status(spi, &status);

    if(status & MCP_RX0IF) {
        read_frame(spi, frame, INSTRUCTION_READ_RX0);
        modify_register(spi, MCP_CANINTF, MCP_RX0IF, 0);
    } else if(status & MCP_RX1IF) {
        read_frame(spi, frame, INSTRUCTION_READ_RX1);
        modify_register(spi, MCP_CANINTF, MCP_RX1IF, 0);
    } else
        ret = ERROR_NOMSG;
    return ret;
}

// This function return the error in the can bus network
uint8_t get_error(MCP2515* mcp_can) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;
    uint8_t err = 0;
    read_register(spi, MCP_EFLG, &err);
    modify_register(spi, MCP_EFLG, MCP_EFLG_RX0OVR, 0);
    modify_register(spi, MCP_EFLG, MCP_EFLG_RX1OVR, 0);
    return err;
}

// This function works to check if there is an error in the CANBUS network
ERROR_CAN check_error(MCP2515* mcp_can) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;

    uint8_t eflg = 0;

    read_register(spi, MCP_EFLG, &eflg);

    if(eflg & MCP_EFLG_ERRORMASK) {
        return ERROR_FAIL;
    } else {
        return ERROR_OK;
    }
}

// This function works to get
ERROR_CAN check_receive(MCP2515* mcp_can) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;

    uint8_t status = read_rx_tx_status(spi);

    if(status & MCP_RX0IF) {
        return ERROR_OK;
    }
    if(status & MCP_RX1IF) {
        return ERROR_OK;
    }

    return ERROR_NOMSG;
}

// write the id in the tx register
void write_id(FuriHalSpiBusHandle* spi, uint8_t address, CANFRAME* frame) {
    uint32_t can_id = frame->canId;

    if(can_id > (0x7FF)) frame->ext = 1;

    uint8_t extension = frame->ext;
    uint16_t canid;
    uint8_t tbufdata[4];

    canid = (uint16_t)(can_id & 0x0FFFF);

    if(extension == 1) {
        tbufdata[MCP_EID0] = (uint8_t)(canid & 0xFF);
        tbufdata[MCP_EID8] = (uint8_t)(canid >> 8);
        canid = (uint16_t)(can_id >> 16);
        tbufdata[MCP_SIDL] = (uint8_t)(canid & 0x03);
        tbufdata[MCP_SIDL] += (uint8_t)((canid & 0x1C) << 3);
        tbufdata[MCP_SIDL] |= MCP_TXB_EXIDE_M;
        tbufdata[MCP_SIDH] = (uint8_t)(canid >> 5);
    } else {
        tbufdata[MCP_SIDH] = (uint8_t)(canid >> 3);
        tbufdata[MCP_SIDL] = (uint8_t)((canid & 0x07) << 5);
        tbufdata[MCP_EID0] = 0;
        tbufdata[MCP_EID8] = 0;
    }

    uint8_t instruction[] = {INSTRUCTION_WRITE, address};

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);

    for(uint8_t i = 0; i < 4; i++) {
        furi_hal_spi_bus_tx(spi, &tbufdata[i], 1, TIMEOUT_SPI);
    }

    furi_hal_spi_release(spi);
}

// write the data lenght in it respective register
void write_dlc_register(FuriHalSpiBusHandle* spi, uint8_t address, CANFRAME* frame) {
    uint8_t data_lenght = frame->data_lenght;
    uint8_t request = frame->req;

    if(request == 1) data_lenght |= MCP_RTR_MASK;
    set_register(spi, address + 4, data_lenght);
}

// write data in the registers
void write_buffer(FuriHalSpiBusHandle* spi, uint8_t address, CANFRAME* frame) {
    uint8_t data_lenght = frame->data_lenght;

    address = address + 5;

    uint8_t instruction[] = {INSTRUCTION_WRITE, address};

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);

    for(uint8_t i = 0; i < data_lenght; i++) {
        furi_hal_spi_bus_tx(spi, &frame->buffer[i], 1, TIMEOUT_SPI);
    }

    furi_hal_spi_release(spi);
}

uint8_t get_free_buffer(FuriHalSpiBusHandle* spi) {
    static uint8_t status = 0;
    uint8_t instruction = INSTRUCTION_READ_STATUS;

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, &instruction, 1, TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, &status, 1, TIMEOUT_SPI);
    furi_hal_spi_release(spi);

    uint8_t status_TX0 = status & MCP_STAT_TX0_PENDING;
    uint8_t status_TX1 = status & MCP_STAT_TX1_PENDING;
    uint8_t status_TX2 = status & MCP_STAT_TX2_PENDING;

    if(!status_TX0) return MCP_TXB0CTRL;
    if(!status_TX1) return MCP_TXB1CTRL;
    if(!status_TX2) return MCP_TXB2CTRL;

    return 0xFF;
}

// send can message
ERROR_CAN send_can_message(FuriHalSpiBusHandle* spi, CANFRAME* frame, uint8_t tx_buffer) {
    static CANFRAME auxiliar_frame;
    memset(&auxiliar_frame, 0, sizeof(CANFRAME));
    auxiliar_frame.canId = frame->canId;
    auxiliar_frame.data_lenght = frame->data_lenght;
    auxiliar_frame.ext = frame->ext;
    auxiliar_frame.req = frame->req;

    for(uint8_t i = 0; i < auxiliar_frame.data_lenght; i++) {
        auxiliar_frame.buffer[i] = frame->buffer[i];
    }

    ERROR_CAN res = ERROR_FAILTX;
    uint8_t is_send_it = 0;
    uint8_t free_buffer = tx_buffer + 1;
    uint32_t time_waiting = furi_get_tick();

    write_id(spi, free_buffer, &auxiliar_frame);

    write_dlc_register(spi, free_buffer, &auxiliar_frame);

    write_buffer(spi, free_buffer, &auxiliar_frame);

    modify_register(spi, tx_buffer, MCP_TXB_TXREQ_M, MCP_TXB_TXREQ_M);

    time_waiting = furi_get_tick();

    uint8_t instruction[] = {INSTRUCTION_READ, free_buffer - 1};

    do {
        furi_hal_spi_acquire(spi);

        furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);
        furi_hal_spi_bus_rx(spi, &is_send_it, 1, TIMEOUT_SPI);

        furi_hal_spi_release(spi);

        furi_delay_us(1);
        if(is_send_it == 0) res = ERROR_OK;

    } while((res != ERROR_OK) && ((furi_get_tick() - time_waiting) < 5));

    if(is_send_it) return res;

    return ERROR_OK;
}

// This function is to sent a can message
ERROR_CAN send_can_frame(MCP2515* mcp_can, CANFRAME* frame) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;

    uint8_t free_buffer = get_free_buffer(spi);

    if(free_buffer == 0xFF) return ERROR_ALLTXBUSY;

    return send_can_message(spi, frame, free_buffer);
}

uint8_t read_detection_baudrate(FuriHalSpiBusHandle* spi) {
    uint8_t data_canintf = 0;

    uint8_t instruction[] = {INSTRUCTION_READ, MCP_CANINTF};
    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, &data_canintf, 1, TIMEOUT_SPI);
    furi_hal_spi_release(spi);

    return (data_canintf & 0xf0);
}

// Function to detect the baudrate
ERROR_CAN is_this_bitrate(MCP2515* mcp_can, MCP_BITRATE bitrate) {
    FuriHalSpiBusHandle* spi = mcp_can->spi;
    ERROR_CAN ret = ERROR_OK;

    set_config_mode(mcp_can);

    mcp_set_bitrate(spi, bitrate, mcp_can->clck);

    set_listen_only_mode(mcp_can);

    if(check_receive(mcp_can) == ERROR_NOMSG) return ERROR_NOMSG;

    uint8_t data_canintf = 0;

    uint8_t instruction[] = {INSTRUCTION_READ, MCP_CANINTF};
    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, instruction, sizeof(instruction), TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, &data_canintf, 1, TIMEOUT_SPI);
    furi_hal_spi_release(spi);

    data_canintf &= 0x80;

    if(data_canintf == 0x80) ret = ERROR_FAIL;

    set_register(spi, MCP_CANINTF, 0);

    return ret;
}

// This function works to alloc the struct
MCP2515* mcp_alloc(MCP_MODE mode, MCP_CLOCK clck, MCP_BITRATE bitrate) {
    MCP2515* mcp_can = malloc(sizeof(MCP2515));
    mcp_can->spi = spi_alloc();
    mcp_can->mode = mode;
    mcp_can->bitRate = bitrate;
    mcp_can->clck = clck;
    return mcp_can;
}

// To deinit
void deinit_mcp2515(MCP2515* mcp_can) {
    mcp_reset(mcp_can->spi);
    furi_hal_spi_bus_handle_deinit(mcp_can->spi);
}

// free instance
void free_mcp2515(MCP2515* mcp_can) {
    free(mcp_can->spi);
    free(mcp_can);
}

// This function starts the SPI communication and set the MCP2515 device
ERROR_CAN mcp2515_start(MCP2515* mcp_can) {
    furi_hal_spi_bus_handle_init(mcp_can->spi);

    bool ret = true;

    mcp_reset(mcp_can->spi);

    set_new_mode(mcp_can, MODE_CONFIG);

    mcp_set_bitrate(mcp_can->spi, mcp_can->bitRate, mcp_can->clck);

    init_can_buffer(mcp_can->spi);

    set_registers_init(mcp_can->spi);

    ret = set_new_mode(mcp_can, mcp_can->mode);
    if(!ret) return ERROR_FAILINIT;

    return ERROR_OK;
}

// Init the mcp2515 device
ERROR_CAN mcp2515_init(MCP2515* mcp_can) {
    return mcp2515_start(mcp_can);
}
