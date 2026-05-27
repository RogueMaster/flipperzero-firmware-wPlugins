#include "wmbus_medium.h"

const char* wmbus_medium_str(uint8_t code) {
    switch(code) {
    case 0x00: return "Other";
    case 0x01: return "Oil";
    case 0x02: return "Electricity";
    case 0x03: return "Gas";
    case 0x04: return "Heat (out)";
    case 0x05: return "Steam";
    case 0x06: return "Hot water";
    case 0x07: return "Water";
    case 0x08: return "Heat cost allocator";
    case 0x09: return "Compressed air";
    case 0x0A: return "Cooling (out)";
    case 0x0B: return "Cooling (in)";
    case 0x0C: return "Heat (in)";
    case 0x0D: return "Heat/cooling";
    case 0x0E: return "Bus/system";
    case 0x0F: return "Unknown medium";
    case 0x15: return "Hot water";
    case 0x16: return "Cold water";
    case 0x17: return "Dual water";
    case 0x18: return "Pressure";
    case 0x19: return "A/D converter";
    case 0x1A: return "Smoke detector";
    case 0x1B: return "Room sensor";
    case 0x1C: return "Gas detector";
    case 0x20: return "Breaker (electricity)";
    case 0x21: return "Valve (gas/water)";
    case 0x25: return "Customer unit (display)";
    case 0x28: return "Waste water";
    case 0x29: return "Garbage";
    case 0x37: return "Radio converter";
    /* Techem-private codes seen in EU residential buildings; not in the
     * OMS standard table but worth labelling so the medium column is
     * meaningful for the majority of frames. */
    case 0x62: return "Heat (volumetric)";
    case 0x72: return "Heat (compact)";
    case 0x80: return "Electricity";
    default:   return "Unknown";
    }
}
