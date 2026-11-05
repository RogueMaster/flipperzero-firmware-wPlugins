#pragma once

// Lectura de ficheros de la aplicacion eMRTD (LDS) del DNIe bajo Secure Messaging

#include "dnie_card.h"
#include "dnie_sm.h"

#define DNIE_DG1_MAX      128
#define DNIE_FID_DG1      0x0101
#define DNIE_FID_DG2      0x0102 // foto
#define DNIE_LDS_MAX_FILE 0x7FFF // READ BINARY con offset de 15 bits

typedef enum {
    DnieLdsOk,
    DnieLdsErrorTransport,
    DnieLdsErrorSm, // fallo de Secure Messaging (ver sm_result)
    DnieLdsErrorSelectApp, // el chip no acepto SELECT de la app eMRTD
    DnieLdsErrorSelectFile, // no encontro el fichero
    DnieLdsErrorRead, // READ BINARY con SW de error
    DnieLdsErrorFormat, // cabecera TLV invalida
    DnieLdsErrorTooBig,
    DnieLdsErrorSink, // no se pudo guardar (SD llena, sin SD...)
} DnieLdsResult;

typedef struct {
    DnieLdsResult result;
    DnieSmResult sm_result;
    uint16_t sw;
    uint8_t step; // 1 = SELECT app, 2 = SELECT fichero, 3 = READ
} DnieLdsStatus;

// Recibe los datos en orden segun llegan. Devuelve false para abortar.
typedef bool (*DnieLdsSink)(const uint8_t* data, size_t len, void* context);
// Progreso de lectura (bytes leidos / total)
typedef void (*DnieLdsProgress)(size_t done, size_t total, void* context);

// SELECT de la aplicacion eMRTD (una vez por sesion)
DnieLdsResult dnie_lds_select_app(DnieTransport* poller, DnieSm* sm, DnieLdsStatus* status);

// Selecciona y lee un fichero TLV completo. Solo dentro del callback del poller.
DnieLdsResult dnie_lds_read_file(
    DnieTransport* poller,
    DnieSm* sm,
    uint16_t fid,
    DnieLdsSink sink,
    DnieLdsProgress progress,
    void* context,
    size_t* total_out,
    DnieLdsStatus* status);

// Atajo: lee DG1 en memoria (requiere dnie_lds_select_app antes)
DnieLdsResult dnie_lds_read_dg1(
    DnieTransport* poller,
    DnieSm* sm,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    DnieLdsStatus* status);

// Un comando cualquiera por el canal seguro (lo usa Chip Authentication). false = abortar.
bool dnie_lds_sm_command(
    DnieTransport* poller,
    DnieSm* sm,
    const uint8_t hdr[4],
    const uint8_t* data,
    size_t len,
    int le,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    DnieLdsStatus* status);

const char* dnie_lds_result_str(DnieLdsResult result);
