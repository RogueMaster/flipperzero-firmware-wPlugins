#pragma once

#include <gui/view.h>

typedef struct StrataHeroSplashScreen StrataHeroSplashScreen;

typedef void (*StrataHeroSplashScreenAdvanceCallback)(void*);

StrataHeroSplashScreen* stratahero_splash_screen_alloc();
void stratahero_splash_screen_free(StrataHeroSplashScreen* screen);
View* stratahero_splash_screen_get_view(StrataHeroSplashScreen* screen);

void stratahero_splash_screen_set_advance_callback(
    StrataHeroSplashScreen* screen,
    StrataHeroSplashScreenAdvanceCallback callback,
    void* context);
