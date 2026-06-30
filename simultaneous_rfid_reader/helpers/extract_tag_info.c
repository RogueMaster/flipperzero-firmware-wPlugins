#include "extract_tag_info.h"

/**
 * @brief      Function to extract the EPC value
 * @details    This function extracts the EPC from the YRM100 Buffer
 * @param      array  The uint8_t* array 
 * @param      length. The length of the array
 * @return     The converted value from the given input
*/
char* convertToHexString(uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        return " ";
    }
    FuriString* temp_str = furi_string_alloc();

    for(size_t i = 0; i < length; i++) {
        furi_string_cat_printf(temp_str, "%02X", array[i]);
    }
    const char* furi_str = furi_string_get_cstr(temp_str);

    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(sizeof(char) * str_len);

    memcpy(str, furi_str, str_len);
    furi_string_free(temp_str);
    return str;
}

/**
 * @brief      Function to extract the TID value from the saved epcs file
 * @details    This function extracts the TID from the saved epcs file.
 * @param      Input  The char* input for the saved UHF tag in the file
 * @return     The TID value from the given input
*/
char* extract_epc(const char* Input) {
    
    // Find the position of the first colon
    const char* FirstColonPos = strchr(Input, ':');
    
    if(FirstColonPos != NULL) {
        
        const char* StartOfSecondValue = FirstColonPos + 1;
        // Move to the character after the first colon
        const char* SecondColonPos = strchr(FirstColonPos + 1, ':');
       
        if(SecondColonPos != NULL) {
            size_t SecondValueLen = SecondColonPos - StartOfSecondValue;
            // Move to the character after the second colon
            char* Result = malloc(SecondValueLen + 1);
                
                if(Result != NULL) {
                    
                    // Copy the third value into the result
                    strncpy(Result, StartOfSecondValue, SecondValueLen);

                    // Null-terminate the string
                    Result[SecondValueLen] = '\0'; 
                    return Result;
                }

        }
    }

    return NULL;
}

/**
 * @brief      Function to extract the Reserved Memory value from the saved epcs file
 * @details    This function extracts the Reserved Memory from the saved epcs file.
 * @param      Input  The char* input for the saved UHF tag in the file
 * @return     The Reserved Memory value from the given input
*/
char* extract_res(const char* Input) {
    
    // There is probably a better way to do this...
    // Find the position of the first colon
    const char* FirstColonPos = strchr(Input, ':');
    
    if(FirstColonPos != NULL) {
        
        // Move to the character after the first colon
        const char* SecondColonPos = strchr(FirstColonPos + 1, ':');
        
        if(SecondColonPos != NULL) {
            
            // Move to the character after the second colon
            const char* ThirdColonPos = strchr(SecondColonPos + 1, ':');
            
            if(ThirdColonPos != NULL) {
                
                // Move to the character after the third colon
                const char* StartOfFourthValue = ThirdColonPos + 1;

                // Find the next colon after the start of the fourth value
                const char* FourthColonPos = strchr(StartOfFourthValue, ':');
                
                if(FourthColonPos != NULL) {
                    
                    // Calculate the length of the fourth value by subtracting the positions
                    size_t FourthValueLen = FourthColonPos - StartOfFourthValue;

                    // Allocate memory for the fourth value
                    char* Result = malloc(FourthValueLen + 1);
                    
                    if(Result != NULL) {
                        
                        // Copy the fourth value into the result
                        strncpy(Result, StartOfFourthValue, FourthValueLen);
                        
                        // Null-terminate the string
                        Result[FourthValueLen] = '\0'; 

                        return Result;
                    }
                }
            }
        }
    }

    return NULL;
}

/**
 * @brief      Function to extract the sixth value from the saved epcs file
 * @details    This function extracts the sixth value from the saved epcs file.
 * @param      Input  The char* input for the saved UHF tag in the file
 * @return     The sixth value from the given input
 */
char* extract_pc(const char* Input) {
    
    // Find the position of the first colon
    const char* FirstColonPos = strchr(Input, ':');
    
    if(FirstColonPos != NULL) {
        
        // Move to the character after the first colon
        const char* SecondColonPos = strchr(FirstColonPos + 1, ':');
        
        if(SecondColonPos != NULL) {
            
            // Move to the character after the second colon
            const char* ThirdColonPos = strchr(SecondColonPos + 1, ':');
            
            if(ThirdColonPos != NULL) {
                
                // Move to the character after the third colon
                const char* FourthColonPos = strchr(ThirdColonPos + 1, ':');
                
                if(FourthColonPos != NULL) {
                    
                    // Move to the character after the fourth colon
                    const char* FifthColonPos = strchr(FourthColonPos + 1, ':');
                    
                    if(FifthColonPos != NULL) {
                        
                        // Move to the character after the fifth colon
                        const char* StartOfSixthValue = FifthColonPos + 1;
                        
                        // Find the next colon after the start of the sixth value
                        const char* SixthColonPos = strchr(StartOfSixthValue, ':');
                        
                        // Calculate the length of the sixth value
                        size_t SixthValueLen;
                        if (SixthColonPos != NULL) {
                            SixthValueLen = SixthColonPos - StartOfSixthValue;
                        } else {
                            // If there is no sixth colon, take the rest of the string
                            SixthValueLen = strlen(StartOfSixthValue);
                        }

                        // Allocate memory for the sixth value
                        char* Result = malloc(SixthValueLen + 1);
                        
                        if(Result != NULL) {
                            
                            // Copy the sixth value into the result
                            strncpy(Result, StartOfSixthValue, SixthValueLen);
                            
                            // Null-terminate the string
                            Result[SixthValueLen] = '\0';
                            
                            return Result;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}


/**
 * @brief      Function to extract the TID value from the saved epcs file
 * @details    This function extracts the TID from the saved epcs file.
 * @param      Input  The char* input for the saved UHF tag in the file
 * @return     The TID value from the given input
*/
char* extract_tid(const char* Input) {
    
    // Find the position of the first colon
    const char* FirstColonPos = strchr(Input, ':');
    
    if(FirstColonPos != NULL) {
        
        // Move to the character after the first colon
        const char* SecondColonPos = strchr(FirstColonPos + 1, ':');
        
        if(SecondColonPos != NULL) {
            
            // Move to the character after the second colon
            const char* StartOfThirdValue = SecondColonPos + 1;

            // Find the next colon after the start of the third value
            const char* ThirdColonPos = strchr(StartOfThirdValue, ':');
            
            if(ThirdColonPos != NULL) {
                
                // Calculate the length of the third value by subtracting the positions
                size_t ThirdValueLen = ThirdColonPos - StartOfThirdValue;

                // Allocate memory for the third value
                char* Result = malloc(ThirdValueLen + 1);
                
                if(Result != NULL) {
                    
                    // Copy the third value into the result
                    strncpy(Result, StartOfThirdValue, ThirdValueLen);

                    // Null-terminate the string
                    Result[ThirdValueLen] = '\0'; 
                    return Result;
                }
            }
        }
    }

    return NULL;
}


/**
 * @brief      Function to extract the fifth value from the saved epcs file
 * @details    This function extracts the fifth value from the saved epcs file.
 * @param      Input  The char* input for the saved UHF tag in the file
 * @return     The fifth value from the given input
 */
char* extract_mem(const char* Input) {
    
    // Find the position of the first colon
    const char* FirstColonPos = strchr(Input, ':');
    
    if(FirstColonPos != NULL) {
        
        // Move to the character after the first colon
        const char* SecondColonPos = strchr(FirstColonPos + 1, ':');
        
        if(SecondColonPos != NULL) {
            
            // Move to the character after the second colon
            const char* ThirdColonPos = strchr(SecondColonPos + 1, ':');
            
            if(ThirdColonPos != NULL) {
                
                // Move to the character after the third colon
                const char* FourthColonPos = strchr(ThirdColonPos + 1, ':');
                
                if(FourthColonPos != NULL) {
                    
                    // Move to the character after the fourth colon
                    const char* StartOfFifthValue = FourthColonPos + 1;
                    
                    // Find the next colon after the start of the fifth value
                    const char* FifthColonPos = strchr(StartOfFifthValue, ':');
                    
                    if(FifthColonPos != NULL) {
                        
                        // Calculate the length of the fifth value by subtracting the positions
                        size_t FifthValueLen = FifthColonPos - StartOfFifthValue;
                        
                        // Allocate memory for the fifth value
                        char* Result = malloc(FifthValueLen + 1);
                        
                        if(Result != NULL) {
                            
                            // Copy the fifth value into the result
                            strncpy(Result, StartOfFifthValue, FifthValueLen);
                            
                            // Null-terminate the string
                            Result[FifthValueLen] = '\0';
                            
                            return Result;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}
/**
 * @brief      Function to extract the crc value from the saved epcs file
 * @details    This function extracts the crc from the saved epcs file.
 * @param      Input  The char* input for the saved UHF tag in the file
 * @return     The crc value from the given input
*/
char* extract_crc(const char* Input) {

    // Find the position of the last colon
    const char* LastColonPos = strrchr(Input, ':'); 
    
    if(LastColonPos != NULL) {
        
        // Move to the character after the last colon
        const char* StartOfLastValue = LastColonPos + 1;

        // Calculate the length of the last value
        size_t LastValueLen = strlen(StartOfLastValue);

        // Allocate memory for the last value
        char* Result = malloc(LastValueLen + 1);
        
        if(Result != NULL) {
            
            // Copy the last value into the result
            strncpy(Result, StartOfLastValue, LastValueLen);

            // Null-terminate the string
            Result[LastValueLen] = '\0'; 

            return Result;
        }
    }

    return NULL;
}

/**
 * @brief      Function to extract the Name from the saved epcs file
 * @details    This function extracts the Name from the saved epcs file.
 * @param      Input  The char* input for the saved UHF tag in the file
 * @return     The Name from the given input
*/
char* extract_name(const char* Input) {
    
    // Find the position of the colon
    const char* ColonPos = strchr(Input, ':');

    if(ColonPos != NULL) {
        
        // Calculate the length of the name
        size_t Len = ColonPos - Input;

        // Allocate memory for the name
        char* Result = malloc(Len + 1);

        if(Result != NULL) {
           
            // Copy the name into the result
            strncpy(Result, Input, Len);

            // Null-terminate the string
            Result[Len] = '\0'; 
            return Result;
        }
    }
    return NULL;
}
