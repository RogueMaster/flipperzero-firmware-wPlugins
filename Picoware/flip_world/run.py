"""FlipWorld Run class for MicroPython"""

from micropython import const
from gc import mem_free, collect
from ujson import dumps, loads
from utime import sleep_ms
from picoware.system.vector import Vector
from picoware.engine.entity import (
    ENTITY_TYPE_PLAYER,
    ENTITY_TYPE_ENEMY,
    ENTITY_TYPE_NPC,
    ENTITY_STATE_IDLE,
    ENTITY_STATE_MOVING,
    ENTITY_STATE_MOVING_TO_START,
    ENTITY_STATE_MOVING_TO_END,
    ENTITY_STATE_ATTACKING,
    ENTITY_STATE_ATTACKED,
    ENTITY_STATE_DEAD,
    Entity,
    SPRITE_3D_NONE,
)
from picoware.engine.level import Level
from flip_world.general import (
    IconSpec,
    IconGroupContext,
    LEVEL_HOME_WOODS,
    LEVEL_ROCK_WORLD,
    LEVEL_FOREST_WORLD,
    LEVEL_UNKNOWN,
    ICON_ID_HOUSE,
    ICON_ID_PLANT,
    ICON_ID_TREE,
    ICON_ID_FENCE,
    ICON_ID_FLOWER,
    ICON_ID_ROCK_LARGE,
    ICON_ID_ROCK_MEDIUM,
    ICON_ID_ROCK_SMALL,
    ICON_ID_INVALID,
)

from flip_world.player import (
    Player,
    INPUT_KEY_MAX,
    COLOR_WHITE,
    COLOR_BLACK,
    REQUEST_TYPE_SAVE_STATS,
    REQUEST_TYPE_STOP_WEBSOCKET,
)

from flip_world.assets import (
    icon_house_48x32px,
    icon_plant_16x16,
    icon_tree_16x16,
    icon_fence_16x8px,
    icon_flower_16x16,
    icon_rock_large_18x19px,
    icon_rock_medium_16x14px,
    icon_rock_small_10x8px,
    player_left_sword_15x11px,
    player_right_sword_15x11px,
)

from flip_world.sprite import Sprite

DEBUG = const(False)
MAX_WEBSOCKET_SIZE = const(256)


class QueuedMessage:
    """Structure to hold queued websocket messages"""

    __slots__ = ("message", "message_len")

    def __init__(self):
        self.message: str = ""
        self.message_len: int = 0

    def __del__(self):
        del self.message
        self.message = None
        self.message_len = 0


class FlipWorldRun:
    """Main game run class for FlipWorld."""

    MAX_QUEUED_MESSAGES = 35

    def __init__(self, view_manager):
        """Initialize the FlipWorld run instance."""
        self.view_manager = view_manager

        self.screen_size: Vector = view_manager.draw.size

        # Game state
        self.engine = None
        self.player: Player = None
        self.is_game_running: bool = False
        self.is_pve_mode: bool = False
        self.is_lobby_host: bool = False
        self.should_return_to_menu: bool = False
        self.should_debounce: bool = False

        # Input state
        self.last_input: int = INPUT_KEY_MAX

        # Message queue for websocket
        self.message_queue: list[QueuedMessage] = [
            QueuedMessage() for _ in range(self.MAX_QUEUED_MESSAGES)
        ]
        self.queue_head: int = 0
        self.queue_tail: int = 0
        self.queue_size: int = 0

        # Level management
        self.current_level_index: int = LEVEL_HOME_WOODS
        self.level_names: list = ["Home Woods", "Rock World", "Forest World"]
        self.total_levels: int = 3

        # Sound/vibration settings
        self.sound_toggle: int = 1
        self.vibration_toggle: int = 1

        # icon stuff (we'll use the map as a lookup table instead of storing icon data directly)
        self.current_icon_group: IconGroupContext = IconGroupContext([])
        self.icon_map = {
            ICON_ID_HOUSE: icon_house_48x32px,
            ICON_ID_PLANT: icon_plant_16x16,
            ICON_ID_TREE: icon_tree_16x16,
            ICON_ID_FENCE: icon_fence_16x8px,
            ICON_ID_FLOWER: icon_flower_16x16,
            ICON_ID_ROCK_LARGE: icon_rock_large_18x19px,
            ICON_ID_ROCK_MEDIUM: icon_rock_medium_16x14px,
            ICON_ID_ROCK_SMALL: icon_rock_small_10x8px,
        }

    def __del__(self):
        """Clean up resources."""
        if self.engine:
            self.engine.stop()
            del self.engine
            self.engine = None
        if self.player:
            del self.player
            self.player = None
        self.message_queue.clear()
        self.message_queue = None
        self.level_names.clear()
        self.level_names = None
        del self.screen_size
        self.screen_size = None
        del self.current_icon_group
        self.current_icon_group = None

    @property
    def draw(self):
        """Get the draw object from the view manager."""
        return self.view_manager.draw

    @property
    def current_input(self) -> int:
        """Get the last input key pressed."""
        return self.last_input

    @property
    def is_active(self) -> bool:
        """Check if the game is active."""
        return not self.should_return_to_menu

    @property
    def is_running(self) -> bool:
        """Check if the game engine is running."""
        return self.is_game_running

    @property
    def is_host(self) -> bool:
        """Check if this player is the lobby host."""
        return self.is_lobby_host

    @property
    def is_in_pve_mode(self) -> bool:
        """Check if in PvE mode."""
        return self.is_pve_mode

    def add_remote_player(self, username: str) -> bool:
        """Add a remote player to the current level (PvE mode only)."""
        if not self.is_pve_mode or not username:
            return False

        if (
            not self.engine
            or not self.engine.game
            or not self.engine.game.current_level
        ):
            return False
        from picoware.engine.image import Image

        current_level = self.engine.game.current_level

        # Add new remote player entity
        remote_player = Entity(
            username,  # name
            ENTITY_TYPE_PLAYER,  # type
            Vector(384, 192),  # position
            Vector(15, 11),  # size
            Image(Vector(15, 11), True, player_left_sword_15x11px),  # sprite_data
            Image(Vector(15, 11), True, player_left_sword_15x11px),  # sprite_left_data
            Image(
                Vector(15, 11), True, player_right_sword_15x11px
            ),  # sprite_right_data
            None,  # start
            None,  # stop
            None,  # update
            self.pve_render,  # render callback for PvE mode
            None,  # collision callback
            True,  # is 8-bit
            SPRITE_3D_NONE,  # no 3D sprite for remote players
        )

        if remote_player:
            remote_player.name = username
            # Set some default stats for now (should be updated later)
            remote_player.health = 100.0
            remote_player.max_health = 100.0
            remote_player.strength = 10.0
            remote_player.xp = 0.0
            remote_player.level = 1.0

            current_level.entity_add(remote_player)

            self.sync_multiplayer_level()  # send current level when a new player joins
            return True

        print(f"Failed to create remote player entity for {username}")
        return False

    def end_game(self):
        """End the game and return to the submenu."""
        self.should_return_to_menu = True
        self.is_game_running = False

        if self.player:
            if self.player.ws:
                self.player.user_request(REQUEST_TYPE_STOP_WEBSOCKET)
                sleep_ms(100)
            self.player.user_request(REQUEST_TYPE_SAVE_STATS)
            if self.player.http is not None:
                while self.player.http.in_progress:
                    if not self.player.loading:
                        from picoware.gui.loading import Loading

                        self.player.loading = Loading(
                            self.draw, COLOR_BLACK, COLOR_WHITE
                        )
                        self.player.loading.text = "Saving..."
                        self.player.loading.animate()
                    else:
                        self.player.loading.animate()
                        self.view_manager.thread_manager.run()  # Process HTTP thread
        if self.engine:
            self.engine.stop()
            del self.engine
            self.engine = None

        self.is_pve_mode = False
        self.is_lobby_host = False

    def entity_json_update(self, entity, response: str) -> bool:
        """Update an entity's state based on JSON response from server."""
        if not response or len(response) == 0:
            return False

        # Parse the response
        # expected response:
        # {
        #     "u": "JBlanked",
        #     "xp": 37743,
        #     "h": 207,
        #     "ehr": 0.7,
        #     "eat": 127.5,
        #     "d": 2,
        #     "s": 1,
        #     "sp": {
        #         "x": 381.0,
        #         "y": 192.0
        #     }
        # }

        try:
            response = response.strip()
            data = loads(response)
        except Exception:
            print("entity_json_update: Failed to parse JSON")
            return False

        u = data.get("u", None)
        if u is None:
            print("entity_json_update: Failed to get username")
            return False

        # Check if the username matches
        if u != entity.name:
            return False

        # Get health, elapsed attack timer, direction, xp, and position
        h = data.get("h", None)
        eat = data.get("eat", None)
        d = data.get("d", None)
        xp = data.get("xp", None)
        sp = data.get("sp", None)

        if h is None or eat is None or d is None or sp is None or xp is None:
            return False

        x = sp.get("x", None)
        y = sp.get("y", None)

        if x is None or y is None:
            return False

        # Set entity info
        entity.health = float(h)
        if entity.health <= 0:
            entity.health = 0
            entity.state = ENTITY_STATE_DEAD
            entity.position.x = float(-100)
            entity.position.y = float(-100)
            return True

        entity.elapsed_attack_timer = float(eat)

        # Set direction
        d_val = int(d)
        if d_val == 0:
            # ENTITY_DIRECTION_LEFT
            entity.direction.x = -1
            entity.direction.y = 0
        elif d_val == 1:
            # ENTITY_DIRECTION_RIGHT
            entity.direction.x = 1
            entity.direction.y = 0
        elif d_val == 2:
            # ENTITY_DIRECTION_UP
            entity.direction.x = 0
            entity.direction.y = -1
        elif d_val == 3:
            # ENTITY_DIRECTION_DOWN
            entity.direction.x = 0
            entity.direction.y = 1
        else:
            # default to right
            entity.direction.x = 1
            entity.direction.y = 0

        # Set XP and calculate level
        entity.xp = int(xp)
        entity.level = 1
        xp_required = 100  # Base XP for level 2

        while entity.level < 100 and entity.xp >= xp_required:
            entity.level += 1
            xp_required = int(xp_required * 1.5)

        # Set position
        entity.position.x = float(x)
        entity.position.y = float(y)

        return True

    def get_current_level_index(self) -> int:
        """Get the index of the current level."""
        if not self.engine:
            print("Engine is not initialized")
            return LEVEL_UNKNOWN

        current_level = self.engine.game.current_level
        if not current_level:
            print("Current level is not set")
            return LEVEL_UNKNOWN

        level_map = {
            "Home Woods": LEVEL_HOME_WOODS,
            "Rock World": LEVEL_ROCK_WORLD,
            "Forest World": LEVEL_FOREST_WORLD,
        }
        return level_map.get(current_level.name, LEVEL_UNKNOWN)

    def get_icon_spec(self, name: str) -> IconSpec:
        """Returns the IconSpec for a given icon name."""
        spec_map = {
            "house": IconSpec(ICON_ID_HOUSE, 48, 32),
            "plant": IconSpec(ICON_ID_PLANT, 16, 16),
            "tree": IconSpec(ICON_ID_TREE, 16, 16),
            "fence": IconSpec(ICON_ID_FENCE, 16, 8),
            "flower": IconSpec(ICON_ID_FLOWER, 16, 16),
            "rock_large": IconSpec(
                ICON_ID_ROCK_LARGE,
                18,
                19,
            ),
            "rock_medium": IconSpec(
                ICON_ID_ROCK_MEDIUM,
                16,
                14,
            ),
            "rock_small": IconSpec(ICON_ID_ROCK_SMALL, 10, 8),
        }
        if name in spec_map:
            return spec_map[name]
        return IconSpec(ICON_ID_INVALID, 0, 0)

    def get_level(self, index: int, game) -> Level:
        """Get a Level object by index."""

        print(f"Creating level {index} - {self.level_names[index]}")

        level = Level(
            self.get_level_name(index),
            Vector(768, 384),
            game if game else self.engine.game,
        )
        if not level:
            print("Failed to create Level object")
            return None

        print("Level object now created, setting icon group...")

        if index == LEVEL_HOME_WOODS:
            spr1 = Sprite(
                "Cyclops",
                ENTITY_TYPE_ENEMY,
                Vector(350.0, 210.0),
                Vector(390.0, 210.0),
                2.0,
                30.0,
                0.4,
                10.0,
                100.0,
            )
            print("Sprite 1 created, now creating remaining sprites...")
            spr2 = Sprite(
                "Ogre",
                ENTITY_TYPE_ENEMY,
                Vector(200.0, 320.0),
                Vector(220.0, 320.0),
                0.5,
                45.0,
                0.6,
                20.0,
                200.0,
            )
            print("Sprite 2 created, now creating remaining sprites...")
            spr3 = Sprite(
                "Ghost",
                ENTITY_TYPE_ENEMY,
                Vector(100.0, 80.0),
                Vector(180.0, 85.0),
                2.2,
                55.0,
                0.5,
                30.0,
                300.0,
            )
            print("Sprite 3 created, now creating remaining sprites...")
            spr4 = Sprite(
                "Ogre",
                ENTITY_TYPE_ENEMY,
                Vector(400.0, 50.0),
                Vector(490.0, 50.0),
                1.7,
                35.0,
                1.0,
                20.0,
                200.0,
            )
            print("Sprite 4 created, now creating remaining sprites...")
            spr5 = Sprite(
                "Funny NPC",
                ENTITY_TYPE_NPC,
                Vector(350.0, 180.0),
                Vector(350.0, 180.0),
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
            )
            print("Sprites created, adding to level...")
            #
            spr1.flip_world_run = self
            spr2.flip_world_run = self
            spr3.flip_world_run = self
            spr4.flip_world_run = self
            spr5.flip_world_run = self
            #
            print("Sprites linked to FlipWorldRun instance, now adding to level...")
            level.entity_add(spr1)
            level.entity_add(spr2)
            level.entity_add(spr3)
            level.entity_add(spr4)
            level.entity_add(spr5)
            print("Entities added to level")

        elif index == LEVEL_ROCK_WORLD:
            spr1 = Sprite(
                "Ghost",
                ENTITY_TYPE_ENEMY,
                Vector(180.0, 80.0),
                Vector(160.0, 80.0),
                1.0,
                32.0,
                0.5,
                10.0,
                100.0,
            )
            spr2 = Sprite(
                "Ogre",
                ENTITY_TYPE_ENEMY,
                Vector(220.0, 140.0),
                Vector(200.0, 140.0),
                1.5,
                20.0,
                1.0,
                10.0,
                100.0,
            )
            spr3 = Sprite(
                "Cyclops",
                ENTITY_TYPE_ENEMY,
                Vector(400.0, 200.0),
                Vector(450.0, 200.0),
                2.0,
                15.0,
                1.2,
                20.0,
                200.0,
            )
            spr4 = Sprite(
                "Ogre",
                ENTITY_TYPE_ENEMY,
                Vector(600.0, 150.0),
                Vector(580.0, 150.0),
                1.8,
                28.0,
                1.0,
                40.0,
                400.0,
            )
            spr5 = Sprite(
                "Ghost",
                ENTITY_TYPE_ENEMY,
                Vector(500.0, 250.0),
                Vector(480.0, 250.0),
                1.2,
                30.0,
                0.6,
                10.0,
                100.0,
            )
            #
            spr1.flip_world_run = self
            spr2.flip_world_run = self
            spr3.flip_world_run = self
            spr4.flip_world_run = self
            spr5.flip_world_run = self
            #
            level.entity_add(spr1)
            level.entity_add(spr2)
            level.entity_add(spr3)
            level.entity_add(spr4)
            level.entity_add(spr5)

        elif index == LEVEL_FOREST_WORLD:
            spr1 = Sprite(
                "Ghost",
                ENTITY_TYPE_ENEMY,
                Vector(50.0, 120.0),
                Vector(100.0, 120.0),
                1.0,
                30.0,
                0.5,
                10.0,
                100.0,
            )
            spr2 = Sprite(
                "Cyclops",
                ENTITY_TYPE_ENEMY,
                Vector(300.0, 60.0),
                Vector(250.0, 60.0),
                1.5,
                20.0,
                0.8,
                30.0,
                300.0,
            )
            spr3 = Sprite(
                "Ogre",
                ENTITY_TYPE_ENEMY,
                Vector(400.0, 200.0),
                Vector(450.0, 200.0),
                1.7,
                15.0,
                1.0,
                10.0,
                100.0,
            )
            spr4 = Sprite(
                "Ghost",
                ENTITY_TYPE_ENEMY,
                Vector(700.0, 150.0),
                Vector(650.0, 150.0),
                1.2,
                25.0,
                0.6,
                10.0,
                100.0,
            )
            spr5 = Sprite(
                "Cyclops",
                ENTITY_TYPE_ENEMY,
                Vector(200.0, 300.0),
                Vector(250.0, 300.0),
                2.0,
                18.0,
                0.9,
                20.0,
                200.0,
            )
            spr6 = Sprite(
                "Ogre",
                ENTITY_TYPE_ENEMY,
                Vector(300.0, 300.0),
                Vector(350.0, 300.0),
                1.5,
                15.0,
                1.2,
                50.0,
                500.0,
            )
            spr7 = Sprite(
                "Ghost",
                ENTITY_TYPE_ENEMY,
                Vector(500.0, 200.0),
                Vector(550.0, 200.0),
                1.3,
                20.0,
                0.7,
                40.0,
                400.0,
            )
            #
            spr1.flip_world_run = self
            spr2.flip_world_run = self
            spr3.flip_world_run = self
            spr4.flip_world_run = self
            spr5.flip_world_run = self
            spr6.flip_world_run = self
            spr7.flip_world_run = self
            #
            level.entity_add(spr1)
            level.entity_add(spr2)
            level.entity_add(spr3)
            level.entity_add(spr4)
            level.entity_add(spr5)
            level.entity_add(spr6)
            level.entity_add(spr7)
        print("Entities added to level")
        level.clear_allowed = False  # swap is done by Player.draw_current_view
        return level

    def get_level_json(self, index: int) -> list[dict]:
        """Return the JSON data for a level by index."""
        if index == LEVEL_HOME_WOODS:
            return [
                {"i": "rock_medium", "x": 100, "y": 100, "a": 10, "h": True},
                {"i": "rock_medium", "x": 400, "y": 300, "a": 6, "h": True},
                {"i": "rock_small", "x": 600, "y": 200, "a": 8, "h": True},
                {"i": "fence", "x": 50, "y": 50, "a": 10, "h": True},
                {"i": "fence", "x": 250, "y": 150, "a": 12, "h": True},
                {"i": "fence", "x": 550, "y": 350, "a": 12, "h": True},
                {"i": "rock_large", "x": 400, "y": 70, "a": 12, "h": True},
                {"i": "rock_large", "x": 200, "y": 200, "a": 6, "h": False},
                {"i": "tree", "x": 5, "y": 5, "a": 45, "h": True},
                {"i": "tree", "x": 5, "y": 5, "a": 20, "h": False},
                {"i": "tree", "x": 22, "y": 22, "a": 44, "h": True},
                {"i": "tree", "x": 22, "y": 22, "a": 20, "h": False},
                {"i": "tree", "x": 5, "y": 347, "a": 45, "h": True},
                {"i": "tree", "x": 5, "y": 364, "a": 45, "h": True},
                {"i": "tree", "x": 735, "y": 37, "a": 18, "h": False},
                {"i": "tree", "x": 752, "y": 37, "a": 18, "h": False},
            ]
        if index == LEVEL_ROCK_WORLD:
            return [
                {"i": "house", "x": 100, "y": 50, "a": 1, "h": True},
                {"i": "tree", "x": 650, "y": 420, "a": 5, "h": True},
                {"i": "rock_large", "x": 150, "y": 150, "a": 2, "h": True},
                {"i": "rock_medium", "x": 210, "y": 80, "a": 3, "h": True},
                {"i": "rock_small", "x": 480, "y": 110, "a": 4, "h": False},
                {"i": "flower", "x": 280, "y": 140, "a": 3, "h": True},
                {"i": "plant", "x": 120, "y": 130, "a": 2, "h": True},
                {"i": "rock_large", "x": 400, "y": 200, "a": 3, "h": True},
                {"i": "rock_medium", "x": 600, "y": 50, "a": 5, "h": False},
                {"i": "rock_small", "x": 500, "y": 100, "a": 6, "h": True},
                {"i": "tree", "x": 650, "y": 20, "a": 4, "h": True},
                {"i": "flower", "x": 550, "y": 250, "a": 8, "h": True},
                {"i": "plant", "x": 300, "y": 300, "a": 5, "h": True},
                {"i": "rock_large", "x": 700, "y": 180, "a": 2, "h": True},
                {"i": "tree", "x": 50, "y": 300, "a": 10, "h": True},
                {"i": "flower", "x": 350, "y": 100, "a": 7, "h": True},
            ]
        if index == LEVEL_FOREST_WORLD:
            return [
                {"i": "rock_small", "x": 120, "y": 20, "a": 10, "h": False},
                {"i": "tree", "x": 50, "y": 50, "a": 10, "h": True},
                {"i": "flower", "x": 200, "y": 70, "a": 8, "h": False},
                {"i": "rock_small", "x": 250, "y": 100, "a": 8, "h": True},
                {"i": "rock_medium", "x": 300, "y": 140, "a": 2, "h": True},
                {"i": "plant", "x": 50, "y": 300, "a": 10, "h": True},
                {"i": "rock_large", "x": 650, "y": 250, "a": 3, "h": True},
                {"i": "flower", "x": 300, "y": 350, "a": 5, "h": True},
                {"i": "tree", "x": 20, "y": 150, "a": 10, "h": False},
                {"i": "tree", "x": 5, "y": 5, "a": 45, "h": True},
                {"i": "tree", "x": 5, "y": 5, "a": 20, "h": False},
                {"i": "tree", "x": 22, "y": 22, "a": 44, "h": True},
                {"i": "tree", "x": 22, "y": 22, "a": 20, "h": False},
                {"i": "tree", "x": 5, "y": 347, "a": 45, "h": True},
                {"i": "tree", "x": 5, "y": 364, "a": 45, "h": True},
                {"i": "tree", "x": 735, "y": 37, "a": 18, "h": False},
                {"i": "tree", "x": 752, "y": 37, "a": 18, "h": False},
            ]
        print(f"Unknown level index: {index}")
        return []

    def get_level_name(self, index: int) -> str:
        """Get the name of a level by index."""
        if 0 <= index < len(self.level_names):
            return self.level_names[index]
        return "Unknown"

    def input_manager(self):
        """Manage input for the game, called from update_input."""
        # Pass input to player for processing
        if self.player:
            self.player.last_input = self.last_input
            self.player.process_input()

    def remove_remote_player(self, username: str) -> bool:
        """Remove a remote player from the current level (PvE mode only)."""
        if not self.is_pve_mode or not username:
            return False

        if (
            not self.engine
            or not self.engine.game
            or not self.engine.game.current_level
        ):
            return False

        current_level = self.engine.game.current_level

        for i in range(current_level.entity_count):
            entity = current_level.get_entity(i)
            if entity and entity.type == ENTITY_TYPE_PLAYER:
                if entity.name == username and entity != self.player:
                    current_level.entity_remove(entity)
                    return True

        return False

    def reset_input(self):
        """Reset input after processing."""
        self.last_input = INPUT_KEY_MAX

    def set_is_lobby_host(self, is_host: bool):
        """Set if this player is the lobby host."""
        self.is_lobby_host = is_host

    def set_pve_mode(self, enabled: bool):
        """Enable/disable PvE mode."""
        self.is_pve_mode = enabled

    def should_process_enemy_ai(self) -> bool:
        """Check if enemy AI should be processed (host only in PvE mode)."""
        if self.is_pve_mode:
            return self.is_lobby_host
        return True

    def should_update_entity(self, entity) -> bool:
        """Check if a specific entity should be updated."""
        if not entity:
            return False
        if self.is_pve_mode:
            if entity.type == ENTITY_TYPE_PLAYER:
                return entity == self.player
            return self.is_lobby_host
        return True

    def start_game(self) -> bool:
        """Start the actual game."""
        from picoware.engine.engine import GameEngine
        from picoware.engine.game import Game

        self.draw.fill_screen(COLOR_WHITE)
        self.draw.text(Vector(0, 10), "Initializing game...", COLOR_BLACK)
        self.draw.swap()

        if self.is_game_running or self.engine:
            return True

        # Create the player instance if it doesn't exist
        if not self.player:
            self.player = Player()
            if not self.player:
                return False

        self.player.flip_world_run = self
        self.player.user_stats_pos.x = 5
        self.player.user_stats_pos.y = self.player.screen_size.y - 30
        self.player.user_stats_size = Vector(
            int(self.player.screen_size.x) // 5, int(self.player.screen_size.y) // 10
        )

        # Create the game instance
        game = Game(
            "FlipWorld",
            Vector(768, 384),
            self.draw,
            self.view_manager.input_manager,
            COLOR_WHITE,
            COLOR_BLACK,
        )
        if not game:
            return False

        self.draw.fill_screen(COLOR_WHITE)
        self.draw.text(Vector(0, 10), "Adding levels and player...", COLOR_BLACK)
        self.draw.swap()

        print("Getting levels and adding player...")

        # Create levels
        level1 = self.get_level(LEVEL_HOME_WOODS, game)
        level2 = self.get_level(LEVEL_ROCK_WORLD, game)
        level3 = self.get_level(LEVEL_FOREST_WORLD, game)
        print("Levels created")

        # Set icon group for first level
        if not self.set_icon_group(LEVEL_HOME_WOODS):
            print("Failed to set icon group for level 0")
            return False

        print("Icon group set for level 0")

        # Add player to all levels
        level1.entity_add(self.player)
        level2.entity_add(self.player)
        level3.entity_add(self.player)

        print("Player added to levels")

        # Add levels to game
        game.level_add(level1)
        game.level_add(level2)
        game.level_add(level3)

        print("Levels added to game")

        # Start with first level
        game.level_switch(0)

        if game.current_level is None:
            print("Failed to switch to first level")
            return False

        print(
            f"Switched to first level {game.current_level.name} with {game.current_level.entity_count} entities"
        )

        # Set game position to center of player
        game.position = Vector(384, 192)

        self.engine = GameEngine(game, 30)
        if not self.engine:
            print("Failed to create GameEngine")
            return False

        self.draw.fill_screen(COLOR_WHITE)
        if self.is_pve_mode:
            self.draw.text(Vector(0, 10), "Starting multiplayer...", COLOR_BLACK)
        else:
            self.draw.text(Vector(0, 10), "Starting single player...", COLOR_BLACK)
        self.draw.swap()

        self.is_game_running = True
        print("Gaming engine started")
        return True

    def switch_to_level(self, level_index: int):
        """Switch to a specific level by index."""
        if not self.is_game_running or not self.engine or not self.engine.game:
            print(
                "Cannot switch levels - game is not running or engine/game is not initialized"
            )
            return

        if level_index < 0 or level_index >= self.total_levels:
            return

        if not self.engine.game.level_exists(self.get_level_name(level_index)):
            print(f"Level {level_index} does not exist in game, creating...")
            # create level if it doesn't exist yet
            level = self.get_level(level_index, self.engine.game)
            if level:
                level.entity_add(self.player)
                self.engine.game.level_add(level)
            else:
                print(f"Failed to create level {level_index}")
                return

        self.current_level_index = level_index
        self.engine.game.level_switch(self.current_level_index)
        self.set_icon_group(self.current_level_index)

        print(f"Switched to level {level_index} - {self.get_level_name(level_index)}")

    def switch_to_next_level(self):
        """Switch to the next level in the game."""
        if not self.is_game_running or not self.engine or not self.engine.game:
            return

        self.current_level_index = (self.current_level_index + 1) % self.total_levels
        self.engine.game.level_switch(self.current_level_index)
        self.set_icon_group(self.current_level_index)

    def entity_to_json(self, entity, websocket_parsing: bool = False) -> str:
        """Convert an entity to JSON string format."""
        if not entity:
            return None

        # Helper to convert direction to code
        def direction_to_code(direction: int) -> int:
            if direction.x == -1 and direction.y == 0:  # ENTITY_DIRECTION_LEFT
                return 0
            if direction.x == 1 and direction.y == 0:  # ENTITY_DIRECTION_RIGHT
                return 1
            if direction.x == 0 and direction.y == -1:  # ENTITY_DIRECTION_UP
                return 2
            if direction.x == 0 and direction.y == 1:  # ENTITY_DIRECTION_DOWN
                return 3
            return 1  # Default right

        def direction_to_str(direction: int) -> str:
            if direction.x == -1 and direction.y == 0:  # ENTITY_DIRECTION_LEFT
                return "left"
            if direction.x == 1 and direction.y == 0:  # ENTITY_DIRECTION_RIGHT
                return "right"
            if direction.x == 0 and direction.y == -1:  # ENTITY_DIRECTION_UP
                return "up"
            if direction.x == 0 and direction.y == 1:  # ENTITY_DIRECTION_DOWN
                return "down"
            return "right"  # Default right

        # Helper to convert state to code
        def state_to_code(state: int) -> int:
            code_map = {
                ENTITY_STATE_IDLE: 0,
                ENTITY_STATE_MOVING: 1,
                ENTITY_STATE_MOVING_TO_START: 2,
                ENTITY_STATE_MOVING_TO_END: 3,
                ENTITY_STATE_ATTACKING: 4,
                ENTITY_STATE_ATTACKED: 5,
                ENTITY_STATE_DEAD: 6,
            }
            return code_map.get(state, 0)  # Default idle

        def state_to_str(state: int) -> str:
            str_map = {
                ENTITY_STATE_IDLE: "idle",
                ENTITY_STATE_MOVING: "moving",
                ENTITY_STATE_MOVING_TO_START: "moving_to_start",
                ENTITY_STATE_MOVING_TO_END: "moving_to_end",
                ENTITY_STATE_ATTACKING: "attacking",
                ENTITY_STATE_ATTACKED: "attacked",
                ENTITY_STATE_DEAD: "dead",
            }
            return str_map.get(state, "unknown")  # Default idle

        if websocket_parsing:
            # Minimal format for websocket messages (reduced bandwidth)
            return (
                f'{{"u":"{entity.name if entity.name else ""}",'
                f'"xp":{int(entity.xp)},'
                f'"h":{int(entity.health)},'
                f'"ehr":{entity.elapsed_health_regen:.1f},'
                f'"eat":{entity.elapsed_attack_timer:.1f},'
                f'"d":{direction_to_code(entity.direction)},'
                f'"s":{state_to_code(entity.state)},'
                f'"sp":{{"x":{entity.position.x:.1f},"y":{entity.position.y:.1f}}}}}'
            )

        # Full format with all entity data
        stats = {
            "username": entity.name,
            "level": int(entity.level),
            "xp": int(entity.xp),
            "health": int(entity.health),
            "strength": entity.strength,
            "max_health": int(entity.max_health),
            "health_regen": entity.health_regen,
            "elapsed_health_regen": entity.elapsed_health_regen,
            "attack_timer": entity.attack_timer,
            "elapsed_attack_timer": entity.elapsed_attack_timer,
            "direction": direction_to_str(entity.direction),
            "state": state_to_str(entity.state),
            "start_position_x": entity.start_position.x,
            "start_position_y": entity.start_position.y,
            "dx": entity.direction.x,
            "dy": entity.direction.y,
        }
        # return username: user, game_stats: stats
        return {"username": entity.name, "game_stats": stats}

    def get_memory_usage(self) -> int:
        """Get total memory used by message queue."""
        total = 0

        # Sum up queued message memory
        for i in range(self.queue_size):
            idx = (self.queue_head + i) % self.MAX_QUEUED_MESSAGES
            if self.message_queue[idx].message:
                total += len(self.message_queue[idx].message)

        return total

    def handle_incoming_multiplayer_data(self, message: str):
        """
        Route incoming websocket data to appropriate handler.

        New Expected responees (as of December 22, 2025):
            - {"type": "player", "data": {"u": "FlipWorldTester", "xp": 0, "h": 100, "ehr": 22.9, "eat": 22.9, "d": 2, "s": 0, "sp": {"x": 414.0, "y": 172.0}}}
            - {"type": "enemy", "data": {"u": "Ogre", "xp": 0, "h": 200, "ehr": 0.0, "eat": 44.3, "d": 1, "s": 3, "sp": {"x": 213.5, "y": 320.0}}}
            - {"type": "enemy", "data": {"u": "Cyclops", "xp": 0, "h": 100, "ehr": 0.0, "eat": 42.2, "d": 0, "s": 0, "sp": {"x": 350.5, "y": 210.0}}}
            - {"type": "level", "level_index": 0}

        """
        if not message:
            return

        self.process_complete_multiplayer_message(message)

    def parse_entity_data_from_json(self, entity, json_data: str) -> bool:
        """Parse entity data from JSON and update the entity."""
        if not entity or not json_data:
            return False

        try:
            data = loads(json_data)
        except Exception:
            print("parseEntityDataFromJSON: Failed to parse JSON:", json_data)
            return False

        # Get direction
        d = data.get("d", None)
        if d is not None:
            d_val = int(d)
            if d_val == 0:
                # ENTITY_DIRECTION_LEFT
                entity.direction.x = -1
                entity.direction.y = 0
            elif d_val == 1:
                # ENTITY_DIRECTION_RIGHT
                entity.direction.x = 1
                entity.direction.y = 0
            elif d_val == 2:
                # ENTITY_DIRECTION_UP
                entity.direction.x = 0
                entity.direction.y = -1
            elif d_val == 3:
                # ENTITY_DIRECTION_DOWN
                entity.direction.x = 0
                entity.direction.y = 1

        # Get state
        s = data.get("s", None)
        if s is not None:
            s_val = int(s)
            state_map = {
                0: ENTITY_STATE_IDLE,
                1: ENTITY_STATE_MOVING,
                2: ENTITY_STATE_MOVING_TO_START,
                3: ENTITY_STATE_MOVING_TO_END,
                4: ENTITY_STATE_ATTACKING,
                5: ENTITY_STATE_ATTACKED,
                6: ENTITY_STATE_DEAD,
            }
            entity.state = state_map.get(s_val, entity.state)

        # Get health
        h = data.get("h", None)
        if h is not None:
            entity.health = float(h)

        # Get XP
        xp = data.get("xp", None)
        if xp is not None:
            entity.xp = int(xp)

            entity.level = 1
            xp_required = 100
            while entity.level < 100 and entity.xp >= xp_required:
                entity.level += 1
                xp_required = int(xp_required * 1.5)

        # Get elapsed attack timer
        eat = data.get("eat", None)
        if eat is not None:
            entity.elapsed_attack_timer = float(eat)

        # Get position
        sp = data.get("sp", None)
        if sp is not None:
            x = sp.get("x", None)
            y = sp.get("y", None)
            if x is not None and y is not None:
                entity.position.x = float(x)
                entity.position.y = float(y)

        return True

    def process_complete_multiplayer_message(self, message: str):
        """Process a complete multiplayer message."""
        if not message:
            return

        try:
            data = loads(message)
        except Exception:
            print("processCompleteMultiplayerMessage: Failed to parse JSON")
            return

        msg_type = data.get("type", None)
        if not msg_type:
            return

        if msg_type == "player":
            # Handle remote player update
            player_data = data.get("data", None)
            if not player_data:
                return

            username = player_data.get("u", None)
            if not username:
                return

            # Don't process our own player
            if self.player and username == self.player.name:
                return

            if (
                not self.engine
                or not self.engine.game
                or not self.engine.game.current_level
            ):
                return

            current_level = self.engine.game.current_level

            # Find existing player or add new one
            found = False
            for i in range(current_level.entity_count):
                entity = current_level.get_entity(i)
                if (
                    entity
                    and entity.type == ENTITY_TYPE_PLAYER
                    and entity.name == username
                ):
                    # Update existing player
                    self.parse_entity_data_from_json(entity, dumps(player_data))
                    found = True
                    break

            if not found:
                # Add new remote player
                self.add_remote_player(username)

        elif msg_type == "enemy":
            # Handle enemy update (host syncs enemies to followers)
            if self.is_lobby_host:
                return  # Host doesn't process enemy updates from others

            enemy_data = data.get("data", None)
            if not enemy_data:
                return

            # Find the enemy and update it
            if (
                not self.engine
                or not self.engine.game
                or not self.engine.game.current_level
            ):
                return

            current_level = self.engine.game.current_level
            enemy_name = enemy_data.get("u", None)

            if enemy_name:
                for i in range(current_level.entity_count):
                    entity = current_level.get_entity(i)
                    if (
                        entity
                        and entity.type == ENTITY_TYPE_ENEMY
                        and entity.name == enemy_name
                    ):
                        self.parse_entity_data_from_json(entity, dumps(enemy_data))
                        break

        elif msg_type == "level":
            # Handle level switch (followers sync to host's level)
            if self.is_lobby_host:
                return  # Host doesn't need to sync levels

            level_index = data.get("level_index", None)
            if level_index is not None:
                level_index = int(level_index)
                if level_index != self.current_level_index:
                    self.switch_to_level(level_index)

    def process_multiplayer_update(self):
        """Process multiplayer updates - called from update_draw."""

        # Always process the websocket message queue first
        self.process_websocket_queue()

        available_heap = mem_free()

        # Force garbage collection if available heap is getting dangerously low
        if 0 < available_heap < 8192:
            collect()
            if DEBUG:
                available_heap = mem_free()
                print(f"Emergency cleanup complete, free heap now: {available_heap}")

        # Check for incoming messages
        incoming_message = self.player.ws_data
        if (
            incoming_message
            and isinstance(incoming_message, str)
            and len(incoming_message) > 0
        ):
            if DEBUG:
                print(
                    f"Processing incoming message: {incoming_message}, free heap: {available_heap}, queue size: {self.queue_size}"
                )
            self.handle_incoming_multiplayer_data(incoming_message)
            self.player.ws_data = None  # Clear after processing

    def queue_websocket_message(self, message: str) -> bool:
        """Add a message to the websocket send queue."""
        if not message:
            return False

        # If queue is very full (85%), drop oldest messages
        if self.queue_size >= int(self.MAX_QUEUED_MESSAGES * 0.85):
            while self.queue_size > int(self.MAX_QUEUED_MESSAGES * 0.7):
                if DEBUG:
                    print(
                        f"Queue very full ({self.queue_size}/{self.MAX_QUEUED_MESSAGES}), dropping oldest"
                    )
                old_msg = self.message_queue[self.queue_head]
                old_msg.message = ""
                old_msg.message_len = 0
                self.queue_head = (self.queue_head + 1) % self.MAX_QUEUED_MESSAGES
                self.queue_size -= 1

        if self.queue_size >= self.MAX_QUEUED_MESSAGES:
            if DEBUG:
                print("Message queue full, dropping message")
            return False

        # Check memory limit (12KB)
        current_memory = self.get_memory_usage()
        message_len = len(message)
        if current_memory + message_len + 1 > 12288:
            if DEBUG:
                print(
                    f"Would exceed memory limit ({current_memory} + {message_len} > 12288)"
                )
            return False

        # Add to queue
        self.message_queue[self.queue_tail].message = message
        self.message_queue[self.queue_tail].message_len = message_len
        self.queue_tail = (self.queue_tail + 1) % self.MAX_QUEUED_MESSAGES
        self.queue_size += 1

        return True

    def safe_websocket_send(self, message: str) -> bool:
        """
        Safely send a websocket message by queueing it.

        Expected messages:
            - {"type":"player","data":{"u":"JBlanked","xp":328417,"h":300,"ehr":70.9,"eat":92.4,"d":3,"s":5,"sp":{"x":399.0,"y":177.0}}}
            - {"type":"player","data":{"u":"FlipWorldTester","h":92,"d":1,"s":0,"sp":{"x":384.0,"y":192.0}}}
            - {"type":"level","level_index":0}
            - {"type":"enemy","data":{"u":"Cyclops","h":100,"d":1,"s":0,"sp":{"x":360.0,"y":210.0}}}
            - {"type":"enemy","data":{"u":"Ogre","h":200,"d":1,"s":0,"sp":{"x":215.0,"y":320.0}}}
            - etc.
        """
        if not message:
            return False
        return self.queue_websocket_message(message)

    def process_websocket_queue(self):
        """Process the websocket message queue, sending pending messages."""
        if self.queue_size == 0:
            return

        # Send next message
        msg = self.message_queue[self.queue_head]
        if msg.message:
            self.player.ws.send(msg.message)

            if DEBUG:
                print(f"Sent websocket message: {msg.message}")

            # Clean up
            msg.message = ""
            msg.message_len = 0
            self.queue_head = (self.queue_head + 1) % self.MAX_QUEUED_MESSAGES
            self.queue_size -= 1

    def sync_multiplayer_entity(self, entity):
        """Sync a specific entity's state with other players."""
        if not self.is_pve_mode or not entity:
            return

        entity_json = self.entity_to_json(entity, True)
        if not entity_json:
            return

        if entity.type == ENTITY_TYPE_PLAYER:
            message = f'{{"type":"player","data":{entity_json}}}'
        elif entity.type == ENTITY_TYPE_ENEMY:
            message = f'{{"type":"enemy","data":{entity_json}}}'
        else:
            return

        self.safe_websocket_send(message)

    def sync_multiplayer_level(self):
        """Sync current level info with other players."""
        if not self.is_pve_mode or not self.is_lobby_host:
            return

        level_message = (
            f'{{"type":"level","level_index":{self.get_current_level_index()}}}'
        )
        self.safe_websocket_send(level_message)

    def set_icon_group(self, index: int) -> bool:
        """Set the icon group for a level based on its JSON data."""
        # Parse the icon data from the level JSON
        icons_data = self.get_level_json(index)
        if not icons_data:
            return False

        # Build icon specs (stored for rendering)
        self.current_icon_group.clear()

        for item in icons_data:
            icon_name = item.get("i", "")
            base_x = float(item.get("x", 0))
            base_y = float(item.get("y", 0))
            amount = item.get("a", 1)
            is_horizontal = item.get("h", True)
            spacing = 17

            for j in range(amount):
                spec = self.get_icon_spec(icon_name)
                if spec.id == ICON_ID_INVALID:
                    print(f"  - Unknown icon '{icon_name}', skipping")
                    continue

                if is_horizontal:
                    spec.x = base_x + (j * spacing)
                    spec.y = base_y
                else:
                    spec.x = base_x
                    spec.y = base_y + (j * spacing)

                self.current_icon_group.append(spec)

        return True

    @staticmethod
    def pve_render(entity, canvas, game):
        """Render PvE-specific elements like player usernames above their heads."""
        if not entity or not entity.name or len(entity.name) == 0:
            return

        # Calculate screen position after camera offset
        screen_x = entity.position.x - game.position.x
        screen_y = entity.position.y - game.position.y

        # Check if visible
        text_width = len(entity.name) * 4 + 1
        if (
            screen_x - text_width / 2 < 0
            or screen_x + text_width / 2 > game.draw.size.x
            or screen_y - 10 < 0
            or screen_y > game.draw.size.y
        ):
            return

        # Draw box around name
        name_len = len(entity.name)
        canvas.fill_rectangle(
            Vector(screen_x - (name_len * 2) - 1, screen_y - 7),
            Vector(name_len * 4 + 1, 8),
            COLOR_WHITE,
        )

        # Draw name over player's head
        canvas.text(
            Vector(screen_x - (name_len * 2), screen_y - 2),
            entity.name,
            COLOR_BLACK,
        )

    def update_draw(self):
        """Update and draw the game."""
        # Initialize player if not already done
        if not self.player:
            self.player = Player()
            if self.player:
                self.player.flip_world_run = self
                self.player.screen_size = self.screen_size
                self.player.sound_toggle = self.sound_toggle
                self.player.vibration_toggle = self.vibration_toggle

        # Process multiplayer updates if in PvE mode
        if self.is_pve_mode and self.is_game_running:
            self.process_multiplayer_update()

        # Let the player handle all drawing
        if self.player:
            if self.player.should_leave_game:
                self.sound_toggle = self.player.sound_toggle
                self.vibration_toggle = self.player.vibration_toggle
                self.end_game()
            else:
                self.player.draw_current_view(self.draw)

    def update_input(self, key: int):
        """Update input for the game."""
        self.last_input = key

        # Only run input_manager when not in an active game to avoid input conflicts
        if not (self.player and self.is_game_running):
            self.input_manager()
