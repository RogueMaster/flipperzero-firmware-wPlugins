#include "actions.h"

static void actions_tap(Transport* transport, TransportKey key) {
    transport_press(transport, key);
    transport_release(transport, key);
}

/* docs/guide/claude-code-setup.md binds the Ctrl+X chords on the Mac to `command:` lines, as typed
 * text would come out garbled under a non-Latin layout, and its Enter would send the draft. It
 * binds Ctrl+Q to `voice:pushToTalk`: a Ctrl letter stays a control code under any layout, where
 * an Option chord arrives as a character no binding matches. Effort max's chord must not end in
 * Ctrl+Q, which starts a recording even as a chord's second key. */
void actions_perform(Transport* transport, Action action) {
    switch(action) {
    case ActionEnter:
        actions_tap(transport, TransportKeyEnter);
        break;
    case ActionArrowUp:
        actions_tap(transport, TransportKeyArrowUp);
        break;
    case ActionArrowDown:
        actions_tap(transport, TransportKeyArrowDown);
        break;
    case ActionEsc:
        actions_tap(transport, TransportKeyEsc);
        break;
    case ActionEffortHigh:
        actions_tap(transport, TransportKeyControlX);
        actions_tap(transport, TransportKeyControlF);
        break;
    case ActionEffortMax:
        actions_tap(transport, TransportKeyControlX);
        actions_tap(transport, TransportKeyControlX);
        break;
    case ActionClear:
        actions_tap(transport, TransportKeyControlX);
        actions_tap(transport, TransportKeyControlL);
        break;
    case ActionVoiceStart:
        transport_press(transport, TransportKeyControlQ);
        break;
    case ActionVoiceStop:
        transport_release(transport, TransportKeyControlQ);
        break;
    case ActionGiveUp:
        transport_withdraw(transport);
        break;
    case ActionStartOver:
        transport_offer(transport);
        break;
    case ActionUseUsb:
        transport_use_usb(transport);
        break;
    case ActionUseBluetooth:
        transport_use_bluetooth(transport);
        break;
    default:
        break;
    }
}
