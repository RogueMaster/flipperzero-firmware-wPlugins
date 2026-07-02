#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <input/input.h>
#include <storage/storage.h>

#include "cocktail_book_icons.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define APP_VERSION "0.4.0"
#define APP_CREDITS "Developed by resu95, recipes by @der_spirituelle "
#define MENU_ITEM_ABOUT UINT32_MAX

#define COCKTAIL_SD_DIR "/ext/apps_data/cocktail_book"
#define COCKTAIL_SD_FILE "/ext/apps_data/cocktail_book/recipes.txt"

#define MAX_COCKTAILS 40
#define SD_FILE_BUFFER_SIZE 8192
#define SD_STRING_POOL_SIZE 8192

#define RECIPE_VISIBLE_LINES 3
#define RECIPE_LINE_MAX 72

typedef enum {
    CocktailBookViewTitle,
    CocktailBookViewDisclaimer,
    CocktailBookViewCategoryMenu,
    CocktailBookViewCocktailMenu,
    CocktailBookViewRecipe,
    CocktailBookViewAbout,
} CocktailBookView;

typedef enum {
    CocktailCategoryAll = 0,
    CocktailCategoryClassic,
    CocktailCategorySour,
    CocktailCategorySpritz,
    CocktailCategoryTikiDrinks,
    CocktailCategoryAlcoholFree,
    CocktailCategoryDigestive,
    CocktailCategoryHighball,
    CocktailCategoryCount,
} CocktailCategory;

typedef enum {
    RecipeSectionOverview = 0,
    RecipeSectionIngredients,
    RecipeSectionMethod,
    RecipeSectionNote,
    RecipeSectionCount,
} RecipeSection;

typedef struct {
    const char* name;
    CocktailCategory category;
    const char* base;
    const char* glass;
    const char* ice;
    const char* ingredients;
    const char* method;
    const char* garnish;
    const char* note;
} Cocktail;

typedef struct {
    const Cocktail* cocktail;
    RecipeSection section;
    uint8_t scroll_line;
} RecipeViewModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;

    View* title_view;
    View* disclaimer_view;
    Submenu* category_menu;
    Submenu* cocktail_menu;
    View* recipe_view;
    View* about_view;

    CocktailBookView current_view;
    CocktailCategory selected_category;
    uint32_t selected_cocktail;

    Cocktail cocktails[MAX_COCKTAILS];
    uint32_t cocktail_count;

    bool sd_loaded;
    char title_status[48];

    char sd_file_buffer[SD_FILE_BUFFER_SIZE];
    char sd_string_pool[SD_STRING_POOL_SIZE];
    size_t sd_pool_used;
} CocktailBookApp;

static const Cocktail fallback_cocktails[] = {
    {
        .name = "Negroni",
        .category = CocktailCategoryClassic,
        .base = "Gin",
        .glass = "Tumbler",
        .ice = "Block",
        .ingredients =
            "30 ml Gin\n"
            "30 ml Campari\n"
            "30 ml red Vermouth",
        .method =
            "Stir all ingredients well on ice.\n"
            "Strain on ice into chilled Tumbler.",
        .garnish = "Orange zest",
        .note = "Bitter, strong, classic.",
    },
    {
        .name = "Daiquiri",
        .category = CocktailCategoryTikiDrinks,
        .base = "Rum",
        .glass = "Coupe",
        .ice = "Cubes",
        .ingredients =
            "60 ml white Rum\n"
            "25 ml Lime juice\n"
            "15 ml Simple Sirup",
        .method =
            "Shake well on ice.\n"
            "Strain into frozen Coupe.",
        .garnish = "Lime zest optional",
        .note = "tart, fresh, dry.",
    },
    {
        .name = "Dry Martini",
        .category = CocktailCategoryClassic,
        .base = "Gin | Vodka",
        .glass = "Martini",
        .ice = "none",
        .ingredients =
            "60 ml Dry Gin\n"
            "20 ml Dry Vermouth",
        .method =
            "Stir on Ice.\n"
            "Strain into frozen Martini glass.",
        .garnish = "Green Olive,Lemon Zest",
        .note = "strong, dry, classic.",
    },
    {
        .name = "Old Fashioned",
        .category = CocktailCategoryClassic,
        .base = "Whiskey",
        .glass = "Tumbler",
        .ice = "Block",
        .ingredients =
            "60 ml Bourbon or Rye\n"
            "1 Bsp Simple Sirup\n"
            "2 dsh Angostura",
        .method =
            "Stir in glass with ice\n"
            "Strain into chilled Tumbler",
        .garnish = "Orange zest",
        .note = "Spirit-forward. Dont dilute too much.",
    },
    {
        .name = "Paloma",
        .category = CocktailCategoryHighball,
        .base = "Tequila",
        .glass = "Highball",
        .ice = "Cubes",
        .ingredients =
            "60 ml Tequila Blanco\n"
            "15 ml Lime juice\n"
            "150 ml pink grapefruit soda\n"
            "pinch Sea salt",
        .method =
            "Build in glass with ice\n"
            "Stir gently",
        .garnish = "Lime wedge",
        .note = "easy drinking / sweet / sour",
    },
    {
        .name = "Whiskey Sour",
        .category = CocktailCategorySour,
        .base = "Whiskey",
        .glass = "Coupe / Tumbler",
        .ice = "Cubes",
        .ingredients =
            "50 ml Bourbon\n"
            "25 ml Lemon juice\n"
            "20 ml Simple Sirup\n"
            "Optional Eggwhites",
        .method =
            "Optional Dry Shake.\n"
            "Shake on ice.\n"
            "Strain well into chilled Tumbler.",
        .garnish = "Zest or Cherry",
        .note = "Balance: Sweet / Sour / Power.",
    },
    {
        .name = "Margarita",
        .category = CocktailCategoryClassic,
        .base = "Tequila",
        .glass = "Coupe",
        .ice = "Cubes",
        .ingredients =
            "50 ml Tequila Blanco\n"
            "25 ml Lime juice\n"
            "20 ml Cointreau\n"
            "Optional 5 ml Agave sirup",
        .method =
            "Shake with ice.\n"
            "Strain in chilled Coupe.",
        .garnish = "salted Rim / Lime",
        .note = "Agave only if needed.",
    },
    {
        .name = "Aperol Spritz",
        .category = CocktailCategorySpritz,
        .base = "Aperol",
        .glass = "Wine",
        .ice = "Cubes",
        .ingredients =
            "90 ml Prosecco\n"
            "60 ml Aperol\n"
            "30 ml Soda",
        .method =
            "Fill glass with ice.\n"
            "Build and stir gentle.",
        .garnish = "Orange slice",
        .note = "Dont stir too hard.",
    },
    {
        .name = "Limoncello Spritz",
        .category = CocktailCategorySpritz,
        .base = "Limoncello",
        .glass = "Wine",
        .ice = "Cubes",
        .ingredients =
            "90 ml Prosecco\n"
            "50 ml Limoncello\n"
            "30 ml Soda\n"
            "10 ml optional Lemon juice",
        .method =
            "Build in glass on ice\n"
            "Stir carefully.",
        .garnish = "Lemon Zest | green Olive",
        .note = "Fresh, bright, summery.",
    },
    {
        .name = "Espresso Martini",
        .category = CocktailCategoryClassic,
        .base = "Vodka / Coffee",
        .glass = "Coupe",
        .ice = "Cubes",
        .ingredients =
            "40 ml Vodka\n"
            "30 ml Espresso\n"
            "20 ml Kahlua\n"
            "10 ml Simple Sirup",
        .method =
            "Brew fresh Espresso.\n"
            "Immediately shake hard over ice.\n"
            "Strain into frozen Coupe.",
        .garnish = "3 Coffee beans",
        .note = "Shake hard for nice foam.",
    },
    {
        .name = "Sazerac",
        .category = CocktailCategoryClassic,
        .base = "Bourbon",
        .glass = "Tumbler",
        .ice = "Cubes",
        .ingredients =
            "50 ml Bourbon\n"
            "15 ml Simple Sirup\n"
            "3-4 dsh Peychauds\n"
            "5 ml Absinth",
        .method =
            "Rinse tumbler with Absinth, discard\n"
            "Stir over ice.\n"
            "Strain over ice.",
        .garnish = "Lemon zest",
        .note = "Strong / Classy / special",
    },
    {
        .name = "Last Word",
        .category = CocktailCategoryClassic,
        .base = "Gin",
        .glass = "Coupe",
        .ice = "Cubes",
        .ingredients =
            "30 ml Gin\n"
            "30 ml Green Chartreuse\n"
            "30 ml Maraschino Luxardo\n"
            "30 ml Lime juice",
        .method =
            "Shake over ice\n"
            "Strain in coupe without ice",
        .garnish = "Cherry",
        .note = "Tart / Herbal / refreshing",
    },
    {
        .name = "Virgin Mule",
        .category = CocktailCategoryAlcoholFree,
        .base = "Ginger Beer",
        .glass = "Highball",
        .ice = "Cubes",
        .ingredients =
            "120 ml Ginger Beer\n"
            "25 ml Lime juice\n"
            "15 ml Simple sirup",
        .method =
            "Build on ice in glass.\n"
            "Gentle stir.",
        .garnish = "Lime / Mint",
        .note = "Fresh, spicy, NA.",
    },
      {
        .name = "Virgin Mary",
        .category = CocktailCategoryAlcoholFree,
        .base = "Tomato Juice",
        .glass = "Highball",
        .ice = "Cubes",
        .ingredients =
            "200 ml Tomato juice\n"
            "25 ml Lime juice\n"
            "2-3 dsh Tabasco",
        .method =
            "Build on ice in glass.\n"
            "Stir well.",
        .garnish = "Pepper / Celery",
        .note = "Salty, spicy, NA.",
    },
};

#define FALLBACK_COCKTAIL_COUNT (sizeof(fallback_cocktails) / sizeof(fallback_cocktails[0]))

static void cocktail_book_build_cocktail_menu(CocktailBookApp* app);
static void cocktail_book_switch_to(CocktailBookApp* app, CocktailBookView view);

static const char* sample_recipes_txt =
    "# Cocktail Book recipes.txt\n"
    "# Location: /ext/apps_data/cocktail_book/recipes.txt\n"
    "# A Cocktail begins with [Cocktail]\n"
    "# Divide multiple lines in field with|.\n"
    "# Ice has to be: Cubes, Block, Crushed\n"
    "\n"
    "[Cocktail]\n"
    "name=Negroni\n"
    "category=Classics\n"
    "base=Gin\n"
    "glass=Tumbler\n"
    "ice=Block\n"
    "ingredients=30 ml Gin|30 ml Campari|30 ml Red Vermouth\n"
    "method=Stir all ingredients on ice.|Strain into Tumbler on ice.\n"
    "garnish=Orange zest\n"
    "note=Bitter, strong, classic.\n"
    "\n"
    "[Cocktail]\n"
    "name=Last Word\n"
    "category=Classics\n"
    "base=Gin\n"
    "glass=Coupe\n"
    "ice=Cubes\n"
    "ingredients=30 ml Gin|30 ml Maraschino Luxardo|30 ml Green Chartreuse|Lime juice\n"
    "method=Shake all ingredients on ice.|Strain into Coupe.\n"
    "garnish=Cherry\n"
    "note=Tart, Herbal, refreshing.\n"
    "\n"
    "[Cocktail]\n"
    "name=Daiquiri\n"
    "category=TikiDrinks\n"
    "base=Rum\n"
    "glass=Coupe\n"
    "ice=Cubes\n"
    "ingredients=60 ml white Rum|25 ml Lime juice|15 ml Simple Sirup\n"
    "method=Shake well with ice.|Strain into chilled Coupe\n"
    "garnish=Lime zest optional\n"
    "note=tart, fresh, dry.\n"
    "\n"
    "[Cocktail]\n"
    "name=Paloma\n"
    "category=TikiDrinks\n"
    "base=Tequila\n"
    "glass=Highball\n"
    "ice=Cubes\n"
    "ingredients=60 ml Tequila blanco |15 ml Lime juice|150 ml pink grapefruit soda\n"
    "method=Build on ice.\n"
    "garnish=Lime wedge\n"
    "note=tart, fresh, dry.\n"
    "\n"
    "[Cocktail]\n"
    "name=Old Fashioned\n"
    "category=Classics\n"
    "base=Whiskey\n"
    "glass=Tumbler\n"
    "ice=Block\n"
    "ingredients=60 ml Bourbon or Rye|1 Bsp Simple Sirup|2 dsh Angostura\n"
    "method=Stir with ice.| if needed add water.\n"
    "garnish=Orange zest\n"
    "note=Spirit-forward. Dont overdilute.\n"
    "\n"
    "[Cocktail]\n"
    "name=Whiskey Sour\n"
    "category=Sours\n"
    "base=Whiskey\n"
    "glass=Coupe / Tumbler\n"
    "ice=Cubes\n"
    "ingredients=50 ml Bourbon|25 ml Lemon juice|20 ml Simple Sirup|Optional Eggwhite\n"
    "method=Optional Dry Shake.|Shake on Ice.|Strain into chilled Coupe or Tumbler.\n"
    "garnish=Zest or Cherry\n"
    "note=Balance: Sweet / Sour / Power.\n"
    "\n"
    "[Cocktail]\n"
    "name=Margarita\n"
    "category=Sours\n"
    "base=Tequila\n"
    "glass=Coupe\n"
    "ice=Cubes\n"
    "ingredients=50 ml Tequila Blanco|25 ml Lime juice|20 ml Cointreau|Optional 5 ml Agave sirup\n"
    "method=Shake with ice.|Strain into frozen Coupe\n"
    "garnish=salted Rim / Lime\n"
    "note=Agave if needed.\n"
    "\n"
    "[Cocktail]\n"
    "name=Aperol Spritz\n"
    "category=Spritz\n"
    "base=Aperol\n"
    "glass=Wine\n"
    "ice=Cubes\n"
    "ingredients=90 ml Prosecco|60 ml Aperol|30 ml Soda\n"
    "method=Build in glass on ice|Stir gentle.\n"
    "garnish=Orange slice\n"
    "note=Dont stir too hard\n"
    "\n"
    "[Cocktail]\n"
    "name=Limoncello Spritz\n"
    "category=Spritz\n"
    "base=Limoncello\n"
    "glass=Wine\n"
    "ice=Cubes\n"
    "ingredients=90 ml Prosecco|50 ml Limoncello|30 ml Soda|10 ml Lemon juice optional\n"
    "method=Build in glass on ice|Stir gentle.\n"
    "garnish=Lemon zest\n"
    "note=Fresh, brigh, summery.\n"
    "\n"
    "[Cocktail]\n"
    "name=Espresso Martini\n"
    "category=Classics\n"
    "base=Vodka / Coffee\n"
    "glass=Coupe\n"
    "ice=Cubes\n"
    "ingredients=40 ml Vodka|30 ml Espresso|20 ml Kahlua|10 ml Simple Sirup\n"
    "method=Brew Espresso fresh.|Shake hard on ice.|Strain into chilled coupe.\n"
    "garnish=3 Coffee beans\n"
    "note=Hard shake for nice foam.\n"
     "\n"
    "[Cocktail]\n"
    "name=Virgin Mary\n"
    "category=Alcohol Free\n"
    "base=Tomato juice\n"
    "glass=Highball\n"
    "ice=Cubes\n"
    "ingredients=200 ml tomato juice|25 ml lime juice|2-3 dsh Tabasco\n"
    "method=Build in Glass.|Stir well.\n"
    "garnish=Pepper|Celery\n"
    "note=Spicy|Salty\n"
    "\n"
    "[Cocktail]\n"
    "name=Virgin Mule\n"
    "category=AlcoholFree\n"
    "base=Ginger Beer\n"
    "glass=Highball\n"
    "ice=Cubes\n"
    "ingredients=120 ml Ginger Beer|25 ml Lime Juice|15 ml Simple Sirup\n"
    "method=Build in glass on ice|short stir\n"
    "garnish=Lime / Mint\n"
    "note=Fresh, spicy, NA.\n";

static char* cocktail_book_trim(char* text) {
    if(!text) return text;

    while(*text && isspace((unsigned char)*text)) {
        text++;
    }

    char* end = text + strlen(text);
    while(end > text && isspace((unsigned char)*(end - 1))) {
        end--;
        *end = '\0';
    }

    return text;
}

static bool cocktail_book_streq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

static bool text_contains_ci(const char* haystack, const char* needle) {
    if(!haystack || !needle || !needle[0]) {
        return false;
    }

    size_t needle_len = strlen(needle);

    for(const char* h = haystack; *h; h++) {
        size_t i = 0;

        while(i < needle_len &&
              h[i] &&
              tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }

        if(i == needle_len) {
            return true;
        }
    }

    return false;
}

static const char* cocktail_category_get_name(CocktailCategory category) {
    switch(category) {
    case CocktailCategoryAll:
        return "All Cocktails";
    case CocktailCategoryClassic:
        return "Classics";
    case CocktailCategorySour:
        return "Sours";
    case CocktailCategorySpritz:
        return "Spritz";
    case CocktailCategoryTikiDrinks:
        return "TikiDrinks";
    case CocktailCategoryAlcoholFree:
        return "Non Alcoholic";
        case CocktailCategoryDigestive:
    return "Digestive";
    case CocktailCategoryHighball:
    return "Highball";
    default:
        return "Unknown";
    }
}

static CocktailCategory cocktail_category_from_string(const char* text) {
    if(cocktail_book_streq(text, "Classics") || cocktail_book_streq(text, "Classic")) {
        return CocktailCategoryClassic;
    }

    if(cocktail_book_streq(text, "Sours") || cocktail_book_streq(text, "Sour")) {
        return CocktailCategorySour;
    }

    if(cocktail_book_streq(text, "Spritz")) {
        return CocktailCategorySpritz;
    }

    if(cocktail_book_streq(text, "TikiDrinks") || cocktail_book_streq(text, "TikiDrinks")) {
        return CocktailCategoryTikiDrinks;
    }

    if(cocktail_book_streq(text, "Non Alcoholic") ||
       cocktail_book_streq(text, "AlcoholFree") ||
       cocktail_book_streq(text, "NonAlcoholic")) {
        return CocktailCategoryAlcoholFree;
    }
    if(cocktail_book_streq(text, "Digestive")) {
    return CocktailCategoryDigestive;
}
 if(cocktail_book_streq(text, "Highball")) {
    return CocktailCategoryHighball;
}
    return CocktailCategoryClassic;
}

static bool cocktail_matches_category(const Cocktail* cocktail, CocktailCategory category) {
    if(category == CocktailCategoryAll) {
        return true;
    }

    return cocktail->category == category;
}

static const char* recipe_section_get_title(RecipeSection section) {
    switch(section) {
    case RecipeSectionOverview:
        return "Info";
    case RecipeSectionIngredients:
        return "Ingredients";
    case RecipeSectionMethod:
        return "Method";
    case RecipeSectionNote:
        return "Note";
    default:
        return "";
    }
}

static const char* recipe_section_get_text(const Cocktail* cocktail, RecipeSection section) {
    if(!cocktail) return "-";

    switch(section) {
    case RecipeSectionOverview:
        return cocktail->base;
    case RecipeSectionIngredients:
        return cocktail->ingredients;
    case RecipeSectionMethod:
        return cocktail->method;
    case RecipeSectionNote:
        return cocktail->note;
    default:
        return "-";
    }
}

static uint32_t cocktail_book_count_lines(const char* text) {
    if(!text || !text[0]) {
        return 1;
    }

    uint32_t lines = 1;
    for(const char* p = text; *p; p++) {
        if(*p == '\n') {
            lines++;
        }
    }

    return lines;
}

static uint8_t cocktail_book_max_scroll_for_text(const char* text) {
    uint32_t lines = cocktail_book_count_lines(text);

    if(lines <= RECIPE_VISIBLE_LINES) {
        return 0;
    }

    uint32_t max_scroll = lines - RECIPE_VISIBLE_LINES;
    if(max_scroll > 255) {
        max_scroll = 255;
    }

    return (uint8_t)max_scroll;
}

static char* cocktail_book_pool_strdup(CocktailBookApp* app, const char* text, bool pipe_to_newline) {
    if(!text) return NULL;

    size_t len = strlen(text);
    if(app->sd_pool_used + len + 1 >= SD_STRING_POOL_SIZE) {
        return NULL;
    }

    char* dst = &app->sd_string_pool[app->sd_pool_used];

    for(size_t i = 0; i < len; i++) {
        if(pipe_to_newline && text[i] == '|') {
            dst[i] = '\n';
        } else {
            dst[i] = text[i];
        }
    }

    dst[len] = '\0';
    app->sd_pool_used += len + 1;

    return dst;
}

static void cocktail_book_init_empty_cocktail(Cocktail* cocktail) {
    cocktail->name = NULL;
    cocktail->category = CocktailCategoryClassic;
    cocktail->base = "-";
    cocktail->glass = "-";
    cocktail->ice = "Cubes";
    cocktail->ingredients = "-";
    cocktail->method = "-";
    cocktail->garnish = "-";
    cocktail->note = "-";
}

static void cocktail_book_add_parsed_cocktail(CocktailBookApp* app, Cocktail* cocktail) {
    if(!cocktail->name) {
        return;
    }

    if(app->cocktail_count >= MAX_COCKTAILS) {
        return;
    }

    app->cocktails[app->cocktail_count] = *cocktail;
    app->cocktail_count++;
}

static void cocktail_book_load_fallback_recipes(CocktailBookApp* app) {
    app->cocktail_count = 0;

    for(uint32_t i = 0; i < FALLBACK_COCKTAIL_COUNT && i < MAX_COCKTAILS; i++) {
        app->cocktails[app->cocktail_count] = fallback_cocktails[i];
        app->cocktail_count++;
    }

    app->sd_loaded = false;
}

static bool cocktail_book_parse_sd_buffer(CocktailBookApp* app) {
    app->cocktail_count = 0;
    app->sd_pool_used = 0;

    Cocktail current;
    cocktail_book_init_empty_cocktail(&current);

    bool in_cocktail = false;
    char* cursor = app->sd_file_buffer;

    while(cursor && *cursor) {
        char* line = cursor;
        char* newline = strchr(cursor, '\n');

        if(newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = NULL;
        }

        line = cocktail_book_trim(line);

        if(line[0] == '\0' || line[0] == '#') {
            continue;
        }

        if(cocktail_book_streq(line, "[Cocktail]")) {
            if(in_cocktail) {
                cocktail_book_add_parsed_cocktail(app, &current);
            }

            cocktail_book_init_empty_cocktail(&current);
            in_cocktail = true;
            continue;
        }

        if(!in_cocktail) {
            continue;
        }

        char* equals = strchr(line, '=');
        if(!equals) {
            continue;
        }

        *equals = '\0';

        char* key = cocktail_book_trim(line);
        char* value = cocktail_book_trim(equals + 1);

        if(cocktail_book_streq(key, "name")) {
            current.name = cocktail_book_pool_strdup(app, value, false);
        } else if(cocktail_book_streq(key, "category")) {
            current.category = cocktail_category_from_string(value);
        } else if(cocktail_book_streq(key, "base")) {
            current.base = cocktail_book_pool_strdup(app, value, false);
        } else if(cocktail_book_streq(key, "glass")) {
            current.glass = cocktail_book_pool_strdup(app, value, false);
        } else if(cocktail_book_streq(key, "ice")) {
            current.ice = cocktail_book_pool_strdup(app, value, false);
        } else if(cocktail_book_streq(key, "ingredients")) {
            current.ingredients = cocktail_book_pool_strdup(app, value, true);
        } else if(cocktail_book_streq(key, "method")) {
            current.method = cocktail_book_pool_strdup(app, value, true);
        } else if(cocktail_book_streq(key, "garnish")) {
            current.garnish = cocktail_book_pool_strdup(app, value, false);
        } else if(cocktail_book_streq(key, "note")) {
            current.note = cocktail_book_pool_strdup(app, value, true);
        }
    }

    if(in_cocktail) {
        cocktail_book_add_parsed_cocktail(app, &current);
    }

    return app->cocktail_count > 0;
}

static void cocktail_book_create_sample_file_if_missing(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, COCKTAIL_SD_DIR);

    if(storage_file_exists(storage, COCKTAIL_SD_FILE)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    File* file = storage_file_alloc(storage);
    bool opened = storage_file_open(file, COCKTAIL_SD_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(opened) {
        storage_file_write(file, sample_recipes_txt, strlen(sample_recipes_txt));
        storage_file_sync(file);
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

static bool cocktail_book_try_load_sd_recipes(CocktailBookApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool loaded = false;
    bool opened = storage_file_open(file, COCKTAIL_SD_FILE, FSAM_READ, FSOM_OPEN_EXISTING);

    if(opened) {
        uint64_t file_size_u64 = storage_file_size(file);

        if(file_size_u64 > 0) {
            size_t bytes_to_read = (file_size_u64 >= SD_FILE_BUFFER_SIZE) ?
                                       (SD_FILE_BUFFER_SIZE - 1) :
                                       (size_t)file_size_u64;

            size_t bytes_read = storage_file_read(file, app->sd_file_buffer, bytes_to_read);
            app->sd_file_buffer[bytes_read] = '\0';

            loaded = cocktail_book_parse_sd_buffer(app);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!loaded) {
        cocktail_book_create_sample_file_if_missing();
    }

    app->sd_loaded = loaded;
    return loaded;
}

static void cocktail_book_switch_to(CocktailBookApp* app, CocktailBookView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->dispatcher, view);
}

static void draw_text_fit(Canvas* canvas, int32_t x, int32_t y, uint8_t max_width, const char* text) {
    if(!text) {
        return;
    }

    char buffer[RECIPE_LINE_MAX];
    size_t input_len = strlen(text);

    if(input_len >= sizeof(buffer)) {
        input_len = sizeof(buffer) - 1;
    }

    memcpy(buffer, text, input_len);
    buffer[input_len] = '\0';

    size_t len = strlen(buffer);

    while(len > 0 && canvas_string_width(canvas, buffer) > max_width) {
        len--;
        buffer[len] = '\0';
    }

    if(len < strlen(text) && len > 3) {
        buffer[len - 3] = '.';
        buffer[len - 2] = '.';
        buffer[len - 1] = '.';
    }

    canvas_draw_str(canvas, x, y, buffer);
}

static void draw_multiline_window(
    Canvas* canvas,
    const char* text,
    uint8_t scroll_line,
    int32_t x,
    int32_t y,
    uint8_t width) {
    if(!text || !text[0]) {
        canvas_draw_str(canvas, x, y, "-");
        return;
    }

    uint32_t current_line = 0;
    uint32_t drawn = 0;
    const char* cursor = text;

    while(*cursor && drawn < RECIPE_VISIBLE_LINES) {
        const char* line_start = cursor;

        while(*cursor && *cursor != '\n') {
            cursor++;
        }

        size_t line_len = cursor - line_start;

        if(current_line >= scroll_line) {
            char line_buffer[RECIPE_LINE_MAX];

            if(line_len >= sizeof(line_buffer)) {
                line_len = sizeof(line_buffer) - 1;
            }

            memcpy(line_buffer, line_start, line_len);
            line_buffer[line_len] = '\0';

            draw_text_fit(canvas, x, y + (drawn * 10), width, line_buffer);
            drawn++;
        }

        if(*cursor == '\n') {
            cursor++;
        }

        current_line++;
    }

    if(drawn == 0) {
        canvas_draw_str(canvas, x, y, "-");
    }
}

/* -------------------------------------------------------------------------- */
/* Icon selection                                                             */
/* -------------------------------------------------------------------------- */

static bool cocktail_text_contains(const Cocktail* cocktail, const char* needle) {
    if(!cocktail || !needle) {
        return false;
    }

    return text_contains_ci(cocktail->name, needle) ||
           text_contains_ci(cocktail->base, needle) ||
           text_contains_ci(cocktail->glass, needle) ||
           text_contains_ci(cocktail->ice, needle);
}

static const Icon* header_icon_for_cocktail(const Cocktail* cocktail) {
    if(!cocktail) {
        return &I_martini_10x10;
    }

    if(text_contains_ci(cocktail->name, "Margarita") ||
       (cocktail->glass && text_contains_ci(cocktail->glass, "Margarita"))) {
        return &I_margarita_10x10;
    }

    if(cocktail->category == CocktailCategorySpritz) {
        return &I_spritz_10x10;
    }

    if(cocktail->category == CocktailCategorySour) {
        return &I_sour_10x10;
    }

    if(cocktail->category == CocktailCategoryAlcoholFree) {
        return &I_highball_10x10;
    }

    if(cocktail->category == CocktailCategoryTikiDrinks) {
        return &I_tiki_10x10;
    }
    if(cocktail->category == CocktailCategoryHighball) {
        return &I_highball_10x10;
    }

    if(cocktail_text_contains(cocktail, "Tumbler")) {
        return &I_tumbler_10x10;
    }

    if(cocktail_text_contains(cocktail, "Coupette") ||
       cocktail_text_contains(cocktail, "Coupe")) {
        return &I_coup_10x10;
    }

    return &I_martini_10x10;
}

static const Icon* glass_icon_for_cocktail(const Cocktail* cocktail) {
    if(!cocktail || !cocktail->glass) {
        return &I_martini_10x10;
    }

    /* Das tatsächliche Servierglas hat Vorrang vor der Basisspirituose,
       damit z.B. ein Tequila-Drink im Highball-Glas nicht faelschlich
       das Margarita-Icon bekommt. */
    if(text_contains_ci(cocktail->glass, "Tumbler")) {
        return &I_tumbler_10x10;
    }

    if(text_contains_ci(cocktail->glass, "Coupe") ||
       text_contains_ci(cocktail->glass, "Coupe")) {
        return &I_coup_10x10;
    }

    if(text_contains_ci(cocktail->glass, "Wineglas") ||
       text_contains_ci(cocktail->glass, "Wine")) {
        return &I_spritz_10x10;
    }

    if(text_contains_ci(cocktail->glass, "Margarita")) {
        return &I_margarita_10x10;
    }

    if(text_contains_ci(cocktail->glass, "Highball")) {
        return &I_highball_10x10;
    }

    return &I_highball_10x10;
}

static const char* ice_label_for_cocktail(const Cocktail* cocktail) {
    if(!cocktail || !cocktail->ice) {
        return "Cubes";
    }

    if(text_contains_ci(cocktail->ice, "block")) {
        return "Block";
    }

    if(text_contains_ci(cocktail->ice, "crushed") ||
       text_contains_ci(cocktail->ice, "crush")) {
        return "Crushed";
    }
 

    return "Cubes";
}

static const Icon* ice_icon_for_cocktail(const Cocktail* cocktail) {
    const char* label = ice_label_for_cocktail(cocktail);

    if(cocktail_book_streq(label, "Block")) {
        return &I_ice_block_10x10;
    }

    if(cocktail_book_streq(label, "Crushed")) {
        return &I_ice_crushed_10x10;
    }

    return &I_ice_cubes_10x10;
}

/* -------------------------------------------------------------------------- */
/* Title and disclaimer views                                                 */
/* -------------------------------------------------------------------------- */

static void title_view_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 0, 0, &I_homescreen_128x64);
}

static bool title_view_input_callback(InputEvent* event, void* context) {
    CocktailBookApp* app = context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyOk) {
        cocktail_book_switch_to(app, CocktailBookViewDisclaimer);
        return true;
    }

    return false;
}

static void disclaimer_view_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 0, 0, &I_warning_128x64);
}

static bool disclaimer_view_input_callback(InputEvent* event, void* context) {
    CocktailBookApp* app = context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyOk) {
        cocktail_book_switch_to(app, CocktailBookViewCategoryMenu);
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* About view                                                                 */
/* -------------------------------------------------------------------------- */

static void about_view_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Cocktail Book");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 6, 22, "Version:");
    canvas_draw_str(canvas, 58, 22, APP_VERSION);

    canvas_draw_str(canvas, 6, 32, "Credits:");
    canvas_draw_str(canvas, 58, 32, "@resu95");

    canvas_draw_str(canvas, 6, 42, "Recipes:");
    canvas_draw_str(canvas, 58, 42, "@der_spirituelle");

    canvas_draw_line(canvas, 2, 50, 125, 50);
    canvas_draw_str(canvas, 6, 59, "DB: /apps_data/recipes.txt");
}

static bool about_view_input_callback(InputEvent* event, void* context) {
    CocktailBookApp* app = context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(event->key == InputKeyOk || event->key == InputKeyBack) {
        cocktail_book_switch_to(app, CocktailBookViewCategoryMenu);
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* Recipe view                                                                */
/* -------------------------------------------------------------------------- */

static void recipe_view_draw_callback(Canvas* canvas, void* model) {
    RecipeViewModel* recipe_model = model;
    const Cocktail* cocktail = recipe_model->cocktail;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);

    if(!cocktail) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "No Recipe");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "Back = Menu");
        return;
    }

    canvas_draw_icon(canvas, 5, 2, header_icon_for_cocktail(cocktail));

    canvas_set_font(canvas, FontPrimary);
    draw_text_fit(canvas, 19, 11, 55, cocktail->name);

    canvas_set_font(canvas, FontSecondary);
    draw_text_fit(canvas, 78, 11, 46, recipe_section_get_title(recipe_model->section));

    canvas_draw_line(canvas, 2, 14, 125, 14);

    canvas_set_font(canvas, FontSecondary);

    if(recipe_model->section == RecipeSectionOverview) {
        canvas_draw_icon(canvas, 5, 18, &I_bottle_10x10);
        draw_text_fit(canvas, 19, 27, 105, cocktail->base);

        canvas_draw_icon(canvas, 5, 31, glass_icon_for_cocktail(cocktail));
        draw_text_fit(canvas, 19, 40, 43, cocktail->glass);

        canvas_draw_icon(canvas, 66, 31, ice_icon_for_cocktail(cocktail));
        draw_text_fit(canvas, 80, 40, 43, ice_label_for_cocktail(cocktail));

        canvas_draw_icon(canvas, 5, 44, &I_garnish_10x10);
        draw_text_fit(canvas, 19, 53, 105, cocktail->garnish);
    } else {
        const char* section_text = recipe_section_get_text(cocktail, recipe_model->section);
        uint8_t max_scroll = cocktail_book_max_scroll_for_text(section_text);

        draw_multiline_window(canvas, section_text, recipe_model->scroll_line, 4, 25, 108);

        if(recipe_model->scroll_line > 0) {
            canvas_draw_str(canvas, 119, 28, "^");
        }

        if(recipe_model->scroll_line < max_scroll) {
            canvas_draw_str(canvas, 119, 50, "v");
        }

        canvas_draw_line(canvas, 2, 54, 125, 54);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 61, "OK Next");
        canvas_draw_str(canvas, 84, 61, "<> Tab");
    }
}

static bool recipe_view_input_callback(InputEvent* event, void* context) {
    CocktailBookApp* app = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool handled = false;

    if(event->key == InputKeyOk || event->key == InputKeyRight) {
        with_view_model(
            app->recipe_view,
            RecipeViewModel * model,
            {
                model->section = (RecipeSection)((model->section + 1) % RecipeSectionCount);
                model->scroll_line = 0;
            },
            true);

        handled = true;
    } else if(event->key == InputKeyLeft) {
        with_view_model(
            app->recipe_view,
            RecipeViewModel * model,
            {
                if(model->section == 0) {
                    model->section = RecipeSectionCount - 1;
                } else {
                    model->section = (RecipeSection)(model->section - 1);
                }

                model->scroll_line = 0;
            },
            true);

        handled = true;
    } else if(event->key == InputKeyDown) {
        with_view_model(
            app->recipe_view,
            RecipeViewModel * model,
            {
                const char* section_text = recipe_section_get_text(model->cocktail, model->section);
                uint8_t max_scroll = cocktail_book_max_scroll_for_text(section_text);

                if(model->scroll_line < max_scroll) {
                    model->scroll_line++;
                }
            },
            true);

        handled = true;
    } else if(event->key == InputKeyUp) {
        with_view_model(
            app->recipe_view,
            RecipeViewModel * model,
            {
                if(model->scroll_line > 0) {
                    model->scroll_line--;
                }
            },
            true);

        handled = true;
    }

    return handled;
}

/* -------------------------------------------------------------------------- */
/* Menu callbacks                                                             */
/* -------------------------------------------------------------------------- */

static void category_selected_callback(void* context, uint32_t index) {
    CocktailBookApp* app = context;

    if(index == MENU_ITEM_ABOUT) {
        cocktail_book_switch_to(app, CocktailBookViewAbout);
        return;
    }

    app->selected_category = (CocktailCategory)index;
    cocktail_book_build_cocktail_menu(app);
    cocktail_book_switch_to(app, CocktailBookViewCocktailMenu);
}

static void cocktail_selected_callback(void* context, uint32_t index) {
    CocktailBookApp* app = context;

    if(index >= app->cocktail_count) {
        return;
    }

    app->selected_cocktail = index;
    const Cocktail* cocktail = &app->cocktails[index];

    with_view_model(
        app->recipe_view,
        RecipeViewModel * model,
        {
            model->cocktail = cocktail;
            model->section = RecipeSectionOverview;
            model->scroll_line = 0;
        },
        true);

    cocktail_book_switch_to(app, CocktailBookViewRecipe);
}

static void cocktail_book_build_cocktail_menu(CocktailBookApp* app) {
    submenu_reset(app->cocktail_menu);
    submenu_set_header(app->cocktail_menu, cocktail_category_get_name(app->selected_category));

    uint32_t visible_count = 0;

    for(uint32_t i = 0; i < app->cocktail_count; i++) {
        if(cocktail_matches_category(&app->cocktails[i], app->selected_category)) {
            submenu_add_item(
                app->cocktail_menu,
                app->cocktails[i].name,
                i,
                cocktail_selected_callback,
                app);

            visible_count++;
        }
    }

    if(visible_count == 0) {
        submenu_add_item(
            app->cocktail_menu,
            "Keine Cocktails",
            UINT32_MAX,
            cocktail_selected_callback,
            app);
    }

    submenu_set_selected_item(app->cocktail_menu, 0);
}

static bool cocktail_book_back_callback(void* context) {
    CocktailBookApp* app = context;

    switch(app->current_view) {
    case CocktailBookViewRecipe:
        cocktail_book_switch_to(app, CocktailBookViewCocktailMenu);
        return true;

    case CocktailBookViewCocktailMenu:
        cocktail_book_switch_to(app, CocktailBookViewCategoryMenu);
        return true;

    case CocktailBookViewAbout:
        cocktail_book_switch_to(app, CocktailBookViewCategoryMenu);
        return true;

    case CocktailBookViewCategoryMenu:
        cocktail_book_switch_to(app, CocktailBookViewDisclaimer);
        return true;

    case CocktailBookViewDisclaimer:
        cocktail_book_switch_to(app, CocktailBookViewTitle);
        return true;

    case CocktailBookViewTitle:
    default:
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
}

/* -------------------------------------------------------------------------- */
/* View creation                                                              */
/* -------------------------------------------------------------------------- */

static void cocktail_book_create_title_view(CocktailBookApp* app) {
    app->title_view = view_alloc();

    view_set_context(app->title_view, app);
    view_set_draw_callback(app->title_view, title_view_draw_callback);
    view_set_input_callback(app->title_view, title_view_input_callback);
}

static void cocktail_book_create_disclaimer_view(CocktailBookApp* app) {
    app->disclaimer_view = view_alloc();

    view_set_context(app->disclaimer_view, app);
    view_set_draw_callback(app->disclaimer_view, disclaimer_view_draw_callback);
    view_set_input_callback(app->disclaimer_view, disclaimer_view_input_callback);
}

static void cocktail_book_create_about_view(CocktailBookApp* app) {
    app->about_view = view_alloc();

    view_set_context(app->about_view, app);
    view_set_draw_callback(app->about_view, about_view_draw_callback);
    view_set_input_callback(app->about_view, about_view_input_callback);
}

static void cocktail_book_create_category_menu(CocktailBookApp* app) {
    app->category_menu = submenu_alloc();
    submenu_set_header(app->category_menu, "Category");

    for(CocktailCategory cat = CocktailCategoryAll; cat < CocktailCategoryCount; cat++) {
        submenu_add_item(
            app->category_menu,
            cocktail_category_get_name(cat),
            cat,
            category_selected_callback,
            app);
    }

    submenu_add_item(
        app->category_menu,
        "About / Credits",
        MENU_ITEM_ABOUT,
        category_selected_callback,
        app);
}

static void cocktail_book_create_cocktail_menu(CocktailBookApp* app) {
    app->cocktail_menu = submenu_alloc();
    app->selected_category = CocktailCategoryAll;
    cocktail_book_build_cocktail_menu(app);
}

static void cocktail_book_create_recipe_view(CocktailBookApp* app) {
    app->recipe_view = view_alloc();

    view_allocate_model(app->recipe_view, ViewModelTypeLocking, sizeof(RecipeViewModel));
    view_set_context(app->recipe_view, app);
    view_set_draw_callback(app->recipe_view, recipe_view_draw_callback);
    view_set_input_callback(app->recipe_view, recipe_view_input_callback);

    with_view_model(
        app->recipe_view,
        RecipeViewModel * model,
        {
            model->cocktail = NULL;
            model->section = RecipeSectionOverview;
            model->scroll_line = 0;
        },
        false);
}

/* -------------------------------------------------------------------------- */
/* App lifecycle                                                              */
/* -------------------------------------------------------------------------- */

static CocktailBookApp* cocktail_book_alloc(void) {
    CocktailBookApp* app = malloc(sizeof(CocktailBookApp));
    memset(app, 0, sizeof(CocktailBookApp));

    cocktail_book_load_fallback_recipes(app);

    if(cocktail_book_try_load_sd_recipes(app)) {
        snprintf(app->title_status, sizeof(app->title_status), "SD: recipes.txt geladen");
    } else {
        cocktail_book_load_fallback_recipes(app);
        snprintf(app->title_status, sizeof(app->title_status), "Fallback + sample erstellt");
    }

    app->gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();

    app->current_view = CocktailBookViewTitle;
    app->selected_category = CocktailCategoryAll;
    app->selected_cocktail = 0;

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, cocktail_book_back_callback);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    cocktail_book_create_title_view(app);
    cocktail_book_create_disclaimer_view(app);
    cocktail_book_create_about_view(app);
    cocktail_book_create_category_menu(app);
    cocktail_book_create_cocktail_menu(app);
    cocktail_book_create_recipe_view(app);

    view_dispatcher_add_view(app->dispatcher, CocktailBookViewTitle, app->title_view);
    view_dispatcher_add_view(app->dispatcher, CocktailBookViewDisclaimer, app->disclaimer_view);
    view_dispatcher_add_view(
        app->dispatcher,
        CocktailBookViewCategoryMenu,
        submenu_get_view(app->category_menu));
    view_dispatcher_add_view(
        app->dispatcher,
        CocktailBookViewCocktailMenu,
        submenu_get_view(app->cocktail_menu));
    view_dispatcher_add_view(app->dispatcher, CocktailBookViewRecipe, app->recipe_view);
    view_dispatcher_add_view(app->dispatcher, CocktailBookViewAbout, app->about_view);

    return app;
}

static void cocktail_book_free(CocktailBookApp* app) {
    view_dispatcher_remove_view(app->dispatcher, CocktailBookViewTitle);
    view_dispatcher_remove_view(app->dispatcher, CocktailBookViewDisclaimer);
    view_dispatcher_remove_view(app->dispatcher, CocktailBookViewCategoryMenu);
    view_dispatcher_remove_view(app->dispatcher, CocktailBookViewCocktailMenu);
    view_dispatcher_remove_view(app->dispatcher, CocktailBookViewRecipe);
    view_dispatcher_remove_view(app->dispatcher, CocktailBookViewAbout);

    view_free(app->title_view);
    view_free(app->disclaimer_view);
    view_free(app->about_view);

    submenu_free(app->category_menu);
    submenu_free(app->cocktail_menu);

    view_free_model(app->recipe_view);
    view_free(app->recipe_view);

    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t cocktail_book_app(void* p) {
    UNUSED(p);

    CocktailBookApp* app = cocktail_book_alloc();

    cocktail_book_switch_to(app, CocktailBookViewTitle);
    view_dispatcher_run(app->dispatcher);

    cocktail_book_free(app);

    return 0;
}
