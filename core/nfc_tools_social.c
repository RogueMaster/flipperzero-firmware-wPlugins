#include "../include/nfc_tools_social.h"

// List sorted alphabetically.
// url_prefix : everything before the username.
// url_suffix : everything after  (empty for the majority).

const NfcToolsSocialNetwork nfc_tools_social_networks[] = {
    {"Bluesky",     "https://bsky.app/profile/",            ""},
    {"DeviantArt",  "https://www.deviantart.com/",          ""},
    {"Dribbble",    "https://www.dribbble.com/",            ""},
    {"Facebook",    "https://www.facebook.com/",            ""},
    {"Flickr",      "https://www.flickr.com/people/",       ""},
    {"GitHub",      "https://github.com/",                  ""},
    {"ICQ",         "https://icq.com/people/",              ""},
    {"Instagram",   "https://instagram.com/",               ""},
    {"Line",        "https://line.me/R/ti/p/",              ""},
    {"LinkedIn",    "https://www.linkedin.com/in/",         ""},
    {"Mastodon",    "https://mastodon.social/@",            ""},
    {"Medium",      "https://medium.com/@",                 ""},
    {"Pinterest",   "https://www.pinterest.com/",           ""},
    {"Reddit",      "https://www.reddit.com/user/",         ""},
    {"Skype",       "skype:",                               ""},
    {"Slack",       "https://",                             ".slack.com"},
    {"Snapchat",    "https://www.snapchat.com/add/",        ""},
    {"SoundCloud",  "https://www.soundcloud.com/",          ""},
    {"Steam",       "https://www.steamcommunity.com/id/",   ""},
    {"Telegram",    "https://t.me/",                        ""},
    {"Threads",     "https://www.threads.net/@",            ""},
    {"TikTok",      "https://www.tiktok.com/@",             ""},
    {"Tumblr",      "https://",                             ".tumblr.com"},
    {"Twitch",      "https://www.twitch.tv/",               ""},
    {"Unit.Link",   "https://unit.link/",                   ""},
    {"VK",          "https://vk.com/",                      ""},
    {"WeChat",      "weixin://dl/chat?",                    ""},
    {"WhatsApp",    "https://wa.me/",                       ""},
    {"X",           "https://www.twitter.com/",             ""},
    {"YouTube",     "https://www.youtube.com/@",            ""},
};

const uint8_t nfc_tools_social_networks_count =
    sizeof(nfc_tools_social_networks) / sizeof(nfc_tools_social_networks[0]);
