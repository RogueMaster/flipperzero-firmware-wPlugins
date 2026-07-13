#pragma once

// Single source of truth for the app version. `application.fam` parses this file
// too, so bumping the number here updates the .fap metadata, the catalog listing
// and the About screen all at once. Format must be major.minor (catalog rule).
#define POCKETLAB_VERSION FAP_VERSION
