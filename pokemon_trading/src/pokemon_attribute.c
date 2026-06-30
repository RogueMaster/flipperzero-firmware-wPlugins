#include <stdint.h>
#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>
#include <src/include/pokemon_attribute.h>

static const char* gender_str[] = {
    "Unknown",
    "Female",
    "Male",
};

/* This returns a string pointer if the gender is static, NULL if it is not and
 * the gender needs to be calculated.
 */
const char* pokemon_gender_is_static(PokemonData* pdata, uint8_t ratio) {
    switch(ratio) {
    case 0xFF:
        return gender_str[0];
    case 0xFE:
        return gender_str[1];
    case 0x00:
        if(pokemon_stat_get(pdata, STAT_NUM, NONE) != 0xEB) { // Tyrogue can be either gender
            return gender_str[2];
        }
        break;
    default:
        break;
    }

    return NULL;
}

const char* pokemon_gender_get(PokemonData* pdata) {
    uint8_t ratio = table_stat_base_get(
        pdata->pokemon_table,
        pokemon_stat_get(pdata, STAT_NUM, NONE),
        STAT_BASE_GENDER_RATIO,
        NONE);
    uint8_t atk_iv;
    const char* rc;

    rc = pokemon_gender_is_static(pdata, ratio);
    if(rc) return rc;

    /* Falling through here means now we need to calculate the gender from
     * its ratio and ATK_IV.
     */
    atk_iv = pokemon_stat_get(pdata, STAT_ATK_IV, NONE);
    if(atk_iv * 17 <= ratio)
        return gender_str[1];
    else
        return gender_str[2];
}

void pokemon_gender_set(PokemonData* pdata, Gender gender) {
    uint8_t ratio = table_stat_base_get(
        pdata->pokemon_table,
        pokemon_stat_get(pdata, STAT_NUM, NONE),
        STAT_BASE_GENDER_RATIO,
        NONE);
    uint8_t atk_iv = pokemon_stat_get(pdata, STAT_ATK_IV, NONE);

    /* If we need to make the pokemon a male, increase atk IV until it exceeds
     * the gender ratio.
     *
     * Note that, there is no checking here for impossible situations as the
     * scene enter function will immediately quit if its not possible to change
     * the gender (the extremes of gender_ratio value).
     *
     * The check for gender is a percentage, if ATK_IV*(255/15) <= the ratio,
     * then the pokemon is a female. The gender ratio values end up being:
     * DEF GENDER_F0      EQU   0 percent
     * DEF GENDER_F12_5   EQU  12 percent + 1
     * DEF GENDER_F25     EQU  25 percent
     * DEF GENDER_F50     EQU  50 percent
     * DEF GENDER_F75     EQU  75 percent
     * DEF GENDER_F100    EQU 100 percent - 1
     * Where percent is (255/100)
     */
    if(gender == GENDER_MALE) {
        while((atk_iv * 17) <= ratio) atk_iv++;
    } else {
        while((atk_iv * 17) > ratio) atk_iv--;
    }

    pokemon_stat_set(pdata, STAT_ATK_IV, NONE, atk_iv);
}

static const char* pokerus_states[] = {
    "Clean",
    "Infected",
    "Cured",
    "",
};

const char* pokerus_get_status_str(PokemonData* pdata) {
    uint8_t pokerus;

    pokerus = pokemon_stat_get(pdata, STAT_POKERUS, NONE);

    if(pokerus == 0x00) return pokerus_states[0];

    if((pokerus & 0x0f) != 0x00) return pokerus_states[1];

    return pokerus_states[2];
}

void pokerus_set_strain(PokemonData* pdata, uint8_t strain) {
    uint8_t pokerus;

    /* Need to read/modify/write the existing stat */
    pokerus = pokemon_stat_get(pdata, STAT_POKERUS, NONE);
    pokerus &= 0x0f;
    pokerus |= (strain << 4);

    if((pokerus & 0xf0) == 0x00) pokerus = 0;

    pokemon_stat_set(pdata, STAT_POKERUS, NONE, pokerus);
}

void pokerus_set_days(PokemonData* pdata, uint8_t days) {
    uint8_t pokerus;

    days &= 0x0f;

    /* Need to read/modify/write the existing stat */
    pokerus = pokemon_stat_get(pdata, STAT_POKERUS, NONE);
    pokerus &= 0xf0;
    pokerus |= days;
    pokemon_stat_set(pdata, STAT_POKERUS, NONE, pokerus);
}

/* This just assumes gen ii for now */
/* For a Gen II pokemon to be shiny, the following must be met:
 * Spd, Def, and Spc must all be 10
 * Atk must be 2, 3, 6, 7, 10, 11, 14, or 15
 */
bool pokemon_is_shiny(PokemonData* pdata) {
    uint8_t atk_iv = pokemon_stat_get(pdata, STAT_ATK_IV, NONE);
    uint8_t def_iv = pokemon_stat_get(pdata, STAT_DEF_IV, NONE);
    uint8_t spd_iv = pokemon_stat_get(pdata, STAT_SPD_IV, NONE);
    uint8_t spc_iv = pokemon_stat_get(pdata, STAT_SPC_IV, NONE);
    bool rc = 1;

    if(spd_iv != 10) rc = 0;
    if(def_iv != 10) rc = 0;
    if(spc_iv != 10) rc = 0;
    switch(atk_iv) {
    case 0:
    case 1:
    case 4:
    case 5:
    case 8:
    case 9:
    case 12:
    case 13:
        rc = 0;
        break;
    default:
        break;
    }

    return rc;
}

void pokemon_set_shiny(PokemonData* pdata, bool shiny) {
    if(!shiny) {
        do {
            /* First, reset the IV to the selected stat */
            pokemon_stat_set(pdata, STAT_SEL, NONE, pokemon_stat_get(pdata, STAT_SEL, NONE));

            /* XXX: This may not be right? */
            /* Next, ensure the current IVs wouldn't make the pokemon shiny */
        } while(pokemon_is_shiny(pdata));
    } else {
        /* Set Def, Spd, Spc to 10 */
        pokemon_stat_set(pdata, STAT_DEF_IV, NONE, 10);
        pokemon_stat_set(pdata, STAT_SPD_IV, NONE, 10);
        pokemon_stat_set(pdata, STAT_SPC_IV, NONE, 10);

        /* Increase ATK IV until we hit a shiny number. Note that, this only
         * affects IVs that are randomly generated, max IV will already be set
         * at 15 which will make it shiny.
         */
        while(!pokemon_is_shiny(pdata)) {
            pokemon_stat_set(
                pdata, STAT_ATK_IV, NONE, pokemon_stat_get(pdata, STAT_ATK_IV, NONE) + 1);
        }
    }
}

/* This is used to get the current IVs from the trade struct.
 * Unown form is calculated by taking the middle bytes of each nibble of IV,
 * pressing them in order to a single byte, and dividing that by 10 (rounded
 * down/floor). This will create a value from 0 to 25 that is a 1:1 mapping
 * of the English alphabet and is how Unown forms are represented.
 *
 * C integer division truncates to 0 rather than does any proper rounding.
 *
 * https://bulbapedia.bulbagarden.net/wiki/Individual_values#Unown's_letter
 */
static uint8_t unown_ivs_get(PokemonData* pdata) {
    furi_assert(pdata);
    uint16_t ivs = pokemon_stat_get(pdata, STAT_IV, NONE);
    uint8_t ivs_mid;

    ivs_mid =
        (((ivs & 0x6000) >> 7) | ((ivs & 0x0600) >> 5) | ((ivs & 0x0060) >> 3) |
         ((ivs & 0x0006) >> 1));

    return ivs_mid;
}

static void unown_ivs_set(PokemonData* pdata, uint8_t ivs_mid) {
    furi_assert(pdata);
    uint16_t ivs = pokemon_stat_get(pdata, STAT_IV, NONE);

    /* Clear the middle bits of each nibble */
    ivs &= ~(0x6666);

    /* Set the updated ivs_mid in to those cleared bits */
    ivs |=
        (((ivs_mid & 0xC0) << 7) | ((ivs_mid & 0x30) << 5) | ((ivs_mid & 0x0C) << 3) |
         ((ivs_mid & 0x03) << 1));
    pokemon_stat_set(pdata, STAT_IV, NONE, ivs);
}

char unown_form_get(PokemonData* pdata) {
    uint8_t form = unown_ivs_get(pdata);

    /* The forumula is specifically the center two bits of each IV slapped
     * together and floor(/10)
     */
    form /= 10;
    form += 'A';

    return form;
}

/* Try and get to the desired form by adding/subtracting the current IVs */
void unown_form_set(PokemonData* pdata, char letter) {
    uint8_t ivs = unown_ivs_get(pdata);
    uint8_t form;

    letter = toupper(letter);
    furi_check(isalpha(letter));

    while(1) {
        form = ((ivs / 10) + 'A');
        if(form == letter) break;
        if(form > letter)
            ivs--;
        else
            ivs++;
    }

    /* form is now the target letter, set IVs back up */
    unown_ivs_set(pdata, ivs);
}
