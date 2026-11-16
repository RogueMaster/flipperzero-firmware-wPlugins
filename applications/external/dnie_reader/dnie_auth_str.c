// Textos de la verificacion que se muestran sin tener cargado el plugin de autenticidad.

#include "dnie_pa.h"
#include "dnie_csca.h"

const char* dnie_pa_result_str(DniePaResult r) {
    switch(r) {
    case DniePaOk:
        return "OK";
    case DniePaBadSignature:
        return "INVALID";
    case DniePaNoCert:
        return "no certificate";
    case DniePaUnsupported:
        return "unsupported alg.";
    case DniePaFormat:
        return "malformed";
    case DniePaNoMemory:
        return "out of memory";
    default:
        return "not checked";
    }
}

const char* dnie_csca_result_str(DnieCscaResult r) {
    switch(r) {
    case DnieCscaOk:
        return "OK";
    case DnieCscaBadSignature:
        return "FAKE";
    case DnieCscaUnknownIssuer:
        return "unknown issuer";
    case DnieCscaUnsupported:
        return "unsupported alg.";
    case DnieCscaFormat:
        return "malformed";
    case DnieCscaNoMemory:
        return "out of memory";
    case DnieCscaKeyNotFound:
        return "key not found";
    default:
        return "not checked";
    }
}
