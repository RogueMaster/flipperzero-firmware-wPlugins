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
static const char* const clipboard_layout_titles[] = {
    "English US",
    "Turkish Q",
    "Bosnian",
    "Czech",
    "Danish",
    "German Swiss",
    "German",
    "Dvorak",
    "English UK",
    "Spanish",
    "Spanish Latin Am.",
    "French Belgium",
    "French Canada",
    "French Swiss",
    "French Mac",
    "French",
    "Croatian",
    "Hungarian",
    "Italian Mac",
    "Italian",
    "Japanese",
    "Norwegian",
    "Dutch",
    "Portuguese Brazil",
    "Portuguese",
    "Slovenian",
    "Slovak",
    "Swedish",
};
static const char* const clipboard_target_titles[] = {"macOS", "Windows", "Linux"};
_Static_assert(
    sizeof(clipboard_layout_titles) / sizeof(clipboard_layout_titles[0]) == CLIPBOARD_LAYOUT_COUNT,
    "Every layout must have a device title");
