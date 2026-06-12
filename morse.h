#pragma once

#include <stdint.h>

// Cantidad de nodos del arbol binario (profundidad 5): indice 0 = raiz,
// hijo punto = 2i+1, hijo raya = 2i+2. El nivel 5 contiene los numeros
// y algunos signos.
#define MORSE_TREE_NODES 63

// Caracter de cada nodo del arbol ('\0' si el nodo no tiene caracter).
extern const char morse_tree_letters[MORSE_TREE_NODES];

// Devuelve el codigo Morse (cadena de '.' y '-') de un caracter, o NULL.
const char* morse_lookup(char c);
