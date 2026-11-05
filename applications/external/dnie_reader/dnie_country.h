#pragma once

// Nombre del pais a partir del codigo ICAO de 3 letras de la MRZ (Alemania es "D").
// Si no lo conocemos, devuelve el propio codigo.
const char* dnie_country_name(const char* code);
