/***
 * Noptel LRF rangefinder sampler for the Flipper Zero
 * Version: 2.4
 *
 * Test boot time view
***/

/*** Routines ***/

/** Test boot time view enter callback **/
void testboottime_view_enter_callback(void*);

/** Test boot time view exit callback **/
void testboottime_view_exit_callback(void*);

/** Draw callback for the test boot time view **/
void testboottime_view_draw_callback(Canvas*, void*);

/** Input callback for the test boot time view **/
bool testboottime_view_input_callback(InputEvent*, void*);
