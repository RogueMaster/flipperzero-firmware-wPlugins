#include "types.h"

const char* bitraw_label_name(BitrawLabel label) {
    switch(label) {
    case BitrawLabelNoise: return "NOISE";
    case BitrawLabelAmrMeter: return "AMR_METER";
    case BitrawLabelTpms: return "TPMS";
    case BitrawLabelAlarmSensor: return "ALARM_SENSOR";
    case BitrawLabelShutterBlind: return "SHUTTER_BLIND";
    case BitrawLabelDoorbell: return "DOORBELL";
    case BitrawLabelOutletSwitch: return "OUTLET_SWITCH";
    case BitrawLabelGarageRemote: return "GARAGE_REMOTE";
    case BitrawLabelKeyfobRemote: return "KEYFOB_REMOTE";
    case BitrawLabelWeatherStation: return "WEATHER_STATION";
    case BitrawLabelUnknownStructured: return "UNKNOWN_STRUCTURED";
    }
    return "UNKNOWN";
}

const char* bitraw_confidence_name(BitrawConfidence c) {
    switch(c) {
    case BitrawConfHigh: return "HIGH";
    case BitrawConfMedium: return "MEDIUM";
    case BitrawConfLow: return "LOW";
    }
    return "?";
}

const char* bitraw_manchester_name(ManchesterConvention c) {
    return c == ManchesterGEThomas ? "G.E.Thomas (1=high-low)" : "IEEE 802.3 (1=low-high)";
}
