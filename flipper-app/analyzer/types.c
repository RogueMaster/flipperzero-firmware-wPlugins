#include "types.h"

const char* subhound_label_name(SubhoundLabel label) {
    switch(label) {
    case SubhoundLabelNoise: return "NOISE";
    case SubhoundLabelAmrMeter: return "AMR_METER";
    case SubhoundLabelTpms: return "TPMS";
    case SubhoundLabelWmbusMeter: return "WMBUS_METER";
    case SubhoundLabelHoneywell5800: return "HONEYWELL_5800";
    case SubhoundLabelAlarmSensor: return "ALARM_SENSOR";
    case SubhoundLabelShutterBlind: return "SHUTTER_BLIND";
    case SubhoundLabelEnoceanSwitch: return "ENOCEAN_SWITCH";
    case SubhoundLabelDoorbell: return "DOORBELL";
    case SubhoundLabelOutletSwitch: return "OUTLET_SWITCH";
    case SubhoundLabelGarageRemote: return "GARAGE_REMOTE";
    case SubhoundLabelKeyfobRemote: return "KEYFOB_REMOTE";
    case SubhoundLabelWeatherStation: return "WEATHER_STATION";
    case SubhoundLabelLoraBeacon: return "LORA_BEACON";
    case SubhoundLabelPt2262Remote: return "PT2262_REMOTE";
    case SubhoundLabelEv1527Remote: return "EV1527_REMOTE";
    case SubhoundLabelUnknownStructured: return "UNKNOWN_STRUCTURED";
    }
    return "UNKNOWN";
}

const char* subhound_confidence_name(SubhoundConfidence c) {
    switch(c) {
    case SubhoundConfHigh: return "HIGH";
    case SubhoundConfMedium: return "MEDIUM";
    case SubhoundConfLow: return "LOW";
    }
    return "?";
}

const char* subhound_manchester_name(ManchesterConvention c) {
    switch(c) {
    case ManchesterGEThomas: return "G.E.Thomas (1=high-low)";
    case ManchesterIEEE8023: return "IEEE 802.3 (1=low-high)";
    case ManchesterDiffTransitionIsOne: return "Differential Manchester (transition=1)";
    case ManchesterDiffTransitionIsZero: return "Differential Manchester (transition=0)";
    }
    return "?";
}
