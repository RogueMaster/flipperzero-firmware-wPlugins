// clang-format off
// X-macro scene list: the single source of truth for GurpilScene's enumerators and for the
// on_enter/on_event/on_exit handler arrays gurpil_scene.c generates. Add a scene by adding one
// line here and a matching gurpil_scene_<name>_on_{enter,event,exit} triplet in src/scenes/.
ADD_SCENE(gurpil, menu, Menu)
ADD_SCENE(gurpil, game, Game)
ADD_SCENE(gurpil, how_to_play, HowToPlay)
ADD_SCENE(gurpil, credits, Credits)
