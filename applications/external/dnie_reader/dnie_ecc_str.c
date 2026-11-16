// Lo que la app necesita de PACE y Chip Authentication sin tener cargado el plugin de curva
// eliptica: elegir la variante de PACE y los textos de resultado.

#include "dnie_pace.h"
#include "dnie_ca.h"

#define PACE_MAPPING_ECDH_GM 2
#define PACE_CIPHER_AES128   2
#define PACE_PARAM_BP256R1   13

bool dnie_pace_choose(const DniePaceInfo* infos, size_t n, DniePaceInfo* chosen) {
    for(size_t i = 0; i < n; i++) {
        if(infos[i].mapping == PACE_MAPPING_ECDH_GM && infos[i].cipher == PACE_CIPHER_AES128 &&
           infos[i].has_param_id && infos[i].param_id == PACE_PARAM_BP256R1) {
            *chosen = infos[i];
            return true;
        }
    }
    return false;
}

const char* dnie_pace_result_str(DniePaceResult result) {
    switch(result) {
    case DniePaceOk:
        return "OK";
    case DniePaceErrorUnsupported:
        return "PACE variant\nnot supported";
    case DniePaceErrorTransport:
        return "Radio error\n(don't move the ID)";
    case DniePaceErrorMse:
        return "Chip rejected\nMSE:Set AT";
    case DniePaceErrorNonce:
        return "Chip rejected\nstep 1 (nonce)";
    case DniePaceErrorMap:
        return "Chip rejected\nstep 2 (mapping)";
    case DniePaceErrorKeyAgreement:
        return "Chip rejected\nstep 3 (ECDH)";
    case DniePaceErrorWrongCan:
        return "Wrong CAN or\nMRZ data";
    case DniePaceErrorChipToken:
        return "Chip token\nmismatch";
    case DniePaceErrorFormat:
        return "Response has\nunexpected format";
    case DniePaceErrorBadPoint:
        return "Invalid curve\npoint";
    case DniePaceErrorCrypto:
        return "Crypto error";
    case DniePaceErrorNoMemory:
        return "Not enough memory:\nclose other apps\nor restart the Flipper";
    case DniePaceErrorPlugin:
        return "Missing app files:\nreinstall IDentity";
    }
    return "Unknown error";
}

const char* dnie_ca_result_str(DnieCaResult r) {
    switch(r) {
    case DnieCaOk:
        return "GENUINE CHIP";
    case DnieCaNotAvailable:
        return "not available";
    case DnieCaUnsupported:
        return "unsupported variant";
    case DnieCaRejected:
        return "rejected by chip";
    case DnieCaFailed:
        return "FAIL: possible clone";
    case DnieCaTransport:
        return "radio error";
    case DnieCaCrypto:
        return "crypto error";
    case DnieCaNoMemory:
        return "out of memory";
    default:
        return "not performed";
    }
}
