from micropython import const

MAX_WORLD_OBJECTS = const(25)

# Game view constants
GAME_VIEW_TITLE = const(0)  # story, pvp, and pve (menu)
GAME_VIEW_SYSTEM_MENU = const(1)  # profile, settings (menu)
GAME_VIEW_GAME = const(2)  # story mode
GAME_VIEW_LOGIN = const(3)  # login view
GAME_VIEW_REGISTRATION = const(4)  # registration view
GAME_VIEW_USER_INFO = const(5)  # user info view
GAME_VIEW_LOBBIES = const(6)  # lobbies view
GAME_VIEW_JOIN_LOBBY = const(7)  # join lobby view

# Title index
TITLE_INDEX_STORY = const(0)  # switch to story mode
TITLE_INDEX_PVE = const(1)  # switch to PvE mode

# Menu index
MENU_INDEX_PROFILE = const(0)  # profile
MENU_INDEX_ABOUT = const(1)  # about
MENU_INDEX_LEAVE_GAME = const(2)  # leave game

# Game states
GAME_STATE_PLAYING = const(0)  # game is currently playing
GAME_STATE_MENU = const(1)  # game is in menu state
GAME_STATE_SWITCHING_LEVELS = const(2)  # game is switching levels
GAME_STATE_LEAVING_GAME = const(3)  # game is leaving

# Toggle states
TOGGLE_ON = const(0)  # On
TOGGLE_OFF = const(1)  # Off

# Level index
LEVEL_UNKNOWN = const(-1)  # Unknown level
LEVEL_HOME_WOODS = const(0)  # Home Woods level
LEVEL_ROCK_WORLD = const(1)  # Rock World level
LEVEL_FOREST_WORLD = const(2)  # Forest World level

# Icon IDs
ICON_ID_INVALID = const(-1)
ICON_ID_HOUSE = const(0)
ICON_ID_PLANT = const(1)
ICON_ID_TREE = const(2)
ICON_ID_FENCE = const(3)
ICON_ID_FLOWER = const(4)
ICON_ID_ROCK_LARGE = const(5)
ICON_ID_ROCK_MEDIUM = const(6)
ICON_ID_ROCK_SMALL = const(7)


def constraint(amt, low, high):
    """Constrains amt to be between low and high."""
    return max(low, min(amt, high))


class IconSpec:
    """Contains the specification for an icon."""

    __slots__ = ("id", "x", "y", "width", "height")

    def __init__(
        self, icon_id: int, width: int, height: int, pos_x: int = 0, pos_y: int = 0
    ) -> None:
        self.id = icon_id
        self.x = pos_x
        self.y = pos_y
        self.width = width
        self.height = height


class IconGroupContext:
    """Contains a group of icons."""

    def __init__(self, icons: list[IconSpec]):
        self.icons: list[IconSpec] = icons  # list of IconSpec objects

    def __del__(self):
        self.icons.clear()
        del self.icons
        self.icons = None

    @property
    def count(self) -> int:
        """Returns the number of icons in the group."""
        return len(self.icons)

    def append(self, icon_spec: IconSpec):
        """Appends an icon specification to the group."""
        try:
            self.icons.append(icon_spec)
        except MemoryError as e:
            print(f"IconGroupContext.append: MemoryError: {e}")

    def clear(self):
        """Clears the icon group."""
        self.icons.clear()


def toggle_to_bool(state: int) -> bool:
    """Returns True if toggle is On, False if Off."""
    return state == TOGGLE_ON


def toggle_to_string(state: int) -> str:
    """Returns "On" or "Off" based on toggle state."""
    return "On" if state == TOGGLE_ON else "Off"
