#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FNApp FNApp;

/** Создать приложение; NULL — нет памяти или порт UART занят. */
FNApp* fn_test_app_alloc(void);

/** Освободить приложение (можно вызывать с частично инициализированным FNApp). */
void fn_test_app_free(FNApp* app);

#ifdef __cplusplus
}
#endif
