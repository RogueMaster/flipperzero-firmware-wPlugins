from picoware.engine.entity import (
    Entity,
    ENTITY_STATE_IDLE,
    ENTITY_STATE_MOVING_TO_START,
    ENTITY_STATE_MOVING_TO_END,
    ENTITY_STATE_ATTACKED,
    ENTITY_STATE_DEAD,
    ENTITY_TYPE_ENEMY,
    ENTITY_TYPE_PLAYER,
    ENTITY_TYPE_NPC,
)
from picoware.system.vector import Vector
from math import fabs, sqrt
from picoware.system.buttons import BUTTON_CENTER, BUTTON_NONE
from picoware.engine.image import Image
from flip_world.assets import (
    enemy_left_ghost_15x15px,
    enemy_right_ghost_15x15px,
    enemy_left_cyclops_10x11px,
    enemy_right_cyclops_10x11px,
    enemy_left_ogre_10x13px,
    enemy_right_ogre_10x13px,
    npc_left_funny_15x21px,
    npc_right_funny_15x21px,
)


class Sprite(Entity):
    """Class representing a game sprite (enemy or NPC)."""

    def __init__(
        self,
        name: str,
        entity_type: int,
        start_position: Vector,
        end_position: Vector,
        move_timer: float,
        speed: float,
        attack_timer: float,
        strength: float,
        health: float,
    ):

        super().__init__(
            name,  # name
            entity_type,  # entity_type
            start_position,  # position
            Vector(0, 0),  # size
            None,  # sprite data (Image)
            None,  # sprite_left data (Image)
            None,  # sprite_right data (Image)
            None,  # start
            None,  # stop
            self.update,  # update
            self.render,  # render
            self.collision,  # collision
            True,  # is 8_bit
        )
        # Create copies to avoid reference sharing
        self.start_position = Vector(start_position.x, start_position.y)
        self.end_position = Vector(end_position.x, end_position.y)
        self.state = ENTITY_STATE_MOVING_TO_END
        self.move_timer = move_timer
        self.speed = speed
        self.attack_timer = attack_timer
        self.strength = strength
        self.health = health
        self.elapsed_move_timer = 0.0
        self.elapsed_attack_timer = 0.0
        self.flip_world_run = None
        self._update_new_pos = Vector(0, 0)
        self._update_direction = Vector(0, 0)
        self.cent_box_pos = Vector(0, 0)
        self.cent_box_size = Vector(0, 0)
        self.cent_box_text = Vector(0, 0)
        self._draw_pos = Vector(0, 0)
        self._data_left = None
        self._data_right = None

        if name == "Cyclops":
            self.size = Vector(10, 11)
            self.sprite = Image(self.size, True, enemy_left_cyclops_10x11px)
            self.sprite_left = Image(self.size, True, enemy_left_cyclops_10x11px)
            self.sprite_right = Image(self.size, True, enemy_right_cyclops_10x11px)
            self._data_left = enemy_left_cyclops_10x11px
            self._data_right = enemy_right_cyclops_10x11px

        elif name == "Ogre":
            self.size = Vector(10, 13)
            self.sprite = Image(self.size, True, enemy_left_ogre_10x13px)
            self.sprite_left = Image(self.size, True, enemy_left_ogre_10x13px)
            self.sprite_right = Image(self.size, True, enemy_right_ogre_10x13px)
            self._data_left = enemy_left_ogre_10x13px
            self._data_right = enemy_right_ogre_10x13px

        elif name == "Ghost":
            self.size = Vector(15, 15)
            self.sprite = Image(self.size, True, enemy_left_ghost_15x15px)
            self.sprite_left = Image(self.size, True, enemy_left_ghost_15x15px)
            self.sprite_right = Image(self.size, True, enemy_right_ghost_15x15px)
            self._data_left = enemy_left_ghost_15x15px
            self._data_right = enemy_right_ghost_15x15px

        elif name == "Funny NPC":
            self.size = Vector(15, 21)
            self.sprite = Image(self.size, True, npc_left_funny_15x21px)
            self.sprite_left = Image(self.size, True, npc_left_funny_15x21px)
            self.sprite_right = Image(self.size, True, npc_right_funny_15x21px)
            self._data_left = npc_left_funny_15x21px
            self._data_right = npc_right_funny_15x21px

        else:
            # Default sprite
            self.sprite = None
            self.sprite_left = None
            self.sprite_right = None

    def __del__(self):
        """Destructor to clean up resources."""
        self._update_new_pos = None
        self._update_direction = None
        self.cent_box_pos = None
        self.cent_box_size = None
        self.cent_box_text = None
        self._draw_pos = None
        self._data_left = None
        self._data_right = None

    def collision(self, other, game):
        """Handles collision with another entity."""
        if self.type != ENTITY_TYPE_ENEMY or other.type != ENTITY_TYPE_PLAYER:
            return  # Only handle collisions between enemies and players

        # Get positions of the enemy and the player
        enemy_pos = self.position
        player_pos = other.position

        # Determine if the enemy is facing the player or player is facing the enemy
        enemy_is_facing_player = False
        player_is_facing_enemy = False

        if (
            (self.direction.x == -1 and player_pos.x < enemy_pos.x)
            or (self.direction.x == 1 and player_pos.x > enemy_pos.x)
            or (self.direction.y == -1 and player_pos.y < enemy_pos.y)
            or (self.direction.y == 1 and player_pos.y > enemy_pos.y)
        ):
            enemy_is_facing_player = True

        if (
            (other.direction.x == -1 and enemy_pos.x < player_pos.x)
            or (other.direction.x == 1 and enemy_pos.x > player_pos.x)
            or (other.direction.y == -1 and enemy_pos.y < player_pos.y)
            or (other.direction.y == 1 and enemy_pos.y > player_pos.y)
        ):

            player_is_facing_enemy = True

        # Handle Player Attacking Enemy (Press OK, facing enemy, and enemy not facing player)
        # we need to store the last button pressed to prevent multiple attacks
        if (
            player_is_facing_enemy
            and game.input == BUTTON_CENTER
            and not enemy_is_facing_player
        ):
            # Reset last button
            game.input = BUTTON_NONE

            # check if enough time has passed since the last attack
            if other.elapsed_attack_timer >= other.attack_timer:
                # Reset player's elapsed attack timer
                other.elapsed_attack_timer = 0
                self.elapsed_attack_timer = (
                    0  # Reset enemy's attack timer to block enemy attack
                )

                # Increase XP by the enemy's strength
                other.xp += self.strength

                # Increase health by 10% of the enemy's strength
                other.health += self.strength * 0.1

                # check max health
                other.health = min(other.health, 100)

                # Decrease enemy health by player strength
                self.health -= other.strength

                # check if enemy is dead
                if self.health > 0:
                    self.state = ENTITY_STATE_ATTACKED
                    self.elapsed_move_timer = 0
                    self.position = self.old_position

                # update multiplayer state after being attacked
                # we must allow both host and clients to sync enemy state after being attacked
                self.sync_multiplayer_state(False, other)

        # Handle Enemy Attacking Player (enemy facing player)
        elif enemy_is_facing_player:
            # check if enough time has passed since the last attack
            if self.elapsed_attack_timer >= self.attack_timer:
                # Reset enemy's elapsed attack timer
                self.elapsed_attack_timer = 0

                # Decrease player health by enemy strength
                other.health -= self.strength

                # check if player is dead
                if other.health > 0:
                    other.state = ENTITY_STATE_ATTACKED
                    other.position = other.old_position

                # update multiplayer state after being attacked
                # we must allow both host and clients to sync enemy state after being attacked
                self.sync_multiplayer_state(False, other)

        # check if player is dead
        if other.health <= 0:
            other.state = ENTITY_STATE_DEAD
            other.position = other.start_position
            other.health = other.max_health

        # check if enemy is dead
        if self.health <= 0:
            self.state = ENTITY_STATE_DEAD
            self.position = Vector(-100, -100)
            self.health = 0
            self.elapsed_move_timer = 0

    def draw_username(self, pos, game):
        """Draws the username or health above the sprite's head."""
        _name = ""
        if self.type == ENTITY_TYPE_ENEMY:
            _name = str(self.health)
        elif self.type == ENTITY_TYPE_NPC:
            _name = "NPC"
        if not _name or not game:
            return

        # Calculate the center of the sprite horizontally
        sprite_center_x = int(pos.x + self.size.x // 2)
        screen_x = int(sprite_center_x - game.position.x)
        screen_y = int(pos.y - game.position.y)

        # Calculate text width using font size
        text_width = int(len(_name) * game.draw.font_size.x)
        font_height = int(game.draw.font_size.y)

        # Calculate box dimensions with padding
        box_padding = 2
        box_width = int(text_width + (box_padding * 2))
        box_height = int(font_height + (box_padding * 2))

        # Position box above the sprite at consistent height
        vertical_offset = int(game.draw.size.x // 22)

        # Center the box horizontally on the sprite
        box_x = int(screen_x - box_width // 2)
        box_y = int(screen_y - vertical_offset)

        # Check if box is within screen bounds
        if box_x < 0 or box_x + box_width > game.draw.size.x:
            return
        if box_y < 0 or box_y + box_height > game.draw.size.y:
            return

        # Draw centered box
        self.cent_box_pos.x = box_x
        self.cent_box_pos.y = box_y
        self.cent_box_size.x = box_width
        self.cent_box_size.y = box_height
        game.draw.fill_rectangle(
            self.cent_box_pos,
            self.cent_box_size,
            0xFFFF,  # white
        )

        # Center the text in the box
        text_x = int(screen_x - text_width // 2)
        text_y = int(box_y + box_padding)
        self.cent_box_text.x = text_x
        self.cent_box_text.y = text_y
        game.draw.text(
            self.cent_box_text,
            _name,
            0x0000,  # black
        )

    def render(self, draw, game):
        """Renders the sprite on the screen."""
        if self.state == ENTITY_STATE_DEAD:
            self.is_visible = False
            return

        # if not on screen, skip rendering
        screen_x = int(self.position.x - game.position.x)
        screen_y = int(self.position.y - game.position.y)
        if (
            screen_x + self.size.x < 0
            or screen_x > game.draw.size.x
            or screen_y + self.size.y < 0
            or screen_y > game.draw.size.y
        ):
            self.is_visible = False
            return

        # Choose sprite and data based on direction
        if self.direction.x == -1 and self.direction.y == 0:  # moving left
            self.sprite = self.sprite_left
            current_data = self._data_left
        elif self.direction.x == 1 and self.direction.y == 0:  # moving right
            self.sprite = self.sprite_right
            current_data = self._data_right
        else:
            current_data = self._data_left  # default

        # Draw sprite explicitly
        if current_data:
            self._draw_pos.x = screen_x
            self._draw_pos.y = screen_y
            draw.image_bytearray(self._draw_pos, self.size, current_data)

        # draw health of enemy
        self.draw_username(self.position, game)

        # Suppress engine auto-draw (sprite already drawn above)
        self.is_visible = False

    def sync_multiplayer_state(self, host_only: bool = True, other: Entity = None):
        """Sync's the sprites data for multiplayer"""
        if not self.flip_world_run or not self.flip_world_run.player:
            return

        if (
            not self.flip_world_run.is_pve_mode  # must be pve mode
            or (
                host_only and not self.flip_world_run.is_lobby_host
            )  # only lobby host can sync enemies
            or self.flip_world_run.player.ws is None  # ws must be set
        ):
            self.flip_world_run.player.ws is None,  # ws must be set
            return

        # send sprite data to server
        self.flip_world_run.sync_multiplayer_entity(self)
        if other is not None:
            # send other entity data to server (e.g., player)
            self.flip_world_run.sync_multiplayer_entity(other)

    def update(self, game):
        """Updates the sprite's position and state."""
        # check if enemy is dead
        if self.state == ENTITY_STATE_DEAD:
            return

        if "npc" in self.name.lower():
            return  # NPCs do not move

        # float delta_time = 1.0 / game->fps;
        delta_time = 1.0 / 30  # 30 frames per second

        # Increment the elapsed_attack_timer for the enemy
        self.elapsed_attack_timer += delta_time

        if self.state == ENTITY_STATE_IDLE:
            # Increment the elapsed_move_timer
            self.elapsed_move_timer += delta_time

            # Check if it's time to move again
            if self.elapsed_move_timer >= self.move_timer:
                # Determine the next state based on the current position
                if (
                    fabs(self.position.x - self.start_position.x) < 1
                    and fabs(self.position.y - self.start_position.y) < 1
                ):
                    self.state = ENTITY_STATE_MOVING_TO_END
                elif (
                    fabs(self.position.x - self.end_position.x) < 1
                    and fabs(self.position.y - self.end_position.y) < 1
                ):
                    self.state = ENTITY_STATE_MOVING_TO_START
                # Reset the elapsed_move_timer
                self.elapsed_move_timer = 0
        elif self.state in (
            ENTITY_STATE_MOVING_TO_END,
            ENTITY_STATE_MOVING_TO_START,
            ENTITY_STATE_ATTACKED,
        ):
            # determine the direction vector
            self._update_direction.x = 0
            self._update_direction.y = 0

            # if attacked, change state to moving based on the direction
            if self.state == ENTITY_STATE_ATTACKED:
                self.state = (
                    ENTITY_STATE_MOVING_TO_END
                    if self.position.x < self.old_position.x
                    else ENTITY_STATE_MOVING_TO_START
                )

            # Determine the target position based on the current state
            target_position = (
                self.end_position
                if self.state == ENTITY_STATE_MOVING_TO_END
                else self.start_position
            )

            # Calculate direction towards the target
            if self.position.x < target_position.x:
                # ENTITY_RIGHT
                self._update_direction.x = 1.0
                self.direction = Vector(1, 0)

            elif self.position.x > target_position.x:
                # ENTITY_LEFT
                self._update_direction.x = -1.0
                self.direction = Vector(-1, 0)

            elif self.position.y < target_position.y:
                # ENTITY_DOWN
                self._update_direction.y = 1.0
                self.direction = Vector(0, 1)

            elif self.position.y > target_position.y:
                # ENTITY_UP
                self._update_direction.y = -1.0
                self.direction = Vector(0, -1)

            # Normalize direction vector
            length = sqrt(
                self._update_direction.x * self._update_direction.x
                + self._update_direction.y * self._update_direction.y
            )
            if length != 0:
                self._update_direction.x /= length
                self._update_direction.y /= length

            # Update position based on direction and speed
            self._update_new_pos.x = self.position.x
            self._update_new_pos.y = self.position.y
            self._update_new_pos.x += self._update_direction.x * self.speed * delta_time
            self._update_new_pos.y += self._update_direction.y * self.speed * delta_time

            # Clamp the position to the target to prevent overshooting
            if (
                self._update_direction.x > 0
                and self._update_new_pos.x > target_position.x
            ) or (
                self._update_direction.x < 0
                and self._update_new_pos.x < target_position.x
            ):
                self._update_new_pos.x = target_position.x

            if (
                self._update_direction.y > 0
                and self._update_new_pos.y > target_position.y
            ) or (
                self._update_direction.y < 0
                and self._update_new_pos.y < target_position.y
            ):
                self._update_new_pos.y = target_position.y

            # Set the new position
            self.position = self._update_new_pos

            # Sync multiplayer state
            if self.has_changed_position():
                self.sync_multiplayer_state()

            # Check if the enemy has reached or surpassed the target_position
            reached_x = fabs(self._update_new_pos.x - target_position.x) < 1
            reached_y = fabs(self._update_new_pos.y - target_position.y) < 1

            if reached_x and reached_y:
                # Set the state to idle
                self.state = ENTITY_STATE_IDLE
                self.elapsed_move_timer = 0
