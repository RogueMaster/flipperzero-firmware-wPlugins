# WikiFlip

WikiFlip es una aplicacion FAP para Flipper Zero que implementa un diccionario offline de ciberseguridad. La version 1.1 usa datos estaticos en C para compilar sin microSD, parsers externos ni conectividad.

## Contenido

WikiFlip incluye 100 terminos distribuidos asi:

```text
Protocolos              12
OWASP Top 10:2025       10
NIST                    10
CVE                     12
BlueTeam                12
RedTeam                 12
Threat Intelligence     10
HardwareHacking         12
ISO                     10
```

La categoria OWASP contiene el Top 10:2025 completo:

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

## Arquitectura

```text
WikiFlipApp
  Gui*
  ViewDispatcher*
  Submenu* categories_menu
  Submenu* terms_menu
  Widget* definition_widget
  FuriString* definition_buffer
  WikiFlipCategory* current_category
```

La GUI usa `ViewDispatcher` con tres vistas:

```text
WikiFlipViewCategories   -> Submenu principal de categorias
WikiFlipViewTerms        -> Submenu de terminos por categoria
WikiFlipViewDefinition   -> Widget con scroll vertical
```

Las definiciones largas se muestran con `widget_add_text_scroll_element()`. El scroll se controla con D-pad Up/Down de forma nativa.

## Mapa de navegacion

```text
Abrir app
  -> Categorias
      OK: abrir terminos de categoria
      Back: salir

  -> Terminos
      OK: abrir definicion
      Back: volver a categorias

  -> Definicion
      Up/Down: scroll vertical
      Back: volver a terminos
```

## Como anadir terminos

1. Abrir `wikiflip.c`.
2. Ubicar el array de la categoria:

```c
static const WikiFlipTerm wikiflip_redteam_terms[] = {
    {
        .title = "C2",
        .definition = "Definicion...",
    },
};
```

3. Agregar una entrada:

```c
{
    .title = "Nuevo Termino",
    .definition =
        "Definicion tecnica larga. Puede usar saltos de linea con \\n\\n "
        "y literales concatenados para mantener el codigo legible.",
},
```

`WIKIFLIP_ARRAY_SIZE()` calcula el contador en compilacion.

## Como anadir categorias

1. Crear un array:

```c
static const WikiFlipTerm wikiflip_forense_terms[] = {
    {
        .title = "Timeline",
        .definition = "Secuencia temporal de eventos para analisis forense.",
    },
};
```

2. Registrar la categoria en `wikiflip_categories[]`:

```c
{
    .name = "Forense",
    .terms = wikiflip_forense_terms,
    .term_count = WIKIFLIP_ARRAY_SIZE(wikiflip_forense_terms),
},
```

## Compilacion con uFBT

Desde la raiz del proyecto:

```powershell
ufbt
```

Salida esperada:

```text
dist/wikiflip.fap
```

Para instalar y lanzar en un Flipper conectado por USB:

```powershell
ufbt launch
```

## Compilacion dentro de firmware

Copiar el proyecto dentro de `applications_user` del firmware oficial, Momentum u otro fork compatible con FBT:

```powershell
Copy-Item -Recurse "C:\Users\R0G3R\Documents\Tools\Flipper Zero\WikiFlip" "C:\ruta\firmware\applications_user\WikiFlip"
```

Compilar:

```powershell
.\fbt.cmd build APPSRC=applications_user\WikiFlip
```

Lanzar:

```powershell
.\fbt.cmd launch APPSRC=applications_user\WikiFlip
```

Build directo por App ID:

```powershell
.\fbt.cmd fap_wikiflip
```

## Icono

El icono FAP esta en:

```text
images/wikiflip_10px.png
```

Es un PNG 10x10 px, 1-bit, con silueta de libro y letras `WF`.

## Publicacion en Flipper Lab

Flipper Lab no recibe el binario manualmente para publicarlo en el catalogo. La publicacion se hace mediante pull request al repositorio oficial `flipperdevices/flipper-application-catalog`, apuntando a un repositorio GitHub publico que contenga esta app.

Guia local:

```text
docs/flipper_lab_submission.md
```

Plantilla de manifest de catalogo:

```text
docs/manifest.yml.example
```

Requisitos importantes:

```text
Repositorio GitHub publico
Licencia open source
application.fam con appid, version, categoria e icono
README.md
docs/changelog.md
screenshots reales tomadas con qFlipper
manifest.yml en applications/Tools/wikiflip/ dentro del catalogo oficial
```

## Ciclo de vida de memoria

Inicializacion:

```text
malloc(WikiFlipApp)
furi_record_open(RECORD_GUI)
view_dispatcher_alloc()
submenu_alloc()
widget_alloc()
furi_string_alloc()
view_dispatcher_add_view()
view_dispatcher_attach_to_gui()
```

Liberacion:

```text
view_dispatcher_remove_view()
widget_free()
submenu_free()
view_dispatcher_free()
furi_string_free()
furi_record_close(RECORD_GUI)
free(WikiFlipApp)
```

Este orden evita que el dispatcher mantenga referencias a vistas ya liberadas.
