/*****************************************************************************
 * @file    ble_gatt_aci.h
 * @brief   STM32WB BLE API (gatt_aci)
 *          Auto-generated file: do not edit!
 *****************************************************************************
 * @attention
 *
 * Copyright (c) 2018-2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 *****************************************************************************
 */

#ifndef BLE_GATT_ACI_H__
#define BLE_GATT_ACI_H__

//#include "ble_types.h"
#include <ble/core/auto/ble_types.h>

/**
 * @brief ACI_GATT_INIT
 * Initializes the GATT layer for server and client roles. It also adds the
 * GATT service with Service Changed Characteristic.
 * Until this command is issued the GATT channel does not process any commands
 * even if the connection is opened. This command has to be given before using
 * any of the GAP features.
 * 
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_init(void);

/**
 * @brief ACI_GATT_ADD_SERVICE
 * Add a service to GATT Server. When a service is created in the server, the
 * host needs to reserve the handle ranges for this service using
 * Max_Attribute_Records parameter. This parameter specifies the maximum number
 * of attribute records that can be added to this service (including the
 * service attribute, include attribute, characteristic attribute,
 * characteristic value attribute and characteristic descriptor attribute).
 * Handle of the created service is returned in command complete event. Service
 * declaration is taken from the service pool.
 * The attributes for characteristics and descriptors are allocated from the
 * attribute pool.
 * 
 * @param Service_UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128
 *        bits UUID
 * @param Service_UUID See @ref Service_UUID_t
 * @param Service_Type Service type.
 *        Values:
 *        - 0x01: Primary Service
 *        - 0x02: Secondary Service
 * @param Max_Attribute_Records Maximum number of attribute records that can be
 *        added to this service
 * @param[out] Service_Handle Handle of the Service.
 *        When this service is added, a handle is allocated by the server for
 *        this service.
 *        Server also allocates a range of handles for this service from
 *        serviceHandle to <serviceHandle + max_attr_records - 1>
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_add_service(
    uint8_t Service_UUID_Type,
    const Service_UUID_t* Service_UUID,
    uint8_t Service_Type,
    uint8_t Max_Attribute_Records,
    uint16_t* Service_Handle);

/**
 * @brief ACI_GATT_INCLUDE_SERVICE
 * Include a service given by Include_Start_Handle and Include_End_Handle to
 * another service given by Service_Handle. Attribute server creates an INCLUDE
 * definition attribute and return the handle of this attribute in
 * Included_handle.
 * 
 * @param Service_Handle Handle of the Service to which another service has to
 *        be included.
 * @param Include_Start_Handle Start Handle of the Service which has to be
 *        included in service
 * @param Include_End_Handle End Handle of the Service which has to be included
 *        in service
 * @param Include_UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128
 *        bits UUID
 * @param Include_UUID See @ref Include_UUID_t
 * @param[out] Include_Handle Handle of the include declaration
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_include_service(
    uint16_t Service_Handle,
    uint16_t Include_Start_Handle,
    uint16_t Include_End_Handle,
    uint8_t Include_UUID_Type,
    const Include_UUID_t* Include_UUID,
    uint16_t* Include_Handle);

/**
 * @brief ACI_GATT_ADD_CHAR
 * Adds a characteristic to a service.
 * The command returns the handle of the declaration attribute. The attribute
 * that holds the Characteristic Value is always allocated at the next handle
 * (Char_Handle + 1). The Characteristic Value is immediately followed, in
 * order, by:
 * - the Server Characteristic Configuration descriptor if CHAR_PROP_BROADCAST
 * is selected;
 * - the Client Characteristic Configuration descriptor if CHAR_PROP_NOTIFY or
 * CHAR_PROP_INDICATE properties is selected;
 * - the Characteristic Extended Properties descriptor if CHAR_PROP_EXT is
 * selected.
 * For instance, if CHAR_PROP_NOTIFY is selected but not CHAR_PROP_BROADCAST
 * nor CHAR_PROP_EXT, then the Client Characteristic Configuration attribute
 * handle is Char_Handle + 2.
 * Additional descriptors can be added to the characteristic by calling the
 * ACI_GATT_ADD_CHAR_DESC command immediately after calling this command.
 * 
 * @param Service_Handle Handle of the Service to which the characteristic will
 *        be added
 * @param Char_UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128 bits
 *        UUID
 * @param Char_UUID See @ref Char_UUID_t
 * @param Char_Value_Length Maximum length of the characteristic value.
 * @param Char_Properties Characteristic Properties (Volume 3, Part G, section
 *        3.3.1.1 of Bluetooth Core Specification)
 *        Flags:
 *        - 0x00: CHAR_PROP_NONE
 *        - 0x01: CHAR_PROP_BROADCAST (Broadcast)
 *        - 0x02: CHAR_PROP_READ (Read)
 *        - 0x04: CHAR_PROP_WRITE_WITHOUT_RESP (Write w/o resp)
 *        - 0x08: CHAR_PROP_WRITE (Write)
 *        - 0x10: CHAR_PROP_NOTIFY (Notify)
 *        - 0x20: CHAR_PROP_INDICATE (Indicate)
 *        - 0x40: CHAR_PROP_SIGNED_WRITE (Authenticated Signed Writes)
 *        - 0x80: CHAR_PROP_EXT (Extended Properties)
 * @param Security_Permissions Security permission flags.
 *        Flags:
 *        - 0x00: None
 *        - 0x01: AUTHEN_READ (Need authentication to read)
 *        - 0x02: AUTHOR_READ (Need authorization to read)
 *        - 0x04: ENCRY_READ (Need encryption to read)
 *        - 0x08: AUTHEN_WRITE (need authentication to write)
 *        - 0x10: AUTHOR_WRITE (need authorization to write)
 *        - 0x20: ENCRY_WRITE (need encryption to write)
 * @param GATT_Evt_Mask GATT event mask.
 *        Flags:
 *        - 0x00: GATT_DONT_NOTIFY_EVENTS
 *        - 0x01: GATT_NOTIFY_ATTRIBUTE_WRITE
 *        - 0x02: GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP
 *        - 0x04: GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP
 *        - 0x08: GATT_NOTIFY_NOTIFICATION_COMPLETION
 * @param Enc_Key_Size Minimum encryption key size required to read the
 *        characteristic.
 *        Values:
 *        - 0x07 ... 0x10
 * @param Is_Variable Specify if the characteristic value has a fixed length or
 *        a variable length.
 *        Values:
 *        - 0x00: Fixed length
 *        - 0x01: Variable length
 * @param[out] Char_Handle Handle of the characteristic that has been added (it
 *        is the handle of the characteristic declaration).
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_add_char(
    uint16_t Service_Handle,
    uint8_t Char_UUID_Type,
    const Char_UUID_t* Char_UUID,
    uint16_t Char_Value_Length,
    uint8_t Char_Properties,
    uint8_t Security_Permissions,
    uint8_t GATT_Evt_Mask,
    uint8_t Enc_Key_Size,
    uint8_t Is_Variable,
    uint16_t* Char_Handle);

/**
 * @brief ACI_GATT_ADD_CHAR_DESC
 * Adds a characteristic descriptor to a service.
 * Note that this command allocates the new handle for the descriptor after the
 * currently allocated handles. It is therefore advisable to call this command
 * following the call of the command ACI_GATT_ADD_CHAR which created the
 * characteristic containing this descriptor.
 * 
 * @param Service_Handle Handle of service to which the characteristic belongs
 * @param Char_Handle Handle of the characteristic to which description has to
 *        be added
 * @param Char_Desc_Uuid_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128
 *        bits UUID
 * @param Char_Desc_Uuid See @ref Char_Desc_Uuid_t
 * @param Char_Desc_Value_Max_Len The maximum length of the descriptor value
 * @param Char_Desc_Value_Length Current Length of the characteristic
 *        descriptor value
 * @param Char_Desc_Value Value of the characteristic description
 * @param Security_Permissions Security permission flags.
 *        Flags:
 *        - 0x00: None
 *        - 0x01: AUTHEN_READ (Need authentication to read)
 *        - 0x02: AUTHOR_READ (Need authorization to read)
 *        - 0x04: ENCRY_READ (Need encryption to read)
 *        - 0x08: AUTHEN_WRITE (need authentication to write)
 *        - 0x10: AUTHOR_WRITE (need authorization to write)
 *        - 0x20: ENCRY_WRITE (need encryption to write)
 * @param Access_Permissions Access permission
 *        Flags:
 *        - 0x00: None
 *        - 0x01: READ
 *        - 0x02: WRITE
 *        - 0x04: WRITE_WO_RESP
 *        - 0x08: SIGNED_WRITE
 * @param GATT_Evt_Mask GATT event mask.
 *        Flags:
 *        - 0x00: GATT_DONT_NOTIFY_EVENTS
 *        - 0x01: GATT_NOTIFY_ATTRIBUTE_WRITE
 *        - 0x02: GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP
 *        - 0x04: GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP
 * @param Enc_Key_Size Minimum encryption key size required to read the
 *        characteristic.
 *        Values:
 *        - 0x07 ... 0x10
 * @param Is_Variable Specify if the characteristic value has a fixed length or
 *        a variable length.
 *        Values:
 *        - 0x00: Fixed length
 *        - 0x01: Variable length
 * @param[out] Char_Desc_Handle Handle of the characteristic descriptor
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_add_char_desc(
    uint16_t Service_Handle,
    uint16_t Char_Handle,
    uint8_t Char_Desc_Uuid_Type,
    const Char_Desc_Uuid_t* Char_Desc_Uuid,
    uint8_t Char_Desc_Value_Max_Len,
    uint8_t Char_Desc_Value_Length,
    const uint8_t* Char_Desc_Value,
    uint8_t Security_Permissions,
    uint8_t Access_Permissions,
    uint8_t GATT_Evt_Mask,
    uint8_t Enc_Key_Size,
    uint8_t Is_Variable,
    uint16_t* Char_Desc_Handle);

/**
 * @brief ACI_GATT_UPDATE_CHAR_VALUE
 * Updates a characteristic value in a service. If notifications (or
 * indications) are enabled on that characteristic, a notification (or
 * indication) is sent to any client that has registered for notifications (or
 * indications) via the Client Characteristic Configuration.
 * Notes:
 * - The command is disallowed if it would cause the generation of an
 * indication on a bearer which is still awaiting confirmation of a previous
 * indication.
 * - The command does not execute and returns BLE_STATUS_BUSY if notifications
 * from a previous call are not completed. The application can enable and wait
 * for the event ACI_GATT_NOTIFICATION_COMPLETE_EVENT to avoid this case.
 * - The command does not execute and returns BLE_STATUS_INSUFFICIENT_RESOURCES
 * if there is no more room in the TX pool to allocate notification (or
 * indication) packets. This happens if notifications (or indications) are
 * enabled and the application calls this command at an higher rate than what
 * is allowed by the link. Throughput on BLE link depends on connection
 * interval and connection length parameters (decided by the Central, see
 * ACI_L2CAP_CONNECTION_PARAMETER_UPDATE_REQ for more information on how to
 * suggest new connection parameters from a Peripheral). The application can
 * wait for the event ACI_GATT_TX_POOL_AVAILABLE_EVENT before retrying a call
 * to this command. It can also retry the call until it does not return
 * BLE_STATUS_INSUFFICIENT_RESOURCES anymore.
 * - When calling this command, the characteristic value is updated only if the
 * command returns BLE_STATUS_SUCCESS or BLE_STATUS_SEC_PERMISSION_ERROR. The
 * security permission error means that at least one client has not been
 * notified due to security requirements not met.
 * 
 * @param Service_Handle Handle of service to which the characteristic belongs
 * @param Char_Handle Handle of the characteristic declaration
 * @param Val_Offset The offset from which the attribute value has to be
 *        updated.
 *        If this is set to 0 and the attribute value is of variable length,
 *        then the length of the attribute will be set to the
 *        Char_Value_Length.
 *        If the Val_Offset is set to a value greater than 0, then the length
 *        of the attribute will be set to the maximum length as specified for
 *        the attribute while adding the characteristic.
 * @param Char_Value_Length Length of the Char_Value parameter in octets.
 *        On STM32WB, this value must not exceed (BLE_CMD_MAX_PARAM_LEN - 6)
 *        i.e. 249 for BLE_CMD_MAX_PARAM_LEN default value.
 * @param Char_Value Characteristic value
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_update_char_value(
    uint16_t Service_Handle,
    uint16_t Char_Handle,
    uint8_t Val_Offset,
    uint8_t Char_Value_Length,
    const uint8_t* Char_Value);

/**
 * @brief ACI_GATT_DEL_CHAR
 * Deletes the specified characteristic from the service.
 * 
 * @param Serv_Handle Handle of service to which the characteristic belongs
 * @param Char_Handle Handle of the characteristic which has to be deleted
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_del_char(uint16_t Serv_Handle, uint16_t Char_Handle);

/**
 * @brief ACI_GATT_DEL_SERVICE
 * Deletes the specified service from the GATT server database.
 * 
 * @param Serv_Handle Handle of the service to be deleted
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_del_service(uint16_t Serv_Handle);

/**
 * @brief ACI_GATT_DEL_INCLUDE_SERVICE
 * Deletes the Include definition from the service.
 * 
 * @param Serv_Handle Handle of the service to which the include service
 *        belongs
 * @param Include_Handle Handle of the included service which has to be deleted
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_del_include_service(uint16_t Serv_Handle, uint16_t Include_Handle);

/**
 * @brief ACI_GATT_SET_EVENT_MASK
 * Masks events from the GATT. If the bit in the GATT_Evt_Mask is set to a one,
 * then the event associated with that bit will be enabled.
 * The default configuration is all the events masked.
 * 
 * @param GATT_Evt_Mask GATT/ATT event mask.
 *        Values:
 *        - 0x00000001: ACI_GATT_ATTRIBUTE_MODIFIED_EVENT
 *        - 0x00000002: ACI_GATT_PROC_TIMEOUT_EVENT
 *        - 0x00000004: ACI_ATT_EXCHANGE_MTU_RESP_EVENT
 *        - 0x00000008: ACI_ATT_FIND_INFO_RESP_EVENT
 *        - 0x00000010: ACI_ATT_FIND_BY_TYPE_VALUE_RESP_EVENT
 *        - 0x00000020: ACI_ATT_READ_BY_TYPE_RESP_EVENT
 *        - 0x00000040: ACI_ATT_READ_RESP_EVENT
 *        - 0x00000080: ACI_ATT_READ_BLOB_RESP_EVENT
 *        - 0x00000100: ACI_ATT_READ_MULTIPLE_RESP_EVENT
 *        - 0x00000200: ACI_ATT_READ_BY_GROUP_TYPE_RESP_EVENT
 *        - 0x00000800: ACI_ATT_PREPARE_WRITE_RESP_EVENT
 *        - 0x00001000: ACI_ATT_EXEC_WRITE_RESP_EVENT
 *        - 0x00002000: ACI_GATT_INDICATION_EVENT
 *        - 0x00004000: ACI_GATT_NOTIFICATION_EVENT
 *        - 0x00008000: ACI_GATT_ERROR_RESP_EVENT
 *        - 0x00010000: ACI_GATT_PROC_COMPLETE_EVENT
 *        - 0x00020000: ACI_GATT_DISC_READ_CHAR_BY_UUID_RESP_EVENT
 *        - 0x00040000: ACI_GATT_TX_POOL_AVAILABLE_EVENT
 *        - 0x00100000: ACI_GATT_READ_EXT_EVENT
 *        - 0x00200000: ACI_GATT_INDICATION_EXT_EVENT
 *        - 0x00400000: ACI_GATT_NOTIFICATION_EXT_EVENT
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_set_event_mask(uint32_t GATT_Evt_Mask);

/**
 * @brief ACI_GATT_EXCHANGE_CONFIG
 * Performs an ATT MTU exchange procedure.
 * When the ATT MTU exchange procedure is completed, a
 * ACI_ATT_EXCHANGE_MTU_RESP_EVENT event is generated. A
 * ACI_GATT_PROC_COMPLETE_EVENT event is also generated to indicate the end of
 * the procedure.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_exchange_config(uint16_t Connection_Handle);

/**
 * @brief ACI_ATT_FIND_INFO_REQ
 * Sends a Find Information Request.
 * This command is used to obtain the mapping of attribute handles with their
 * associated types. The responses of the procedure are given through the
 * ACI_ATT_FIND_INFO_RESP_EVENT event. The end of the procedure is indicated by
 * a ACI_GATT_PROC_COMPLETE_EVENT event.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @param Start_Handle First requested handle number
 * @param End_Handle Last requested handle number
 * @return Value indicating success or error code.
 */
tBleStatus
    aci_att_find_info_req(uint16_t Connection_Handle, uint16_t Start_Handle, uint16_t End_Handle);

/**
 * @brief ACI_ATT_FIND_BY_TYPE_VALUE_REQ
 * Sends a Find By Type Value Request
 * The Find By Type Value Request is used to obtain the handles of attributes
 * that have a given 16-bit UUID attribute type and a given attribute value.
 * The responses of the procedure are given through the
 * ACI_ATT_FIND_BY_TYPE_VALUE_RESP_EVENT event.
 * The end of the procedure is indicated by a ACI_GATT_PROC_COMPLETE_EVENT
 * event.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @param Start_Handle First requested handle number
 * @param End_Handle Last requested handle number
 * @param UUID 2 octet UUID to find (little-endian)
 * @param Attribute_Val_Length Length of attribute value (maximum value is
 *        ATT_MTU - 7).
 * @param Attribute_Val Attribute value to find
 * @return Value indicating success or error code.
 */
tBleStatus aci_att_find_by_type_value_req(
    uint16_t Connection_Handle,
    uint16_t Start_Handle,
    uint16_t End_Handle,
    uint16_t UUID,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_ATT_READ_BY_TYPE_REQ
 * Sends a Read By Type Request.
 * The Read By Type Request is used to obtain the values of attributes where
 * the attribute type is known but the handle is not known.
 * The responses are given through the ACI_ATT_READ_BY_TYPE_RESP_EVENT event.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @param Start_Handle First requested handle number
 * @param End_Handle Last requested handle number
 * @param UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128 bits UUID
 * @param UUID See @ref UUID_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_att_read_by_type_req(
    uint16_t Connection_Handle,
    uint16_t Start_Handle,
    uint16_t End_Handle,
    uint8_t UUID_Type,
    const UUID_t* UUID);

/**
 * @brief ACI_ATT_READ_BY_GROUP_TYPE_REQ
 * Sends a Read By Group Type Request.
 * The Read By Group Type Request is used to obtain the values of grouping
 * attributes where the attribute type is known but the handle is not known.
 * Grouping attributes are defined at GATT layer. The grouping attribute types
 * are: "Primary Service", "Secondary Service" and "Characteristic".
 * The responses of the procedure are given through the
 * ACI_ATT_READ_BY_GROUP_TYPE_RESP_EVENT event.
 * The end of the procedure is indicated by a ACI_GATT_PROC_COMPLETE_EVENT.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @param Start_Handle First requested handle number
 * @param End_Handle Last requested handle number
 * @param UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128 bits UUID
 * @param UUID See @ref UUID_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_att_read_by_group_type_req(
    uint16_t Connection_Handle,
    uint16_t Start_Handle,
    uint16_t End_Handle,
    uint8_t UUID_Type,
    const UUID_t* UUID);

/**
 * @brief ACI_ATT_PREPARE_WRITE_REQ
 * Sends a Prepare Write Request.
 * The Prepare Write Request is used to request the server to prepare to write
 * the value of an attribute.
 * The responses of the procedure are given through the
 * ACI_ATT_PREPARE_WRITE_RESP_EVENT event.
 * The end of the procedure is indicated by a ACI_GATT_PROC_COMPLETE_EVENT.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @param Attr_Handle Handle of the attribute to be written
 * @param Val_Offset The offset of the first octet to be written
 * @param Attribute_Val_Length Length of attribute value (maximum value is
 *        ATT_MTU - 5).
 * @param Attribute_Val The value of the attribute to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_att_prepare_write_req(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint16_t Val_Offset,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_ATT_EXECUTE_WRITE_REQ
 * Sends an Execute Write Request.
 * The Execute Write Request is used to request the server to write or cancel
 * the write of all the prepared values currently held in the prepare queue
 * from this client.
 * The result of the procedure is given through the
 * ACI_ATT_EXEC_WRITE_RESP_EVENT event.
 * The end of the procedure is indicated by a ACI_GATT_PROC_COMPLETE_EVENT
 * event.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @param Execute Execute or cancel writes.
 *        Values:
 *        - 0x00: Cancel all prepared writes
 *        - 0x01: Immediately write all pending prepared values
 * @return Value indicating success or error code.
 */
tBleStatus aci_att_execute_write_req(uint16_t Connection_Handle, uint8_t Execute);

/**
 * @brief ACI_GATT_DISC_ALL_PRIMARY_SERVICES
 * Starts the GATT client procedure to discover all primary services on the
 * server.
 * The responses of the procedure are given through the
 * ACI_ATT_READ_BY_GROUP_TYPE_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_disc_all_primary_services(uint16_t Connection_Handle);

/**
 * @brief ACI_GATT_DISC_PRIMARY_SERVICE_BY_UUID
 * Starts the procedure to discover the primary services of the specified UUID
 * on the server.
 * The responses of the procedure are given through the
 * ACI_ATT_FIND_BY_TYPE_VALUE_RESP_EVENT event.
 * The end of the procedure is indicated by a ACI_GATT_PROC_COMPLETE_EVENT
 * event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128 bits UUID
 * @param UUID See @ref UUID_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_disc_primary_service_by_uuid(
    uint16_t Connection_Handle,
    uint8_t UUID_Type,
    const UUID_t* UUID);

/**
 * @brief ACI_GATT_FIND_INCLUDED_SERVICES
 * Starts the procedure to find all included services.
 * The responses of the procedure are given through the
 * ACI_ATT_READ_BY_TYPE_RESP_EVENT event.
 * The end of the procedure is indicated by a ACI_GATT_PROC_COMPLETE_EVENT
 * event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Start_Handle Start attribute handle of the service
 * @param End_Handle End attribute handle of the service
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_find_included_services(
    uint16_t Connection_Handle,
    uint16_t Start_Handle,
    uint16_t End_Handle);

/**
 * @brief ACI_GATT_DISC_ALL_CHAR_OF_SERVICE
 * Starts the procedure to discover all the characteristics of a given service.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packets are given
 * through ACI_ATT_READ_BY_TYPE_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Start_Handle Start attribute handle of the service
 * @param End_Handle End attribute handle of the service
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_disc_all_char_of_service(
    uint16_t Connection_Handle,
    uint16_t Start_Handle,
    uint16_t End_Handle);

/**
 * @brief ACI_GATT_DISC_CHAR_BY_UUID
 * Starts the procedure to discover all the characteristics specified by a
 * UUID.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packets are given
 * through ACI_GATT_DISC_READ_CHAR_BY_UUID_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Start_Handle Start attribute handle of the service
 * @param End_Handle End attribute handle of the service
 * @param UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128 bits UUID
 * @param UUID See @ref UUID_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_disc_char_by_uuid(
    uint16_t Connection_Handle,
    uint16_t Start_Handle,
    uint16_t End_Handle,
    uint8_t UUID_Type,
    const UUID_t* UUID);

/**
 * @brief ACI_GATT_DISC_ALL_CHAR_DESC
 * Starts the procedure to discover all characteristic descriptors on the
 * server.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packets are given
 * through ACI_ATT_FIND_INFO_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Char_Handle Handle of the characteristic value
 * @param End_Handle End handle of the characteristic
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_disc_all_char_desc(
    uint16_t Connection_Handle,
    uint16_t Char_Handle,
    uint16_t End_Handle);

/**
 * @brief ACI_GATT_READ_CHAR_VALUE
 * Starts the procedure to read the attribute value.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packet is given through
 * ACI_ATT_READ_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the characteristic value to be read
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_char_value(uint16_t Connection_Handle, uint16_t Attr_Handle);

/**
 * @brief ACI_GATT_READ_USING_CHAR_UUID
 * This command sends a Read By Type Request packet to the server in order to
 * read the value attribute of the characteristics specified by the UUID.
 * When the procedure is completed, an ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion, the response packet is given through
 * one ACI_GATT_DISC_READ_CHAR_BY_UUID_RESP_EVENT event per reported attribute.
 * Note: the number of bytes of a value reported by
 * ACI_GATT_DISC_READ_CHAR_BY_UUID_RESP_EVENT event cannot exceed
 * BLE_EVT_MAX_PARAM_LEN - 7 i.e. 248 bytes for default value of
 * BLE_EVT_MAX_PARAM_LEN.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Start_Handle Starting handle of the range to be searched
 * @param End_Handle End handle of the range to be searched
 * @param UUID_Type UUID type: 0x01 = 16 bits UUID while 0x02 = 128 bits UUID
 * @param UUID See @ref UUID_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_using_char_uuid(
    uint16_t Connection_Handle,
    uint16_t Start_Handle,
    uint16_t End_Handle,
    uint8_t UUID_Type,
    const UUID_t* UUID);

/**
 * @brief ACI_GATT_READ_LONG_CHAR_VALUE
 * Starts the procedure to read a long characteristic value.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packets are given
 * through ACI_ATT_READ_BLOB_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the characteristic value to be read
 * @param Val_Offset Offset from which the value needs to be read
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_long_char_value(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint16_t Val_Offset);

/**
 * @brief ACI_GATT_READ_MULTIPLE_CHAR_VALUE
 * Starts a procedure to read multiple characteristic values from a server.
 * The command must specify the handles of the characteristic values to be
 * read.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packets are given
 * through ACI_ATT_READ_MULTIPLE_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Number_of_Handles Number of handles in the following table
 *        Values:
 *        - 0x02 ... 0x7E
 * @param Handle_Entry See @ref Handle_Entry_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_multiple_char_value(
    uint16_t Connection_Handle,
    uint8_t Number_of_Handles,
    const Handle_Entry_t* Handle_Entry);

/**
 * @brief ACI_GATT_WRITE_CHAR_VALUE
 * Starts the procedure to write a characteristic value.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated.
 * The length of the value to be written must not exceed (ATT_MTU - 3). On
 * STM32WB, it must also not exceed (BLE_CMD_MAX_PARAM_LEN - 5) i.e. 250 for
 * BLE_CMD_MAX_PARAM_LEN default value.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the characteristic value to be written
 * @param Attribute_Val_Length Length of the value to be written
 * @param Attribute_Val Value to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_write_char_value(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_WRITE_LONG_CHAR_VALUE
 * Starts the procedure to write a long characteristic value.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. During the procedure, ACI_ATT_PREPARE_WRITE_RESP_EVENT and
 * ACI_ATT_EXEC_WRITE_RESP_EVENT events are raised.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the characteristic value to be written
 * @param Val_Offset Offset at which the attribute has to be written
 * @param Attribute_Val_Length Length of the value to be written.
 *        On STM32WB, this value must not exceed (BLE_CMD_MAX_PARAM_LEN - 7)
 *        i.e. 248 for BLE_CMD_MAX_PARAM_LEN default value.
 * @param Attribute_Val Value to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_write_long_char_value(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint16_t Val_Offset,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_WRITE_CHAR_RELIABLE
 * Starts the procedure to write a characteristic reliably.
 * When the procedure is completed, a  ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. During the procedure, ACI_ATT_PREPARE_WRITE_RESP_EVENT and
 * ACI_ATT_EXEC_WRITE_RESP_EVENT events are raised.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the attribute to be written
 * @param Val_Offset Offset at which the attribute has to be written
 * @param Attribute_Val_Length Length of the value to be written.
 *        On STM32WB, this value must not exceed (BLE_CMD_MAX_PARAM_LEN - 7)
 *        i.e. 248 for BLE_CMD_MAX_PARAM_LEN default value.
 * @param Attribute_Val Value to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_write_char_reliable(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint16_t Val_Offset,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_WRITE_LONG_CHAR_DESC
 * Starts the procedure to write a long characteristic descriptor.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. During the procedure, ACI_ATT_PREPARE_WRITE_RESP_EVENT and
 * ACI_ATT_EXEC_WRITE_RESP_EVENT events are raised.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the attribute to be written
 * @param Val_Offset Offset at which the attribute has to be written
 * @param Attribute_Val_Length Length of the value to be written.
 *        On STM32WB, this value must not exceed (BLE_CMD_MAX_PARAM_LEN - 7)
 *        i.e. 248 for BLE_CMD_MAX_PARAM_LEN default value.
 * @param Attribute_Val Value to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_write_long_char_desc(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint16_t Val_Offset,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_READ_LONG_CHAR_DESC
 * Starts the procedure to read a long characteristic value.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packets are given
 * through ACI_ATT_READ_BLOB_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the characteristic descriptor
 * @param Val_Offset Offset from which the value needs to be read
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_long_char_desc(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint16_t Val_Offset);

/**
 * @brief ACI_GATT_WRITE_CHAR_DESC
 * Starts the procedure to write a characteristic descriptor.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated.
 * The length of the value to be written must not exceed (ATT_MTU - 3). On
 * STM32WB, it must also not exceed (BLE_CMD_MAX_PARAM_LEN - 5) i.e. 250 for
 * BLE_CMD_MAX_PARAM_LEN default value.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the attribute to be written
 * @param Attribute_Val_Length Length of the value to be written
 * @param Attribute_Val Value to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_write_char_desc(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_READ_CHAR_DESC
 * Starts the procedure to read the descriptor specified.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated.
 * Before procedure completion the response packet is given through
 * ACI_ATT_READ_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the descriptor to be read
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_char_desc(uint16_t Connection_Handle, uint16_t Attr_Handle);

/**
 * @brief ACI_GATT_WRITE_WITHOUT_RESP
 * Starts the procedure to write a characteristic value without waiting for any
 * response from the server. No events are generated after this command is
 * executed.
 * The length of the value to be written must not exceed (ATT_MTU - 3). On
 * STM32WB, it must also not exceed (BLE_CMD_MAX_PARAM_LEN - 5) i.e. 250 for
 * BLE_CMD_MAX_PARAM_LEN default value.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the characteristic value to be written
 * @param Attribute_Val_Length Length of the value to be written
 * @param Attribute_Val Value to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_write_without_resp(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_SIGNED_WRITE_WITHOUT_RESP
 * Starts a signed write without response from the server.
 * The procedure is used to write a characteristic value with an authentication
 * signature without waiting for any response from the server. It cannot be
 * used when the link is encrypted.
 * The length of the value to be written must not exceed (ATT_MTU - 15). On
 * STM32WB, it must also not exceed (BLE_CMD_MAX_PARAM_LEN - 5) i.e. 250 for
 * BLE_CMD_MAX_PARAM_LEN default value.
 * 
 * @param Connection_Handle Connection handle for which the command applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF
 * @param Attr_Handle Handle of the characteristic value to be written
 * @param Attribute_Val_Length Length of the value to be written
 * @param Attribute_Val Value to be written
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_signed_write_without_resp(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_CONFIRM_INDICATION
 * Allow application to confirm indication. This command has to be sent when
 * the application receives the event ACI_GATT_INDICATION_EVENT.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_confirm_indication(uint16_t Connection_Handle);

/**
 * @brief ACI_GATT_WRITE_RESP
 * Allow or reject a write request from a client.
 * This command has to be sent by the application when it receives the
 * ACI_GATT_WRITE_PERMIT_REQ_EVENT. If the write can be allowed, then the
 * status and error code have to be set to 0. If the write cannot be allowed,
 * then the status has to be set to 1 and the error code has to be set to the
 * error code that has to be passed to the client.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Attr_Handle Handle of the attribute that was passed in the event
 *        ACI_GATT_WRITE_PERMIT_REQ_EVENT
 * @param Write_status If the value can be written or not.
 *        Values:
 *        - 0x00: The value can be written to the attribute specified by
 *          attr_handle
 *        - 0x01: The value cannot be written to the attribute specified by the
 *          attr_handle
 * @param Error_Code The error code that has to be passed to the client in case
 *        the write has to be rejected
 * @param Attribute_Val_Length Length of the value to be written as passed in
 *        the event ACI_GATT_WRITE_PERMIT_REQ_EVENT
 * @param Attribute_Val Value as passed in the event
 *        ACI_GATT_WRITE_PERMIT_REQ_EVENT
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_write_resp(
    uint16_t Connection_Handle,
    uint16_t Attr_Handle,
    uint8_t Write_status,
    uint8_t Error_Code,
    uint8_t Attribute_Val_Length,
    const uint8_t* Attribute_Val);

/**
 * @brief ACI_GATT_ALLOW_READ
 * Allow the GATT server to send a response to a read request from a client.
 * The application has to send this command when it receives the
 * ACI_GATT_READ_PERMIT_REQ_EVENT or ACI_GATT_READ_MULTI_PERMIT_REQ_EVENT. This
 * command indicates to the stack that the response can be sent to the client.
 * So if the application wishes to update any of the attributes before they are
 * read by the client, it must update the characteristic values using the
 * ACI_GATT_UPDATE_CHAR_VALUE and then give this command. The application
 * should perform the required operations within 30 seconds. Otherwise the GATT
 * procedure will be timeout.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_allow_read(uint16_t Connection_Handle);

/**
 * @brief ACI_GATT_SET_SECURITY_PERMISSION
 * This command sets the security permission for the attribute handle
 * specified. It is allowed only for the Client Characteristic Configuration
 * descriptor.
 * 
 * @param Serv_Handle Handle of the service which contains the attribute whose
 *        security permission has to be modified
 * @param Attr_Handle Handle of the attribute whose security permission has to
 *        be modified
 * @param Security_Permissions Security permission flags.
 *        Flags:
 *        - 0x00: None
 *        - 0x01: AUTHEN_READ (Need authentication to read)
 *        - 0x02: AUTHOR_READ (Need authorization to read)
 *        - 0x04: ENCRY_READ (Need encryption to read)
 *        - 0x08: AUTHEN_WRITE (need authentication to write)
 *        - 0x10: AUTHOR_WRITE (need authorization to write)
 *        - 0x20: ENCRY_WRITE (need encryption to write)
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_set_security_permission(
    uint16_t Serv_Handle,
    uint16_t Attr_Handle,
    uint8_t Security_Permissions);

/**
 * @brief ACI_GATT_SET_DESC_VALUE
 * This command sets the value of the descriptor specified by Char_Desc_Handle.
 * 
 * @param Serv_Handle Handle of the service which contains the characteristic
 *        descriptor
 * @param Char_Handle Handle of the characteristic which contains the
 *        descriptor
 * @param Char_Desc_Handle Handle of the descriptor whose value has to be set
 * @param Val_Offset Offset from which the descriptor value has to be updated
 * @param Char_Desc_Value_Length Length of the descriptor value
 * @param Char_Desc_Value Descriptor value
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_set_desc_value(
    uint16_t Serv_Handle,
    uint16_t Char_Handle,
    uint16_t Char_Desc_Handle,
    uint16_t Val_Offset,
    uint8_t Char_Desc_Value_Length,
    const uint8_t* Char_Desc_Value);

/**
 * @brief ACI_GATT_READ_HANDLE_VALUE
 * Reads the value of the attribute handle specified from the local GATT
 * database.
 * 
 * @param Attr_Handle Handle of the attribute to read
 * @param Offset Offset from which the value needs to be read
 * @param Value_Length_Requested Maximum number of octets to be returned as
 *        attribute value
 * @param[out] Length Length of the attribute value
 * @param[out] Value_Length Length in octets of the Value parameter
 * @param[out] Value Attribute value
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_handle_value(
    uint16_t Attr_Handle,
    uint16_t Offset,
    uint16_t Value_Length_Requested,
    uint16_t* Length,
    uint16_t* Value_Length,
    uint8_t* Value);

/**
 * @brief ACI_GATT_UPDATE_CHAR_VALUE_EXT
 * This command is a more flexible version of ACI_GATT_UPDATE_CHAR_VALUE to
 * support update of long attribute up to 512 bytes and indicate selectively
 * the generation of Indication/Notification.
 * The description notes for the ACI_GATT_UPDATE_CHAR_VALUE command also apply
 * here.
 * 
 * @param Conn_Handle_To_Notify Specifies the client(s) to be notified.
 *        Values:
 *        - 0x0000: Notify all subscribed clients on their unenhanced ATT
 *          bearer
 *        - 0x0001 ... 0x0EFF: Notify one client on the specified unenhanced
 *          ATT bearer (the parameter is the connection handle)
 *        - 0xEA00 ... 0xEA3F: Notify one client on the specified enhanced ATT
 *          bearer (the LSB-byte of the parameter is the connection-oriented
 *          channel index)
 * @param Service_Handle Handle of service to which the characteristic belongs
 * @param Char_Handle Handle of the characteristic declaration
 * @param Update_Type Allow Notification or Indication generation, if enabled
 *        in the client characteristic configuration descriptor
 *        Flags:
 *        - 0x00: Do not notify
 *        - 0x01: Notification
 *        - 0x02: Indication
 * @param Char_Length Total length of the characteristic value.
 *        In case of a variable size characteristic, this field specifies the
 *        new length of the characteristic value after the update; in case of
 *        fixed length characteristic this field is ignored.
 * @param Value_Offset The offset from which the attribute value has to be
 *        updated.
 * @param Value_Length Length of the Value parameter in octets.
 *        On STM32WB, this value must not exceed (BLE_CMD_MAX_PARAM_LEN - 12)
 *        i.e. 243 for BLE_CMD_MAX_PARAM_LEN default value.
 * @param Value Updated characteristic value
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_update_char_value_ext(
    uint16_t Conn_Handle_To_Notify,
    uint16_t Service_Handle,
    uint16_t Char_Handle,
    uint8_t Update_Type,
    uint16_t Char_Length,
    uint16_t Value_Offset,
    uint8_t Value_Length,
    const uint8_t* Value);

/**
 * @brief ACI_GATT_DENY_READ
 * This command is used to deny the GATT server to send a response to a read
 * request from a client.
 * The application may send this command when it receives the
 * ACI_GATT_READ_PERMIT_REQ_EVENT or  ACI_GATT_READ_MULTI_PERMIT_REQ_EVENT.
 * This command indicates to the stack that the client is not allowed to read
 * the requested characteristic due to e.g. application restrictions.
 * The Error code shall be either 0x08 (Insufficient Authorization) or a value
 * in the range 0x80-0x9F (Application Error).
 * The application should issue the ACI_GATT_DENY_READ  or ACI_GATT_ALLOW_READ
 * command within 30 seconds from the reception of the
 * ACI_GATT_READ_PERMIT_REQ_EVENT or ACI_GATT_READ_MULTI_PERMIT_REQ_EVENT
 * events; otherwise the GATT procedure issues a timeout.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Error_Code Error code for the command
 *        Values:
 *        - 0x08: Insufficient Authorization
 *        - 0x80 ... 0x9F: Application Error
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_deny_read(uint16_t Connection_Handle, uint8_t Error_Code);

/**
 * @brief ACI_GATT_SET_ACCESS_PERMISSION
 * This command sets the access permission for the attribute handle specified.
 * 
 * @param Serv_Handle Handle of the service which contains the attribute whose
 *        access permission has to be modified
 * @param Attr_Handle Handle of the attribute whose security permission has to
 *        be modified
 * @param Access_Permissions Access permission
 *        Flags:
 *        - 0x00: None
 *        - 0x01: READ
 *        - 0x02: WRITE
 *        - 0x04: WRITE_WO_RESP
 *        - 0x08: SIGNED_WRITE
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_set_access_permission(
    uint16_t Serv_Handle,
    uint16_t Attr_Handle,
    uint8_t Access_Permissions);

/**
 * @brief ACI_GATT_STORE_DB
 * This command forces the saving of the GATT database for all active
 * connections. Note that, by default, the GATT database is saved per active
 * connection at the time of disconnection.
 * 
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_store_db(void);

/**
 * @brief ACI_GATT_SEND_MULT_NOTIFICATION
 * This command sends a Multiple Handle Value Notification over the ATT bearer
 * specified in parameter. The handles provided as parameters must be the
 * handles of the characteristic declarations.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Number_of_Handles Number of handles in the following table
 *        Values:
 *        - 0x02 ... 0x7E
 * @param Handle_Entry See @ref Handle_Entry_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_send_mult_notification(
    uint16_t Connection_Handle,
    uint8_t Number_of_Handles,
    const Handle_Entry_t* Handle_Entry);

/**
 * @brief ACI_GATT_READ_MULTIPLE_VAR_CHAR_VALUE
 * Starts a procedure to read multiple variable length characteristic values
 * from a server.
 * The command must specify the handles of the characteristic values to be
 * read.
 * When the procedure is completed, a ACI_GATT_PROC_COMPLETE_EVENT event is
 * generated. Before procedure completion the response packets are given
 * through ACI_ATT_READ_MULTIPLE_RESP_EVENT event.
 * 
 * @param Connection_Handle Specifies the ATT bearer for which the command
 *        applies.
 *        Values:
 *        - 0x0000 ... 0x0EFF: Unenhanced ATT bearer (the parameter is the
 *          connection handle)
 *        - 0xEA00 ... 0xEA3F: Enhanced ATT bearer (the LSB-byte of the
 *          parameter is the connection-oriented channel index)
 * @param Number_of_Handles Number of handles in the following table
 *        Values:
 *        - 0x02 ... 0x7E
 * @param Handle_Entry See @ref Handle_Entry_t
 * @return Value indicating success or error code.
 */
tBleStatus aci_gatt_read_multiple_var_char_value(
    uint16_t Connection_Handle,
    uint8_t Number_of_Handles,
    const Handle_Entry_t* Handle_Entry);

#endif /* BLE_GATT_ACI_H__ */
