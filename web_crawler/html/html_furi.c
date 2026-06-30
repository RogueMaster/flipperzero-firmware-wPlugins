#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <html/html_furi.h>

/*
 * Checks if the substring of the FuriString starting at index `pos`
 * matches the given C-string `needle`.
 */
static bool furi_string_sub_equals(FuriString *str, int pos, const char *needle)
{
    size_t needle_len = strlen(needle);
    if ((size_t)pos + needle_len > furi_string_size(str))
    {
        return false;
    }
    for (size_t i = 0; i < needle_len; i++)
    {
        if (furi_string_get_char(str, pos + i) != needle[i])
        {
            return false;
        }
    }
    return true;
}

/*
 * Parse the content for a given HTML tag <tag> in `html`, handling nested tags.
 * Returns a newly allocated FuriString or NULL on error.
 *
 * @param tag    e.g. "<p>"
 * @param html   The HTML string to parse.
 * @param index  The position in `html` from where to start searching.
 */
FuriString *html_furi_find_tag(const char *tag, FuriString *html, size_t index)
{
    int tag_len = strlen(tag);
    if (tag_len < 3)
    {
        FURI_LOG_E("html_furi_parse", "Invalid tag length");
        return NULL;
    }

    // Extract the tag name from <p> => "p"
    int inner_len = tag_len - 2; // exclude '<' and '>'
    char inner_tag[inner_len + 1];
    for (int i = 0; i < inner_len; i++)
    {
        inner_tag[i] = tag[i + 1];
    }
    inner_tag[inner_len] = '\0';

    // Build closing tag => "</p>"
    char closing_tag[inner_len + 4];
    snprintf(closing_tag, sizeof(closing_tag), "</%s>", inner_tag);

    int html_len = furi_string_size(html);

    // Find the first occurrence of the opening tag
    int open_tag_index = -1;
    for (int i = index; i <= html_len - tag_len; i++)
    {
        if (furi_string_sub_equals(html, i, tag))
        {
            open_tag_index = i;
            break;
        }
    }
    if (open_tag_index == -1)
    {
        // Tag not found
        return NULL;
    }

    // Content starts after the opening tag
    int content_start = open_tag_index + tag_len;

    // Skip leading whitespace
    while (content_start < html_len && furi_string_get_char(html, content_start) == ' ')
    {
        content_start++;
    }

    // Find matching closing tag, accounting for nested tags
    int depth = 1;
    int i = content_start;
    int matching_close_index = -1;
    while (i <= html_len - 1)
    {
        if (furi_string_sub_equals(html, i, tag))
        {
            depth++;
            i += tag_len;
            continue;
        }
        if (furi_string_sub_equals(html, i, closing_tag))
        {
            depth--;
            if (depth == 0)
            {
                matching_close_index = i;
                break;
            }
            i += strlen(closing_tag);
            continue;
        }
        i++;
    }

    if (matching_close_index == -1)
    {
        // No matching close => return NULL or partial content as you choose
        return NULL;
    }

    // Copy the content between <tag>...</tag>
    size_t content_length = matching_close_index - content_start;

    if (memmgr_get_free_heap() < (content_length + 1 + 1024))
    {
        FURI_LOG_E("html_furi_parse", "Not enough heap to allocate result");
        return NULL;
    }

    // Allocate and copy
    FuriString *result = furi_string_alloc();
    furi_string_reserve(result, content_length + 1);
    furi_string_set_n(result, html, content_start, content_length);
    furi_string_trim(result);
    return result;
}

static FuriString *_html_furi_find_tag(const char *tag, FuriString *html, size_t index, int *out_next_index)
{
    // Clear next index in case of early return
    *out_next_index = -1;

    int tag_len = strlen(tag);
    if (tag_len < 3)
    {
        FURI_LOG_E("html_furi_parse", "Invalid tag length");
        return NULL;
    }

    // Extract "p" from "<p>"
    int inner_len = tag_len - 2;
    char inner_tag[inner_len + 1];
    for (int i = 0; i < inner_len; i++)
    {
        inner_tag[i] = tag[i + 1];
    }
    inner_tag[inner_len] = '\0';

    // Create closing tag => "</p>"
    char closing_tag[inner_len + 4];
    snprintf(closing_tag, sizeof(closing_tag), "</%s>", inner_tag);

    int html_len = furi_string_size(html);

    // 1) Find opening tag from `index`.
    int open_tag_index = -1;
    for (int i = index; i <= html_len - tag_len; i++)
    {
        if (furi_string_sub_equals(html, i, tag))
        {
            open_tag_index = i;
            break;
        }
    }
    if (open_tag_index == -1)
    {
        return NULL; // no more occurrences
    }

    // The content begins after the opening tag.
    int content_start = open_tag_index + tag_len;

    // skip leading spaces
    while (content_start < html_len && furi_string_get_char(html, content_start) == ' ')
    {
        content_start++;
    }

    int depth = 1;
    int i = content_start;
    int matching_close_index = -1;

    while (i < html_len)
    {
        if (furi_string_sub_equals(html, i, tag))
        {
            depth++;
            i += tag_len;
        }
        else if (furi_string_sub_equals(html, i, closing_tag))
        {
            depth--;
            i += strlen(closing_tag);
            if (depth == 0)
            {
                matching_close_index = i - strlen(closing_tag);
                // i now points just after "</p>"
                break;
            }
        }
        else
        {
            i++;
        }
    }

    if (matching_close_index == -1)
    {
        // No matching close tag found
        return NULL;
    }

    size_t content_length = matching_close_index - content_start;

    // Allocate the result
    FuriString *result = furi_string_alloc();
    furi_string_reserve(result, content_length + 1); // +1 for safety
    furi_string_set_n(result, html, content_start, content_length);
    furi_string_trim(result);
    *out_next_index = i;

    return result;
}

/*
 * Parse *all* occurrences of <tag> in `html`, handling nested tags.
 * Returns a FuriString concatenating all parsed contents.
 */
FuriString *html_furi_find_tags(const char *tag, FuriString *html)
{
    FuriString *result = furi_string_alloc();
    size_t index = 0;

    while (true)
    {
        int next_index;
        FuriString *parsed = _html_furi_find_tag(tag, html, index, &next_index);
        if (parsed == NULL)
        {
            // No more tags from 'index' onward
            break;
        }

        // Append the found content
        furi_string_cat(result, parsed);
        furi_string_cat_str(result, "\n");
        furi_string_free(parsed);

        // Resume searching at `next_index` (just after `</tag>`).
        index = next_index;
    }

    return result;
}

/*
 * @brief Check if an HTML tag exists in the provided HTML string.
 * @param tag The HTML tag to search for (including the angle brackets).
 * @param html The HTML string to search (as a FuriString).
 * @param index The starting index to search from.
 * @return True if the tag exists in the HTML string, false otherwise.
 */
bool html_furi_tag_exists(const char *tag, FuriString *html, size_t index)
{
    int tag_len = strlen(tag);
    if (tag_len < 3)
    {
        FURI_LOG_E("html_furi_parse", "Invalid tag length");
        return false;
    }

    int html_len = furi_string_size(html);

    for (int i = index; i <= html_len - tag_len; i++)
    {
        if (furi_string_sub_equals(html, i, tag))
        {
            return true;
        }
    }

    return false;
}