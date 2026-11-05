#pragma once

// DG1 del DNIe: 61 L { 5F1F L <MRZ TD1: 3 lineas x 30 caracteres> }  (ICAO 9303-5)
//
//  Linea 1: tipo(2) emisor(3) n_documento(9) control(1) opcional(15)  <- el DNIe pone aqui el DNI
//  Linea 2: nacimiento(6) control(1) sexo(1) caducidad(6) control(1) nacionalidad(3)
//           opcional(11) control_compuesto(1)
//  Linea 3: APELLIDOS<<NOMBRE<NOMBRE2

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DNIE_MRZ_LEN     90
#define DNIE_MRZ_TD3_LEN 88 // pasaporte: 2 lineas x 44

typedef struct {
    char mrz[DNIE_MRZ_LEN + 1]; // MRZ en bruto

    char doc_type[3];
    char issuer[4];
    char doc_number[10]; // numero de soporte (IDESP...)
    char personal_number[16]; // en el DNIe: el numero de DNI con letra
    char birth[7]; // AAMMDD
    char sex;
    char expiry[7]; // AAMMDD
    char nationality[4];
    char surnames[31];
    char given_names[31];

    // Digitos de control (detectan errores de lectura o manipulacion de la MRZ)
    bool check_doc_number;
    bool check_birth;
    bool check_expiry;
    bool check_composite;
} DnieMrz;

// Digito de control ICAO: pesos 7,3,1; 0-9 = valor, A-Z = 10..35, '<' = 0
int dnie_mrz_check_digit(const char* s, size_t len);

// Parsea DG1 completo (con su TLV): TD1 (DNIe) o TD3 (pasaporte).
bool dnie_mrz_parse_dg1(const uint8_t* dg1, size_t len, DnieMrz* out);

bool dnie_mrz_all_checks_ok(const DnieMrz* mrz);
