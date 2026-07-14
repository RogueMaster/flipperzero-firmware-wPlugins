#pragma once
#include <stddef.h>

// Cache de notes par preset, persisté sur SD dans notes_cache.txt.
// Clé = primary tag du preset (ex. "amenity=bench").
// Permet de retrouver "wooden bench" la prochaine fois qu'on sélectionne Bench.

// Charge la note associée au primary_tag dans out. Écrit chaîne vide si absente.
void notes_cache_load(const char* primary_tag, char* out, size_t out_size);

// Enregistre/écrase la note associée au primary_tag. Si note est NULL ou vide,
// supprime l'entrée du cache.
void notes_cache_save(const char* primary_tag, const char* note);
