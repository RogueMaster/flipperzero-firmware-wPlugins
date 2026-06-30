# Ghost ESP App Confirmation View Guide

## Overview
The Confirmation View is a UI component that provides a modal dialog for confirming user actions. It features a header, message text, and OK/Cancel functionality with callbacks.

## Component Structure
```c
typedef struct {
   View* view;                        // Base view
   ConfirmationViewCallback ok_callback;     // OK button callback
   ConfirmationViewCallback cancel_callback; // Cancel button callback
   void* callback_context;            // Context passed to callbacks
} ConfirmationView;

typedef struct {
   const char* header;  // Dialog header text
   const char* text;    // Dialog message text
} ConfirmationViewModel;
```

## Using Confirmation View

### 1. Create Instance
```c
// Allocate and initialize the view
ConfirmationView* confirmation = confirmation_view_alloc();

// Add to view dispatcher
view_dispatcher_add_view(dispatcher, view_id, confirmation_view_get_view(confirmation));
```

### 2. Configure Dialog
```c
// Set the header and message
confirmation_view_set_header(confirmation, "Warning");
confirmation_view_set_text(confirmation, "This action cannot be undone.\nContinue?");

// Set callbacks
confirmation_view_set_ok_callback(confirmation, ok_handler, context);
confirmation_view_set_cancel_callback(confirmation, cancel_handler, context);
```

### 3. Show Dialog
```c
// Switch to confirmation view
view_dispatcher_switch_to_view(dispatcher, confirmation_view_id);
```

## Callback Implementation

### Basic Callbacks
```c
static void ok_callback(void* context) {
   AppState* state = context;
   // Handle confirmation
   // Return to previous view
   view_dispatcher_switch_to_view(state->view_dispatcher, previous_view);
}

static void cancel_callback(void* context) {
   AppState* state = context;
   // Handle cancellation
   // Return to previous view
   view_dispatcher_switch_to_view(state->view_dispatcher, previous_view);
}
```

### Command Execution Callback
```c
static void command_ok_callback(void* context) {
   MenuCommandContext* cmd_ctx = context;
   if(cmd_ctx && cmd_ctx->state && cmd_ctx->command) {
       // Execute command
       send_uart_command(cmd_ctx->command->command, cmd_ctx->state);
       // Handle response
       uart_receive_data(
           cmd_ctx->state->uart_context,
           cmd_ctx->state->view_dispatcher,
           cmd_ctx->state,
           cmd_ctx->command->capture_prefix,
           cmd_ctx->command->file_ext,
           cmd_ctx->command->folder);
   }
   free(cmd_ctx);  // Clean up context
}
```

## Visual Layout
```c
static void confirmation_view_draw_callback(Canvas* canvas, void* _model) {
   // Draw outer frame
   canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);
   canvas_draw_rframe(canvas, 1, 1, 126, 62, 2);

   // Draw header
   canvas_set_font(canvas, FontPrimary);
   elements_multiline_text_aligned(canvas, 64, 8, AlignCenter, AlignTop, header);

   // Draw message
   canvas_set_font(canvas, FontSecondary);
   elements_multiline_text_aligned(canvas, 64, 22, AlignCenter, AlignTop, text);

   // Draw OK button
   elements_button_center(canvas, "OK");
}
```

## Input Handling
```c
static bool confirmation_view_input_callback(InputEvent* event, void* context) {
   ConfirmationView* instance = context;
   bool consumed = false;

   if(event->type == InputTypeShort) {
       switch(event->key) {
           case InputKeyOk:
               if(instance->ok_callback) {
                   instance->ok_callback(instance->callback_context);
               }
               consumed = true;
               break;

           case InputKeyBack:
               if(instance->cancel_callback) {
                   instance->cancel_callback(instance->callback_context);
               }
               consumed = true;
               break;
       }
   }
   return consumed;
}
```

## Best Practices

1. Message Format
  - Clear, concise headers
  - Detailed but brief messages
  - Use newlines for readability
  - Keep text within view bounds

2. Callback Handling
  - Always check context validity
  - Clean up resources
  - Return to appropriate view
  - Handle errors gracefully

3. Memory Management
  - Free command context after use
  - Check for NULL pointers
  - Initialize all pointers
  - Clean up on view free

4. Visual Design
  - Consistent spacing
  - Clear visual hierarchy
  - Readable text size
  - Visible button indicator

## Example Usage

### Dangerous Action
```c
void show_dangerous_action_confirmation(AppState* state) {
   confirmation_view_set_header(state->confirmation_view, "Warning");
   confirmation_view_set_text(state->confirmation_view, 
       "This will affect all devices.\nContinue?");
   confirmation_view_set_ok_callback(state->confirmation_view, 
       dangerous_action_ok, state);
   confirmation_view_set_cancel_callback(state->confirmation_view, 
       dangerous_action_cancel, state);
   view_dispatcher_switch_to_view(state->view_dispatcher, 
       CONFIRMATION_VIEW_ID);
}
```

### Settings Reset
```c
void show_reset_confirmation(AppState* state) {
   confirmation_view_set_header(state->confirmation_view, "Reset Settings");
   confirmation_view_set_text(state->confirmation_view,
       "All settings will be reset\nto default values.\n\nContinue?");
   confirmation_view_set_ok_callback(state->confirmation_view,
       reset_settings_ok, state);
   confirmation_view_set_cancel_callback(state->confirmation_view,
       reset_settings_cancel, state);
   view_dispatcher_switch_to_view(state->view_dispatcher,
       CONFIRMATION_VIEW_ID);
}
```