"""FlipWorld Player class for MicroPython"""

from micropython import const
from picoware.system.vector import Vector
from picoware.engine.entity import (
    Entity,
    ENTITY_TYPE_PLAYER,
    ENTITY_TYPE_ENEMY,
    ENTITY_STATE_DEAD,
)
from picoware.gui.loading import Loading
from ujson import loads as json_loads
from flip_world.assets import player_left_sword_15x11px, player_right_sword_15x11px

# GameMainView
GAME_VIEW_TITLE = const(0)  # title, start, and menu (menu)
GAME_VIEW_SYSTEM_MENU = const(1)  # profile, settings, about (menu)
GAME_VIEW_GAME = const(2)  # game view (gameplay)
GAME_VIEW_LOGIN = const(3)  # login view
GAME_VIEW_REGISTRATION = const(4)  # registration view
GAME_VIEW_USER_INFO = const(5)  # user info view
GAME_VIEW_LOBBIES = const(6)  # lobbies view
GAME_VIEW_JOIN_LOBBY = const(7)  # join lobby view

# LoginStatus
LOGIN_CREDENTIALS_MISSING = const(-1)
LOGIN_SUCCESS = const(0)
LOGIN_USER_NOT_FOUND = const(1)
LOGIN_WRONG_PASSWORD = const(2)
LOGIN_WAITING = const(3)
LOGIN_NOT_STARTED = const(4)
LOGIN_REQUEST_ERROR = const(5)

# RegistrationStatus
REGISTRATION_CREDENTIALS_MISSING = const(-1)
REGISTRATION_SUCCESS = const(0)
REGISTRATION_USER_EXISTS = const(1)
REGISTRATION_REQUEST_ERROR = const(2)
REGISTRATION_NOT_STARTED = const(3)
REGISTRATION_WAITING = const(4)

# UserInfoStatus
USER_INFO_CREDENTIALS_MISSING = const(-1)
USER_INFO_SUCCESS = const(0)
USER_INFO_REQUEST_ERROR = const(1)
USER_INFO_NOT_STARTED = const(2)
USER_INFO_WAITING = const(3)
USER_INFO_PARSE_ERROR = const(4)

# LobbiesStatus
LOBBIES_CREDENTIALS_MISSING = const(-1)
LOBBIES_SUCCESS = const(0)
LOBBIES_REQUEST_ERROR = const(1)
LOBBIES_NOT_STARTED = const(2)
LOBBIES_WAITING = const(3)
LOBBIES_PARSE_ERROR = const(4)

# JoinLobbyStatus
JOIN_LOBBY_CREDENTIALS_MISSING = const(-1)
JOIN_LOBBY_SUCCESS = const(0)
JOIN_LOBBY_REQUEST_ERROR = const(1)
JOIN_LOBBY_NOT_STARTED = const(2)
JOIN_LOBBY_WAITING = const(3)
JOIN_LOBBY_PARSE_ERROR = const(4)

# RequestType
REQUEST_TYPE_LOGIN = const(0)
REQUEST_TYPE_REGISTRATION = const(1)
REQUEST_TYPE_USER_INFO = const(2)
REQUEST_TYPE_LOBBIES = const(3)
REQUEST_TYPE_JOIN_LOBBY = const(4)
REQUEST_TYPE_START_WEBSOCKET = const(5)
REQUEST_TYPE_STOP_WEBSOCKET = const(6)
REQUEST_TYPE_SAVE_STATS = const(7)

# TitleIndex constants
TITLE_INDEX_STORY = const(0)  # story mode
TITLE_INDEX_PVE = const(1)  # pve multiplayer mode

# MenuIndex constants
MENU_INDEX_PROFILE = const(0)
MENU_INDEX_ABOUT = const(1)
MENU_INDEX_LEAVE_GAME = const(2)

# ToggleState constants
TOGGLE_STATE_OFF = const(0)
TOGGLE_STATE_ON = const(1)

# GameState constants
GAME_STATE_PLAYING = const(0)
GAME_STATE_MENU = const(1)
GAME_STATE_SWITCHING_LEVELS = const(2)
GAME_STATE_LEAVING_GAME = const(3)

# Input key constants
INPUT_KEY_UP = const(0)  # BUTTON_UP
INPUT_KEY_DOWN = const(1)  # BUTTON_DOWN
INPUT_KEY_LEFT = const(2)  # BUTTON_LEFT
INPUT_KEY_RIGHT = const(3)  # BUTTON_RIGHT
INPUT_KEY_OK = const(4)  # BUTTON_OK
INPUT_KEY_BACK = const(5)  # BUTTON_BACK
INPUT_KEY_MAX = const(-1)  # BUTTON_NONE

# Color constants
COLOR_WHITE = const(0x0000)  # inverted on purpose
COLOR_BLACK = const(0xFFFF)  # inverted on purpose

VERSION_TAG = "FlipWorld v0.1"


class LobbyInfo:
    """Structure to store lobby information."""

    def __init__(self):
        self.id: str = ""
        self.name: str = ""
        self.player_count: int = 0
        self.max_players: int = 0

    def __del__(self):
        del self.id
        self.id = None
        del self.name
        self.name = None
        self.player_count = 0
        self.max_players = 0


class Player(Entity):
    """Player entity for the FlipWorld game."""

    def __init__(self):
        from picoware.engine.image import Image

        super().__init__(
            "Player",
            ENTITY_TYPE_PLAYER,
            Vector(384, 192),
            Vector(15, 11),
            Image(Vector(15, 11), True, player_left_sword_15x11px),  # sprite
            Image(Vector(15, 11), True, player_left_sword_15x11px),  # sprite left
            Image(Vector(15, 11), True, player_right_sword_15x11px),  # sprite right
            None,  # start
            None,  # stop
            self.update,  # update
            self.render,  # render
            None,  # collision
            True,  # is 8_bit
        )

        self.ws = None
        self.ws_data = None

        # Mark this entity as a player
        self.is_player = True
        self.end_position = Vector(384, 192)
        self.start_position = Vector(384, 192)
        self.player_name = "Player"
        self.name = self.player_name

        # Initialize player stats
        self.level = 1
        self.health = 100
        self.max_health = 100
        self.strength = 10
        self.attack_timer = 1
        self.health_regen = 1
        self.xp = 0
        self.elapsed_health_regen = 0.0
        self.elapsed_attack_timer = 0.0

        # Current view and menu state
        self.current_title_index: int = TITLE_INDEX_STORY
        self.current_lobby_index: int = 0
        self.current_main_view: int = GAME_VIEW_TITLE
        self.current_system_menu_index: int = MENU_INDEX_PROFILE

        # Reference to run instance
        self.flip_world_run = None
        self.screen_size: Vector = Vector(128, 64)  # is set run.update_draw later
        self.game_state: int = GAME_STATE_PLAYING

        # Track various states
        self.has_been_positioned: bool = False
        self.input_held: bool = False
        self.just_started: bool = True
        self.just_switched_levels: bool = False
        self.level_completion_cooldown: float = 0.0
        self.last_input: int = INPUT_KEY_MAX
        self.leave_game: int = TOGGLE_STATE_OFF

        # Network/loading state
        self.loading = None
        self.lobbies: list = [LobbyInfo() for _ in range(4)]
        self.lobbies_status: int = LOBBIES_NOT_STARTED
        self.lobby_count: int = 0
        self.login_status: int = LOGIN_NOT_STARTED
        self.rain_frame: int = 0
        self.registration_status: int = REGISTRATION_NOT_STARTED
        self.system_menu_debounce_timer: float = 0.0
        self.user_info_status: int = USER_INFO_NOT_STARTED
        self.join_lobby_status: int = JOIN_LOBBY_NOT_STARTED

        # HTTP instance
        self.http = None

        # Loaded credentials
        self.loaded_username: str = ""
        self.loaded_password: str = ""

        self._update_new_pos = Vector(0, 0)
        self._update_old_pos = Vector(0, 0)

        self.old_xp = 0

        self.user_stats_pos = Vector(0, 0)
        self._img_vec = Vector(0, 0)
        self._img_size = Vector(0, 0)
        self._sprite_pos = Vector(0, 0)
        self._data_left = player_left_sword_15x11px
        self._data_right = player_right_sword_15x11px

        self.cent_box_pos = Vector(0, 0)
        self.cent_box_size = Vector(0, 0)
        self.cent_box_text = Vector(0, 0)

        self.user_stats_rect_pos = Vector(0, 0)
        self.user_stats_size = Vector(0, 0)
        self.user_stats_text = Vector(0, 0)

    def __del__(self):
        if self.loading:
            del self.loading
            self.loading = None
        if self.http:
            del self.http
            self.http = None
        #
        del self.screen_size
        self.screen_size = None
        self.lobbies.clear()
        self.lobbies = None
        del self.user_stats_pos
        self.user_stats_pos = None
        del self._img_vec
        self._img_vec = None
        del self._img_size
        self._img_size = None
        del self._update_new_pos
        self._update_new_pos = None
        del self._update_old_pos
        self._update_old_pos = None
        del self.cent_box_pos
        self.cent_box_pos = None
        del self.cent_box_size
        self.cent_box_size = None
        del self.cent_box_text
        self.cent_box_text = None
        del self.user_stats_size
        self.user_stats_size = None
        del self.user_stats_text
        self.user_stats_text = None
        del self.user_stats_rect_pos
        self.user_stats_rect_pos = None
        self._sprite_pos = None
        self._data_left = None
        self._data_right = None

    @property
    def should_leave_game(self) -> bool:
        """Check if the player has chosen to leave the game."""
        return self.leave_game == TOGGLE_STATE_ON

    @property
    def password(self) -> str:
        """Get password from storage."""
        if self.loaded_password:
            return self.loaded_password
        if not self.flip_world_run or not self.flip_world_run.view_manager:
            return ""
        view_manager = self.flip_world_run.view_manager
        data = view_manager.storage.read("picoware/flip_social/password.json")
        if data:
            try:
                obj = json_loads(data)
                if "password" in obj:
                    self.loaded_password = obj["password"]
                    return self.loaded_password
            except Exception:
                pass
        return ""

    @property
    def username(self) -> str:
        """Get username from storage."""
        if self.loaded_username:
            return self.loaded_username
        if not self.flip_world_run or not self.flip_world_run.view_manager:
            return ""
        view_manager = self.flip_world_run.view_manager
        data = view_manager.storage.read("picoware/flip_social/username.json")
        if data:
            try:
                obj = json_loads(data)
                if "username" in obj:
                    self.loaded_username = obj["username"]
                    return self.loaded_username
            except Exception:
                pass
        return ""

    def are_all_enemies_dead(self, game) -> bool:
        """Check if all enemies in the current level are dead."""
        if not game or not game.current_level:
            return False

        current_level = game.current_level
        total_enemies = 0
        dead_enemies = 0

        for i in range(current_level.entity_count):
            entity = current_level.get_entity(i)
            if entity and entity.type == ENTITY_TYPE_ENEMY:
                total_enemies += 1
                if entity.state == ENTITY_STATE_DEAD:
                    dead_enemies += 1
                else:
                    return False

        if total_enemies == 0:
            return False

        return dead_enemies == total_enemies

    def check_for_level_completion(self, game):
        """Check if all enemies are dead and switch to next level if needed."""
        if not self.flip_world_run or not self.flip_world_run.is_running:
            return
        if self.current_main_view != GAME_VIEW_GAME:
            return
        if not self.has_changed_position():
            return

        # Update cooldown timer
        self.level_completion_cooldown -= 1.0 / 60.0
        if self.level_completion_cooldown > 0:
            return

        if self.just_switched_levels:
            self.just_switched_levels = False
            self.level_completion_cooldown = 1.0
            return

        if self.are_all_enemies_dead(game):
            print("All enemies defeated! Switching levels...")
            current_level_index = self.flip_world_run.current_level_index

            # Determine next level (cycle through 0, 1, 2)
            next_level_index = (current_level_index + 1) % 3

            if self.flip_world_run.engine and self.flip_world_run.engine.game:
                self.game_state = GAME_STATE_SWITCHING_LEVELS
                self.flip_world_run.engine.game.level_switch(next_level_index)

                self.flip_world_run.sync_multiplayer_level()

                self.flip_world_run.set_icon_group(next_level_index)

                # Reset player position
                self.position = self.start_position

                self.just_switched_levels = True
                self.health = self.max_health
                self.level_completion_cooldown = 2.0
                self.game_state = GAME_STATE_PLAYING

    def draw_current_view(self, canvas):
        """Draw the current view based on the game state."""
        if not canvas:
            return

        # Update debounce timer
        if self.system_menu_debounce_timer > 0.0:
            self.system_menu_debounce_timer -= 1.0 / 120.0
            self.system_menu_debounce_timer = max(self.system_menu_debounce_timer, 0.0)

        if self.current_main_view == GAME_VIEW_TITLE:
            self.draw_title_view(canvas)
        elif self.current_main_view == GAME_VIEW_GAME:
            self.draw_game_view(canvas)
        elif self.current_main_view == GAME_VIEW_LOGIN:
            self.draw_login_view(canvas)
        elif self.current_main_view == GAME_VIEW_REGISTRATION:
            self.draw_registration_view(canvas)
        elif self.current_main_view == GAME_VIEW_USER_INFO:
            self.draw_user_info_view(canvas)
        elif self.current_main_view == GAME_VIEW_LOBBIES:
            self.draw_lobbies_view(canvas)
        elif self.current_main_view == GAME_VIEW_JOIN_LOBBY:
            self.draw_join_lobby_view(canvas)
        elif self.current_main_view == GAME_VIEW_SYSTEM_MENU:
            self.draw_system_menu_view(canvas)
        else:
            canvas.fill_screen(COLOR_WHITE)
            canvas.text(Vector(0, 10), "Unknown View", COLOR_BLACK)

        canvas.swap()

    def draw_game_view(self, canvas):
        """Draw the game view."""
        if self.flip_world_run.is_running:
            engine = self.flip_world_run.engine
            if engine:
                canvas.fill_screen(engine.game.background_color)
                # Handle system menu input
                current_input = self.flip_world_run.current_input
                if (
                    current_input == INPUT_KEY_BACK
                    and self.system_menu_debounce_timer <= 0.0
                ):
                    self.current_main_view = GAME_VIEW_SYSTEM_MENU
                    self.system_menu_debounce_timer = 0.05
                    self.flip_world_run.reset_input()
                    return

                engine.update_game_input(current_input)
                self.flip_world_run.reset_input()
                engine.run_async(False)
            return

        canvas.fill_screen(COLOR_WHITE)
        canvas.text(Vector(25, 32), "Starting Game...", COLOR_BLACK)
        game_started = self.flip_world_run.start_game()
        if game_started and self.flip_world_run.engine:
            self.flip_world_run.engine.run_async(False)

    def draw_join_lobby_view(self, canvas):
        """Draw the join lobby view."""
        canvas.fill_screen(COLOR_WHITE)
        if self.join_lobby_status == JOIN_LOBBY_WAITING:
            if not self.loading:
                self.user_request(REQUEST_TYPE_JOIN_LOBBY)
                self.loading = Loading(canvas, COLOR_BLACK, COLOR_WHITE)
                self.loading.text = "Joining..."
            if self.http and not self.http.is_request_complete():
                self.loading.animate(swap=False)
            else:
                if self.loading:
                    del self.loading
                    self.loading = None
                if self.http:
                    response = self.http.response.text if self.http.response else ""
                    if response:
                        self.join_lobby_status = JOIN_LOBBY_SUCCESS
                        self.current_main_view = GAME_VIEW_GAME
                        self.http.close()
                        del self.http
                        self.http = None
                        self.user_request(REQUEST_TYPE_START_WEBSOCKET)
                        self.flip_world_run.set_pve_mode(True)
                        self.flip_world_run.start_game()
                    else:
                        self.join_lobby_status = JOIN_LOBBY_REQUEST_ERROR
                        self.http.close()
                        del self.http
                        self.http = None
        elif self.join_lobby_status == JOIN_LOBBY_SUCCESS:
            canvas.text(Vector(0, 10), "Joined lobby!", COLOR_BLACK)
        elif self.join_lobby_status == JOIN_LOBBY_CREDENTIALS_MISSING:
            canvas.text(Vector(0, 10), "Missing credentials!", COLOR_BLACK)
        elif self.join_lobby_status == JOIN_LOBBY_REQUEST_ERROR:
            canvas.text(Vector(0, 10), "Join lobby failed!", COLOR_BLACK)
            canvas.text(Vector(0, 20), "Check your network.", COLOR_BLACK)
        else:
            canvas.text(Vector(0, 10), "Joining lobby...", COLOR_BLACK)

    def draw_lobbies_view(self, canvas):
        """Draw the lobbies view."""
        canvas.fill_screen(COLOR_WHITE)
        if self.lobbies_status == LOBBIES_WAITING:
            if not self.loading:
                self.loading = Loading(canvas, COLOR_BLACK, COLOR_WHITE)
                self.loading.text = "Fetching..."
                self.user_request(REQUEST_TYPE_LOBBIES)
                return
            if self.http and not self.http.is_request_complete():
                self.loading.animate(swap=False)
            else:
                if self.loading:
                    del self.loading
                    self.loading = None
                if self.http:
                    response = self.http.response.text if self.http.response else ""
                    if response:
                        self.lobbies_status = LOBBIES_SUCCESS
                        self.lobby_count = 0
                        self.current_lobby_index = 0
                        try:
                            data = json_loads(response)
                            lobbies_list = data.get("lobbies", [])
                            for i, lobby in enumerate(lobbies_list[:4]):
                                self.lobbies[i].id = lobby.get("id", "")
                                self.lobbies[i].name = lobby.get("name", f"Lobby {i}")
                                self.lobbies[i].player_count = lobby.get(
                                    "player_count", 0
                                )
                                self.lobbies[i].max_players = lobby.get(
                                    "max_players", 10
                                )
                                self.lobby_count += 1
                        except Exception:
                            self.lobbies_status = LOBBIES_PARSE_ERROR
                    else:
                        self.lobbies_status = LOBBIES_REQUEST_ERROR
                    del self.http
                    self.http = None
        elif self.lobbies_status == LOBBIES_SUCCESS:
            canvas.text(Vector(5, 10), "Select a Lobby:", COLOR_BLACK)
            if self.lobby_count == 0:
                canvas.text(Vector(5, 25), "No lobbies available", COLOR_BLACK)
            else:
                start_y = int(self.screen_size.y) // 5
                item_height = int(self.screen_size.y) // 10
                for i in range(min(4, self.lobby_count)):
                    y = start_y + i * item_height
                    if i == self.current_lobby_index:
                        canvas.fill_rectangle(
                            Vector(3, y - 2),
                            Vector(self.screen_size.x - 6, item_height),
                            COLOR_BLACK,
                        )
                        color = COLOR_WHITE
                    else:
                        color = COLOR_BLACK
                    lobby_text = f"{self.lobbies[i].name} ({self.lobbies[i].player_count}/{self.lobbies[i].max_players})"
                    canvas.text(Vector(5, y + 7), lobby_text[:25], color)
        elif self.lobbies_status == LOBBIES_CREDENTIALS_MISSING:
            canvas.text(Vector(0, 10), "Missing credentials!", COLOR_BLACK)
        elif self.lobbies_status == LOBBIES_REQUEST_ERROR:
            canvas.text(Vector(0, 10), "Lobbies request failed!", COLOR_BLACK)
        else:
            canvas.text(Vector(0, 10), "Loading lobbies...", COLOR_BLACK)

    def draw_login_view(self, canvas):
        """Draw the login view."""
        canvas.fill_screen(COLOR_WHITE)
        if self.login_status == LOGIN_WAITING:
            if not self.loading:
                self.user_request(REQUEST_TYPE_LOGIN)
                self.loading = Loading(canvas, COLOR_BLACK, COLOR_WHITE)
                self.loading.text = "Logging in..."
            if self.http and not self.http.is_request_complete():
                self.loading.animate(swap=False)
            else:
                if self.loading:
                    del self.loading
                    self.loading = None
                if self.http:
                    response = self.http.response.text if self.http.response else ""
                    if "[SUCCESS]" in response:
                        self.login_status = LOGIN_SUCCESS
                        self.current_main_view = GAME_VIEW_USER_INFO
                        self.user_info_status = USER_INFO_WAITING
                    elif "User not found" in response:
                        self.login_status = LOGIN_NOT_STARTED
                        self.current_main_view = GAME_VIEW_REGISTRATION
                        self.registration_status = REGISTRATION_WAITING
                    elif "Incorrect password" in response:
                        self.login_status = LOGIN_WRONG_PASSWORD
                    else:
                        self.login_status = LOGIN_REQUEST_ERROR
                    del self.http
                    self.http = None
        elif self.login_status == LOGIN_SUCCESS:
            canvas.text(Vector(0, 10), "Login successful!", COLOR_BLACK)
        elif self.login_status == LOGIN_CREDENTIALS_MISSING:
            canvas.text(Vector(0, 10), "Missing credentials!", COLOR_BLACK)
            canvas.text(Vector(0, 20), "Set username/password", COLOR_BLACK)
        elif self.login_status == LOGIN_REQUEST_ERROR:
            canvas.text(Vector(0, 10), "Login failed!", COLOR_BLACK)
            canvas.text(Vector(0, 20), "Check your network.", COLOR_BLACK)
        elif self.login_status == LOGIN_WRONG_PASSWORD:
            canvas.text(Vector(0, 10), "Wrong password!", COLOR_BLACK)
        else:
            canvas.text(Vector(0, 10), "Logging in...", COLOR_BLACK)

    def draw_rain_effect(self, canvas):
        """Draw rain/star droplet effect."""
        width = canvas.size.x
        height = canvas.size.y

        # Rain droplets/star droplets effect
        for i in range(16):
            # Use pseudo-random offsets based on frame and droplet index
            seed = (self.rain_frame + i * 37) & 0xFF
            x = (self.rain_frame + seed * 13) % width
            y = (self.rain_frame * 2 + seed * 7 + i * 23) % height

            # Draw star-like droplet with bounds checking
            canvas._pixel(x, y, COLOR_BLACK)
            if x >= 1:
                canvas._pixel(x - 1, y, COLOR_BLACK)
            if x <= width - 2:
                canvas._pixel(x + 1, y, COLOR_BLACK)
            if y >= 1:
                canvas._pixel(x, y - 1, COLOR_BLACK)
            if y <= height - 2:
                canvas._pixel(x, y + 1, COLOR_BLACK)

        self.rain_frame += 1
        if self.rain_frame >= width:
            self.rain_frame = 0

    def draw_registration_view(self, canvas):
        """Draw the registration view."""
        canvas.fill_screen(COLOR_WHITE)
        if self.registration_status == REGISTRATION_WAITING:
            if not self.loading:
                self.user_request(REQUEST_TYPE_REGISTRATION)
                self.loading = Loading(canvas, COLOR_BLACK, COLOR_WHITE)
                self.loading.text = "Registering..."
            if self.http and not self.http.is_request_complete():
                self.loading.animate(swap=False)
            else:
                if self.loading:
                    del self.loading
                    self.loading = None
                if self.http:
                    response = self.http.response.text if self.http.response else ""
                    if "[SUCCESS]" in response:
                        self.registration_status = REGISTRATION_SUCCESS
                        self.current_main_view = GAME_VIEW_USER_INFO
                        self.user_info_status = USER_INFO_WAITING
                    else:
                        self.registration_status = REGISTRATION_REQUEST_ERROR
                    del self.http
                    self.http = None
        elif self.registration_status == REGISTRATION_SUCCESS:
            canvas.text(Vector(0, 10), "Registration successful!", COLOR_BLACK)
        elif self.registration_status == REGISTRATION_CREDENTIALS_MISSING:
            canvas.text(Vector(0, 10), "Missing credentials!", COLOR_BLACK)
        elif self.registration_status == REGISTRATION_REQUEST_ERROR:
            canvas.text(Vector(0, 10), "Registration failed!", COLOR_BLACK)
        else:
            canvas.text(Vector(0, 10), "Registering...", COLOR_BLACK)

    def draw_system_menu_view(self, canvas):
        """Draw the system menu view."""
        current_input = self.flip_world_run.current_input

        if current_input == INPUT_KEY_BACK and self.system_menu_debounce_timer <= 0.0:
            self.current_main_view = GAME_VIEW_GAME
            self.system_menu_debounce_timer = 0.05
            self.flip_world_run.reset_input()
            return

        if current_input == INPUT_KEY_UP and self.system_menu_debounce_timer <= 0.0:
            if self.current_system_menu_index > MENU_INDEX_PROFILE:
                self.current_system_menu_index -= 1
            self.system_menu_debounce_timer = 0.05
            self.flip_world_run.reset_input()
        elif current_input == INPUT_KEY_DOWN and self.system_menu_debounce_timer <= 0.0:
            if self.current_system_menu_index < MENU_INDEX_LEAVE_GAME:
                self.current_system_menu_index += 1
            self.system_menu_debounce_timer = 0.05
            self.flip_world_run.reset_input()
        elif current_input == INPUT_KEY_OK and self.system_menu_debounce_timer <= 0.0:
            if self.current_system_menu_index == MENU_INDEX_LEAVE_GAME:
                self.leave_game = TOGGLE_STATE_ON
                return
            self.system_menu_debounce_timer = 0.3
            self.flip_world_run.reset_input()

        canvas.fill_screen(COLOR_WHITE)

        # Menu box dimensions
        menu_x = int(self.screen_size.x * 0.625)
        menu_y = int(self.screen_size.y * 0.1875)
        menu_width = int(self.screen_size.x * 0.28125)
        menu_height = int(self.screen_size.y * 0.65625)

        # Menu item positions
        menu_item_y_offset = int(self.screen_size.y * 0.15625)
        menu_item_spacing = int(self.screen_size.y * 0.15625)
        menu_text_x = menu_x + 6

        if self.current_system_menu_index == MENU_INDEX_PROFILE:
            # Content positions using screen_size
            content_x = int(self.screen_size.x * 0.0546875)
            content_start_y = int(self.screen_size.y * 0.25)
            line_height = int(self.screen_size.y * 0.109375)

            canvas.text(
                Vector(content_x, content_start_y), self.name or "Player", COLOR_BLACK
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 2),
                f"Level   : {int(self.level)}",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 3),
                f"Health  : {int(self.health)}",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 4),
                f"XP      : {int(self.xp)}",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 5),
                f"Strength: {int(self.strength)}",
                COLOR_BLACK,
            )

        elif self.current_system_menu_index == MENU_INDEX_ABOUT:
            # Content positions using screen_size
            content_x = int(self.screen_size.x * 0.0546875)
            content_start_y = int(self.screen_size.y * 0.25)
            line_height = int(self.screen_size.y * 0.109375)

            canvas.text(Vector(content_x, content_start_y), VERSION_TAG, COLOR_BLACK)
            canvas.text(
                Vector(content_x, content_start_y + line_height),
                "Developed by",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 2),
                "JBlanked and Derek",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 3),
                "Jamison. Graphics",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 4),
                "from Pr3!",
                COLOR_BLACK,
            )

        elif self.current_system_menu_index == MENU_INDEX_LEAVE_GAME:
            # Content positions using screen_size
            content_x = int(self.screen_size.x * 0.0546875)
            content_start_y = int(self.screen_size.y * 0.25)
            line_height = int(self.screen_size.y * 0.109375)

            canvas.text(Vector(content_x, content_start_y), "Leave Game", COLOR_BLACK)
            canvas.text(
                Vector(content_x, content_start_y + line_height * 2),
                "Are you sure you",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 3),
                "want to leave",
                COLOR_BLACK,
            )
            canvas.text(
                Vector(content_x, content_start_y + line_height * 4),
                "the game?",
                COLOR_BLACK,
            )

        # Draw menu box
        canvas.rect(
            Vector(menu_x, menu_y), Vector(menu_width, menu_height), COLOR_BLACK
        )

        # Draw menu items with highlight rectangle around current selection
        menu_items = ["Info", "More", "Quit"]
        highlight_padding = 2
        highlight_height = int(self.screen_size.y * 0.125)

        for i, item in enumerate(menu_items):
            item_y = menu_y + menu_item_y_offset + (i * menu_item_spacing)

            # Draw highlight rectangle around current menu item
            if i == self.current_system_menu_index:
                canvas.rect(
                    Vector(menu_text_x - highlight_padding, item_y - highlight_padding),
                    Vector(menu_width - 12, highlight_height),
                    COLOR_BLACK,
                )

            canvas.text(Vector(menu_text_x, item_y), item, COLOR_BLACK)

    def draw_title_view(self, canvas):
        """Draw the title view."""
        canvas.fill_screen(COLOR_WHITE)
        self.draw_rain_effect(canvas)

        button_x = int(self.screen_size.x * 0.28125)
        button_y1 = int(self.screen_size.y * 0.25)
        button_width = int(self.screen_size.x * 0.4375)
        button_height = int(self.screen_size.y * 0.25)
        button_y2 = int(self.screen_size.y * 0.5)
        text_x = int(self.screen_size.x * 0.421875)
        text_y1 = button_y1 + int(button_height * 0.6875)
        text_y2 = button_y2 + int(button_height * 0.625)

        if self.current_title_index == TITLE_INDEX_STORY:
            canvas.fill_rectangle(
                Vector(button_x, button_y1),
                Vector(button_width, button_height),
                COLOR_BLACK,
            )
            canvas.text(Vector(text_x, text_y1), "Story", COLOR_WHITE)
            canvas.fill_rectangle(
                Vector(button_x, button_y2),
                Vector(button_width, button_height),
                COLOR_WHITE,
            )
            canvas.text(Vector(text_x, text_y2), "PvE", COLOR_BLACK)
        else:
            canvas.fill_rectangle(
                Vector(button_x, button_y1),
                Vector(button_width, button_height),
                COLOR_WHITE,
            )
            canvas.text(Vector(text_x, text_y1), "Story", COLOR_BLACK)
            canvas.fill_rectangle(
                Vector(button_x, button_y2),
                Vector(button_width, button_height),
                COLOR_BLACK,
            )
            canvas.text(Vector(text_x, text_y2), "PvE", COLOR_WHITE)

    def draw_user_info_view(self, canvas):
        """Draw the user info view."""
        canvas.fill_screen(COLOR_WHITE)
        if self.user_info_status == USER_INFO_WAITING:
            if not self.loading:
                self.user_request(REQUEST_TYPE_USER_INFO)
                self.loading = Loading(canvas, COLOR_BLACK, COLOR_WHITE)
                self.loading.text = "Syncing..."
            if self.http and not self.http.is_request_complete():
                self.loading.animate(swap=False)
            else:
                if self.loading:
                    del self.loading
                    self.loading = None
                if self.http:
                    response = self.http.response.text if self.http.response else ""
                    if response:
                        try:
                            data = json_loads(response)
                            game_stats = data.get("game_stats", {})
                            if game_stats:
                                self.player_name = game_stats.get("username", "Player")
                                self.name = self.player_name
                                self.level = game_stats.get("level", 1)
                                self.xp = game_stats.get("xp", 0)
                                self.health = game_stats.get("health", 100)
                                self.strength = game_stats.get("strength", 10)
                                self.max_health = game_stats.get("max_health", 100)
                                self.user_info_status = USER_INFO_SUCCESS

                                if self.current_title_index == TITLE_INDEX_STORY:
                                    self.current_main_view = GAME_VIEW_GAME
                                    self.flip_world_run.start_game()
                                else:
                                    self.current_main_view = GAME_VIEW_LOBBIES
                                    self.lobbies_status = LOBBIES_WAITING
                            else:
                                self.user_info_status = USER_INFO_PARSE_ERROR
                        except Exception:
                            self.user_info_status = USER_INFO_PARSE_ERROR
                    else:
                        self.user_info_status = USER_INFO_REQUEST_ERROR
                    del self.http
                    self.http = None
        elif self.user_info_status == USER_INFO_SUCCESS:
            canvas.text(Vector(0, 10), "User info loaded!", COLOR_BLACK)
        elif self.user_info_status == USER_INFO_CREDENTIALS_MISSING:
            canvas.text(Vector(0, 10), "Missing credentials!", COLOR_BLACK)
        elif self.user_info_status == USER_INFO_REQUEST_ERROR:
            canvas.text(Vector(0, 10), "User info request failed!", COLOR_BLACK)
        elif self.user_info_status == USER_INFO_PARSE_ERROR:
            canvas.text(Vector(0, 10), "Failed to parse user info!", COLOR_BLACK)
        else:
            canvas.text(Vector(0, 10), "Loading user info...", COLOR_BLACK)

    def draw_username(self, pos: Vector, game):
        """Draw the username at the specified position."""
        if not self.name or not game:
            return

        # Calculate the center of the player horizontally
        player_center_x = int(pos.x + self.size.x / 2)
        screen_x = int(player_center_x - game.position.x)
        screen_y = int(pos.y - game.position.y)

        # Calculate text width using font size
        text_width = int(len(self.name) * game.draw.font_size.x)
        font_height = int(game.draw.font_size.y)

        # Calculate box dimensions with padding
        box_padding = 2
        box_width = int(text_width + (box_padding * 2))
        box_height = int(font_height + (box_padding * 2))

        # Position box above the player at consistent height
        vertical_offset = int(self.screen_size.x // 18)

        # Center the box horizontally on the player
        box_x = int(screen_x - box_width // 2)
        box_y = int(screen_y - vertical_offset)

        # Check if box is within screen bounds
        if box_x < 0 or box_x + box_width > self.screen_size.x:
            return
        if box_y < 0 or box_y + box_height > self.screen_size.y:
            return

        # Draw centered box
        self.cent_box_pos.x = box_x
        self.cent_box_pos.y = box_y
        self.cent_box_size.x = box_width
        self.cent_box_size.y = box_height
        game.draw.fill_rectangle(
            self.cent_box_pos,
            self.cent_box_size,
            COLOR_WHITE,
        )

        # Center the text in the box
        text_x = int(screen_x - text_width // 2)
        text_y = int(box_y + box_padding)
        self.cent_box_text.x = text_x
        self.cent_box_text.y = text_y
        game.draw.text(
            self.cent_box_text,
            self.name,
            COLOR_BLACK,
        )

    def draw_user_stats(self, pos: Vector, canvas):
        """Draw the user stats at the specified position."""
        self.user_stats_rect_pos.x = int(pos.x - 2)
        self.user_stats_rect_pos.y = int(pos.y - 5)
        canvas.fill_rectangle(
            self.user_stats_rect_pos,
            self.user_stats_size,
            COLOR_WHITE,
        )

        health_str = f"HP : {int(self.health)}"
        level_str = f"LVL: {int(self.level)}"
        if self.xp < 10000:
            xp_str = f"XP : {int(self.xp)}"
        else:
            xp_str = f"XP : {int(self.xp // 1000)}K"

        self.user_stats_text.x = int(pos.x)
        self.user_stats_text.y = int(pos.y)
        canvas.text(self.user_stats_text, health_str, COLOR_BLACK)
        #
        self.user_stats_text.y = int(pos.y + 9)
        canvas.text(self.user_stats_text, xp_str, COLOR_BLACK)
        #
        self.user_stats_text.y = int(pos.y + 18)
        canvas.text(self.user_stats_text, level_str, COLOR_BLACK)

    def icon_group_render(self, game):
        """Render the icon group for the current level."""
        if not self.flip_world_run or not game or not game.draw:
            return

        # Camera bounds for visibility culling
        cam_x = game.position.x
        cam_y = game.position.y
        cam_right = cam_x + self.screen_size.x
        cam_bottom = cam_y + self.screen_size.y

        # Expand by small margin to prevent pop-in
        margin = 20
        cam_x -= margin
        cam_y -= margin
        cam_right += margin
        cam_bottom += margin

        icon_data_map = self.flip_world_run.icon_map

        for spec in self.flip_world_run.current_icon_group.icons:
            # Fast rejection test - is icon center even close to camera?
            if spec.x < cam_x or spec.x > cam_right:
                continue
            if spec.y < cam_y or spec.y > cam_bottom:
                continue

            # Icon is potentially visible, calculate screen position
            half_w = spec.width >> 1
            half_h = spec.height >> 1

            self._img_vec.x = int(spec.x - cam_x - half_w)
            self._img_vec.y = int(spec.y - cam_y - half_h)

            # Final bounds check
            if (
                self._img_vec.x + spec.width < 0
                or self._img_vec.x > self.screen_size.x
                or self._img_vec.y + spec.height < 0
                or self._img_vec.y > self.screen_size.y
            ):
                continue

            self._img_size.x = spec.width
            self._img_size.y = spec.height

            # Draw the icon
            game.draw.image_bytearray(
                self._img_vec, self._img_size, icon_data_map[spec.id]
            )

    def process_input(self):
        """Process input for all views."""
        if not self.flip_world_run:
            return

        current_input = self.last_input
        if current_input == INPUT_KEY_MAX:
            return

        if self.current_main_view == GAME_VIEW_TITLE:
            if current_input == INPUT_KEY_UP:
                self.current_title_index = TITLE_INDEX_STORY

            elif current_input == INPUT_KEY_DOWN:
                self.current_title_index = TITLE_INDEX_PVE

            elif current_input == INPUT_KEY_OK:

                self.current_main_view = GAME_VIEW_LOGIN
                self.login_status = LOGIN_WAITING
            elif current_input == INPUT_KEY_BACK:
                self.leave_game = TOGGLE_STATE_ON

        elif self.current_main_view == GAME_VIEW_LOGIN:
            if current_input == INPUT_KEY_BACK:
                self.current_main_view = GAME_VIEW_TITLE

            elif current_input == INPUT_KEY_OK:
                if self.login_status == LOGIN_SUCCESS:
                    self.current_main_view = GAME_VIEW_USER_INFO
                    self.user_info_status = USER_INFO_WAITING

        elif self.current_main_view == GAME_VIEW_REGISTRATION:
            if current_input == INPUT_KEY_BACK:
                self.current_main_view = GAME_VIEW_TITLE

            elif current_input == INPUT_KEY_OK:
                if self.registration_status == REGISTRATION_SUCCESS:
                    self.current_main_view = GAME_VIEW_USER_INFO
                    self.user_info_status = USER_INFO_WAITING

        elif self.current_main_view == GAME_VIEW_USER_INFO:
            if current_input == INPUT_KEY_BACK:
                self.current_main_view = GAME_VIEW_TITLE

        elif self.current_main_view == GAME_VIEW_LOBBIES:
            if current_input == INPUT_KEY_BACK:
                self.current_main_view = GAME_VIEW_TITLE

            elif current_input == INPUT_KEY_UP:
                if self.lobbies_status == LOBBIES_SUCCESS and self.lobby_count > 0:
                    self.current_lobby_index = (
                        self.current_lobby_index - 1 + self.lobby_count
                    ) % self.lobby_count

            elif current_input == INPUT_KEY_DOWN:
                if self.lobbies_status == LOBBIES_SUCCESS and self.lobby_count > 0:
                    self.current_lobby_index = (
                        self.current_lobby_index + 1
                    ) % self.lobby_count

            elif current_input == INPUT_KEY_OK:
                if self.lobbies_status == LOBBIES_SUCCESS and self.lobby_count > 0:
                    self.current_main_view = GAME_VIEW_JOIN_LOBBY

                    self.join_lobby_status = JOIN_LOBBY_WAITING
                    self.flip_world_run.set_is_lobby_host(
                        self.lobbies[self.current_lobby_index].player_count == 0
                    )
                elif self.lobbies_status != LOBBIES_SUCCESS:
                    self.current_main_view = GAME_VIEW_TITLE

    def render(self, draw, game):
        """Render callback for the player."""
        if self.current_main_view != GAME_VIEW_GAME:
            return
        self.icon_group_render(game)

        # Draw player sprite explicitly
        screen_x = int(self.position.x - game.position.x)
        screen_y = int(self.position.y - game.position.y)
        if (
            screen_x + self.size.x >= 0
            and screen_x < game.draw.size.x
            and screen_y + self.size.y >= 0
            and screen_y < game.draw.size.y
        ):
            _data = self._data_left if self.direction.x == -1 else self._data_right
            self._sprite_pos.x = screen_x
            self._sprite_pos.y = screen_y
            draw.image_bytearray(self._sprite_pos, self.size, _data)

        self.draw_username(self.position, game)
        self.draw_user_stats(self.user_stats_pos, canvas=draw)
        # Suppress engine auto-draw (sprite already drawn above)
        self.is_visible = False

    def sync_multiplayer_state(self):
        """Sync's the sprites data for multiplayer"""
        if (
            self.flip_world_run is None  # class must be set
            or self.ws is None  # ws must be set
        ):
            return
        # send sprite data to server
        self.flip_world_run.sync_multiplayer_entity(self)

    def update(self, game):
        """Update callback for the player."""
        if self.current_main_view != GAME_VIEW_GAME:
            return

        # Health regeneration
        self.elapsed_health_regen += 0.05
        if self.elapsed_health_regen >= 1 and self.health < self.max_health:
            self.health += self.health_regen
            self.elapsed_health_regen = 0
            self.health = min(self.health, self.max_health)

        self.elapsed_attack_timer += 0.05
        self.update_stats()
        self.check_for_level_completion(game)

        self._update_old_pos = self.position
        self._update_new_pos.x = self._update_old_pos.x
        self._update_new_pos.y = self._update_old_pos.y
        should_set_position = False

        if game.input == INPUT_KEY_UP:
            self._update_new_pos.y -= 5
            self.direction = Vector(0, -1)
            should_set_position = True
        elif game.input == INPUT_KEY_DOWN:
            self._update_new_pos.y += 5
            self.direction = Vector(0, 1)
            should_set_position = True
        elif game.input == INPUT_KEY_LEFT:
            self._update_new_pos.x -= 5
            self.direction = Vector(-1, 0)
            should_set_position = True
        elif game.input == INPUT_KEY_RIGHT:
            self._update_new_pos.x += 5
            self.direction = Vector(1, 0)
            should_set_position = True

        game.input = INPUT_KEY_MAX

        # Check boundaries
        if (
            self._update_new_pos.x < 0
            or self._update_new_pos.x + self.size.x > game.size.x
        ):
            should_set_position = False
        if (
            self._update_new_pos.y < 0
            or self._update_new_pos.y + self.size.y > game.size.y
        ):
            should_set_position = False

        if should_set_position:
            has_collision = False

            # Loop over all icon specifications in the current icon group.
            for icon in self.flip_world_run.current_icon_group.icons:

                # Rough bounding box check first
                if (
                    abs(self._update_new_pos.x - icon.x) > 30
                    or abs(self._update_new_pos.y - icon.y) > 30
                ):
                    continue  # Too far away, skip

                # Calculate the difference between the NEW position and the icon's center.
                dx = self._update_new_pos.x - icon.x
                dy = self._update_new_pos.y - icon.y
                radius = (icon.width + icon.height) / 4.0

                # Collision: if player's distance to the icon center is less than the collision radius.
                if (dx * dx + dy * dy) < (radius * radius):
                    has_collision = True
                    break

            # Only update position if there's no collision
            if not has_collision:
                self.position = self._update_new_pos
                self.sync_multiplayer_state()

        # update player sprite based on direction
        if self.direction.x == -1 and self.direction.y == 0:
            self.sprite = self.sprite_left
        elif self.direction.x == 1 and self.direction.y == 0:
            self.sprite = self.sprite_right

        # Update camera
        viewport_width = self.screen_size.x
        viewport_height = self.screen_size.y

        camera_x = self.position.x - (viewport_width / 2)
        camera_y = self.position.y - (viewport_height / 2)

        _pos_x = max(0, min(camera_x, game.size.x - viewport_width))
        _pos_y = max(0, min(camera_y, game.size.y - viewport_height))
        game.position = Vector(_pos_x, _pos_y)

    def update_stats(self):
        """Update player stats based on XP."""
        if self.xp == self.old_xp:
            return

        self.level = 1
        xp_required = 100

        while self.level < 100 and self.xp >= xp_required:
            self.level += 1
            xp_required = int(xp_required * 1.5)

        self.strength = 10 + (self.level * 1)
        self.max_health = 100 + ((self.level - 1) * 10)

        self.old_xp = self.xp

    def __user_request_ws_callback(self, data):
        """WebSocket callback for handling incoming messages."""
        if not data:
            return
        self.ws_data = data

    def user_request(self, request_type: int):
        """Send a user request to the server."""
        if not self.flip_world_run:
            return

        username = self.username
        password = self.password

        if not username or not password:
            if request_type == REQUEST_TYPE_LOGIN:
                self.login_status = LOGIN_CREDENTIALS_MISSING
            elif request_type == REQUEST_TYPE_REGISTRATION:
                self.registration_status = REGISTRATION_CREDENTIALS_MISSING
            elif request_type == REQUEST_TYPE_USER_INFO:
                self.user_info_status = USER_INFO_CREDENTIALS_MISSING
            elif request_type == REQUEST_TYPE_LOBBIES:
                self.lobbies_status = LOBBIES_CREDENTIALS_MISSING
            elif request_type == REQUEST_TYPE_JOIN_LOBBY:
                self.join_lobby_status = JOIN_LOBBY_CREDENTIALS_MISSING
            return

        from picoware.system.http import HTTP

        if self.http:
            self.http.close()
            del self.http
            self.http = None

        view_manager = self.flip_world_run.view_manager
        self.http = HTTP(thread_manager=view_manager.thread_manager)

        try:
            # Create JSON payload for login/registration
            payload = {"username": username, "password": password}
            headers = {
                "Content-Type": "application/json",
                "HTTP_USER_AGENT": "Pico",
                "Setting": "X-Flipper-Redirect",
                "username": username,
                "password": password,
                "User-Agent": "Raspberry Pi Pico W",
            }

            if request_type == REQUEST_TYPE_LOGIN:
                self.http.post_async(
                    "https://www.jblanked.com/flipper/api/user/login/",
                    payload,
                    headers,
                )
            elif request_type == REQUEST_TYPE_REGISTRATION:
                self.http.post_async(
                    "https://www.jblanked.com/flipper/api/user/register/",
                    payload,
                    headers,
                )
            elif request_type == REQUEST_TYPE_USER_INFO:
                self.http.get_async(
                    f"https://www.jblanked.com/flipper/api/user/game-stats/{username}/",
                    headers,
                )
            elif request_type == REQUEST_TYPE_LOBBIES:
                self.http.get_async(
                    "https://www.jblanked.com/flipper/api/world/pve/lobbies/10/4/",
                    headers,
                )
            elif request_type == REQUEST_TYPE_JOIN_LOBBY:
                lobby_id = self.lobbies[self.current_lobby_index].id
                payload2 = {"username": username, "game_id": lobby_id}
                self.http.post_async(
                    "https://www.jblanked.com/flipper/api/world/pve/lobby/join/",
                    payload2,
                    headers,
                )
            elif request_type == REQUEST_TYPE_START_WEBSOCKET:
                del self.http
                self.http = None
                if self.ws is not None:
                    self.ws.close()
                    del self.ws
                    self.ws = None

                from picoware.system.websocket import WebSocketAsync

                self.ws = WebSocketAsync(
                    f"ws://www.jblanked.com/ws/game/{self.lobbies[self.current_lobby_index].id}/",
                    callback=self.__user_request_ws_callback,
                    thread_manager=view_manager.thread_manager,
                )
                if not self.ws or not self.ws.connect():
                    view_manager.log("Failed to start WebSocket", 2)
                    self.join_lobby_status = JOIN_LOBBY_REQUEST_ERROR
                    del self.ws
                    self.ws = None
            elif request_type == REQUEST_TYPE_STOP_WEBSOCKET:
                del self.http
                self.http = None
                if self.ws is not None:
                    self.ws.close()
                    del self.ws
                    self.ws = None
            elif request_type == REQUEST_TYPE_SAVE_STATS:
                player_json = self.flip_world_run.entity_to_json(self)
                if not self.http.post_async(
                    "https://www.jblanked.com/flipper/api/user/update-game-stats/",
                    player_json,
                    headers,
                ):
                    view_manager.log("Failed to save player stats", 2)

        except Exception:
            if request_type == REQUEST_TYPE_LOGIN:
                self.login_status = LOGIN_REQUEST_ERROR
            elif request_type == REQUEST_TYPE_REGISTRATION:
                self.registration_status = REGISTRATION_REQUEST_ERROR
            elif request_type == REQUEST_TYPE_USER_INFO:
                self.user_info_status = USER_INFO_REQUEST_ERROR
            elif request_type == REQUEST_TYPE_LOBBIES:
                self.lobbies_status = LOBBIES_REQUEST_ERROR
            elif request_type == REQUEST_TYPE_JOIN_LOBBY:
                self.join_lobby_status = JOIN_LOBBY_REQUEST_ERROR
