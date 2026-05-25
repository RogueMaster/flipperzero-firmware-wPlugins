#include "types.h"

const char* bitraw_label_name(BitrawLabel label) {
    switch(label) {
    case BitrawLabelNoise: return "NOISE";
    case BitrawLabelAmrMeter: return "AMR_METER";
    case BitrawLabelTpms: return "TPMS";
    case BitrawLabelWmbusMeter: return "WMBUS_METER";
    case BitrawLabelHoneywell5800: return "HONEYWELL_5800";
    case BitrawLabelAlarmSensor: return "ALARM_SENSOR";
    case BitrawLabelShutterBlind: return "SHUTTER_BLIND";
    case BitrawLabelEnoceanSwitch: return "ENOCEAN_SWITCH";
    case BitrawLabelDoorbell: return "DOORBELL";
    case BitrawLabelOutletSwitch: return "OUTLET_SWITCH";
    case BitrawLabelGarageRemote: return "GARAGE_REMOTE";
    case BitrawLabelKeyfobRemote: return "KEYFOB_REMOTE";
    case BitrawLabelWeatherStation: return "WEATHER_STATION";
    case BitrawLabelLoraBeacon: return "LORA_BEACON";
    case BitrawLabelPt2262Remote: return "PT2262_REMOTE";
    case BitrawLabelEv1527Remote: return "EV1527_REMOTE";
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
    switch(c) {
    case ManchesterGEThomas: return "G.E.Thomas (1=high-low)";
    case ManchesterIEEE8023: return "IEEE 802.3 (1=low-high)";
    case ManchesterDiffTransitionIsOne: return "Differential Manchester (transition=1)";
    case ManchesterDiffTransitionIsZero: return "Differential Manchester (transition=0)";
    }
    return "?";
}
