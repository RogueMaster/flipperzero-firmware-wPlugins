#ifndef __CUBERZERO_H__
#define __CUBERZERO_H__

#include "src/session.h"
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

#define CUBERZERO_TAG     "CuberZero"
#define CUBERZERO_VERSION "0.0.1"

#define CUBERZERO_INFO(text, ...) \
    furi_log_print_format(FuriLogLevelInfo, CUBERZERO_TAG, text, ##__VA_ARGS__)
#define CUBERZERO_ERROR(text, ...) \
    furi_log_print_format(FuriLogLevelError, CUBERZERO_TAG, text, ##__VA_ARGS__)

typedef struct {
    struct {
        Submenu* submenu;
    } view;

    Gui* interface;
    ViewDispatcher* dispatcher;
    SceneManager* manager;
    struct {
        struct {
            uint32_t index;
        } home;
    } scene;

    SESSION session;
} CUBERZERO, *PCUBERZERO;

typedef enum {
    VIEW_SUBMENU,
    COUNT_VIEW
} VIEW;

typedef enum {
    SCENE_HOME,
    SCENE_SESSION,
    SCENE_TIMER,
    COUNT_SCENE
} SCENE;

typedef enum {
    SESSION_SUCCESS,
    SESSION_OPEN_FAILED,
    SESSION_INVALID_FORMAT
} SESSIONRESULT;

struct ViewDispatcher {
    bool eventLoopOwned;
    FuriEventLoop* eventLoop;
    FuriMessageQueue* queueInput;
    FuriMessageQueue* queueEvent;
    Gui* interface;
    ViewPort* viewport;
};

#ifdef __cplusplus
extern "C" {
#endif
void SceneHomeEnter(void* const context);
void SceneSessionEnter(void* const context);
void SceneTimerEnter(void* const context);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
bool SceneHomeEvent(void* const context, const SceneManagerEvent event);
#ifdef __cplusplus
}
#endif
#endif
