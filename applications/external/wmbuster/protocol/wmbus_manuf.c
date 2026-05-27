/* FLAG manufacturer-code database for wM-Bus.
 *
 * Codes are 3 ASCII letters packed into 16 bits: 5 bits per letter, A=1.
 * The table covers the manufacturers that show up on European wM-Bus
 * radio in meaningful volume (source: the FLAG association register at
 * dlms.com/flag-id plus on-device captures). It is intentionally larger
 * than the public OMS-registered list because legacy meters still in
 * the field use codes that have since been re-assigned or retired.
 */

#include "wmbus_manuf.h"
#include <string.h>

void wmbus_manuf_decode(uint16_t m, char out[4]) {
    out[0] = (char)(((m >> 10) & 0x1F) + 64);
    out[1] = (char)(((m >> 5)  & 0x1F) + 64);
    out[2] = (char)(( m        & 0x1F) + 64);
    out[3] = '\0';
}

static const struct { const char* code; const char* name; } k_manuf[] = {
    /* --- water / heat / HCA majors (most-seen on EU radio) --- */
    {"KAM", "Kamstrup"},
    {"DME", "Diehl Metering"},
    {"HYD", "Diehl/Hydrometer"},
    {"SAP", "Diehl/Sappel"},
    {"SPX", "Sensus/Spanner-Pollux"},
    {"TCH", "Techem"},
    {"BMT", "BMeters"},
    {"ELS", "Elster"},
    {"EMH", "EMH metering"},
    {"LUG", "Landis+Gyr"},
    {"LGB", "Landis+Gyr"},
    {"LGZ", "Landis+Gyr"},
    {"ITW", "Itron"},
    {"ACW", "Itron/Actaris"},
    {"AMT", "Aquametro"},
    {"APA", "Apator"},
    {"APT", "Apator Powogaz"},
    {"BEF", "Berg Energie"},
    {"EFE", "Engelmann"},
    {"EFN", "Engelmann"},
    {"GWF", "Gioanola/GWF"},
    {"HYS", "Hydrometer"},
    {"INE", "Innotas"},
    {"ISK", "Iskraemeco"},
    {"JUM", "Jumo"},
    {"LSE", "Landis+Staefa"},
    {"MAD", "Maddalena"},
    {"NZR", "NZR"},
    {"QDS", "Qundis"},
    {"RAM", "Rossweiner"},
    {"REL", "Relay"},
    {"SEN", "Sensus"},
    {"SON", "Sontex"},
    {"WEP", "Weptech"},
    {"ZRM", "Zenner"},
    {"ZEN", "Zenner"},

    /* --- electricity / smart-grid (sometimes appear on 868 MHz too) --- */
    {"ABB", "ABB"},
    {"ACE", "Actaris (gas)"},
    {"AEG", "AEG"},
    {"AMP", "Ampy Automation"},
    {"AUR", "Aurora"},
    {"AXI", "UAB Axis"},
    {"BAM", "Balthasar Messtechnik"},
    {"BEG", "Berg"},
    {"BSE", "Basari Elektronik"},
    {"BST", "BESTAS"},
    {"BTR", "Bitronvideo"},
    {"CAL", "Caleffi"},
    {"CBI", "Circutor"},
    {"CMT", "CMT Lighting"},
    {"CRX", "Cryptocard"},
    {"CZM", "Cazzaniga"},
    {"DAN", "Danubia"},
    {"DEV", "Develco"},
    {"DFS", "Danfoss"},
    {"DZG", "DZG"},
    {"EDM", "EDMI"},
    {"EFR", "EFR"},
    {"EKT", "PA KVANT"},
    {"ELE", "ELEQ"},
    {"ELV", "eQ-3 / ELV"},
    {"ELT", "ELTAKO"},
    {"ESY", "EasyMeter"},
    {"EUR", "Eurometers"},
    {"FFD", "Forth Dimension Displays"},
    {"FML", "Siemens Measurements"},
    {"FRC", "FRC Group"},
    {"GAV", "Carlo Gavazzi"},
    {"GMC", "GMC-I"},
    {"HEG", "Hamburger Elektronik"},
    {"HEL", "Heliowatt"},
    {"HND", "Hund"},
    {"HTC", "Horstmann"},
    {"HZC", "HZC Optoelectronics"},
    {"INC", "Incotex"},
    {"INV", "Inventia"},
    {"ISA", "Iskra"},
    {"IST", "ista"},                    /* HCA / heat allocator vendor */
    {"IUS", "Itron US"},
    {"KAA", "Kamstrup A"},
    {"KHL", "Kohler"},
    {"KNX", "KNX"},
    {"KRO", "Kromschröder"},
    {"LEC", "LECO"},
    {"LEM", "LEM HEME"},
    {"LML", "LML"},
    {"LOG", "Logarex"},
    {"LSP", "LSPower"},
    {"MEI", "H. Meinecke"},
    {"MET", "Metrima"},
    {"MIR", "Miromico"},
    {"MKS", "MKS"},
    {"MNS", "MeterSit"},
    {"MTC", "Metrum Sweden"},
    {"MTR", "Multical / Kamstrup"},
    {"MUK", "MeterUtility"},
    {"NES", "Nuri Telecom"},
    {"NRM", "Norm"},
    {"ONY", "Onyx"},
    {"ORM", "Ormazabal"},
    {"PAD", "PadMess"},
    {"PII", "Powermeter Int."},
    {"PMG", "PMG"},
    {"PRI", "Polymeters Response Int."},
    {"QSE", "Q-Service"},
    {"RAS", "Rossweiner Armaturen"},
    {"RKE", "REKO"},
    {"RMG", "RMG / Honeywell"},
    {"SBC", "Saia-Burgess"},
    {"SCE", "Schneider Electric"},
    {"SEL", "Selec"},
    {"SIE", "Siemens"},
    {"SLB", "Schlumberger"},
    {"SMC", "SMC International"},
    {"SMS", "Smart Metering Solutions"},
    {"SOF", "softflowex"},
    {"SOG", "Sogexi"},
    {"STR", "Stromverbrauchsmessung"},
    {"SVM", "AB Svensk Värmemätning"},
    {"TCG", "TCH Group"},
    {"TIP", "TIP Thüringer Industrie"},
    {"TRX", "Trinity Energy"},
    {"UEL", "Uher"},
    {"UNI", "Uniflo"},
    {"VES", "ViewSonic"},
    {"VIE", "Viessmann"},
    {"VPI", "Van Putten Instruments"},
    {"WAH", "Wago"},
    {"WMO", "Westermo"},
    {"YTE", "Yuksek Teknoloji"},
    {"ZAG", "Zellweger Analytics"},
    {"ZIV", "ZIV Aplicaciones"},
};

const char* wmbus_manuf_name(uint16_t m) {
    char c[4]; wmbus_manuf_decode(m, c);
    for(unsigned i = 0; i < sizeof(k_manuf)/sizeof(k_manuf[0]); i++)
        if(memcmp(c, k_manuf[i].code, 3) == 0) return k_manuf[i].name;
    return NULL;
}
