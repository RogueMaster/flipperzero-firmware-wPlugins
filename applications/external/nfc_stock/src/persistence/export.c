#include "include/export.h"
#include "include/fs_compat.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifdef HOST_BUILD

bool stock_export_csv(const char *filepath, StockItem *items, size_t count) {
  FILE *f = fopen(filepath, "w");
  if (!f)
    return false;

  fprintf(f, "Name,Quantity,MinQty,Location,Alert\n");

  for (size_t i = 0; i < count; i++) {
    bool alerta = (items[i].quantity < items[i].min_quantity);
    fprintf(f, "%s,%" PRId32 ",%" PRId32 ",%s,%s\n", items[i].name,
            items[i].quantity, items[i].min_quantity, items[i].location,
            alerta ? "LOW_STOCK" : "OK");
  }

  fclose(f);
  return true;
}

#else

bool stock_export_csv(const char *filepath, StockItem *items, size_t count) {
  char line[192];
  int n = snprintf(line, sizeof(line), "Name,Quantity,MinQty,Location,Alert\n");
  if (n < 0 || (size_t)n >= sizeof(line))
    return false;
  if (!fs_write_replace(filepath, line, (size_t)n))
    return false;

  for (size_t i = 0; i < count; i++) {
    bool alerta = (items[i].quantity < items[i].min_quantity);
    n = snprintf(line, sizeof(line), "%s,%" PRId32 ",%" PRId32 ",%s,%s\n",
                 items[i].name, items[i].quantity, items[i].min_quantity,
                 items[i].location, alerta ? "LOW_STOCK" : "OK");
    if (n < 0 || (size_t)n >= sizeof(line))
      return false;
    if (!fs_append_bytes(filepath, line, (size_t)n))
      return false;
  }
  return true;
}

#endif
