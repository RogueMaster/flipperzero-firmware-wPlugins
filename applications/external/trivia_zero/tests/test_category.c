#include "include/domain/category.h"
#include <assert.h>
#include <string.h>

int main(void) {
    /* Spanish names */
    assert(strcmp(category_name(1, LangEs), "Geografia") == 0);
    assert(strcmp(category_name(2, LangEs), "Entretenimiento") == 0);
    assert(strcmp(category_name(3, LangEs), "Historia") == 0);
    assert(strcmp(category_name(4, LangEs), "Arte y Literatura") == 0);
    assert(strcmp(category_name(5, LangEs), "Ciencia y Naturaleza") == 0);
    assert(strcmp(category_name(6, LangEs), "Deportes y Ocio") == 0);
    assert(strcmp(category_name(7, LangEs), "Cultura General") == 0);

    /* English names */
    assert(strcmp(category_name(1, LangEn), "Geography") == 0);
    assert(strcmp(category_name(2, LangEn), "Entertainment") == 0);
    assert(strcmp(category_name(3, LangEn), "History") == 0);
    assert(strcmp(category_name(4, LangEn), "Arts & Literature") == 0);
    assert(strcmp(category_name(5, LangEn), "Science & Nature") == 0);
    assert(strcmp(category_name(6, LangEn), "Sports & Leisure") == 0);
    assert(strcmp(category_name(7, LangEn), "General Knowledge") == 0);

    /* Out-of-range falls back to a sentinel */
    assert(strcmp(category_name(0, LangEs), "?") == 0);
    assert(strcmp(category_name(8, LangEn), "?") == 0);
    assert(strcmp(category_name(255, LangEs), "?") == 0);

    return 0;
}
