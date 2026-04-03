# micropython implementation of FlipWorld: https://github.com/jblanked/FlipWorld/tree/main
_game = None


def start(view_manager) -> bool:
    """Start the app"""

    wifi = view_manager.wifi

    # if not a wifi device, return
    if not wifi:
        view_manager.alert("WiFi not available...", False)
        return False

    # if wifi isn't connected, return
    if not wifi.is_connected():
        from picoware.applications.wifi.utils import connect_to_saved_wifi

        view_manager.alert("WiFi not connected", False)
        connect_to_saved_wifi(view_manager)
        return False

    global _game

    view_manager.freq(True)  # set to lower frequency

    from flip_world.run import FlipWorldRun

    _game = FlipWorldRun(view_manager)

    return _game is not None


def run(view_manager) -> None:
    """Run the app"""

    inp = view_manager.input_manager
    button = inp.button

    if not _game or not _game.is_active:
        inp.reset()
        view_manager.back()
        return

    _game.update_input(button)
    _game.update_draw()

    if button != -1:
        inp.reset()


def stop(view_manager) -> None:
    """Stop the app"""
    from gc import collect

    global _game

    if _game:
        del _game
        _game = None

    view_manager.freq()  # set to default frequency

    collect()
