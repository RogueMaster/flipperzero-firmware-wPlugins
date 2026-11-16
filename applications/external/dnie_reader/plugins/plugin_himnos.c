// Plugin de himnos: solo las notas. La app las copia y lo descarga antes de empezar a sonar.

#include <flipper_application/flipper_application.h>
#include <string.h>

#include "../dnie_plugin_api.h"

typedef DnieNote Note;

// Marcha Real (Espinosa, 1761, dominio publico). Melodia de la partitura para piano de
// Wikimedia Commons ("Marcha Real de España.pdf"), en Do mayor: frase A (con la 2a casilla)
// y frase B, sin repeticiones. Negra = 400 ms.
#define Q   400
#define G4_ 392.00f
#define A4_ 440.00f
#define B4_ 493.88f
#define C5_ 523.25f
#define D5_ 587.33f
#define E5_ 659.25f
#define F5_ 698.46f
#define G5_ 783.99f
static const Note ANTHEM_ES[] = {
    // A
    {C5_, Q},
    {G4_, Q},
    {E5_, Q},
    {C5_, Q / 2},
    {G5_, Q / 2},
    {F5_, Q / 2},
    {E5_, Q / 2},
    {D5_, Q / 2},
    {C5_, Q / 2},
    {C5_, Q / 2},
    {B4_, Q / 2},
    {A4_, Q / 2},
    {G4_, Q / 2},
    {C5_, Q},
    {D5_, Q},
    {E5_, Q},
    {0, Q / 2},
    {G5_, Q / 2},
    {F5_, Q / 2},
    {E5_, Q / 2},
    {D5_, Q / 2},
    {C5_, Q / 2},
    {G5_, Q * 2},
    // B
    {G5_, Q},
    {E5_, Q * 3 / 4},
    {G5_, Q / 4},
    {F5_, Q},
    {D5_, Q * 3 / 4},
    {F5_, Q / 4},
    {E5_, Q},
    {C5_, Q * 3 / 4},
    {E5_, Q / 4},
    {D5_, Q / 2},
    {G4_, Q / 2},
    {A4_, Q / 2},
    {B4_, Q / 2},
    {C5_, Q},
    {D5_, Q},
    {E5_, Q * 3 / 4},
    {F5_, Q / 4},
    {G5_, Q * 3 / 4},
    {F5_, Q / 4},
    {E5_, Q},
    {D5_, Q},
    {C5_, Q * 2},
};

#include "../dnie_anthems.inc"

#define ANTHEM(cc, code) {cc, ANTHEM_##code, sizeof(ANTHEM_##code) / sizeof(Note)}
static const struct {
    const char* issuer; // emisor de la MRZ (3 letras; Alemania es "D")
    const Note* notes;
    size_t n;
} ANTHEMS[] = {
    ANTHEM("ESP", ES),
    ANTHEM("USA", US),
    ANTHEM("FRA", FR),
    ANTHEM("GBR", GB),
    ANTHEM("CAN", CA),
    ANTHEM("JPN", JP),
    ANTHEM("CHN", CN),
    ANTHEM("IDN", ID),
    ANTHEM("D", DE),
    ANTHEM("DEU", DE),
    ANTHEM("ITA", IT),
    ANTHEM("NLD", NL),
};

static size_t himnos_find(const char* issuer, const DnieNote** notes) {
    for(size_t i = 0; issuer && i < sizeof(ANTHEMS) / sizeof(ANTHEMS[0]); i++) {
        if(strcmp(issuer, ANTHEMS[i].issuer) == 0) {
            *notes = ANTHEMS[i].notes;
            return ANTHEMS[i].n;
        }
    }
    return 0;
}

static const DnieHimnosApi api = {.find = himnos_find};

static const FlipperAppPluginDescriptor descriptor = {
    .appid = DNIE_PLUGIN_APP_ID,
    .ep_api_version = DNIE_PLUGIN_API_VERSION,
    .entry_point = &api,
};

const FlipperAppPluginDescriptor* dnie_himnos_ep(void) {
    return &descriptor;
}
