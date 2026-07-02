# WikiFlip

WikiFlip is a Flipper Zero FAP application that provides an offline cybersecurity dictionary. Version 1.3 uses static C data, so it builds without microSD parsing, external files, or network access at runtime.

English is the default language. Spanish is available from the in-app `Settings` menu.

## Content

WikiFlip includes 100 terms in English and the same 100 terms in Spanish:

```text
Protocols               12
OWASP Top 10:2025       10
NIST                    10
CVE                     12
Blue Team               12
Red Team                12
Threat Intelligence     10
Hardware Hacking        12
ISO                     10
Settings                Language selector
```

The OWASP category includes the full OWASP Top 10:2025:

```text
A01:2025 - Broken Access Control
A02:2025 - Security Misconfiguration
A03:2025 - Software Supply Chain Failures
A04:2025 - Cryptographic Failures
A05:2025 - Injection
A06:2025 - Insecure Design
A07:2025 - Authentication Failures
A08:2025 - Software or Data Integrity Failures
A09:2025 - Security Logging and Alerting Failures
A10:2025 - Mishandling of Exceptional Conditions
```

## Architecture

```text
WikiFlipApp
  Gui*
  ViewDispatcher*
  Submenu* categories_menu
  Submenu* terms_menu
  Submenu* settings_menu
  Widget* definition_widget
  FuriString* definition_buffer
  WikiFlipLanguage language
  WikiFlipCategory* current_category
```

The GUI uses `ViewDispatcher` with four views:

```text
WikiFlipViewCategories   -> main category submenu
WikiFlipViewTerms        -> term submenu for selected category
WikiFlipViewSettings     -> language selector submenu
WikiFlipViewDefinition   -> scrollable definition widget
```

Long definitions are rendered with `widget_add_text_scroll_element()`. D-pad Up/Down provides native vertical scrolling.

## Navigation

```text
Open app
  -> Categories
      OK: open category terms
      Settings: open language selector
      Back: exit

  -> Settings
      OK: choose English or Spanish
      Back: return to categories

  -> Terms
      OK: open definition
      Back: return to categories

  -> Definition
      Up/Down: scroll
      Back: return to terms
```

## Adding Terms

1. Open `wikiflip.c`.
2. Find the target category arrays for both languages:

```c
static const WikiFlipTerm wikiflip_red_team_terms_en[] = {
    {
        .title = "C2",
        .definition = "Definition...",
    },
};
```

```c
static const WikiFlipTerm wikiflip_red_team_terms_es[] = {
    {
        .title = "C2",
        .definition = "Definicion...",
    },
};
```

3. Add the matching entry to both arrays:

```c
{
    .title = "New Term",
    .definition =
        "Long technical definition. Use \\n\\n for paragraph breaks "
        "and adjacent string literals to keep the source readable.",
},
```

`WIKIFLIP_ARRAY_SIZE()` calculates the term count at compile time.

## Adding Categories

1. Create one array per language:

```c
static const WikiFlipTerm wikiflip_forensics_terms_en[] = {
    {
        .title = "Timeline",
        .definition = "Chronological sequence of events used in forensic analysis.",
    },
};
```

2. Register the English category in `wikiflip_categories_en[]` and the Spanish category in `wikiflip_categories_es[]`:

```c
{
    .name = "Forensics",
    .terms = wikiflip_forensics_terms_en,
    .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_forensics_terms_en),
},
```

## Building With uFBT

From the project root:

```powershell
ufbt
```

Expected output:

```text
dist/wikiflip.fap
```

Install and launch on a USB-connected Flipper:

```powershell
ufbt launch
```

## Building Inside Firmware

Copy the project into `applications_user` for official firmware, Momentum, or another FBT-compatible fork:

```powershell
Copy-Item -Recurse "C:\Users\R0G3R\Documents\Tools\Flipper Zero\WikiFlip" "C:\path\firmware\applications_user\WikiFlip"
```

Build:

```powershell
.\fbt.cmd build APPSRC=applications_user\WikiFlip
```

Launch:

```powershell
.\fbt.cmd launch APPSRC=applications_user\WikiFlip
```

Build by App ID:

```powershell
.\fbt.cmd fap_wikiflip
```

## Icon

The FAP icon is:

```text
images/wikiflip_10px.png
```

It is a 10x10 px, 1-bit PNG with a compact book silhouette and `WF` lettering.

## Flipper Lab Submission

Flipper Lab apps are published through the official `flipperdevices/flipper-application-catalog` repository. See:

```text
docs/flipper_lab_submission.md
```

Catalog manifest template:

```text
docs/manifest.yml.example
```

## Memory Lifecycle

Allocation:

```text
malloc(WikiFlipApp)
furi_record_open(RECORD_GUI)
view_dispatcher_alloc()
submenu_alloc()
submenu_alloc(settings_menu)
widget_alloc()
furi_string_alloc()
view_dispatcher_add_view()
view_dispatcher_attach_to_gui()
```

Free:

```text
view_dispatcher_remove_view()
widget_free()
submenu_free(settings_menu)
submenu_free()
view_dispatcher_free()
furi_string_free()
furi_record_close(RECORD_GUI)
free(WikiFlipApp)
```

This order prevents the dispatcher from retaining references to already-freed views.
