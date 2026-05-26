#include "include/version.h"
#include <assert.h>
#include <string.h>

int main(void) {
    /* Smoke test: APP_VERSION must be defined, non-empty, and look like
     * "X.Y.Z" — but the exact value is owned by release-please, so the test
     * is intentionally version-agnostic. Do not hardcode a specific version
     * here, or every release-please bump will break the build. */
    const char *v = APP_VERSION;
    assert(v[0] != '\0');
    assert(strlen(v) >= 5u);
    assert(strchr(v, '.') != NULL);
    return 0;
}
