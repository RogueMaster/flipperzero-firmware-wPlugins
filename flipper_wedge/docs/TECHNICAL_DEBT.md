# Technical Debt

Known issues that are low-priority but worth addressing in future maintenance.

---

## Static Variables in Settings Scene

**File:** `scenes/flipper_wedge_scene_settings.c:84-87`

**Issue:** The keyboard layout selector uses static variables for temporary storage:

```c
static FuriString* layout_custom_names[LAYOUT_MAX_CUSTOM];
static FuriString* layout_custom_paths[LAYOUT_MAX_CUSTOM];
static size_t layout_custom_count = 0;
static size_t layout_total_count = LAYOUT_BUILTIN_COUNT;
```

Static variables persist across scene exits/re-enters. If users rapidly transition in/out of the settings scene, there's potential for memory issues (though unlikely in practice).

**Severity:** Low - only affects edge case of rapid scene transitions

**Proposed Solution:**

1. Add fields to `FlipperWedge` struct in `flipper_wedge.h`:
   ```c
   FuriString* layout_custom_names[16];
   FuriString* layout_custom_paths[16];
   size_t layout_custom_count;
   ```

2. Initialize in `flipper_wedge_app_alloc()`:
   ```c
   app->layout_custom_count = 0;
   for(size_t i = 0; i < 16; i++) {
       app->layout_custom_names[i] = NULL;
       app->layout_custom_paths[i] = NULL;
   }
   ```

3. Free in `flipper_wedge_app_free()`:
   ```c
   for(size_t i = 0; i < app->layout_custom_count; i++) {
       if(app->layout_custom_names[i]) furi_string_free(app->layout_custom_names[i]);
       if(app->layout_custom_paths[i]) furi_string_free(app->layout_custom_paths[i]);
   }
   ```

4. Remove static variables from settings scene
5. Update all references to use `app->` prefix
6. Remove free loop from `on_exit` (app owns lifecycle)

**Risk:** Low - mechanical refactor, compiler catches missing references

**Added:** 2026-02-04
