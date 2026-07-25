#include "i18n.h"

// Solo ASCII: las fuentes del Flipper no traen acentos.
// El indice 0 es el idioma por defecto de la app: ingles.

const char* const i18n_lang_names[2] = {"English", "Espanol"};

const I18nStrings i18n_strings[2] = {
    // English (default)
    {
        .menu_tree = "Decision tree",
        .menu_text = "Text to Morse",
        .menu_settings = "Settings",
        .menu_about = "Help",
        .input_header = "Text to play",
        .hint_key = "OK: tap=.  hold=-",
        .play_playing = "Playing",
        .play_paused = "Paused",
        .play_done = "Done",
        .play_ok_pause = "OK:pause",
        .play_ok_resume = "OK:resume",
        .play_ok_repeat = "OK:replay",
        .snd_on = "Snd:ON",
        .snd_off = "Snd:off",
        .vib_on = "Vib:ON",
        .vib_off = "Vib:off",
        .set_lang = "Language",
        .set_sound = "Sound",
        .set_volume = "Volume",
        .set_tone = "Tone",
        .set_vibro = "Vibration",
        .set_led = "LED",
        .set_wpm = "Speed WPM",
        .set_dit = "Dash threshold",
        .set_letter_gap = "Commit letter",
        .set_word_gap = "Auto space",
        .val_on = "ON",
        .val_off = "OFF",
        .about = "San Morse 1.0\n"
                 "\n"
                 "DECISION TREE\n"
                 "OK is the Morse key:\n"
                 "quick release = dot\n"
                 "hold = dash\n"
                 "A short pause commits\n"
                 "the letter; a longer\n"
                 "pause adds a space.\n"
                 "Left = commit now\n"
                 "Right = play it back\n"
                 "Up = undo symbol /\n"
                 " delete letter\n"
                 "Down = space\n"
                 "Back = root / menu\n"
                 "Numbers and signs sit\n"
                 "one level deeper: at\n"
                 "level 4 the view\n"
                 "zooms in to show them.\n"
                 "\n"
                 "TEXT TO MORSE\n"
                 "Type a text and play\n"
                 "it with sound, light\n"
                 "and vibration.\n"
                 "\n"
                 "PLAYBACK\n"
                 "OK = pause / replay\n"
                 "Up/Down = WPM\n"
                 "Left = sound on/off\n"
                 "Right = vibro on/off\n"
                 "Back = stop\n"
                 "\n"
                 "SETTINGS\n"
                 "Volume, tone, LED,\n"
                 "vibration, speed and\n"
                 "key timings. Saved to\n"
                 "the SD card.\n",
    },
    // Espanol
    {
        .menu_tree = "Arbol de decision",
        .menu_text = "Texto a Morse",
        .menu_settings = "Ajustes",
        .menu_about = "Ayuda",
        .input_header = "Texto a reproducir",
        .hint_key = "OK: corto=.  mantener=-",
        .play_playing = "Reproduciendo",
        .play_paused = "Pausado",
        .play_done = "Fin",
        .play_ok_pause = "OK:pausa",
        .play_ok_resume = "OK:seguir",
        .play_ok_repeat = "OK:repetir",
        .snd_on = "Son:SI",
        .snd_off = "Son:no",
        .vib_on = "Vib:SI",
        .vib_off = "Vib:no",
        .set_lang = "Idioma",
        .set_sound = "Sonido",
        .set_volume = "Volumen",
        .set_tone = "Tono",
        .set_vibro = "Vibracion",
        .set_led = "LED",
        .set_wpm = "Velocidad WPM",
        .set_dit = "Umbral raya",
        .set_letter_gap = "Confirma letra",
        .set_word_gap = "Espacio auto",
        .val_on = "SI",
        .val_off = "NO",
        .about = "San Morse 1.0\n"
                 "\n"
                 "ARBOL DE DECISION\n"
                 "OK es la llave Morse:\n"
                 "soltar rapido = punto\n"
                 "mantener = raya\n"
                 "Pausa corta confirma\n"
                 "la letra; pausa larga\n"
                 "agrega un espacio.\n"
                 "Izq = confirmar ya\n"
                 "Der = reproducir\n"
                 "Arriba = deshacer /\n"
                 " borrar letra\n"
                 "Abajo = espacio\n"
                 "Atras = inicio / menu\n"
                 "Los numeros y signos\n"
                 "estan un nivel mas\n"
                 "abajo: al llegar al\n"
                 "nivel 4 la vista hace\n"
                 "zoom y los muestra.\n"
                 "\n"
                 "TEXTO A MORSE\n"
                 "Escribe un texto y se\n"
                 "reproduce con sonido,\n"
                 "luz y vibracion.\n"
                 "\n"
                 "REPRODUCCION\n"
                 "OK = pausa / repetir\n"
                 "Arriba/Abajo = WPM\n"
                 "Izq = sonido si/no\n"
                 "Der = vibracion si/no\n"
                 "Atras = detener\n"
                 "\n"
                 "AJUSTES\n"
                 "Volumen, tono, LED,\n"
                 "vibracion, velocidad\n"
                 "y tiempos de la llave.\n"
                 "Se guardan en la SD.\n",
    },
};
