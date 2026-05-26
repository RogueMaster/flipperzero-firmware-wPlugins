#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "VolorSavanna.h"

int main()
{
    char pause[1];
    printf("%s",  name_prompt);
    if (scanf("%s",  name) != 1)
    {
        return 1;
    }

    while (true)
    {
        printf("%s",  character_prompt);;
        if (scanf("%s",  character) != 1)
        {
            return 1;
        }
        
        if (strcmp(character,  "e") == 0)
        {
            return 0;
        }
            
        else
        {
            while (true)
            {
                printf("%s",  VolorSavannaGame());
                free(result);
                if (level == 0)
                {
                    printf("%s",  "\nPress any key to continue!");
                    if (scanf("%s",  pause) != 1)
                    {
                        return 1;
                    }
                    break;
                }

                else if (level == -1)
                {
                    level = 0;
                    break;
                }
                
                if (scanf("%s",  choice) != 1)
                {
                    return 1;
                }
            }

        }
    }
    
    return 0;
}
