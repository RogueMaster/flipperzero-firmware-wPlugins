/* The single source of truth for which drivers ship in the app.
 *
 * Adding a new meter is two lines: one `extern` and one entry in
 * the array below. Order matters when MVT lists overlap — the
 * first matching driver wins. Specific (M,V,T) tuples come first;
 * brand-wide wildcards (with version=0xFF or medium=0xFF in the
 * driver's MVT list) come later in the same file so they only
 * match when no specialised driver does.
 *
 * The conventions documented here are enforced by code review.
 * See `drivers/CONTRIBUTING.md` for the full PR walkthrough.
 */

#include "driver.h"

/* === Specialised compact-frame drivers (Techem) === */
extern const WmbusDriver wmbus_drv_techem_fhkv;
extern const WmbusDriver wmbus_drv_techem_mkradio3;
extern const WmbusDriver wmbus_drv_techem_mkradio4;
extern const WmbusDriver wmbus_drv_techem_compact5;
extern const WmbusDriver wmbus_drv_techem_vario;
extern const WmbusDriver wmbus_drv_techem_smoke;

/* === HCA drivers with custom 12-byte preamble === */
extern const WmbusDriver wmbus_drv_qundis_hca;
extern const WmbusDriver wmbus_drv_ista_hca;
extern const WmbusDriver wmbus_drv_brunata_hca;

/* === OMS-walker drivers (no custom decode, just labels) === */
extern const WmbusDriver wmbus_drv_diehl_hydrus;
extern const WmbusDriver wmbus_drv_diehl_sharky;
extern const WmbusDriver wmbus_drv_diehl_aerius;
extern const WmbusDriver wmbus_drv_diehl_water;
extern const WmbusDriver wmbus_drv_diehl_izar;        /* PRIOS LFSR  */

/* === Reverse-engineered drivers ported from wmbusmeters .cc === */
extern const WmbusDriver wmbus_drv_bmeters_hydrodigit;
extern const WmbusDriver wmbus_drv_engelmann_hydroclima;
extern const WmbusDriver wmbus_drv_sontex_rfmtx1;
extern const WmbusDriver wmbus_drv_zenner0b;
extern const WmbusDriver wmbus_drv_apator_na1;
extern const WmbusDriver wmbus_drv_techem_mkradio3a;
extern const WmbusDriver wmbus_drv_gwf_water;

extern const WmbusDriver wmbus_drv_kamstrup_multical;
extern const WmbusDriver wmbus_drv_kamstrup_flowiq;
extern const WmbusDriver wmbus_drv_kamstrup_omnipower;
extern const WmbusDriver wmbus_drv_kamstrup_kampress;

extern const WmbusDriver wmbus_drv_sensus_iperl;
extern const WmbusDriver wmbus_drv_sensus_pollucom;
extern const WmbusDriver wmbus_drv_sensus_spx;

extern const WmbusDriver wmbus_drv_itron_water;
extern const WmbusDriver wmbus_drv_itron_heat;
extern const WmbusDriver wmbus_drv_itron_gas;

extern const WmbusDriver wmbus_drv_apator_amiplus;
extern const WmbusDriver wmbus_drv_apator_162;
extern const WmbusDriver wmbus_drv_apator_172;
extern const WmbusDriver wmbus_drv_apator_08;
extern const WmbusDriver wmbus_drv_apator_na1_oms;
extern const WmbusDriver wmbus_drv_apator_op041a;
extern const WmbusDriver wmbus_drv_apator_elf;
extern const WmbusDriver wmbus_drv_apator_hca;
extern const WmbusDriver wmbus_drv_apator_ultrimis;
extern const WmbusDriver wmbus_drv_apator_aquastream;

extern const WmbusDriver wmbus_drv_qundis_water;
extern const WmbusDriver wmbus_drv_qundis_heat;
extern const WmbusDriver wmbus_drv_qundis_smoke;

extern const WmbusDriver wmbus_drv_engelmann_sensostar;
extern const WmbusDriver wmbus_drv_engelmann_faw;
extern const WmbusDriver wmbus_drv_engelmann_hcae2;
extern const WmbusDriver wmbus_drv_waterstarm;

extern const WmbusDriver wmbus_drv_zenner_minomess;
extern const WmbusDriver wmbus_drv_zenner_c5isf;
extern const WmbusDriver wmbus_drv_zenner_hca;

extern const WmbusDriver wmbus_drv_bm_hydrodigit;
extern const WmbusDriver wmbus_drv_bm_iwmtx5;
extern const WmbusDriver wmbus_drv_bm_hydrocalm3;
extern const WmbusDriver wmbus_drv_bm_hydrocalm4;
extern const WmbusDriver wmbus_drv_bm_hydroclima;
extern const WmbusDriver wmbus_drv_bm_rfmamb;

extern const WmbusDriver wmbus_drv_elster;

extern const WmbusDriver wmbus_drv_sontex_868;
extern const WmbusDriver wmbus_drv_sontex_supercal;
extern const WmbusDriver wmbus_drv_sontex_supercom;

extern const WmbusDriver wmbus_drv_aquametro_topas;
extern const WmbusDriver wmbus_drv_aquametro_calec;

extern const WmbusDriver wmbus_drv_maddalena_water;
extern const WmbusDriver wmbus_drv_maddalena_microclima;
extern const WmbusDriver wmbus_drv_maddalena_evo868;

extern const WmbusDriver wmbus_drv_lg_ultraheat;
extern const WmbusDriver wmbus_drv_lse_water;
extern const WmbusDriver wmbus_drv_qheat5;
extern const WmbusDriver wmbus_drv_lg_fallback;

extern const WmbusDriver wmbus_drv_abb_b23;
extern const WmbusDriver wmbus_drv_ebzwmbe;
extern const WmbusDriver wmbus_drv_emh_ehzp;
extern const WmbusDriver wmbus_drv_esys_wm;
extern const WmbusDriver wmbus_drv_em24;
extern const WmbusDriver wmbus_drv_iem3000;
extern const WmbusDriver wmbus_drv_ime;
extern const WmbusDriver wmbus_drv_eltako;
extern const WmbusDriver wmbus_drv_gransystems;

extern const WmbusDriver wmbus_drv_lansendw;
extern const WmbusDriver wmbus_drv_lansenth;
extern const WmbusDriver wmbus_drv_lansenpu;
extern const WmbusDriver wmbus_drv_lansenrp;
extern const WmbusDriver wmbus_drv_lansensm;
extern const WmbusDriver wmbus_drv_munia;
extern const WmbusDriver wmbus_drv_piigth;
extern const WmbusDriver wmbus_drv_elvsense;
extern const WmbusDriver wmbus_drv_ei6500;

extern const WmbusDriver wmbus_drv_aventies;
extern const WmbusDriver wmbus_drv_unismart;
extern const WmbusDriver wmbus_drv_eurisii;
extern const WmbusDriver wmbus_drv_gwf;
extern const WmbusDriver wmbus_drv_elr;
extern const WmbusDriver wmbus_drv_werhle;
extern const WmbusDriver wmbus_drv_watertech;
extern const WmbusDriver wmbus_drv_weh;
extern const WmbusDriver wmbus_drv_axis;
extern const WmbusDriver wmbus_drv_vipa_kaden;
extern const WmbusDriver wmbus_drv_relay;
extern const WmbusDriver wmbus_drv_picoflux;
extern const WmbusDriver wmbus_drv_fiotech;
extern const WmbusDriver wmbus_drv_sappel;
extern const WmbusDriver wmbus_drv_hydrometer;
extern const WmbusDriver wmbus_drv_bfw_radio;

const WmbusDriver* const wmbus_engine_registry[] = {
    /* --- Compact-frame, manufacturer-specific (highest priority) --- */
    &wmbus_drv_techem_fhkv,
    &wmbus_drv_techem_mkradio3,
    &wmbus_drv_techem_mkradio4,
    &wmbus_drv_techem_compact5,
    &wmbus_drv_techem_vario,
    &wmbus_drv_techem_smoke,
    &wmbus_drv_qundis_hca,
    &wmbus_drv_ista_hca,
    &wmbus_drv_brunata_hca,
    &wmbus_drv_diehl_izar,

    /* RE'd drivers — these provide custom decoders that emit real
     * fields, so place them before the brand-wide OMS-walker entries
     * below to win on (M,V,T) ties. */
    &wmbus_drv_bmeters_hydrodigit,
    &wmbus_drv_engelmann_hydroclima,
    &wmbus_drv_sontex_rfmtx1,
    &wmbus_drv_zenner0b,
    &wmbus_drv_apator_na1,
    &wmbus_drv_techem_mkradio3a,
    &wmbus_drv_gwf_water,

    /* --- OMS-walker drivers, organised by manufacturer (alphabetical
     * within each block; specific tuples first, brand-wide fallbacks
     * inside each driver's MVT list). --- */

    /* Diehl */
    &wmbus_drv_diehl_hydrus,
    &wmbus_drv_diehl_sharky,
    &wmbus_drv_diehl_aerius,
    &wmbus_drv_diehl_water,

    /* Kamstrup */
    &wmbus_drv_kamstrup_multical,
    &wmbus_drv_kamstrup_flowiq,
    &wmbus_drv_kamstrup_omnipower,
    &wmbus_drv_kamstrup_kampress,

    /* Sensus */
    &wmbus_drv_sensus_iperl,
    &wmbus_drv_sensus_pollucom,
    &wmbus_drv_sensus_spx,

    /* Itron / Actaris */
    &wmbus_drv_itron_water,
    &wmbus_drv_itron_heat,
    &wmbus_drv_itron_gas,

    /* Apator family (incl. amiplus, NES, DEV electric rebadges) */
    &wmbus_drv_apator_amiplus,
    &wmbus_drv_apator_162,
    &wmbus_drv_apator_172,
    &wmbus_drv_apator_08,
    &wmbus_drv_apator_na1_oms,
    &wmbus_drv_apator_op041a,
    &wmbus_drv_apator_elf,
    &wmbus_drv_apator_hca,
    &wmbus_drv_apator_ultrimis,
    &wmbus_drv_apator_aquastream,

    /* Qundis (water/heat/smoke; HCA already listed above) */
    &wmbus_drv_qundis_water,
    &wmbus_drv_qundis_heat,
    &wmbus_drv_qundis_smoke,

    /* Engelmann */
    &wmbus_drv_engelmann_sensostar,
    &wmbus_drv_engelmann_faw,
    &wmbus_drv_engelmann_hcae2,
    &wmbus_drv_waterstarm,

    /* Zenner / ZRI */
    &wmbus_drv_zenner_minomess,
    &wmbus_drv_zenner_c5isf,
    &wmbus_drv_zenner_hca,

    /* BMeters */
    &wmbus_drv_bm_hydrodigit,
    &wmbus_drv_bm_iwmtx5,
    &wmbus_drv_bm_hydrocalm3,
    &wmbus_drv_bm_hydrocalm4,
    &wmbus_drv_bm_hydroclima,
    &wmbus_drv_bm_rfmamb,

    /* Elster / Sontex / Aquametro / Maddalena */
    &wmbus_drv_elster,
    &wmbus_drv_sontex_868,
    &wmbus_drv_sontex_supercal,
    &wmbus_drv_sontex_supercom,
    &wmbus_drv_aquametro_topas,
    &wmbus_drv_aquametro_calec,
    &wmbus_drv_maddalena_water,
    &wmbus_drv_maddalena_microclima,
    &wmbus_drv_maddalena_evo868,

    /* Landis+Gyr / Landis+Staefa */
    &wmbus_drv_lg_ultraheat,
    &wmbus_drv_lse_water,
    &wmbus_drv_qheat5,

    /* Smart electricity meters */
    &wmbus_drv_abb_b23,
    &wmbus_drv_ebzwmbe,
    &wmbus_drv_emh_ehzp,
    &wmbus_drv_esys_wm,
    &wmbus_drv_em24,
    &wmbus_drv_iem3000,
    &wmbus_drv_ime,
    &wmbus_drv_eltako,
    &wmbus_drv_gransystems,

    /* Sensors */
    &wmbus_drv_lansendw,
    &wmbus_drv_lansenth,
    &wmbus_drv_lansenpu,
    &wmbus_drv_lansenrp,
    &wmbus_drv_lansensm,
    &wmbus_drv_munia,
    &wmbus_drv_piigth,
    &wmbus_drv_elvsense,
    &wmbus_drv_ei6500,

    /* Long-tail vendors */
    &wmbus_drv_aventies,
    &wmbus_drv_unismart,
    &wmbus_drv_eurisii,
    &wmbus_drv_gwf,
    &wmbus_drv_elr,
    &wmbus_drv_werhle,
    &wmbus_drv_watertech,
    &wmbus_drv_weh,
    &wmbus_drv_axis,
    &wmbus_drv_vipa_kaden,
    &wmbus_drv_relay,
    &wmbus_drv_picoflux,
    &wmbus_drv_fiotech,
    &wmbus_drv_sappel,
    &wmbus_drv_hydrometer,
    &wmbus_drv_bfw_radio,

    /* Brand-wide fallback for legacy Landis+Gyr codes that no
     * specific driver claims. Place last so specialised drivers
     * win on (M,V,T) ties. */
    &wmbus_drv_lg_fallback,
};

const size_t wmbus_engine_registry_len =
    sizeof(wmbus_engine_registry) / sizeof(wmbus_engine_registry[0]);
