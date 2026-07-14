#pragma once

// Translation for lab/quiz content. Content strings live in English in
// pocketlab_content.c; this returns the current-language version of one, keyed
// by string content so identical English text translates the same everywhere.
// Falls back to the English original when no translation exists (so untranslated
// labs keep working). Flipper on-device menu names are intentionally left in
// English inside instructions.
const char* pocketlab_tr(const char* en);
