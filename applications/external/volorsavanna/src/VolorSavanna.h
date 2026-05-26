#include <stdlib.h>
#include <string.h>

char * result;
char name[256];
char character[256];
char choice[4];
int level = 0;
int new_rand;
char name_prompt[22] = "What is your name?\n";
char character_prompt[160] = "Welcome to Volor Savanna!\nYou are a member of an African Tribe!\nWho do you want to be?\n\n1- Hunter\n2- Warrior\n3- Crafter\n4- Farmer\n5- Medicine Person\ne- exit\n";

static inline const char * death_level(char * name)
{
    size_t length = strlen(name) + strlen(", YOU LOSE!\nTHE END!") + 1;
    char * result = (char *)malloc(length);
    strcpy(result, name);
    strcpy(result + strlen(result), ", YOU LOSE!\nTHE END!");
    return result;
}

static inline const char * victory_level(char * name)
{
    size_t length = strlen(name) + strlen(", YOU WIN!\nTHE END!") + 1;
    char * result = (char *)malloc(length);
    strcpy(result, name);
    strcpy(result + strlen(result), ", YOU WIN!\nTHE END!");
    return result;
}

static inline const char * VolorSavannaGame()
{   
    if (strcmp(character, "1") == 0 && level == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to be a hunter!\nDo you want to go hunt or stay put?\n\n1- Hunt; 2- Stay put\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to be a hunter!\nDo you want to go hunt or stay put?\n\n1- Hunt; 2- Stay put\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 1;
        return result;
    }
    
    if (strcmp(character, "1") == 0 && level == 1 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go hunt!\nYou have gone hunting.\nYou didn't find any food.\nYou return to your village.\nLuckily, the farmer has enough crops to last your tribe a week.\nDo you want to go hunt right away to make up lost time or do you choose to go eat?\n\n1- Hunt; 2- Eat\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go hunt!\nYou have gone hunting.\nYou didn't find any food.\nYou return to your village.\nLuckily, the farmer has enough crops to last your tribe a week.\nDo you want to go hunt right away to make up lost time or do you choose to go eat?\n\n1- Hunt; 2- Eat\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 2;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 1 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to stay put.\nUnfortunately, your whole village parishes from hunger.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to stay put.\nUnfortunately, your whole village parishes from hunger.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 2 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go hunt!\nYou go on a successful hunt that should last your tribe a month.\nHowever, you are very tired and because you didn't eat, you have become very sick with malnutrition.\nUnluckily for you, the tribes medicine women is very busy and has a lot of patients to take care of first.\nDo you wish to wait for her or tough it out?\n\n1- Wait for her; 2- Tough it out\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go hunt!\nYou go on a successful hunt that should last your tribe a month.\nHowever, you are very tired and because you didn't eat, you have become very sick with malnutrition.\nUnluckily for you, the tribes medicine women is very busy and has a lot of patients to take care of first.\nDo you wish to wait for her or tough it out?\n\n1- Wait for her; 2- Tough it out\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 3;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 2 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to eat.\nYou are fully fueled with energy.\nDo you want to go hunting or play a game with the rest of your tribe?\n\n1- Hunt; 2- Play a game with the rest of my tribe\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to eat.\nYou are fully fueled with energy.\nDo you want to go hunting or play a game with the rest of your tribe?\n\n1- Hunt; 2- Play a game with the rest of my tribe\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 3 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to wait for her.\nBecause You have chosen to wait for her patiently, she attends to you in a matter of a couple days.\nYou are better in a week.\nSome white men have come to your tribe.\nThey're asking to trade you guns for some of your tribe's animal pelts.\nDo you trade them pelts for guns?\n\n1- Trade; 2- Not trade\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result,  "You have chosen to wait for her.\nBecause You have chosen to wait for her patiently, she attends to you in a matter of a couple days.\nYou are better in a week.\nSome white men have come to your tribe.\nThey're asking to trade you guns for some of your tribe's animal pelts.\nDo you trade them pelts for guns?\n\n1- Trade; 2- Not trade\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 5;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 3 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to tough it out.\nUnfortunately, because of this, you die from malnutrition.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to tough it out.\nUnfortunately, because of this, you die from malnutrition.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 4 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to hunt.\nYou have found a bunch of zebras to eat.\nSome white men are willing to trade your tribe rifles for some of your zebras.\nRecently, there has been some highly wanted criminals that trade weapons and drugs on the black market.\nThe problem is, you don't know if that's them.\nDo you trade?\nWhat about decline the offer?\nOr do you report them to the authorities?\n\n1- Trade; 2- Decline the trade; 3- Report them to the authorities\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result,  "You have chosen to hunt.\nYou have found a bunch of zebras to eat.\nSome white men are willing to trade your tribe rifles for some of your zebras.\nRecently, there has been some highly wanted criminals that trade weapons and drugs on the black market.\nThe problem is, you don't know if that's them.\nDo you trade?\nWhat about decline the offer?\nOr do you report them to the authorities?\n\n1- Trade; 2- Decline the trade; 3- Report them to the authorities\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 6;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 4 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to play a game with the rest of your tribe.\nEveryone has a good time and you guys have an awesome feast.\nThere is a hunting challenge that your tribe is having.\nThe challenge is to go hunt The Mighty Lion!\nDo you want to go hunt The Mighty Lion with your tribe to win the challenge?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to play a game with the rest of your tribe.\nEveryone has a good time and you guys have an awesome feast.\nThere is a hunting challenge that your tribe is having.\nThe challenge is to go hunt The Mighty Lion!\nDo you want to go hunt The Mighty Lion with your tribe to win the challenge?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 7;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 5 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to trade.\nThey trade you guns.\nDo you want to go hunt The Mighty Lion?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to trade.\nThey trade you guns.\nDo you want to go hunt The Mighty Lion?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 8;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 5 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to trade.\nBecause of this they want to kill you.\nDo you escape?\n\n1- Find out!\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to trade.\nBecause of this they want to kill you.\nDo you escape?\n\n1- Find out!\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 9;
        new_rand = rand() % 2 + 1;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 6 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to trade.\nLuckily, these aren't the criminals that are wanted.\nA couple months later the white men settle here.\nPeople keep on flooding into the white men's settlement.\nBecause of this your village becomes rich.\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to trade.\nLuckily, these aren't the criminals that are wanted.\nA couple months later the white men settle here.\nPeople keep on flooding into the white men's settlement.\nBecause of this your village becomes rich.\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 6 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to decline the trade.\nUnfortunately, these people have smallpox.\nThey transmit it to your tribe.\nBecause you tribe doesn't have any immunity against smallpox your whole tribe dies!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to decline the trade.\nUnfortunately, these people have smallpox.\nThey transmit it to your tribe.\nBecause you tribe doesn't have any immunity against smallpox your whole tribe dies!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 6 && strcmp(choice, "3") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to report them to the authorities.\nThese guys turn out to be the wanted criminals after all.\nYour tribe gains importance in the African community because you turned these criminals into the authorities.\nDo you want to go hunt The Mighty Lion?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to report them to the authorities.\nThese guys turn out to be the wanted criminals after all.\nYour tribe gains importance in the African community because you turned these criminals into the authorities.\nDo you want to go hunt The Mighty Lion?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 10;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 7 && strcmp(choice, "1") == 0)
    {
        level = 0;
        strncpy(character, "the_mighty_lion", sizeof(character));
    }

    if (strcmp(character, "1") == 0 && level == 7 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to hunt The Mighty Lion.\nUnfortunately, you die from malaria.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to hunt The Mighty Lion.\nUnfortunately, you die from malaria.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 8 && strcmp(choice, "1") == 0)
    {
        level = 0;
        strncpy(character, "the_mighty_lion", sizeof(character));
    }

    if (strcmp(character, "1") == 0 && level == 8 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to hunt The Mighty Lion.\nUnfortunately, a random lightning bolt from out of nowhere, probably from Zeus, kills you.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to hunt The Mighty Lion.\nUnfortunately, a random lightning bolt from out of nowhere, probably from Zeus, kills you.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 9 && strcmp(choice, "1") == 0 && new_rand == 1)
    {
        size_t length = strlen(name) + strlen("Unfortunately, you don't escape.\nThey kill you on the spot with a feather.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "Unfortunately, you don't escape.\nThey kill you on the spot with a feather.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 9 && strcmp(choice, "1") == 0 && new_rand == 2)
    {
        size_t length = strlen(name) + strlen("You manage to escape.\nYou live a long and prosperous life!\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You manage to escape.\nYou live a long and prosperous life!\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "1") == 0 && level == 10 && strcmp(choice, "1") == 0)
    {
        level = 0;
        strncpy(character, "the_mighty_lion", sizeof(character));
    }

    if (strcmp(character, "1") == 0 && level == 10 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to hunt The Mighty Lion.\nUnfortunately, a Chandra Planeswalker burns you to death!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to hunt The Mighty Lion.\nUnfortunately, a Chandra Planeswalker burns you to death!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to be a warrior!\nDo you want to go hunt The Mighty Lion or stay put?\n\n1- Hunt The Mighty Lion; 2- Stay put\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to be a warrior!\nDo you want to go hunt The Mighty Lion or stay put?\n\n1- Hunt The Mighty Lion; 2- Stay put\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 1;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 1 && strcmp(choice, "1") == 0)
    {
        level = 0;
        strncpy(character, "the_mighty_lion", sizeof(character));
    }

    if (strcmp(character, "2") == 0 && level == 1 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to stay put.\nWithout warning a neighboring tribe attacks.\nDo you choose to get your bow and arrows in your hut, or do you choose to fight them off with your knife on hand?\n\n1- Go get my bow and arrows; 2- Use my knife\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to stay put.\nWithout warning a neighboring tribe attacks.\nDo you choose to get your bow and arrows in your hut, or do you choose to fight them off with your knife on hand?\n\n1- Go get my bow and arrows; 2- Use my knife\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 2;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 2 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go get your bow and arrows.\nUnfortunately, the enemy beats you to it and shoots you on the spot.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go get your bow and arrows.\nUnfortunately, the enemy beats you to it and shoots you on the spot.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 2 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to use your knife.\nYou are very skilled with a knife.\nEventually the enemy tribe retreats.\nThe chief has told you to go take a weekend off at the Big Pond.\nYou go to the Big Pond and enjoy it very much.\nYou see a crying baby by the side of the Big Pond.\nDo you pick up the baby and bring him home or just ignore him?\n\n1- Pick him up and bring him home; 2- Just ignore him\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to use your knife.\nYou are very skilled with a knife.\nEventually the enemy tribe retreats.\nThe chief has told you to go take a weekend off at the Big Pond.\nYou go to the Big Pond and enjoy it very much.\nYou see a crying baby by the side of the Big Pond.\nDo you pick up the baby and bring him home or just ignore him?\n\n1- Pick him up and bring him home; 2- Just ignore him\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 3;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 3 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to pick up the baby and bring him home.\nThe baby turns out to be an orphan.\nHis mother was killed by the enemy tribe and the baby was left by the Big Pond.\nYou raise him as your own.\nHe becomes a great warrior and you name him Bobert.\nYou need to go hunt down the enemy that attacked your tribe years ago.\nDo you take your son with you?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to pick up the baby and bring him home.\nThe baby turns out to be an orphan.\nHis mother was killed by the enemy tribe and the baby was left by the Big Pond.\nYou raise him as your own.\nHe becomes a great warrior and you name him Bobert.\nYou need to go hunt down the enemy that attacked your tribe years ago.\nDo you take your son with you?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 3 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to just ignore him.\nDo you choose to go after the enemy tribe with the few warriors you have left?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to just ignore him.\nDo you choose to go after the enemy tribe with the few warriors you have left?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 5;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 4 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to take your son with you.\nYour hunting party would have died if you didnt have your son.\nSince your son is a great warrior the enemy tribe which you now know as Yosemite plans on never attacking your tribe again!\nYou return home and find out your son has a brand-new kid.\nBobert, your son, names him Mighty ") + strlen(name) + strlen(" Jr.\nYou finally have a grandson.\nYou die at a good old age.\nYou pass away happily in your sleep.\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to take your son with you.\nYour hunting party would have died if you didnt have your son.\nSince your son is a great warrior the enemy tribe which you now know as Yosemite plans on never attacking your tribe again!\nYou return home and find out your son has a brand-new kid.\nBobert, your son, names him Mighty ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), " Jr.\nYou finally have a grandson.\nYou die at a good old age.\nYou pass away happily in your sleep.\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 4 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to take your son with you.\nBecause you didnt take your son, the enemy tribe, Yosemite, kills your whole hunting party.\nYou guys are no match for them!\nUnfortunately, while running away from Yosemite you trip over a rock and die!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to take your son with you.\nBecause you didnt take your son, the enemy tribe, Yosemite, kills your whole hunting party.\nYou guys are no match for them!\nUnfortunately, while running away from Yosemite you trip over a rock and die!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 5 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You go after the enemy tribe with the few warriors you have left.\nUnfortunately, while on your way The Mighty Pac-Man gobbles you up.\nYou guys are no match for the The Mighty Pac-Man!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You go after the enemy tribe with the few warriors you have left.\nUnfortunately, while on your way The Mighty Pac-Man gobbles you up.\nYou guys are no match for the The Mighty Pac-Man!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "2") == 0 && level == 5 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to go after the enemy tribe.\nUnfortunately, your village gets swept away by a flash flood!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to go after the enemy tribe.\nUnfortunately, your village gets swept away by a flash flood!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to be a crafter!\nYou need to craft some arrows.\nHow do you want to craft them?\n1- Put red dye on the feathers; 2- Just make them\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to be a crafter!\nYou need to craft some arrows.\nHow do you want to craft them?\n1- Put red dye on the feathers; 2- Just make them\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 1;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 1 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to put red dye on the feathers!\nBecause you put red dye on the feathers none were lost so the hunters were able to bring home more food!\nBecause of this you have one months worth of food for the winter!\nYou have been asked to craft some spearheads.\nHow do you want to craft them?\n1- Put poison on the spearheads; 2- Just make them\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to put red dye on the feathers!\nBecause you put red dye on the feathers none were lost so the hunters were able to bring home more food!\nBecause of this you have one months worth of food for the winter!\nYou have been asked to craft some spearheads.\nHow do you want to craft them?\n1- Put poison on the spearheads; 2- Just make them\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 2;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 1 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to just make the arrows.\nUnfortunately, because of this the hunters lost some arrows and didnt bring home much food.\nEven worse than that, Master Chief comes into your tribe and pwns all of you!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to just make the arrows.\nUnfortunately, because of this the hunters lost some arrows and didnt bring home much food.\nEven worse than that, Master Chief comes into your tribe and pwns all of you!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 2 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to put poison on the spear heads.\nThe animals die easier and your tribe gets more food for the winter.\nTo be exact one more month of food.\nSome white men come to your tribe.\nTheyre paying a fortune for you to craft them some of your best acacia furniture.\nDo you want to craft some expensive acacia furniture for them, or do you not want to craft them the furniture their asking for?\n1- Craft them furniture; 2- Not craft them furniture\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to put poison on the spear heads.\nThe animals die easier and your tribe gets more food for the winter.\nTo be exact one more month of food.\nSome white men come to your tribe.\nTheyre paying a fortune for you to craft them some of your best acacia furniture.\nDo you want to craft some expensive acacia furniture for them, or do you not want to craft them the furniture their asking for?\n1- Craft them furniture; 2- Not craft them furniture\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 3;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 2 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to just make the spear heads.\nBecause you didnt put poison on the spear heads the hunters only brought back three gazelles.\nUnfortunately, the gazelles get mold on them.\nEven worse, you eat the moldy gazelles and die from food poisoning.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to just make the spear heads.\nBecause you didnt put poison on the spear heads the hunters only brought back three gazelles.\nUnfortunately, the gazelles get mold on them.\nEven worse, you eat the moldy gazelles and die from food poisoning.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 3 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to craft them furniture.\nThe people you give the furniture to like your work.\nThey pay you in gold.\nYou can use this to buy a milling machine from a city nearby.\nDo you buy a milling machine?\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to craft them furniture.\nThe people you give the furniture to like your work.\nThey pay you in gold.\nYou can use this to buy a milling machine from a city nearby.\nDo you buy a milling machine?\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 3 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to craft them furniture.\nBecause of this they want to kill you.\nDo you escape?\n1- Find out!\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to craft them furniture.\nBecause of this they want to kill you.\nDo you escape?\n1- Find out!\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 5;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 4 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to buy a milling machine.\nSome business men have come to your tribe and are asking you to build them some wooden toys with your milling machine.\nDo you want to build them wooden toys with your milling machine?\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to buy a milling machine.\nSome business men have come to your tribe and are asking you to build them some wooden toys with your milling machine.\nDo you want to build them wooden toys with your milling machine?\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 6;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 4 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to buy a milling machine.\nUnfortunately, you tribe dies from a famine.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to buy a milling machine.\nUnfortunately, you tribe dies from a famine.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 5 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You escape.\nUnfortunately, an asteroid hits the earth and destroys the whole planet.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You escape.\nUnfortunately, an asteroid hits the earth and destroys the whole planet.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 5 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You dont escape.\nThey kill you on the spot.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You dont escape.\nThey kill you on the spot.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 6 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to use your milling machine to make them wooden toys.\nYou have had a successful life!\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to use your milling machine to make them wooden toys.\nYou have had a successful life!\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "3") == 0 && level == 6 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to make them wooden toys with your milling machine.\nUnfortunately, your milling machine catches on fire and burns down your whole house.\nWhats even more unfortunate is that you get caught on fire and burn to death.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to make them wooden toys with your milling machine.\nUnfortunately, your milling machine catches on fire and burns down your whole house.\nWhats even more unfortunate is that you get caught on fire and burn to death.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to be a farmer!\nDo you want to go farm or sleep in?\n1- Farm; 2- Sleep in\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to be a farmer!\nDo you want to go farm or sleep in?\n1- Farm; 2- Sleep in\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 1;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 1 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to farm!\nYou have had a successful season!\nYour village has a feast.\nDo you want to store the rest of the food for winter or distribute the food to the whole village?\n1- Store the food for winter; 2- Distribute the food to the whole village\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to farm!\nYou have had a successful season!\nYour village has a feast.\nDo you want to store the rest of the food for winter or distribute the food to the whole village?\n1- Store the food for winter; 2- Distribute the food to the whole village\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 2;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 1 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to sleep in.\nUnfortunately, a prairie fire destroys your crops and your whole village.\nEveryone has escaped and survived.\nEveryone except for one person.\nYou.\nYou have died in the fire.\nYou were burnt into ashes in your sleep.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to sleep in.\nUnfortunately, a prairie fire destroys your crops and your whole village.\nEveryone has escaped and survived.\nEveryone except for one person.\nYou.\nYou have died in the fire.\nYou were burnt into ashes in your sleep.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 2 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to store the food for winter.\nBecause you have chosen to store the food for winter your tribe doesnt go hungry.\nLast year you grew wheat.\nDo you want to grow wheat again this year or farm corn for the first time in your life?\n1- Grow wheat again; 2- Farm corn\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to store the food for winter.\nBecause you have chosen to store the food for winter your tribe doesnt go hungry.\nLast year you grew wheat.\nDo you want to grow wheat again this year or farm corn for the first time in your life?\n1- Grow wheat again; 2- Farm corn\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 3;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 2 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to distribute the food to the whole village.\nEveryone has enough food for the whole winter.\nUnfortunately, the food rots in a winter storm.\nYour village dies of starvation.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to distribute the food to the whole village.\nEveryone has enough food for the whole winter.\nUnfortunately, the food rots in a winter storm.\nYour village dies of starvation.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 3 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to grow wheat again for the second year in a row.\nUnfortunately, there are no nutrients in the soil so all your wheat dies.\nYour village starves.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to grow wheat again for the second year in a row.\nUnfortunately, there are no nutrients in the soil so all your wheat dies.\nYour village starves.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 3 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to farm corn for the first time in your life.\nLuckily for you, someone else in your village knows how to grow corn.\nBecause of this you grow a lot of corn and your village has a feast.\nDo you want to store the rest of the food for the winter or distribute the rest of the food to your whole village?\n1- Store the food for winter; 2- Distribute the food to your whole village\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to farm corn for the first time in your life.\nLuckily for you, someone else in your village knows how to grow corn.\nBecause of this you grow a lot of corn and your village has a feast.\nDo you want to store the rest of the food for the winter or distribute the rest of the food to your whole village?\n1- Store the food for winter; 2- Distribute the food to your whole village\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 4 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to store the food for winter.\nThe corn lasts your tribe the whole winter.\nYour tribe is thriving!\nYou find out that you have extra corn.\nIt would be nice to get some oxen to help you plow the field.\nDo you sell your extra corn for oxen?\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to store the food for winter.\nThe corn lasts your tribe the whole winter.\nYour tribe is thriving!\nYou find out that you have extra corn.\nIt would be nice to get some oxen to help you plow the field.\nDo you sell your extra corn for oxen?\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 5;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 4 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to distribute the food to your whole village.\nUnfortunately, because you have chosen to do this your food rots.\nYour whole village starves.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to distribute the food to your whole village.\nUnfortunately, because you have chosen to do this your food rots.\nYour whole village starves.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 5 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to sell your extra corn for oxen.\nYou choose to grow cotton.\nAt the end of the season you have surplus cotton.\nDo you want to sell your surplus cotton?\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to sell your extra corn for oxen.\nYou choose to grow cotton.\nAt the end of the season you have surplus cotton.\nDo you want to sell your surplus cotton?\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 6;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 5 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to sell your extra corn for oxen.\nUnfortunately, Bob the Blobfish plops out from a nearby lake and you die because your eyes bleed to death.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to sell your extra corn for oxen.\nUnfortunately, Bob the Blobfish plops out from a nearby lake and you die because your eyes bleed to death.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 6 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to sell your surplus cotton.\nYou are now rich!\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to sell your surplus cotton.\nYou are now rich!\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "4") == 0 && level == 6 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to sell your surplus cotton.\nUnfortunately, a wizard says some magic words that make you vanish.\nYou are never seen again.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to sell your surplus cotton.\nUnfortunately, a wizard says some magic words that make you vanish.\nYou are never seen again.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to be a medicine person!\nA man is very sick with a fever.\nWhat do you want to do?\n1- Not do anything; 2- Wash him\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to be a medicine person!\nA man is very sick with a fever.\nWhat do you want to do?\n1- Not do anything; 2- Wash him\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 1;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 1 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to do anything.\nUnfortunately, the man dies.\nThe mans family is very angry at you.\nYou are tried for murder.\nYou have been found guilty.\nSo you are put to death.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to do anything.\nUnfortunately, the man dies.\nThe mans family is very angry at you.\nYou are tried for murder.\nYou have been found guilty.\nSo you are put to death.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 1 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to wash him.\nThe man recovers from his fever.\nYou are a hero in your village.\nThe chief rewards you with a big meal.\nThe chief offers you a week off.\nWhat do you do?\n1- Take a week off; 2- Tend to more sick people\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to wash him.\nThe man recovers from his fever.\nYou are a hero in your village.\nThe chief rewards you with a big meal.\nThe chief offers you a week off.\nWhat do you do?\n1- Take a week off; 2- Tend to more sick people\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 2;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 2 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to take a week off.\nYou come back and someone is very sick.\nYou are low on herbs.\nDo you go gather more herbs for your remedies or heal the patient with what you have?\n1- Go gather more herbs; 2- Heal the patient\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to take a week off.\nYou come back and someone is very sick.\nYou are low on herbs.\nDo you go gather more herbs for your remedies or heal the patient with what you have?\n1- Go gather more herbs; 2- Heal the patient\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 3;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 2 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to tend to more sick people.\nYou are very good at your craft.\nYou heal most of the people.\nHowever, you are low on herbs.\nDo you go gather some more herbs or tend to a couple more patients?\n1- Go gather more herbs; 2- Go tend to a couple more patients\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to tend to more sick people.\nYou are very good at your craft.\nYou heal most of the people.\nHowever, you are low on herbs.\nDo you go gather some more herbs or tend to a couple more patients?\n1- Go gather more herbs; 2- Go tend to a couple more patients\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 3 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go gather more herbs.\nBecause of this the patient dies!\nYou are tried for murder.\nAre you found guilty?\n1- Find out!\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go gather more herbs.\nBecause of this the patient dies!\nYou are tried for murder.\nAre you found guilty?\n1- Find out!\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 5;
        new_rand = rand() % 2 + 1;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 3 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to heal the patient.\nLuckily, you have enough herbs to heal the patient.\nYou need to gather more herbs.\nYou go gather some more herbs.\nWhile gathering more herbs you find a signal that says: \"S.O.S. >\"\nWhat do you do?\n1- Go investigate the distress signal; 2- Ignore it; 3- Go back to my tribe and tell someone about it\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to heal the patient.\nLuckily, you have enough herbs to heal the patient.\nYou need to gather more herbs.\nYou go gather some more herbs.\nWhile gathering more herbs you find a signal that says: \"S.O.S. >\"\nWhat do you do?\n1- Go investigate the distress signal; 2- Ignore it; 3- Go back to my tribe and tell someone about it\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 6;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 4 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to gather more herbs.\nYou go gather more herbs.\nLuckily, none of the patients die and you heal all of them.\nWhile gathering more herbs you find a signal that says: \"S.O.S. >\"\n1- Go investigate the distress signal; 2- Ignore it; 3- Tell someone about it\nWhat do you do?\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to gather more herbs.\nYou go gather more herbs.\nLuckily, none of the patients die and you heal all of them.\nWhile gathering more herbs you find a signal that says: \"S.O.S. >\"\n1- Go investigate the distress signal; 2- Ignore it; 3- Tell someone about it\nWhat do you do?\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 7;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 4 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go tend to a couple more patients.\nYou run out of herbs so you go gather more herbs.\nUnfortunately, while gathering herbs, all your patients die!\nYou are tried for murder.\nYou are found guilty.\nHow do you want to be executed?\n1- Arrows; 2- Sword; 3- Flying lawnmower; 4- Musket; 5- Bob the Blobfish\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go tend to a couple more patients.\nYou run out of herbs so you go gather more herbs.\nUnfortunately, while gathering herbs, all your patients die!\nYou are tried for murder.\nYou are found guilty.\nHow do you want to be executed?\n1- Arrows; 2- Sword; 3- Flying lawnmower; 4- Musket; 5- Bob the Blobfish\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 8;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 5 && strcmp(choice, "1") == 0 && new_rand == 1)
    {
        size_t length = strlen(name) + strlen("Unfortunately, you have been found guilty.\nHow do you want to be executed?\n1- Arrows; 2- Sword; 3- Stoned; 4- Knife; 5- Gun\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "Unfortunately, you have been found guilty.\nHow do you want to be executed?\n1- Arrows; 2- Sword; 3- Stoned; 4- Knife; 5- Gun\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 9;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 5 && strcmp(choice, "1") == 0 && new_rand == 2)
    {
        size_t length = strlen(name) + strlen("You are not found guilty.\nSome white man come to your tribe.\nTheyre asking you if they can settle near you.\nDo you want them to settle near you?\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You are not found guilty.\nSome white man come to your tribe.\nTheyre asking you if they can settle near you.\nDo you want them to settle near you?\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 10;
        return result;
    }

    if ((strcmp(character, "5") == 0 && level == 6 && strcmp(choice, "1") == 0) || (strcmp(character, "5") == 0 && level == 7 && strcmp(choice, "1") == 0))
    {
        size_t length = strlen(name) + strlen("You have chosen to go investigate the distress signal.\nUnfortunately, you get eaten by a pack of dingos.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go investigate the distress signal.\nUnfortunately, you get eaten by a pack of dingos.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if ((strcmp(character, "5") == 0 && level == 6 && strcmp(choice, "2") == 0) || (strcmp(character, "5") == 0 && level == 7 && strcmp(choice, "2") == 0))
    {
        size_t length = strlen(name) + strlen("You have chosen to ignore it.\nUnfortunately, on your way back you get eaten by a cheetah.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to ignore it.\nUnfortunately, on your way back you get eaten by a cheetah.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if ((strcmp(character, "5") == 0 && level == 6 && strcmp(choice, "3") == 0) || (strcmp(character, "5") == 0 && level == 7 && strcmp(choice, "3") == 0))
    {
        size_t length = strlen(name) + strlen("You have chosen to tell someone about it.\nApparently, some members in your tribe say a mother was kidnapped and that there is a $10, 000 reward for the capture of the kidnapper!\nDo you want to go hunt the kidnapper?\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to tell someone about it.\nApparently, some members in your tribe say a mother was kidnapped and that there is a $10, 000 reward for the capture of the kidnapper!\nDo you want to go hunt the kidnapper?\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 11;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 8 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to die from arrows.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to die from arrows.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 8 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to die from a sword.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to die from a sword.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 8 && strcmp(choice, "3") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to die from a flying lawnmower.\nSeriously though, how do you manage to die from a flying lawnmower?!\nI guess someone strapped jet engines and/or rockets to a lawnmower and it became a flying lawnmower!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to die from a flying lawnmower.\nSeriously though, how do you manage to die from a flying lawnmower?!\nI guess someone strapped jet engines and/or rockets to a lawnmower and it became a flying lawnmower!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 8 && strcmp(choice, "4") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to die from a musket.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to die from a musket.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 8 && strcmp(choice, "5") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to die from Bob the Blobfish.\nBob the Blobfish rockets out of his toilet and makes your eyes bleed to death!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to die from Bob the Blobfish.\nBob the Blobfish rockets out of his toilet and makes your eyes bleed to death!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 9 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You die from arrows.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You die from arrows.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 9 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You get stabbed to death by a sword.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You get stabbed to death by a sword.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 9 && strcmp(choice, "3") == 0)
    {
        size_t length = strlen(name) + strlen("You get stoned to death.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You get stoned to death.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 9 && strcmp(choice, "4") == 0)
    {
        size_t length = strlen(name) + strlen("You get stabbed to death by a knife.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You get stabbed to death by a knife.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 9 && strcmp(choice, "5") == 0)
    {
        size_t length = strlen(name) + strlen("You are shot to death with a bebe gun.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You are shot to death with a bebe gun.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 10 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to let them settle here.\nThey build a hospital.\nTheir settlement grows.\nYour village thrives because of the new settlement.\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to let them settle here.\nThey build a hospital.\nTheir settlement grows.\nYour village thrives because of the new settlement.\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 10 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You do not let them settle here.\nUnfortunately, Cooper the Dog hears a doorbell on TV.\nHe goes to bark at the door.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You do not let them settle here.\nUnfortunately, Cooper the Dog hears a doorbell on TV.\nHe goes to bark at the door.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 11 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You hunt the kidnapper.\nDo you capture him?\n1- Find out!\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You hunt the kidnapper.\nDo you capture him?\n1- Find out!\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 12;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 11 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You dont hunt the kidnapper.\nUnfortunately, Zeus comes out from the middle of nowhere and shocks you to death.\nHow shocking!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You dont hunt the kidnapper.\nUnfortunately, Zeus comes out from the middle of nowhere and shocks you to death.\nHow shocking!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 12 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You dont capture the kidnapper.\nUnfortunately, the kidnapper puts you guys in a trap and kills you.\nYou die.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You dont capture the kidnapper.\nUnfortunately, the kidnapper puts you guys in a trap and kills you.\nYou die.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "5") == 0 && level == 12 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You capture the kidnapper.\nYou receive $10k from the local authorities.\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You capture the kidnapper.\nYou receive $10k from the local authorities.\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to hunt The Mighty Lion.\nYou have been on your journey for about a week now.\nThe trail splits off into three sections.\nDo you choose to go through the canyon?\nHow about the prairie?\nOr what about keep continuing through the forest?\n\n1- Canyon; 2- Prairie; 3- Forest\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to hunt The Mighty Lion.\nYou have been on your journey for about a week now.\nThe trail splits off into three sections.\nDo you choose to go through the canyon?\nHow about the prairie?\nOr what about keep continuing through the forest?\n\n1- Canyon; 2- Prairie; 3- Forest\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 1;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 1 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go through the canyon!\nThe canyon is the quickest route to The Mighty Lion.\nUnfortunately, you're almost out of the canyon when a flash flood occurs.\nYou get wiped away instantly and you drown to death.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go through the canyon!\nThe canyon is the quickest route to The Mighty Lion.\nUnfortunately, you're almost out of the canyon when a flash flood occurs.\nYou get wiped away instantly and you drown to death.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 1 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go through the prairie!\nWhich way do you travel?\n\n1- Left; 2- Right\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go through the prairie!\nWhich way do you travel?\n\n1- Left; 2- Right\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 2;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 1 && strcmp(choice, "3") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go through the forest!\nYou see a river.\nDo you drink from it?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go through the forest!\nYou see a river.\nDo you drink from it?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 3;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 2 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go left.\nUnfortunately, there are hyenas in front of you after only about an hour of walking.\nThey see you and kill you.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go left.\nUnfortunately, there are hyenas in front of you after only about an hour of walking.\nThey see you and kill you.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 2 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go right.\nThere are zebras in front of you after only about an hour of walking.\nDo you kill them for food?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go right.\nThere are zebras in front of you after only about an hour of walking.\nDo you kill them for food?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 3 && strcmp(choice, "1") == 0)
    {
        level = 0;
        strncpy(character, "the_mighty_lion_terrain", sizeof(character));
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 3 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to drink from the river.\nYour fellow warriors have.\nThe river doesn't make them sick because the river comes from a far off glacier.\nUnfortunately, you cross the river and find no more water sources.\nYou die from dehydration.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to drink from the river.\nYour fellow warriors have.\nThe river doesn't make them sick because the river comes from a far off glacier.\nUnfortunately, you cross the river and find no more water sources.\nYou die from dehydration.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 4 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to kill the zebras.\nBecause you have chosen to do this you don't go hungry.\nNight falls.\nDo you sleep on the ground or in a tree?\n\n1- Ground; 2- Tree\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to kill the zebras.\nBecause you have chosen to do this you don't go hungry.\nNight falls.\nDo you sleep on the ground or in a tree?\n\n1- Ground; 2- Tree\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 5;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 4 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to kill the zebras.\nBecause you have chosen to do this you don't go hungry.\nNight falls.\nDo you sleep on the ground or in a tree?\n\n1- Ground; 2- Tree\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to kill the zebras.\nBecause you have chosen to do this you don't go hungry.\nNight falls.\nDo you sleep on the ground or in a tree?\n\n1- Ground; 2- Tree\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 5;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 4 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to kill the zebras.\nUnfortunately, because of this, you starve.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to kill the zebras.\nUnfortunately, because of this, you starve.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 5 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to sleep on the ground.\nUnfortunately, a dingo comes up and gobbles you up.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to sleep on the ground.\nUnfortunately, a dingo comes up and gobbles you up.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 5 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to sleep in a tree.\nNo animals eat you.\nYou see a river.\nDo you drink from it?\n\n1- Yes; 2- No\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to sleep in a tree.\nNo animals eat you.\nYou see a river.\nDo you drink from it?\n\n1- Yes; 2- No\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 6;
        return result;
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 6 && strcmp(choice, "1") == 0)
    {
        level = 0;
        strncpy(character, "the_mighty_lion_terrain", sizeof(character));
    }

    if (strcmp(character, "the_mighty_lion") == 0 && level == 6 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen not to drink from the river.\nYour fellow warriors have.\nThe river doesn't make them sick because the river comes from a far off glacier.\nUnfortunately, you cross the river and find no more water sources.\nYou die from dehydration.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen not to drink from the river.\nYour fellow warriors have.\nThe river doesn't make them sick because the river comes from a far off glacier.\nUnfortunately, you cross the river and find no more water sources.\nYou die from dehydration.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to drink from the river.\nLuckily, the water doesnt have any diseases or viruses.\nYou are full.\nYou need to cross the river.\nHowever, the river is very deep.\nYou could make a temporary bridge but that would kill vital time to hunt The Mighty Lion.\nThe fastest way to cross the river is to make a raft.\nWhat do you do?\n\n1- Make a bridge; 2- Build a raft\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to drink from the river.\nLuckily, the water doesnt have any diseases or viruses.\nYou are full.\nYou need to cross the river.\nHowever, the river is very deep.\nYou could make a temporary bridge but that would kill vital time to hunt The Mighty Lion.\nThe fastest way to cross the river is to make a raft.\nWhat do you do?\n\n1- Make a bridge; 2- Build a raft\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 1;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 1 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to make a bridge.\nYour patience pays off and you get across the river in no time!\nThe path splits in two.\nDo you travel through the hills or the extreme hills?\n\n1- Hills; 2- Extreme hills\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to make a bridge.\nYour patience pays off and you get across the river in no time!\nThe path splits in two.\nDo you travel through the hills or the extreme hills?\n\n1- Hills; 2- Extreme hills\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 2;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 1 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to build a raft.\nUnfortunately, the river brings you to the rapids before you are able to reach the other side.\nThe rapids are heading towards a waterfall, so you panic.\nYou throw your paddle into the river.\nYou fall down the waterfall and hit the rocks at the bottom.\nYour head splits open and you die!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to build a raft.\nUnfortunately, the river brings you to the rapids before you are able to reach the other side.\nThe rapids are heading towards a waterfall, so you panic.\nYou throw your paddle into the river.\nYou fall down the waterfall and hit the rocks at the bottom.\nYour head splits open and you die!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 2 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go through the hills.\nYou travel through the hills and see The Mighty Lion!\nWhat weapon do you want to use?\n\n1- Knife; 2- Bow and arrows; 3- Spear; 4- Gun\nMake your choice ") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go through the hills.\nYou travel through the hills and see The Mighty Lion!\nWhat weapon do you want to use?\n\n1- Knife; 2- Bow and arrows; 3- Spear; 4- Gun\nMake your choice ");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 3;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 2 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to go through the extreme hills.\nYou freeze to death.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to go through the extreme hills.\nYou freeze to death.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 3 && strcmp(choice, "1") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to use your trusty knife to kill The Mighty Lion!\nSo, +  do you kill The Mighty Lion?\n\n1- Find out!\n") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to use your trusty knife to kill The Mighty Lion!\nSo, +  do you kill The Mighty Lion?\n\n1- Find out!\n");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        new_rand = rand() % 2 + 1;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 3 && strcmp(choice, "2") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to use your mighty bow and arrows to kill The Mighty Lion!\nSo, +  do you kill The Mighty Lion?\n\n1- Find out!\n") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to use your mighty bow and arrows to kill The Mighty Lion!\nSo, +  do you kill The Mighty Lion?\n\n1- Find out!\n");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        new_rand = rand() % 2 + 1;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 3 && strcmp(choice, "3") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to use your powerful spear to kill The Mighty Lion.\nSo, +  do you kill The Mighty Lion?\n\n1- Find out!\n") + strlen(":\n") + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to use your powerful spear to kill The Mighty Lion.\nSo, +  do you kill The Mighty Lion?\n\n1- Find out!\n");
        strcpy(result + strlen(result), name);
        strcpy(result + strlen(result), ":\n");
        level = 4;
        new_rand = rand() % 2 + 1;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 3 && strcmp(choice, "4") == 0)
    {
        size_t length = strlen(name) + strlen("You have chosen to use your overpowered gun to kill The Mighty Lion.\nUnfortunately, your gun explodes in face.\nYou die.\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You have chosen to use your overpowered gun to kill The Mighty Lion.\nUnfortunately, your gun explodes in face.\nYou die.\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 4 && strcmp(choice, "1") == 0 && new_rand == 1)
    {
        size_t length = strlen(name) + strlen("You do not kill The Mighty Lion.\nUnfortunately, The Mighty Lion eats you!\n") + strlen(death_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You do not kill The Mighty Lion.\nUnfortunately, The Mighty Lion eats you!\n");
        strcpy(result + strlen(result), death_level(name));
        level = 0;
        return result;
    }

    if (strcmp(character, "the_mighty_lion_terrain") == 0 && level == 4 && strcmp(choice, "1") == 0 && new_rand == 2)
    {
        size_t length = strlen(name) + strlen("You kill The Mighty Lion!\nYour tribe celebrates!\n") + strlen(victory_level(name)) + 1;
        char * result = (char *)malloc(length);
        strcpy(result, "You kill The Mighty Lion!\nYour tribe celebrates!\n");
        strcpy(result + strlen(result), victory_level(name));
        level = 0;
        return result;
    }
    
    else
    {
        return "\nInvalid option!\n";
    }
}
