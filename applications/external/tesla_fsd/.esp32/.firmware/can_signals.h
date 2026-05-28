#pragma once
/**
 * Tesla CAN signal layout constants.
 *
 * Keep CAN IDs in config.h. Keep byte positions, bit masks, shifts, mux IDs,
 * bit positions, and encoded values here so vehicle/firmware adaptations do
 * not require hunting through handler logic.
 */

// Common CAN frame layout
#define CAN_FRAME_MAX_BITS                 64
#define CAN_FRAME_MAX_DATA_LEN              8

#define CAN_MUX_BYTE                        0
#define CAN_MUX_MASK                     0x07u
#define CAN_MUX_0                           0u
#define CAN_MUX_1                           1u
#define CAN_MUX_2                           2u

// GTW_carConfig (0x398)
#define SIG_GTW_DAS_HW_BYTE                 0
#define SIG_GTW_DAS_HW_SHIFT                6
#define SIG_GTW_DAS_HW_MASK              0x03u
#define SIG_GTW_DAS_HW_LEGACY_0             0u
#define SIG_GTW_DAS_HW_LEGACY_1             1u
#define SIG_GTW_DAS_HW_HW3                  2u
#define SIG_GTW_DAS_HW_HW4                  3u

// GTW_carState (0x318)
#define SIG_GTW_UPDATE_IN_PROGRESS_BYTE     6
#define SIG_GTW_UPDATE_IN_PROGRESS_MASK  0x03u

// UI/DAS autopilot control (0x3FD / 0x3EE)
#define SIG_AP_UI_FSD_SELECTED_BYTE         4
#define SIG_AP_UI_FSD_SELECTED_SHIFT        6
#define SIG_AP_UI_FSD_SELECTED_MASK      0x01u

#define SIG_AP_FSD_ENABLE_BIT              46
#define SIG_AP_NAG_CLEAR_BIT               19
#define SIG_AP_HW4_FSD_ENABLE_BIT          60
#define SIG_AP_HW4_EMERGENCY_VEHICLE_BIT   59
#define SIG_AP_HW4_NAG_CONFIRM_BIT         47

#define SIG_AP_HW3_SPEED_RAW_BYTE           3
#define SIG_AP_HW3_SPEED_RAW_SHIFT          1
#define SIG_AP_HW3_SPEED_RAW_MASK        0x3Fu
#define SIG_AP_HW3_SPEED_RAW_ZERO          30
#define SIG_AP_HW3_SPEED_OFFSET_STEP        5
#define SIG_AP_HW3_SPEED_OFFSET_MIN         0
#define SIG_AP_HW3_SPEED_OFFSET_MAX       100

#define SIG_AP_SPEED_PROFILE_BYTE           6
#define SIG_AP_SPEED_PROFILE_MASK        0x06u
#define SIG_AP_SPEED_PROFILE_SHIFT          1
#define SIG_AP_SPEED_PROFILE_VALUE_MASK  0x03u

#define SIG_AP_HW3_SPEED_OFFSET_LOW_BYTE    0
#define SIG_AP_HW3_SPEED_OFFSET_HIGH_BYTE   1
#define SIG_AP_HW3_SPEED_OFFSET_LOW_MASK 0xC0u
#define SIG_AP_HW3_SPEED_OFFSET_HIGH_MASK 0x3Fu
#define SIG_AP_HW3_SPEED_OFFSET_LOW_VALUE_MASK 0x03u
#define SIG_AP_HW3_SPEED_OFFSET_LOW_SHIFT   6
#define SIG_AP_HW3_SPEED_OFFSET_HIGH_SHIFT  2

#define SIG_AP_HW4_SPEED_PROFILE_BYTE       7
#define SIG_AP_HW4_SPEED_PROFILE_SHIFT      5
#define SIG_AP_HW4_SPEED_PROFILE_MASK    0x07u

// Follow distance / legacy stalk
#define SIG_FOLLOW_DIST_BYTE                5
#define SIG_FOLLOW_DIST_MASK             0xE0u
#define SIG_FOLLOW_DIST_SHIFT               5
#define SIG_LEGACY_STALK_POS_BYTE           1
#define SIG_LEGACY_STALK_POS_SHIFT          5

// ISA speed limit (0x399, HW4 only)
#define SIG_ISA_SOUND_ACTIVE_BYTE           1
#define SIG_ISA_SOUND_ACTIVE_MASK        0x20u

// EPAS3P_sysStatus (0x370)
#define SIG_EPAS_HANDS_ON_BYTE              4
#define SIG_EPAS_HANDS_ON_SHIFT             6
#define SIG_EPAS_HANDS_ON_MASK           0x03u
#define SIG_EPAS_HANDS_ON_OK                1u
#define SIG_EPAS_HANDS_ON_SPOOF_VALUE    0x40u
#define SIG_EPAS_HANDS_ON_CLEAR_MASK     0xC0u

#define SIG_EPAS_COUNTER_BYTE               6
#define SIG_EPAS_COUNTER_MASK            0x0Fu
#define SIG_EPAS_COUNTER_KEEP_MASK       0xF0u

#define SIG_EPAS_TORQUE_HIGH_BYTE           2
#define SIG_EPAS_TORQUE_LOW_BYTE            3
#define SIG_EPAS_TORQUE_HIGH_KEEP_MASK   0xF0u
#define SIG_EPAS_TORQUE_HIGH_VALUE_MASK  0x0Fu
#define SIG_EPAS_TORQUE_HIGH_SHIFT          8
#define SIG_EPAS_TORQUE_LOW_MASK         0xFFu

// BMS frames
#define SIG_BMS_VOLTAGE_L_BYTE              0
#define SIG_BMS_VOLTAGE_H_BYTE              1
#define SIG_BMS_VOLTAGE_SCALE            0.01f
#define SIG_BMS_CURRENT_L_BYTE              2
#define SIG_BMS_CURRENT_H_BYTE              3
#define SIG_BMS_CURRENT_SCALE             0.1f
#define SIG_BMS_SOC_L_BYTE                  0
#define SIG_BMS_SOC_H_BYTE                  1
#define SIG_BMS_SOC_H_MASK               0x03u
#define SIG_BMS_SOC_SCALE                 0.1f
#define SIG_BMS_TEMP_MIN_BYTE               4
#define SIG_BMS_TEMP_MAX_BYTE               5
#define SIG_BMS_TEMP_OFFSET                40

// Trip planning / precondition (0x082)
#define SIG_TRIP_PLANNING_FLAGS_BYTE        0
#define SIG_TRIP_PLANNING_PRECONDITION   0x05u

// DAS autopilot config / TLSSC restore (0x331)
#define SIG_DAS_AP_CONFIG_TIER_BYTE         0
#define SIG_DAS_AP_CONFIG_KEEP_MASK      0xC0u
#define SIG_DAS_AP_CONFIG_SELF_DRIVING   0x1Bu

// DAS status
// Legacy/HW3 source: 0x399
// HW4 source:        0x39B
// byte 0: AP / Autosteer state
// bytes 1-2: speed limit / speed warning state
// bytes 5-6: hands-on / lane-change state
// bytes 6-7: counter / checksum
#define SIG_DAS_HW3_AP_STATE_BYTE           0
#define SIG_DAS_HW3_AP_STATE_MASK        0x0Fu
#define SIG_DAS_HW3_AP_ACTIVE_STATE         3u
#define SIG_DAS_HW4_AP_STATE_BYTE           1
#define SIG_DAS_HW4_AP_STATE_SHIFT          4
#define SIG_DAS_HW4_AP_STATE_MASK        0x0Fu
#define SIG_DAS_HW4_AP_ACTIVE_MIN           2u
#define SIG_DAS_SPEED_LIMIT_BYTE_1          1
#define SIG_DAS_SPEED_LIMIT_BYTE_2          2
#define SIG_DAS_HANDS_ON_STATE_BYTE         5
#define SIG_DAS_HANDS_ON_STATE_SHIFT        2
#define SIG_DAS_HANDS_ON_STATE_MASK      0x0Fu
#define SIG_DAS_LANE_CHANGE_STATE_BYTE      6
#define SIG_DAS_COUNTER_BYTE                6
#define SIG_DAS_CHECKSUM_BYTE               7
#define SIG_DAS_HANDS_ON_NOT_REQUIRED       0u
#define SIG_DAS_HANDS_ON_SUSPENDED          8u
