// DO NOT SET PRAGMA ONCE OR OTHER HEADER GUARDING PATTERNS.
// IT SHOULD BE INCLUDED MULTIPLE TIMES, AND IS INTENDED.

/**
 * This is the file for defining the scenes.
 * 
 * Use it as following:
 * 1. Define the scene via SCENE_ACTION macro. (e.g. SCENE_ACTION(Home))
 * 2. Implement the scene handlers in the corresponding file.
 *    _on_enter, _on_event, _on_exit, _get_view, _alloc, _free
 *      e.g. Home -> Home_on_enter should be implemented
 * 3. Include the scene in the list of scenes in main.h.
 */

SCENE_ACTION(MTP)
