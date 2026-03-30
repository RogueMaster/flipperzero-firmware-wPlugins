// Thanks to atc1441
// https://gist.github.com/atc1441/41af75048e4c22af1f5f0d4c1d94bb56

#include <furi.h>

uint16_t CRC16(uint16_t crc, uint8_t *buffer, int len) // Default CRC16 Algo
{
  while(len--)
  {
    crc ^= *buffer++ << 8;
    int bits = 0;
    do
    {
      if ( (crc & 0x8000) != 0 )
        crc = (2 * crc) ^ 0x1021;
      else
        crc *= 2;
    }
    while ( ++bits < 8 );
  }
  return crc;
}

//uint8_t nfctag_uid[] = {0x04,0xEC,0xFC,0xA2,0x94,0x10,0x90}; // NTAG UID
//uint8_t nfc_second[] = "221214 12K"; // Head MFG String, printed on Head and at memory location 0x23

uint32_t get_sonicare_password(uint8_t nfctag_uid[], uint8_t nfc_second[]) {
    /*
    for (uint16_t i=0; i<7; i++) {
        FURI_LOG_D("sonicare_password", "Input UID byte %i: 0x%02x", i, nfctag_uid[i]);
    }
    for (uint16_t i=0; i<10; i++) {
        FURI_LOG_D("sonicare_password", "Input MFG byte %i: 0x%02x", i, nfc_second[i]);
    }
    */
    uint32_t crc_calc = CRC16(0x49A3, nfctag_uid, 7); // Calculate the NTAG UID CRC
    FURI_LOG_D("sonicare_password", "CRC16 of UID: 0x%08lx", crc_calc);

    crc_calc = crc_calc | (CRC16(crc_calc, nfc_second, 10) << 16); // Calculate the MFG CRC
    FURI_LOG_D("sonicare_password", "CRC16 with MFG: 0x%08lx", crc_calc);
    
    crc_calc = ((crc_calc >> 8) & 0x00FF00FF) | ((crc_calc << 8) & 0xFF00FF00); // Rotate the uin16_t bytes
    FURI_LOG_D("sonicare_password", "Final NFC password: 0x%08lx", crc_calc);
    
    return crc_calc;
}
