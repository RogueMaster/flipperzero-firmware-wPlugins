#include "../include/nfc_tools_search.h"

// List sorted alphabetically.
// url_prefix : everything before the URL-encoded keyword.
// url_suffix : everything after  (empty for almost all entries).

const NfcToolsSearchEngine nfc_tools_search_engines[] = {
    {"Amazon",        "https://www.amazon.com/s?k=",                              ""},
    {"AOL",           "https://search.aol.com/aol/search?q=",                    ""},
    {"Archive.org",   "https://web.archive.org/web/*/",                           ""},
    {"Ask",           "https://www.ask.com/web?q=",                               ""},
    {"Baidu",         "https://www.baidu.com/s?wd=",                              ""},
    {"Bing",          "https://www.bing.com/search?q=",                           ""},
    {"Boardreader",   "https://boardreader.com/s/",                               ".html"},
    {"DogPile",       "https://www.dogpile.com/serp?q=",                          ""},
    {"DuckDuckGo",    "https://duckduckgo.com/?q=",                               ""},
    {"eBay",          "https://www.ebay.com/sch/i.html?mkcid=1&mkevt=1&kw=",     ""},
    {"Ecosia",        "https://www.ecosia.org/search?q=",                         ""},
    {"Ekoru",         "https://www.ekoru.org/?q=",                                ""},
    {"Excite",        "https://results.excite.com/serp?q=",                       ""},
    {"GMX",           "https://search.gmx.com/web?q=",                            ""},
    {"Google",        "https://www.google.com/search?q=",                         ""},
    {"Google Images", "https://www.google.com/search?tbm=isch&q=",               ""},
    {"Google News",   "https://news.google.com/search?q=",                        ""},
    {"Google Videos", "https://www.google.com/search?tbm=vid&q=",                ""},
    {"IMDB",          "https://www.imdb.com/find?q=",                             ""},
    {"Lycos",         "https://search.lycos.com/web/?q=",                         ""},
    {"Naver",         "https://search.naver.com/search.naver?query=",             ""},
    {"Qwant",         "https://www.qwant.com/?q=",                                ""},
    {"Reddit",        "https://www.reddit.com/search/?q=",                        ""},
    {"Seznam",        "https://search.seznam.cz/?q=",                             ""},
    {"SlideShare",    "https://www.slideshare.net/search/slideshow?q=",           ""},
    {"So.com",        "https://www.so.com/s?q=",                                  ""},
    {"Sogou",         "https://www.sogou.com/tx?query=",                          ""},
    {"Swisscows",     "https://swisscows.com/web?query=",                         ""},
    {"Wikipedia",     "https://wikipedia.org/wiki/",                              ""},
    {"WolframAlpha",  "https://www.wolframalpha.com/input/?i=",                   ""},
    {"X",             "https://twitter.com/search?q=",                            ""},
    {"Yahoo",         "https://search.yahoo.com/search?p=",                       ""},
    {"Yandex",        "https://yandex.ru/search/?text=",                          ""},
    {"YouTube",       "https://www.youtube.com/results?search_query=",            ""},
};

const uint8_t nfc_tools_search_engines_count =
    sizeof(nfc_tools_search_engines) / sizeof(nfc_tools_search_engines[0]);
