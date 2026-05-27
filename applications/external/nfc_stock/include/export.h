#pragma once
#include "stock.h"
#include <stdbool.h>
#include <stddef.h>

bool stock_export_csv(const char *filepath, StockItem *items, size_t count);
