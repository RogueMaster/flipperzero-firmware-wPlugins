import flipperzero as f0

f0.dialog_message_set_header('Important',64, 12)
f0.dialog_message_set_text('Shutdown?', 64, 24)
f0.dialog_message_set_button('Yes', f0.INPUT_BUTTON_LEFT)
f0.dialog_message_set_button('No', f0.INPUT_BUTTON_RIGHT)

while f0.dialog_message_show() is not f0.INPUT_BUTTON_LEFT:
  pass
