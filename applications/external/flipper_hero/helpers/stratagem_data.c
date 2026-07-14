#include "stratagem_data.h"

// Yes, I know that these are awful. But I tried using arrays.
// Trust me, I tried. The flipper just doesn't have enough memory for arrays to be used here.
// So these lovecraftian switch statements are what you get.
// They have to be separate functions because:
// get_stratagem_name is called in a unique context, and get_stratagem_icon isn't void. That is because I dont want to have
// any more headaches with pointers. If you want to try to make this more elegant, be my guest.

void get_stratagem_name(char name_buf[32], int i) {
    switch(i) {
    case 0:
        snprintf(name_buf, 32, "MG-43 Machine Gun");
        break;
    case 1:
        snprintf(name_buf, 32, "APW-1 Anti Materiel Rifle");
        break;
    case 2:
        snprintf(name_buf, 32, "M-105 Stalwart");
        break;
    case 3:
        snprintf(name_buf, 32, "EAT-17 Expandable Anti-Tank");
        break;
    case 4:
        snprintf(name_buf, 32, "GR-8 Recoilless Rifle");
        break;
    case 5:
        snprintf(name_buf, 32, "FLAM-40 Flamethrower");
        break;
    case 6:
        snprintf(name_buf, 32, "AC-8 Autocannon");
        break;
    case 7:
        snprintf(name_buf, 32, "MG-206 Heavy Machine Gun");
        break;
    case 8:
        snprintf(name_buf, 32, "RL-77 Airburst Rocket Launcher");
        break;
    case 9:
        snprintf(name_buf, 32, "MLS-4X Commando");
        break;
    case 10:
        snprintf(name_buf, 32, "RS-422 Railgun");
        break;
    case 11:
        snprintf(name_buf, 32, "FAF-14 Spear");
        break;
    case 12:
        snprintf(name_buf, 32, "StA-X3 W.A.S.P. Launcher");
        break;
    case 13:
        snprintf(name_buf, 32, "GL-21 Grenade Launcher");
        break;
    case 14:
        snprintf(name_buf, 32, "LAS-98 Laser Cannon");
        break;
    case 15:
        snprintf(name_buf, 32, "ARC-3 Arc Thrower");
        break;
    case 16:
        snprintf(name_buf, 32, "LAS-99 Quasar Cannon");
        break;
    case 17:
        snprintf(name_buf, 32, "TX-41 Sterilizer");
        break;
    case 18:
        snprintf(name_buf, 32, "CQC-1 One True Flag");
        break;
    case 19:
        snprintf(name_buf, 32, "GL-52 De-Escalator");
        break;
    case 20:
        snprintf(name_buf, 32, "PLAS-45 Epoch");
        break;
    case 21:
        snprintf(name_buf, 32, "S-11 Speargun");
        break;
    case 22:
        snprintf(name_buf, 32, "EAT-700 Expendable Napalm");
        break;
    case 23:
        snprintf(name_buf, 32, "M-1000 Maxigun");
        break;
    case 24:
        snprintf(name_buf, 32, "CQC-9 Defoliation Tool");
        break;
    case 25:
        snprintf(name_buf, 32, "Orbital Gatling Barrage");
        break;
    case 26:
        snprintf(name_buf, 32, "Orbital Airburst Strike");
        break;
    case 27:
        snprintf(name_buf, 32, "Orbital 120mm HE Barrage");
        break;
    case 28:
        snprintf(name_buf, 32, "Orbital 380mm HE Barrage");
        break;
    case 29:
        snprintf(name_buf, 32, "Orbital Walking Barrage");
        break;
    case 30:
        snprintf(name_buf, 32, "Orbital Laser");
        break;
    case 31:
        snprintf(name_buf, 32, "Orbital Napalm Barrage");
        break;
    case 32:
        snprintf(name_buf, 32, "Orbital Railcannon Strike");
        break;
    case 33:
        snprintf(name_buf, 32, "Orbital Precision Strike");
        break;
    case 34:
        snprintf(name_buf, 32, "Orbital Gas Strike");
        break;
    case 35:
        snprintf(name_buf, 32, "Orbital EMS Strike");
        break;
    case 36:
        snprintf(name_buf, 32, "Orbital Smoke Strike");
        break;
    case 37:
        snprintf(name_buf, 32, "Eagle Strafing Run");
        break;
    case 38:
        snprintf(name_buf, 32, "Eagle Airstrike");
        break;
    case 39:
        snprintf(name_buf, 32, "Eagle Cluster Bomb");
        break;
    case 40:
        snprintf(name_buf, 32, "Eagle Napalm Airstrike");
        break;
    case 41:
        snprintf(name_buf, 32, "Eagle Smoke Strike");
        break;
    case 42:
        snprintf(name_buf, 32, "Eagle 110mm Rocket Pods");
        break;
    case 43:
        snprintf(name_buf, 32, "Eagle 500kg Bomb");
        break;
    case 44:
        snprintf(name_buf, 32, "LIFT-850 Jump Pack");
        break;
    case 45:
        snprintf(name_buf, 32, "LIFT-860 Hover Pack");
        break;
    case 46:
        snprintf(name_buf, 32, "LIFT-182 Warp Pack");
        break;
    case 47:
        snprintf(name_buf, 32, "B-1 Supply Pack");
        break;
    case 48:
        snprintf(name_buf, 32, "SH-32 Shield Generator Pack");
        break;
    case 49:
        snprintf(name_buf, 32, "SH-20 Ballistic Shield Backpack");
        break;
    case 50:
        snprintf(name_buf, 32, "SH-51 Directional Shield");
        break;
    case 51:
        snprintf(name_buf, 32, "AX/AR-23 \"Guard Dog\"");
        break;
    case 52:
        snprintf(name_buf, 32, "AX/LAS-5 \"Guard Dog\" Rover");
        break;
    case 53:
        snprintf(name_buf, 32, "AX/TX-13 \"Guard Dog\" Dog Breath");
        break;
    case 54:
        snprintf(name_buf, 32, "AX/ARC-3 \"Guard Dog\" K-9");
        break;
    case 55:
        snprintf(name_buf, 32, "AX-FLAM-75 \"Guard Dog\" Hot Dog");
        break;
    case 56:
        snprintf(name_buf, 32, "B-100 Portable Hellbomb");
        break;
    case 57:
        snprintf(name_buf, 32, "MD-6 Anti-Personnel Minefield");
        break;
    case 58:
        snprintf(name_buf, 32, "MD-I4 Incendiary Mines");
        break;
    case 59:
        snprintf(name_buf, 32, "MD-17 Anti-Tank Mines");
        break;
    case 60:
        snprintf(name_buf, 32, "MD-8 Gas Mines");
        break;
    case 61:
        snprintf(name_buf, 32, "A/ARC-3 Tesla Tower");
        break;
    case 62:
        snprintf(name_buf, 32, "MS-11 Solo Silo");
        break;
    case 63:
        snprintf(name_buf, 32, "E/MG-101 HMG Emplacement");
        break;
    case 64:
        snprintf(name_buf, 32, "E/AT-12 Anti-Tank Emplacement");
        break;
    case 65:
        snprintf(name_buf, 32, "E/GL-21 Grenadier Battlement");
        break;
    case 66:
        snprintf(name_buf, 32, "FX-12 Shield Generator Relay");
        break;
    case 67:
        snprintf(name_buf, 32, "A/MG-43 Machine Gun Sentry");
        break;
    case 68:
        snprintf(name_buf, 32, "A/G-16 Gatling Sentry");
        break;
    case 69:
        snprintf(name_buf, 32, "A/M-12 Mortar Sentry");
        break;
    case 70:
        snprintf(name_buf, 32, "A/AC-8 Autocannon Sentry");
        break;
    case 71:
        snprintf(name_buf, 32, "A/MLS-4X Rocket Sentry");
        break;
    case 72:
        snprintf(name_buf, 32, "A/M-23 EMS Mortar Sentry");
        break;
    case 73:
        snprintf(name_buf, 32, "A/FLAM-40 Flame Sentry");
        break;
    case 74:
        snprintf(name_buf, 32, "A/LAS-98 Laser Sentry");
        break;
    case 75:
        snprintf(name_buf, 32, "M-102 Fast Recon Vehicle");
        break;
    case 76:
        snprintf(name_buf, 32, "EXO-45 Patriot Exosuit");
        break;
    case 77:
        snprintf(name_buf, 32, "EXO-49 Emancipator Exosuit");
        break;
    case 78:
        snprintf(name_buf, 32, "Reinforce");
        break;
    case 79:
        snprintf(name_buf, 32, "Resupply");
        break;
    case 80:
        snprintf(name_buf, 32, "Hellbomb");
        break;
    case 81:
        snprintf(name_buf, 32, "SoS Beacon");
        break;
    case 82:
        snprintf(name_buf, 32, "Eagle Rearm");
        break;
    case 83:
        snprintf(name_buf, 32, "SSSD Delivery");
        break;
    case 84:
        snprintf(name_buf, 32, "Prospecting Drill");
        break;
    case 85:
        snprintf(name_buf, 32, "Super Earth Flag");
        break;
    case 86:
        snprintf(name_buf, 32, "Upload Data");
        break;
    case 87:
        snprintf(name_buf, 32, "Seismic Probe");
        break;
    case 88:
        snprintf(name_buf, 32, "SEAF Artillery");
        break;
    case 89:
        snprintf(name_buf, 32, "Dark Fluid Vessel");
        break;
    case 90:
        snprintf(name_buf, 32, "Tectonic Drill");
        break;
    case 91:
        snprintf(name_buf, 32, "Hive Breaker Drill");
        break;
    case 92:
        snprintf(name_buf, 32, "Cargo Container");
        break;
    default:
        snprintf(name_buf, 32, "%d: INVALID STRATAGEM INDEX", i);
        break;
    }
}
Icon* get_stratagem_icon(int i, bool large_icon) {
    if(large_icon) {
        switch(i) {
        case 0:
            return (Icon*)&I_w_mg_30x30;
        case 1:
            return (Icon*)&I_w_amr_30x30;
        case 2:
            return (Icon*)&I_w_stalwart_30x30;
        case 3:
            return (Icon*)&I_w_eat_30x30;
        case 4:
            return (Icon*)&I_w_recoilless_30x30;
        case 5:
            return (Icon*)&I_w_flamethrower_30x30;
        case 6:
            return (Icon*)&I_w_autocannon_30x30;
        case 7:
            return (Icon*)&I_w_hmg_30x30;
        case 8:
            return (Icon*)&I_w_airburst_30x30;
        case 9:
            return (Icon*)&I_w_commando_30x30;
        case 10:
            return (Icon*)&I_w_railgun_30x30;
        case 11:
            return (Icon*)&I_w_spear_30x30;
        case 12:
            return (Icon*)&I_w_wasp_30x30;
        case 13:
            return (Icon*)&I_w_grenade_launcher_30x30;
        case 14:
            return (Icon*)&I_w_laser_cannon_30x30;
        case 15:
            return (Icon*)&I_w_arc_30x30;
        case 16:
            return (Icon*)&I_w_quasar_30x30;
        case 17:
            return (Icon*)&I_w_sterilizer_30x30;
        case 18:
            return (Icon*)&I_w_flag_30x30;
        case 19:
            return (Icon*)&I_w_de_escalator_30x30;
        case 20:
            return (Icon*)&I_w_epoch_30x30;
        case 21:
            return (Icon*)&I_w_speargun_30x30;
        case 22:
            return (Icon*)&I_w_eat_napalm_30x30;
        case 23:
            return (Icon*)&I_w_maxigun_30x30;
        case 24:
            return (Icon*)&I_w_tool_30x30;
        case 25:
            return (Icon*)&I_o_gatling_30x30;
        case 26:
            return (Icon*)&I_o_airburst_30x30;
        case 27:
            return (Icon*)&I_o_120mm_30x30;
        case 28:
            return (Icon*)&I_o_380mm_30x30;
        case 29:
            return (Icon*)&I_o_walking_30x30;
        case 30:
            return (Icon*)&I_o_laser_30x30;
        case 31:
            return (Icon*)&I_o_napalm_30x30;
        case 32:
            return (Icon*)&I_o_railcannon_30x30;
        case 33:
            return (Icon*)&I_o_precision_30x30;
        case 34:
            return (Icon*)&I_o_gas_30x30;
        case 35:
            return (Icon*)&I_o_ems_30x30;
        case 36:
            return (Icon*)&I_o_smoke_30x30;
        case 37:
            return (Icon*)&I_e_strafing_30x30;
        case 38:
            return (Icon*)&I_e_airstrike_30x30;
        case 39:
            return (Icon*)&I_e_cluster_30x30;
        case 40:
            return (Icon*)&I_e_napalm_30x30;
        case 41:
            return (Icon*)&I_e_smoke_30x30;
        case 42:
            return (Icon*)&I_e_110mm_30x30;
        case 43:
            return (Icon*)&I_e_500kg_30x30;
        case 44:
            return (Icon*)&I_p_jump_30x30;
        case 45:
            return (Icon*)&I_p_hover_30x30;
        case 46:
            return (Icon*)&I_p_warp_30x30;
        case 47:
            return (Icon*)&I_p_supply_30x30;
        case 48:
            return (Icon*)&I_p_s_generator_30x30;
        case 49:
            return (Icon*)&I_p_s_ballistic_30x30;
        case 50:
            return (Icon*)&I_p_s_directional_30x30;
        case 51:
            return (Icon*)&I_gd_30x30;
        case 52:
            return (Icon*)&I_gd_las_30x30;
        case 53:
            return (Icon*)&I_gd_gas_30x30;
        case 54:
            return (Icon*)&I_gd_arc_30x30;
        case 55:
            return (Icon*)&I_gd_flam_30x30;
        case 56:
            return (Icon*)&I_p_hellbomb_30x30;
        case 57:
            return (Icon*)&I_m_ap_30x30;
        case 58:
            return (Icon*)&I_m_incendiary_30x30;
        case 59:
            return (Icon*)&I_m_antitank_30x30;
        case 60:
            return (Icon*)&I_m_gas_30x30;
        case 61:
            return (Icon*)&I_m_tesla_30x30;
        case 62:
            return (Icon*)&I_e_silo_30x30;
        case 63:
            return (Icon*)&I_emp_hmg_30x30;
        case 64:
            return (Icon*)&I_emp_antitank_30x30;
        case 65:
            return (Icon*)&I_m_battlement_30x30;
        case 66:
            return (Icon*)&I_m_relay_30x30;
        case 67:
            return (Icon*)&I_s_mg_30x30;
        case 68:
            return (Icon*)&I_s_gatling_30x30;
        case 69:
            return (Icon*)&I_s_mortar_30x30;
        case 70:
            return (Icon*)&I_s_autocannon_30x30;
        case 71:
            return (Icon*)&I_s_rocket_30x30;
        case 72:
            return (Icon*)&I_s_ems_mortar_30x30;
        case 73:
            return (Icon*)&I_s_flame_30x30;
        case 74:
            return (Icon*)&I_s_laser_30x30;
        case 75:
            return (Icon*)&I_v_frv_30x30;
        case 76:
            return (Icon*)&I_v_patriot_30x30;
        case 77:
            return (Icon*)&I_v_emancipator_30x30;
        case 78:
            return (Icon*)&I_m_reinforce_30x30;
        case 79:
            return (Icon*)&I_m_resupply_30x30;
        case 80:
            return (Icon*)&I_m_hellbomb_30x30;
        case 81:
            return (Icon*)&I_m_sos_30x30;
        case 82:
            return (Icon*)&I_m_rearm_30x30;
        case 83:
            return (Icon*)&I_m_sssd_30x30;
        case 84:
            return (Icon*)&I_m_drill_30x30;
        case 85:
            return (Icon*)&I_m_flag_30x30;
        case 86:
            return (Icon*)&I_m_sssd_30x30;
        case 87:
            return (Icon*)&I_m_seismic_probe_30x30;
        case 88:
            return (Icon*)&I_m_seaf_artillery_30x30;
        case 89:
            return (Icon*)&I_m_vessel_30x30;
        case 90:
            return (Icon*)&I_m_drill_30x30;
        case 91:
            return (Icon*)&I_m_drill_30x30;
        case 92:
            return (Icon*)&I_m_cargo_30x30;
        default:
            return (Icon*)&I_eagle_30x30; // Failsafe. It's a blank eagle icon.
        }
    } else {
        switch(i) {
        case 0:
            return (Icon*)&I_w_mg_15x15;
        case 1:
            return (Icon*)&I_w_amr_15x15;
        case 2:
            return (Icon*)&I_w_stalwart_15x15;
        case 3:
            return (Icon*)&I_w_eat_15x15;
        case 4:
            return (Icon*)&I_w_recoilless_15x15;
        case 5:
            return (Icon*)&I_w_flamethrower_15x15;
        case 6:
            return (Icon*)&I_w_autocannon_15x15;
        case 7:
            return (Icon*)&I_w_hmg_15x15;
        case 8:
            return (Icon*)&I_w_airburst_15x15;
        case 9:
            return (Icon*)&I_w_commando_15x15;
        case 10:
            return (Icon*)&I_w_railgun_15x15;
        case 11:
            return (Icon*)&I_w_spear_15x15;
        case 12:
            return (Icon*)&I_w_wasp_15x15;
        case 13:
            return (Icon*)&I_w_grenade_launcher_15x15;
        case 14:
            return (Icon*)&I_w_laser_cannon_15x15;
        case 15:
            return (Icon*)&I_w_arc_15x15;
        case 16:
            return (Icon*)&I_w_quasar_15x15;
        case 17:
            return (Icon*)&I_w_sterilizer_15x15;
        case 18:
            return (Icon*)&I_w_flag_15x15;
        case 19:
            return (Icon*)&I_w_de_escalator_15x15;
        case 20:
            return (Icon*)&I_w_epoch_15x15;
        case 21:
            return (Icon*)&I_w_speargun_15x15;
        case 22:
            return (Icon*)&I_w_eat_napalm_15x15;
        case 23:
            return (Icon*)&I_w_maxigun_15x15;
        case 24:
            return (Icon*)&I_w_tool_15x15;
        case 25:
            return (Icon*)&I_o_gatling_15x15;
        case 26:
            return (Icon*)&I_o_airburst_15x15;
        case 27:
            return (Icon*)&I_o_120mm_15x15;
        case 28:
            return (Icon*)&I_o_380mm_15x15;
        case 29:
            return (Icon*)&I_o_walking_15x15;
        case 30:
            return (Icon*)&I_o_laser_15x15;
        case 31:
            return (Icon*)&I_o_napalm_15x15;
        case 32:
            return (Icon*)&I_o_railcannon_15x15;
        case 33:
            return (Icon*)&I_o_precision_15x15;
        case 34:
            return (Icon*)&I_o_gas_15x15;
        case 35:
            return (Icon*)&I_o_ems_15x15;
        case 36:
            return (Icon*)&I_o_smoke_15x15;
        case 37:
            return (Icon*)&I_e_strafing_15x15;
        case 38:
            return (Icon*)&I_e_airstrike_15x15;
        case 39:
            return (Icon*)&I_e_cluster_15x15;
        case 40:
            return (Icon*)&I_e_napalm_15x15;
        case 41:
            return (Icon*)&I_e_smoke_15x15;
        case 42:
            return (Icon*)&I_e_110mm_15x15;
        case 43:
            return (Icon*)&I_e_500kg_15x15;
        case 44:
            return (Icon*)&I_p_jump_15x15;
        case 45:
            return (Icon*)&I_p_hover_15x15;
        case 46:
            return (Icon*)&I_p_warp_15x15;
        case 47:
            return (Icon*)&I_p_supply_15x15;
        case 48:
            return (Icon*)&I_p_s_generator_15x15;
        case 49:
            return (Icon*)&I_p_s_ballistic_15x15;
        case 50:
            return (Icon*)&I_p_s_directional_15x15;
        case 51:
            return (Icon*)&I_gd_15x15;
        case 52:
            return (Icon*)&I_gd_las_15x15;
        case 53:
            return (Icon*)&I_gd_gas_15x15;
        case 54:
            return (Icon*)&I_gd_arc_15x15;
        case 55:
            return (Icon*)&I_gd_flam_15x15;
        case 56:
            return (Icon*)&I_p_hellbomb_15x15;
        case 57:
            return (Icon*)&I_m_ap_15x15;
        case 58:
            return (Icon*)&I_m_incendiary_15x15;
        case 59:
            return (Icon*)&I_m_antitank_15x15;
        case 60:
            return (Icon*)&I_m_gas_15x15;
        case 61:
            return (Icon*)&I_m_tesla_15x15;
        case 62:
            return (Icon*)&I_e_silo_15x15;
        case 63:
            return (Icon*)&I_emp_hmg_15x15;
        case 64:
            return (Icon*)&I_emp_antitank_15x15;
        case 65:
            return (Icon*)&I_m_battlement_15x15;
        case 66:
            return (Icon*)&I_m_relay_15x15;
        case 67:
            return (Icon*)&I_s_mg_15x15;
        case 68:
            return (Icon*)&I_s_gatling_15x15;
        case 69:
            return (Icon*)&I_s_mortar_15x15;
        case 70:
            return (Icon*)&I_s_autocannon_15x15;
        case 71:
            return (Icon*)&I_s_rocket_15x15;
        case 72:
            return (Icon*)&I_s_ems_mortar_15x15;
        case 73:
            return (Icon*)&I_s_flame_15x15;
        case 74:
            return (Icon*)&I_s_laser_15x15;
        case 75:
            return (Icon*)&I_v_frv_15x15;
        case 76:
            return (Icon*)&I_v_patriot_15x15;
        case 77:
            return (Icon*)&I_v_emancipator_15x15;
        case 78:
            return (Icon*)&I_m_reinforce_15x15;
        case 79:
            return (Icon*)&I_m_resupply_15x15;
        case 80:
            return (Icon*)&I_m_hellbomb_15x15;
        case 81:
            return (Icon*)&I_m_sos_15x15;
        case 82:
            return (Icon*)&I_m_rearm_15x15;
        case 83:
            return (Icon*)&I_m_sssd_15x15;
        case 84:
            return (Icon*)&I_m_drill_15x15;
        case 85:
            return (Icon*)&I_m_flag_15x15;
        case 86:
            return (Icon*)&I_m_sssd_15x15;
        case 87:
            return (Icon*)&I_m_seismic_probe_15x15;
        case 88:
            return (Icon*)&I_m_seaf_artillery_15x15;
        case 89:
            return (Icon*)&I_m_vessel_15x15;
        case 90:
            return (Icon*)&I_m_drill_15x15;
        case 91:
            return (Icon*)&I_m_drill_15x15;
        case 92:
            return (Icon*)&I_m_cargo_15x15;
        default:
            return (Icon*)&I_eagle_15x15; // Failsafe. It's a blank eagle icon.
        }
    }
    return (Icon*)&I_eagle_15x15; // idk why its making me do this but whatev
}
void get_stratagem_seq(char seq_buf[9], int i) {
    switch(i) {
    case 0:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLDURXXX");
        break; // MG-43 Machine Gun
    case 1:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLRUDXXX");
        break; // Anti Material Rifle
    case 2:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLDUULXX");
        break; // Stalwart
    case 3:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDLURXXX");
        break; // EAT
    case 4:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLRRLXXX");
        break; // Recoilless Rifle
    case 5:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLUDUXXX");
        break; // Flamethrower
    case 6:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLDUURXX");
        break; // Autocannon
    case 7:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLUDDXXX");
        break; // Heavy Machine Gun
    case 8:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DUULRXXX");
        break; // Airburst Rocket Launcher
    case 9:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLUDRXXX");
        break; // Commando
    case 10:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DRDULRXX");
        break; // Railgun
    case 11:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDUDDXXX");
        break; // Spear
    case 12:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDUDRXXX");
        break; // W.A.S.P
    case 13:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLULDXXX");
        break; // GrenadeLauncher
    case 14:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLDULXXX");
        break; // Laser Cannon
    case 15:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DRDULLXX");
        break; // Arc Thrower
    case 16:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDULRXXX");
        break; // Quasar Cannon
    case 17:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLUDLXXX");
        break; // Sterilizer
    case 18:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLRRUXXX");
        break; // One True Flag
    case 19:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DRULRXXX");
        break; // De-Escalator
    case 20:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLULRXXX");
        break; // Epoch
    case 21:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DRDLURXX");
        break; // Speargun
    case 22:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDLULXXX");
        break; // Expendable Napalm
    case 23:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLRDUUXX");
        break; // Maxigun
    case 24:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLRRDXXX");
        break; // Defoliation Tool
    case 25:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RDLUUXXX");
        break; // ORBITALS: Gatling Barrage
    case 26:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RRRXXXXX");
        break; // Airburst
    case 27:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RRDLRDXX");
        break; // 120mm HE Barrage
    case 28:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RDUULDDX");
        break; // 380mm HE Barrage
    case 29:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RDRDRDXX");
        break; // Walking Barrage
    case 30:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RDURDXXX");
        break; // Laser
    case 31:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RRDLRUXX");
        break; // Napalm Barrage
    case 32:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RUDDRXXX");
        break; // Railcannon Strike
    case 33:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RRUXXXXX");
        break; // Precision Strike
    case 34:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RRDRXXXX");
        break; // Gas Strike
    case 35:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RRLDXXXX");
        break; // EMS Strike
    case 36:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RRDUXXXX");
        break; // Smoke Strike
    case 37:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "URRXXXXX");
        break; // EAGLE: Strafing Run
    case 38:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "URDRXXXX");
        break; // Airstrike
    case 39:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "URDDRXXX");
        break; // Cluster Bomb
    case 40:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "URDUXXXX");
        break; // Napalm Airstrike
    case 41:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "URUDXXXX");
        break; // Smoke Strike
    case 42:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "URULXXXX");
        break; // 110mm Rocket Pods
    case 43:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "URDDDXXX");
        break; // 500kg
    case 44:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DUUDUXXX");
        break; // BACKPACKS: Jump
    case 45:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DUUDLRXX");
        break; // Hover
    case 46:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLRDLRXX");
        break; // Warp
    case 47:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLDUUDXX");
        break; // Supply
    case 48:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULRLRXX");
        break; // Shield Generator
    case 49:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLDDULXX");
        break; // Ballistic Shield
    case 50:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULRUUXX");
        break; // Directional Shield
    case 51:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULURDXX");
        break; // Guard Dog
    case 52:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULURRXX");
        break; // Guard Dog Rover
    case 53:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULURUXX");
        break; // Guard Dog Dog Breath
    case 54:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULURLXX");
        break; // Guard Dog K-9
    case 55:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULULLXX");
        break; // Guard Dog Hot Dog
    case 56:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DRUUUXXX");
        break; // Portable Hellbomb
    case 57:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLURXXXX");
        break; // MINES: Anti-Personnel
    case 58:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLLDXXXX");
        break; // Incendiary
    case 59:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLUUXXXX");
        break; // Anti-Tank
    case 60:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DLLRXXXX");
        break; // Gas
    case 61:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURULRXX");
        break; // EMPLACEMENTS: Tesla Tower
    case 62:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURDDXXX");
        break; // Solo Silo
    case 63:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULRRLXX");
        break; // HMG Emplacement
    case 64:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULRRRXX");
        break; // Anti-Tank Emplacement
    case 65:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DRDLRXXX");
        break; // Grenadier Battlement
    case 66:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDLRLRXX");
        break; // Shield Generator Relay
    case 67:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURRUXXX");
        break; // SENTRIES: Machine Gun
    case 68:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURLXXXX");
        break; // Gatling
    case 69:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURRDXXX");
        break; // Mortar
    case 70:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURULUXX");
        break; // Autocannon
    case 71:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURRLXXX");
        break; // Rocket
    case 72:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURDRXXX");
        break; // EMS Mortar
    case 73:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURDUUXX");
        break; // Flame
    case 74:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DURDURXX");
        break; // Laser
    case 75:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "LDRDRDUX");
        break; // VEHICLES: Fast Recon Vehicle
    case 76:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "LDRULDDX");
        break; // Patriot
    case 77:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "LDRULDUX");
        break; // Emancipator
    case 78:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "UDRLUXXX");
        break; // MISSION: Reinforce
    case 79:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDURXXXX");
        break; // Resupply
    case 80:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DULDURDU");
        break; // Hellbomb
    case 81:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "UDRUXXXX");
        break; // SoS Beacon
    case 82:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "UULURXXX");
        break; // Eagle Rearm
    case 83:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDDUUXXX");
        break; // SSSD Delivery
    case 84:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DDLRDDXX");
        break; // Prospecting Drill
    case 85:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "DUDUXXXX");
        break; // Super Earth Flag
    case 86:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "LRUUUXXX");
        break; // Upload Data
    case 87:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "UULRDDXX");
        break; // Seismic Probe
    case 88:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "RUUDXXXX");
        break; // SEAF Artillery
    case 89:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "ULRDUUXX");
        break; // Dark Fluid Vessel
    case 90:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "UDUDUDXX");
        break; // Tectonic Drill
    case 91:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "LUDRDDXX");
        break; // Hive Breaker Drill
    case 92:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "UUDDRDXX");
        break; // Cargo Container
    default:
        snprintf(seq_buf, sizeof("LLLLLLLL"), "LLLLLLLL");
        break; // Failsafe
    }
}
