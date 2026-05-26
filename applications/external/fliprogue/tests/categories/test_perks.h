#pragma once

static void open_test_line(FrGame* game) {
    for(uint8_t x = 4; x <= 10; x++) fr_set_terrain(game, x, 5, FR_TERR_FLOOR);
    fr_update_fov(game);
}

static void test_v12_warrior_perks_have_gameplay_effects(void) {
    FrGame heart;
    make_empty_test_room(&heart);
    heart.player.pending_perks = 1;
    heart.player.hp = 10;
    heart.player.max_hp = 24;
    assert(fr_apply_perk(&heart, 0));
    assert(heart.player.max_hp == 29);
    assert(heart.player.hp == 15);

    FrGame bash;
    make_empty_test_room(&bash);
    bash.seed = 1975u;
    bash.player.perks = FR_PERK_2;
    FrActor* ogre = fr_spawn_actor(&bash, FR_MON_OGRE, 6, 5);
    assert(ogre != NULL);
    assert(fr_move_player(&bash, 1, 0).kind == FR_ACTION_ATTACK);
    assert(ogre->hp <= 6);

    FrGame riposte;
    make_empty_test_room(&riposte);
    riposte.seed = 1975u;
    riposte.player.hp = riposte.player.max_hp;
    riposte.player.perks = FR_PERK_3;
    FrActor* goblin = fr_spawn_actor(&riposte, FR_MON_GOBLIN, 6, 5);
    assert(goblin != NULL);
    assert(fr_rest(&riposte).kind == FR_ACTION_REST);
    assert(goblin->hp == 5);

    FrGame cleave;
    make_empty_test_room(&cleave);
    cleave.player.perks = FR_PERK_4;
    FrActor* snake = fr_spawn_actor(&cleave, FR_MON_SNAKE, 6, 5);
    FrActor* near = fr_spawn_actor(&cleave, FR_MON_OGRE, 7, 5);
    assert(snake != NULL && near != NULL);
    cleave.seed = 1u;
    assert(fr_move_player(&cleave, 1, 0).kind == FR_ACTION_ATTACK);
    assert(!snake->active);
    assert(near->hp == 13);

    FrGame hold;
    make_empty_test_room(&hold);
    hold.player.perks = FR_PERK_5;
    hold.player.shield_lvl = 1;
    assert(fr_spawn_actor(&hold, FR_MON_OGRE, 6, 5) != NULL);
    assert(fr_spawn_actor(&hold, FR_MON_GOBLIN, 5, 6) != NULL);
    assert(fr_player_block(&hold) == 2);
}

static void test_v12_ranger_perks_have_gameplay_effects(void) {
    FrGame pin;
    make_empty_test_room(&pin);
    open_test_line(&pin);
    fr_set_terrain(&pin, 9, 5, FR_TERR_WALL);
    fr_update_fov(&pin);
    pin.player.class_id = FR_CLASS_RANGER;
    pin.player.dex = 6;
    pin.player.bow_lvl = 1;
    pin.player.arrows = 3;
    pin.player.perks = FR_PERK_1;
    pin.seed = 1975u;
    FrActor* ogre = fr_spawn_actor(&pin, FR_MON_OGRE, 8, 5);
    assert(ogre != NULL);
    assert(fr_try_direction(&pin, 1, 0).kind == FR_ACTION_RANGED);
    assert(ogre->hp <= 8);

    FrGame slow;
    make_empty_test_room(&slow);
    open_test_line(&slow);
    slow.player.class_id = FR_CLASS_RANGER;
    slow.player.dex = 6;
    slow.player.bow_lvl = 1;
    slow.player.arrows = 3;
    slow.player.perks = FR_PERK_2;
    slow.seed = 1975u;
    ogre = fr_spawn_actor(&slow, FR_MON_OGRE, 8, 5);
    assert(ogre != NULL);
    assert(fr_try_direction(&slow, 1, 0).kind == FR_ACTION_RANGED);
    assert((ogre->effects & FR_FX_SLOWED) != 0);

    FrGame trap_eye;
    make_empty_test_room(&trap_eye);
    trap_eye.player.class_id = FR_CLASS_RANGER;
    trap_eye.player.dex = 6;
    trap_eye.player.pending_perks = 1;
    uint8_t before_chance = fr_hidden_trap_detection_chance(&trap_eye);
    assert(fr_apply_perk(&trap_eye, 2));
    assert(trap_eye.player.dex == 7);
    assert(fr_hidden_trap_detection_chance(&trap_eye) == before_chance + 5);

    FrGame vantage;
    make_empty_test_room(&vantage);
    open_test_line(&vantage);
    vantage.player.class_id = FR_CLASS_RANGER;
    vantage.player.dex = 6;
    vantage.player.bow_lvl = 1;
    vantage.player.arrows = 3;
    vantage.player.perks = FR_PERK_4;
    ogre = fr_spawn_actor(&vantage, FR_MON_OGRE, 9, 5);
    assert(ogre != NULL);
    assert(fr_try_direction(&vantage, 1, 0).kind == FR_ACTION_RANGED);
    assert(ogre->hp == 8);

    FrGame dart_hand;
    FrGame plain_throw;
    make_empty_test_room(&dart_hand);
    make_empty_test_room(&plain_throw);
    dart_hand.player.class_id = FR_CLASS_RANGER;
    dart_hand.player.perks = FR_PERK_5;
    plain_throw.player.class_id = FR_CLASS_RANGER;
    dart_hand.seed = 7u;
    plain_throw.seed = 7u;
    FrActor* strong = fr_spawn_actor(&dart_hand, FR_MON_OGRE, 7, 5);
    FrActor* plain = fr_spawn_actor(&plain_throw, FR_MON_OGRE, 7, 5);
    assert(strong != NULL && plain != NULL);
    assert(fr_add_inventory(&dart_hand, FR_ITEM_THROWABLE, FR_THROW_STONE, 1));
    assert(fr_add_inventory(&plain_throw, FR_ITEM_THROWABLE, FR_THROW_STONE, 1));
    assert(fr_use_inventory(&plain_throw, 0, FR_USE_THROW, 7, 5).kind == FR_ACTION_ZAP);
    assert(fr_use_inventory(&dart_hand, 0, FR_USE_THROW, 7, 5).kind == FR_ACTION_ZAP);
    assert(strong->hp + 1 == plain->hp);
}

static void test_v12_mage_perks_have_gameplay_effects(void) {
    FrGame overcharge;
    make_empty_test_room(&overcharge);
    open_test_line(&overcharge);
    overcharge.player.class_id = FR_CLASS_MAGE;
    overcharge.player.wil = 6;
    overcharge.player.staff_lvl = 1;
    overcharge.player.charges = 3;
    overcharge.player.perks = FR_PERK_1;
    overcharge.seed = 1975u;
    FrActor* ogre = fr_spawn_actor(&overcharge, FR_MON_OGRE, 8, 5);
    assert(ogre != NULL);
    assert(fr_try_direction(&overcharge, 1, 0).kind == FR_ACTION_RANGED);
    assert(ogre->hp == 4);

    FrGame ember;
    make_empty_test_room(&ember);
    open_test_line(&ember);
    ember.player.class_id = FR_CLASS_MAGE;
    ember.player.wil = 6;
    ember.player.staff_lvl = 1;
    ember.player.charges = 3;
    ember.player.perks = FR_PERK_2;
    ember.seed = 1975u;
    ogre = fr_spawn_actor(&ember, FR_MON_OGRE, 8, 5);
    assert(ogre != NULL);
    assert(fr_try_direction(&ember, 1, 0).kind == FR_ACTION_RANGED);
    assert((ogre->effects & FR_FX_BURNING) != 0);

    FrGame frost;
    make_empty_test_room(&frost);
    open_test_line(&frost);
    frost.player.class_id = FR_CLASS_MAGE;
    frost.player.wil = 6;
    frost.player.staff_lvl = 1;
    frost.player.charges = 3;
    frost.player.perks = FR_PERK_3;
    frost.seed = 1975u;
    ogre = fr_spawn_actor(&frost, FR_MON_OGRE, 8, 5);
    assert(ogre != NULL);
    assert(fr_try_direction(&frost, 1, 0).kind == FR_ACTION_RANGED);
    assert((ogre->effects & FR_FX_SLOWED) != 0);

    FrGame quartz;
    make_empty_test_room(&quartz);
    quartz.player.class_id = FR_CLASS_MAGE;
    quartz.player.pending_perks = 1;
    quartz.player.charges = 5;
    assert(fr_apply_perk(&quartz, 3));
    assert(quartz.player.charges == 7);
    quartz.turn = 4;
    assert(fr_rest(&quartz).kind == FR_ACTION_REST);
    assert(quartz.player.charges == 8);

    FrGame veil;
    make_empty_test_room(&veil);
    veil.player.class_id = FR_CLASS_MAGE;
    veil.player.pending_perks = 1;
    assert(fr_apply_perk(&veil, 4));
    assert((veil.player.perks & FR_PERK_5) != 0);
    assert(strcmp(fr_perk_desc(FR_CLASS_MAGE, 4), "magic hides you") == 0);
}
