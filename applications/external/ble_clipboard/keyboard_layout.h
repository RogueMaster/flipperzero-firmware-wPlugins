#pragma once
#include <stddef.h>
#include <stdint.h>

// Stable IDs shared with KeyboardLayout.swift. US=0 and Turkish Q=1 are legacy.
static const char* const clipboard_layout_files[] = {
    NULL,           "tr-TR.kl", "ba-BA.kl", "cz_CS.kl", "da-DA.kl",     "de-CH.kl", "de-DE.kl",
    "dvorak.kl",    "en-UK.kl", "es-ES.kl", "es-LA.kl", "fr-BE.kl",     "fr-CA.kl", "fr-CH.kl",
    "fr-FR-mac.kl", "fr-FR.kl", "hr-HR.kl", "hu-HU.kl", "it-IT-mac.kl", "it-IT.kl", "ja-JP.kl",
    "nb-NO.kl",     "nl-NL.kl", "pt-BR.kl", "pt-PT.kl", "si-SI.kl",     "sk-SK.kl", "sv-SE.kl",
};
#define CLIPBOARD_LAYOUT_COUNT (sizeof(clipboard_layout_files) / sizeof(clipboard_layout_files[0]))
