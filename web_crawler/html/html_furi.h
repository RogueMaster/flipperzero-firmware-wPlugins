#pragma once
#include <furi.h>
#include <furi_hal.h>

/*
 * @brief Parse a Furigana string from an HTML tag, handling nested child tags.
 *
 * This version accepts an HTML tag as a C-string (e.g., "<p>") and searches
 * for the content inside the corresponding opening and closing tags within
 * the provided HTML string, taking into account nested occurrences of the tag.
 *
 * For example, given the HTML string:
 *     "<p><h1><p><h1>Test</h1></p></h1></p>"
 * and searching with tag "<p>" the function will return:
 *     "<h1><p><h1>Test</h1></p></h1>"
 *
 * @param tag The HTML tag to parse (including the angle brackets).
 * @param html The HTML string to parse (as a FuriString).
 * @return A newly allocated FuriString containing the parsed content,
 *         or an empty FuriString if the tag is not found.
 */
FuriString *html_furi_find_tag(const char *tag, FuriString *html, size_t index);

/*
 * @brief Parse all Furigana strings from an HTML tag, handling nested child tags.
 * @param tag The HTML tag to parse (including the angle brackets).
 * @param html The HTML string to parse (as a FuriString).
 * @return A newly allocated FuriString containing the parsed content,
 *         or an empty FuriString if the tag is not found.
 */
FuriString *html_furi_find_tags(const char *tag, FuriString *html);

/*
 * @brief Check if an HTML tag exists in the provided HTML string.
 * @param tag The HTML tag to search for (including the angle brackets).
 * @param html The HTML string to search (as a FuriString).
 * @param index The starting index to search from.
 * @return True if the tag exists in the HTML string, false otherwise.
 */
bool html_furi_tag_exists(const char *tag, FuriString *html, size_t index);