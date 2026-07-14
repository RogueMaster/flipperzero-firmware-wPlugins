#pragma once

typedef struct Passy Passy;

bool passy_load_mrz_info(Passy* passy);
bool passy_save_mrz_info(Passy* passy);
bool passy_delete_mrz_info(Passy* passy);
