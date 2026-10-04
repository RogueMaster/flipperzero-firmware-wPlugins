/* Compile the shared value helpers as a separate bridge object so native
 * engine and resident LTO builds can coexist without sharing build nodes. */
#include "../mjs_values.c"
