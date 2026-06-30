#pragma once

#include "../ibutton_converter_i.h"

void metakom_to_dallas(uint8_t metakom_code[4], uint8_t dallas_code[8], int reversed);

void cyfral_to_dallas_c1(uint8_t cyfral_code[2], uint8_t dallas_code[8]);

void cyfral_to_dallas_c2(uint8_t cyfral_code[2], uint8_t dallas_code[8]);

void cyfral_to_dallas_c2_alt(uint8_t cyfral_code[2], uint8_t dallas_code[8]);

void cyfral_to_dallas_c3(uint8_t cyfral_code[2], uint8_t dallas_code[8]);

void cyfral_to_dallas_c4(uint8_t cyfral_code[2], uint8_t dallas_code[8]);

void cyfral_to_dallas_c5(uint8_t cyfral_code[2], uint8_t dallas_code[8]);

void cyfral_to_dallas_c6(uint8_t cyfral_code[2], uint8_t dallas_code[8]);

void cyfral_to_dallas_c7(uint8_t cyfral_code[2], uint8_t dallas_code[8]);
