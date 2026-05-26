#include "game_internal.h"

FrActionResult fr_use_inventory(FrGame* game, uint8_t index, uint8_t use_action, uint8_t tx, uint8_t ty) {
    game->log[0] = '\0';
    game->last_event = FR_EVENT_NONE;
    if(index >= game->player.inv_count) {
        fr_log(game, "No item.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }

    FrInvSlot slot = game->player.inv[index];
    if(use_action == FR_USE_DROP) {
        bool ground_full = fr_item_at_xy(game, game->player.x, game->player.y);
        if(fr_drop_inventory_item(game, index)) {
            fr_log(game, "Dropped %s.", fr_inventory_label(game, &slot));
            fr_finish_item_turn(game);
            return (FrActionResult){FR_ACTION_USE};
        }
        fr_log(game, ground_full ? "Ground full." : "No room.");
        return (FrActionResult){FR_ACTION_BLOCKED};
    }

    if(slot.type == FR_ITEM_TRINKET && use_action == FR_USE_EQUIP) {
        if(fr_equip_trinket(game, index)) {
            fr_finish_item_turn(game);
            return (FrActionResult){FR_ACTION_USE};
        }
        return (FrActionResult){FR_ACTION_BLOCKED};
    }

    if(slot.type == FR_ITEM_POTION && use_action == FR_USE_QUAFF) {
        fr_apply_potion_to_player(game, slot.subtype);
        fr_consume_inventory(game, index);
        fr_finish_item_turn(game);
        return (FrActionResult){FR_ACTION_USE};
    }

    if(slot.type == FR_ITEM_POTION && use_action == FR_USE_THROW) {
        if(game->player.cube_hp > 0) {
            fr_log(game, "Cube holds you.");
            return (FrActionResult){FR_ACTION_BLOCKED};
        }
        fr_throw_potion_at(game, slot.subtype, tx, ty);
        fr_consume_inventory(game, index);
        fr_finish_item_turn(game);
        return (FrActionResult){FR_ACTION_USE};
    }

    if(slot.type == FR_ITEM_SCROLL && use_action == FR_USE_READ) {
        if(slot.subtype == FR_SCROLL_BLINK && !fr_resolve_blink_destination(game, tx, ty, &tx, &ty)) {
            fr_log(game, "Nothing happens.");
            return (FrActionResult){FR_ACTION_BLOCKED};
        }
        bool success = true;
        if(slot.subtype == FR_SCROLL_FIRE) {
            fr_fire_burst(game, tx, ty);
        } else if(slot.subtype == FR_SCROLL_ENCHANT_WEAPON) {
            if(game->player.class_id == FR_CLASS_RANGER && game->player.bow_lvl < 3) {
                game->player.bow_lvl++;
                fr_log(game, "Weapon +.");
            } else if(game->player.class_id == FR_CLASS_MAGE && game->player.staff_lvl < 3) {
                game->player.staff_lvl++;
                fr_log(game, "Weapon +.");
            } else if(game->player.sword_lvl < 3) {
                game->player.sword_lvl++;
                fr_log(game, "Weapon +.");
            } else {
                success = false;
            }
        } else if(slot.subtype == FR_SCROLL_ENCHANT_ARMOR) {
            if(game->player.body_lvl < 3) {
                game->player.body_lvl++;
                fr_log(game, "Armor +.");
            } else {
                success = false;
            }
        } else if(slot.subtype == FR_SCROLL_IDENTIFY) {
            bool identified = false;
            if(ty == 0xFF && tx < game->player.inv_count && tx != index) {
                identified = fr_identify_inventory_slot(game, tx);
            }
            if(!identified) identified = fr_identify_one_unknown(game, index);
            success = identified;
            if(identified) fr_log(game, "One name settles.");
        } else if(slot.subtype == FR_SCROLL_BLINK) {
            game->player.x = tx;
            game->player.y = ty;
            fr_log(game, "Blink.");
        } else if(slot.subtype == FR_SCROLL_MAPPING) {
            for(uint8_t y = 0; y < FR_MAP_H; y++) {
                for(uint8_t x = 0; x < FR_MAP_W; x++) game->tiles[y][x] |= FR_TILE_EXPLORED;
            }
            fr_reveal_secrets(game, game->player.x, game->player.y, 63);
            fr_log(game, "Map wakes.");
        } else if(slot.subtype == FR_SCROLL_FEAR) {
            FrActor* actor = fr_actor_at(game, tx, ty);
            if(actor) {
                fr_apply_effect_to_actor(actor, FR_FX_AFRAID, FR_FX_AFRAID_INDEX, 8);
                fr_log(game, "Fear.");
            } else {
                success = false;
            }
        } else if(slot.subtype == FR_SCROLL_CHARGE) {
            bool charged = false;
            for(uint8_t i = 0; i < game->player.inv_count; i++) {
                if(i != index && game->player.inv[i].type == FR_ITEM_WAND) {
                    uint8_t max = game->player.inv[i].flags ? game->player.inv[i].flags : fr_wand_max_charges(game->player.inv[i].subtype);
                    if(game->player.inv[i].amount < max) {
                        game->player.inv[i].amount = max;
                        game->player.inv[i].flags = max;
                        charged = true;
                        break;
                    }
                }
            }
            if(!charged) game->player.charges = (uint8_t)(game->player.charges + 2);
            fr_log(game, charged ? "Wand charged." : "Charge.");
        } else if(slot.subtype == FR_SCROLL_RANDOM_TELEPORT) {
            uint8_t x = game->player.x;
            uint8_t y = game->player.y;
            if(fr_phase_shift_tile_reveal(game, &x, &y, 120)) {
                game->player.x = x;
                game->player.y = y;
                fr_log(game, "World shifts.");
            } else {
                success = false;
            }
        } else if(slot.subtype == FR_SCROLL_REVEAL) {
            bool found = fr_reveal_secrets(game, game->player.x, game->player.y, 8);
            if(found) fr_log(game, "Secrets wake.");
            else success = false;
        } else if(slot.subtype == FR_SCROLL_DECURSE) {
            bool cleared = false;
            for(uint8_t i = 0; i < game->player.inv_count; i++) {
                if(game->player.inv[i].type == FR_ITEM_TRINKET && (game->player.inv[i].flags & FR_INV_CURSED) != 0) {
                    game->player.inv[i].flags &= (uint8_t)~FR_INV_CURSED;
                    cleared = true;
                    break;
                }
            }
            if(cleared) fr_log(game, "Curse cracks.");
            else success = false;
        } else if(slot.subtype == FR_SCROLL_CALL) {
            for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
                if(game->actors[i].active) fr_wake_actor_toward_player(game, &game->actors[i]);
            }
            fr_log(game, "All hear you.");
        }
        if(!success) {
            fr_log(game, "Nothing happens.");
            return (FrActionResult){FR_ACTION_BLOCKED};
        }
        fr_mark_known_scroll(game, slot.subtype);
        if(game->log[0] == '\0') fr_log(game, "Scroll fades.");
        fr_consume_inventory(game, index);
        fr_finish_item_turn(game);
        return (FrActionResult){FR_ACTION_USE};
    }

    if(slot.type == FR_ITEM_THROWABLE && use_action == FR_USE_THROW) {
        if(game->player.cube_hp > 0) {
            fr_log(game, "Cube holds you.");
            return (FrActionResult){FR_ACTION_BLOCKED};
        }
        FrActor* actor = fr_actor_at(game, tx, ty);
        uint8_t dmg = slot.subtype == FR_THROW_DART ? (uint8_t)(3 + fr_rand_u8(game, 3)) : (uint8_t)(1 + fr_rand_u8(game, 2));
        if(game->player.class_id == FR_CLASS_RANGER && (game->player.perks & FR_PERK_5) != 0) dmg++;
        if(actor) fr_damage_actor_kind(game, actor, dmg, slot.subtype == FR_THROW_DART ? "Dart hits" : "Stone hits", FR_DAMAGE_THROWN);
        else fr_log(game, "Clatters.");
        fr_consume_inventory(game, index);
        fr_finish_item_turn(game);
        return (FrActionResult){FR_ACTION_ZAP};
    }

    if(slot.type == FR_ITEM_WAND && use_action == FR_USE_ZAP) {
        if(slot.amount == 0) {
            fr_log(game, "No charges.");
            return (FrActionResult){FR_ACTION_BLOCKED};
        }
        FrActor* actor = fr_actor_at(game, tx, ty);
        if(slot.subtype == FR_WAND_FIRE) {
            if(actor) {
                fr_damage_actor_kind(game, actor, 2, "Fire bolt", FR_DAMAGE_PROJECTILE);
                if(actor->active) fr_apply_effect_to_actor(actor, FR_FX_BURNING, FR_FX_BURNING_INDEX, 8);
            }
            fr_log(game, "Fire bolt.");
        } else if(slot.subtype == FR_WAND_FROST) {
            if(actor) fr_apply_effect_to_actor(actor, FR_FX_SLOWED, FR_FX_SLOWED_INDEX, 10);
            fr_log(game, "Frost ray.");
        } else if(slot.subtype == FR_WAND_FORCE) {
            if(actor) {
                int8_t dx = fr_sign_i8((int16_t)actor->x - (int16_t)game->player.x);
                int8_t dy = fr_sign_i8((int16_t)actor->y - (int16_t)game->player.y);
                fr_force_push_actor(game, actor, dx, dy);
            } else {
                fr_log(game, "Force snaps.");
            }
        } else if(slot.subtype == FR_WAND_BLINK) {
            uint8_t bx = tx;
            uint8_t by = ty;
            if(fr_resolve_blink_destination(game, tx, ty, &bx, &by)) {
                game->player.x = bx;
                game->player.y = by;
            }
            fr_log(game, "Blink wand.");
        } else if(slot.subtype == FR_WAND_VENOM) {
            if(actor) fr_apply_effect_to_actor(actor, FR_FX_POISONED, FR_FX_POISONED_INDEX, 8);
            fr_log(game, "Venom ray.");
        } else if(slot.subtype == FR_WAND_FEAR) {
            if(actor) fr_apply_effect_to_actor(actor, FR_FX_AFRAID, FR_FX_AFRAID_INDEX, 8);
            fr_log(game, "Fear ray.");
        } else if(slot.subtype == FR_WAND_ARC) {
            if(actor) {
                fr_damage_actor_kind(game, actor, 2, "Arc hits", FR_DAMAGE_PROJECTILE);
                for(int8_t oy = -1; oy <= 1; oy++) {
                    for(int8_t ox = -1; ox <= 1; ox++) {
                        if(ox == 0 && oy == 0) continue;
                        int16_t ax = (int16_t)tx + ox;
                        int16_t ay = (int16_t)ty + oy;
                        if(ax < 0 || ay < 0 || ax >= FR_MAP_W || ay >= FR_MAP_H) continue;
                        FrActor* near = fr_actor_at(game, (uint8_t)ax, (uint8_t)ay);
                        if(near) fr_damage_actor_kind(game, near, 1, "Arc jumps", FR_DAMAGE_BURST);
                    }
                }
            }
            fr_log(game, "Arc.");
        } else if(slot.subtype == FR_WAND_REVEAL) {
            bool found = fr_reveal_secrets(game, tx, ty, 2);
            fr_log(game, found ? "Revealed." : "Nothing.");
        }
        if(index < game->player.inv_count) {
            if(game->player.inv[index].amount > 0) game->player.inv[index].amount--;
        }
        fr_finish_item_turn(game);
        return (FrActionResult){FR_ACTION_ZAP};
    }

    fr_log(game, "Can't use that.");
    return (FrActionResult){FR_ACTION_BLOCKED};
}
