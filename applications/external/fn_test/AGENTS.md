# AGENTS.md — FN Tester (fn_test)

> **Обязательно:** при любой задаче по разработке, ревью, отладке или тестированию
> этого приложения сначала загружай скилл **`flipper-zero-app-dev`**
> (`.opencode/skills/flipper-zero-app-dev/SKILL.md`). В нём — актуальные
> соглашения по FAP, манифесту `application.fam`, Furi API, GUI/SceneManager,
> потокам, памяти и тестам на Flipper Zero. Если скилл недоступен, следуй
> правилам из этого файла.
>
> Для любых работ с ФН (команды, кадры, разбор ответов) дополнительно загружай
> скилл **`fn-protocol`** — Протокол v1.2.05, golden-кадры, коды ответов.

## О проекте

Внешнее приложение (FAP) для **Flipper Zero**: тестер фискальных накопителей (ФН),
применяемых в кассовой технике (Россия). Приложение подключается к ФН по UART,
отправляет команды протокола ФН и показывает понятным текстом (английский по
умолчанию, русский — по выбору пользователя):

- информацию о ФН (серийный номер, этап жизни, состояние смены, документ для
  ОФД, версия/тип ФФД, версия ПО, МГМ или боевой ФН, флаги предупреждений);
- информацию о жизненном цикле (дата окончания, число и остаток регистраций,
  остаток дней, ресурс памяти, процент маркировки);
- сброс МГМ (команда `60 16`) с подтверждением и проверкой «МГМ/боевой»;
- справочные экраны: распиновка и «О приложении».

| Параметр | Значение |
|---|---|
| `appid` | `fn_test` |
| Точка входа | `fn_test_app` (`fn_test_app.c`) |
| Тип | `FlipperAppType.EXTERNAL` |
| Категория | `GPIO` |
| Версия | `(0, 6)` — см. `application.fam` и `.catalog/CHANGELOG.md` |
| Стек приложения | `4 * 1024` |
| Приватная библиотека | `lib/fn` (`Lib(name="fn")`) |
| Иконки | `logo_fn.png` (иконка FAP), `icons/` (`fap_icon_assets`) |

Это **самостоятельный репозиторий приложения**, а не каталог дерева прошивки.
Корень репозитория — одновременно каталог приложения.

## Структура

```
flipper-zero_fn_tester/
├── application.fam            # манифест FAP
├── fn_test_app.c              # entry point, alloc/free, ViewDispatcher
├── fn_test_app_i.h            # структура FNApp, enum'ы view и custom-событий
├── fn_test_app.h              # публичный typedef FNApp
├── fn_settings.c/.h           # язык интерфейса в /data/fn_test/settings.txt
├── lib/fn/                    # приватная библиотека (не знает про UI)
│   ├── fn.h                   # umbrella-заголовок для ui/ и fn_test_app.c
│   ├── core/                  # ЧИСТОЕ ядро, без furi (host-тестируемое)
│   │   ├── fn_lang.h          # FnLang: английский (по умолчанию) и русский
│   │   ├── fn_types.h/.c      # Date/DateTime, FnStage, FnFfd, FnDocCode, FnWarnFlags
│   │   ├── fn_wire.h/.c       # u16le/u32le get/put, crc16_ccitt (число, без swap)
│   │   ├── fn_frame.h/.c      # сборка/разбор кадра, FnFrameStatus
│   │   └── fn_error.h/.c      # каталог кодов ответа + title/hint (EN/RU)
│   ├── proto/                 # коды команд и парсеры ответов (без UART/GUI)
│   │   ├── fn_commands.h      # коды команд 30h/32h/33h/3Ah/3Bh/3Dh/10h/60h
│   │   ├── fn_status.h/.c     # 30h, 10h (+ FnInfo)
│   │   ├── fn_ident.h/.c      # 3Ah, 33h
│   │   └── fn_life.h/.c       # 32h, 3Bh, 3Dh (+ FnLifeInfo)
│   ├── transport/             # UART: HAL serial, DMA-RX, таймауты, отмена
│   │   ├── fn_uart.h/_i.h
│   │   └── fn_uart.c
│   ├── fn_worker.h/.c/_i.h    # одна активная операция, дедлайны, события, отмена
│   └── fn_ops.c               # сценарии: detect, read info, read life, flash MGM
├── ui/                        # интерфейс (сцены и view)
│   ├── fn_ui.h                # константы вёрстки 128×64 и лимиты модели
│   ├── fn_font.h/.c           # сгенерированный u8g2-шрифт с кириллицей
│   ├── fn_draw.h/.c           # отрисовка UTF-8 строк с переносом по словам
│   ├── fn_strings.h/.c        # ВСЕ пользовательские строки (EN/RU, fn_str)
│   ├── fn_text_model.h        # FnTextModel/секции/строки — только данные
│   ├── fn_format.h/.c         # заполнение модели экранов (host-тестируемо)
│   ├── views/                 # fn_view_menu / fn_view_wait / fn_view_text / fn_view_confirm / fn_view_language
│   └── scenes/                # fn_scene.c/.h, fn_scene_config.h, fn_scene_*.c
├── unit_tests/                # on-device minunit (профиль FN_TESTS=1)
│   └── lib_tests/fn/{core,proto,transport,worker}/
├── tests/host/                # host-тесты чистого ядра (make, cc)
├── icons/                     # ассеты fap_icon_assets (анимация, схемы)
├── docs/                      # protocol.md, byte-order.md, ui.md
├── specs/                     # Spec Kit: спецификация, план, задачи, baseline
├── .test_scripts/             # запуск тестов и ожидание Flipper по serial
├── .catalog/                  # описание и CHANGELOG для каталога приложений
├── .github/ISSUE_TEMPLATE/    # шаблоны issue
├── .specify/                  # Spec Kit (шаблоны, конституция, скрипты)
└── .opencode/commands/        # слэш-команды /speckit.*
```

## Архитектура

Слои (границы проверяются `make -C tests/host check-layers`):

```
core/          чистая арифметика: байты, кадр, CRC, коды ответа, типы (без furi)
  ↑
proto/         парсеры ответов и коды команд (без UART, без GUI)
  ↑
transport/     UART + DMA + таймауты + отмена (только furi_hal_serial)
  ↑
worker/        fn_worker.c + fn_ops.c: одна операция, дедлайн, событие
  ↑
ui/            сцены и view; знают только fn_worker API и FnTextModel
```

- `fn_test_app_alloc()` открывает `RECORD_GUI` и `RECORD_NOTIFICATION`, создаёт
  `ViewDispatcher` (`ViewDispatcherTypeFullscreen`), `SceneManager`, `FNWorker`,
  четыре view и модели `FnInfo`/`FnLifeInfo`; `fn_test_app_free()` освобождает
  в обратном порядке (`fn_worker_free` — только если воркер не занят).
- Навигация — только через `SceneManager`; экраны — собственные `View` из
  `ui/views/` (меню, ожидание, текст, подтверждение). Стандартные модули
  (`submenu`, `widget`, `popup`) не используются, `dialogs` в `requires` нет.
- `FNWorker` (`lib/fn/fn_worker.c`) — один постоянно живущий поток
  (стек `10240`), одна активная операция с монотонным `op_id`, дедлайны
  `FN_OP_DEADLINE_READ_MS` (10 с) и `FN_OP_DEADLINE_FLASH_MS` (30 с),
  результат — ровно одно событие `FNAppCustomEventWorkerResult`.
- UART (`lib/fn/transport/fn_uart.c`): release — `FuriHalSerialIdUsart`
  (пины 13/14) с отключением определения модулей расширения; `FURI_DEBUG` —
  `FuriHalSerialIdLpuart`; скорость `115200`, формат кадра — `fn_frame.h`.
  RX — DMA в один `FuriStreamBuffer` ёмкостью `FN_FRAME_MAX_LEN`, ожидание —
  one-shot `FuriTimer`, 100 мс.
- Данные ФН — `FnInfo` / `FnLifeInfo` (`proto/`), ошибки — `FNError`
  (`core/fn_error.*`), показ экранов — `ui/fn_format.c` → `FnTextModel`.
- Новая сцена — `ui/scenes/fn_scene_config.h` (`ADD_SCENE`); новое view —
  `enum FNAppView` (`fn_test_app_i.h`) и `view_dispatcher_add_view`.
- Все пользовательские строки — только в `ui/fn_strings.c` (выбор через
  `fn_str(lang, FnStrId)`); в сценах и view литералов пользовательского текста
  нет. Язык хранится в `FNApp.lang` и сохраняется `fn_settings.c`.

### ⚠️ Ловушка байтового порядка

В старом коде (`lib/fn/fn_helpers.c`, удалён в R1) имена были противоположны
смыслу: `two_uint8t_to_uint16t_LE()` читала **big-endian**, `calc_crc16()`
переставляла байты результата и вычитала единицу из верхней границы. **Не
«чинить» функцию по её имени** — это ломает CRC и все вызовы. Порядок байтов
задаётся только именами `fn_wire_u16le_get/put`, `fn_wire_u32le_get/put`,
`fn_wire_crc16_ccitt()` (число без перестановки); кадр — только через
`fn_frame_build`/`fn_frame_parse`. Подробности и запреты — `docs/byte-order.md`.

## Сборка (только `ufbt`)

Проект собирается **исключительно** `ufbt` (uFBT). Не использовать `./fbt`,
`scons` и файлы сборки из дерева прошивки.

```bash
cd /Users/igordanilov/CLionProjects/flipper/flipper-zero_fn_tester

ufbt                  # собрать приложение из текущего каталога
ufbt APPID=fn_test    # явно указать appid
ufbt FORCE=1          # принудительная пересборка
ufbt VERBOSE=1        # полные команды компиляции/линковки
ufbt cdb              # compile_commands.json для CLion/clangd
ufbt format           # clang-format (.clang-format в корне)
ufbt lint             # статический анализ
ufbt update           # обновить SDK/toolchain (release / -c dev / -c rc)
```

Результат: `dist/fn_test.fap`. SDK и toolchain лежат в `~/.ufbt/current`
(локально установлен канал `release`).

## Запуск и логи

```bash
ufbt launch                                  # собрать, залить и запустить по USB
ufbt launch FLIP_PORT=/dev/cu.usbmodemflip_Rogour1   # если подключено несколько
ufbt cli                                     # CLI-сессия по USB
```

В CLI Flipper полезно: `log debug`, `log`, `free`, `top`, `device_info`, `help`.

## Тесты

Два уровня: host-тесты чистого ядра (ПК) и on-device minunit (сам Flipper).
Каждая новая или изменённая функция `core/`/`proto/` получает host-тест, а
новая функция библиотеки — ещё и `MU_TEST` в реестре.

### Host-тесты (`tests/host/`)

```bash
make -C tests/host              # все host-тесты (cc -std=c11 -Wall -Wextra -Werror)
make -C tests/host asan         # то же под ASan/UBSan
make -C tests/host check-layers # границы слоёв (grep-правила)
make -C tests/host clean
```

- Компилируются `lib/fn/core/*.c`, `lib/fn/proto/*.c`, `ui/fn_format.c` и
  `ui/fn_strings.c`; включать `ui/views`, `ui/scenes`, `ui/fn_font.*` и любые
  пути SDK запрещено.
- `tests/host/golden_frames.h` — единственный источник истины для кадров
  Протокола (используется и в on-device тестах).
- `tests/host/check_layers.sh`: в `core/` и `proto/` нет `furi`/`gui/`;
  в `ui/` нет `fn_uart`, `fn_frame`, `fn_parse_`, `fn_wire`, `FN_CMD`;
  в `lib/fn` нет `fn_test_app`, `scene_`, `view_`.

### On-device тесты (`unit_tests/`)

- `unit_tests/minunit.h`, `minunit_vars*.h` — фреймворк minunit (не менять без
  необходимости).
- `unit_tests/test_index.c` — реестр наборов, `fn_register_tests()` /
  `fn_unregister_tests()`: регистрируют CLI-команду `fn_test`.
- `unit_tests/lib_tests/fn/{core,proto,transport,worker}/` — наборы
  `fn_types`, `fn_wire`, `fn_frame`, `fn_error`, `fn_status`, `fn_ident`,
  `fn_life`, `fn_uart`, `fn_worker`.
- `unit_tests/base_test_vars.h`, `fn_mgm_test_params.h` — тестовые константы
  (серийник/ПО МГМ).
- `.test_scripts/units.py` — прогон тестов на устройстве;
  `.test_scripts/await_flipper.py` — ожидание Flipper по serial.

Тестовый профиль — `FN_TESTS=1`: `application.fam` добавляет `-DFN_TESTS`,
включает `unit_tests/**` и CLI-команду `fn_test`. В обычной сборке ни тестов,
ни команды нет — это ожидаемо.

В тестовом профиле:

- `fn_register_tests(app->worker)` переиспользует воркер и UART запущенного
  приложения (порт уже занят), консоль VCP не отключается;
- разрушительные сценарии запрещены: автотесты **не отправляют `60h`**
  (инцидент Фазы 5, `specs/001-fn-tester-ux/baseline.md` §6.4);
- тесты, которым нужен реальный ФН, при отсутствии ответа печатают `[SKIP]`.

```bash
# один раз: pyserial (Homebrew Python защищён PEP 668)
python3 -m pip install --user --break-system-packages pyserial

FN_TESTS=1 ufbt launch FLIP_PORT=/dev/cu.usbmodemflip_Rogour1
python3 .test_scripts/await_flipper.py Rogour
python3 .test_scripts/units.py Rogour
python3 .test_scripts/units.py Rogour fn_worker   # только один набор
```

`units.py <name>` ищет порт через `await_flipper.flp_serial_by_name()`, шлёт в
CLI `fn_test` и разбирает `Failed tests: N`, `Consumed: N ms`, `Leaked: N`,
`Status: PASSED|FAILED`. Код возврата `0` — все тесты прошли.

## Правила для агента

- **Перед началом работы загружать скилл `flipper-zero-app-dev`**, а для работ
  по протоколу ФН — ещё и `fn-protocol`; при расхождении с этим файлом
  приоритет у явных требований пользователя и кода проекта.
- Сборка/запуск/отладка — только `ufbt`; не вызывать `./fbt` и не менять файлы
  прошивки. Все изменения — внутри этого репозитория.
- Язык документации, комментариев, логов и ответов — **русский**. Идентификаторы
  кода — английские.
- Соблюдать стиль проекта: `ufbt format`, `.clang-format`, `TAG` +
  `FURI_LOG_*`, `furi_check`/`furi_assert` для проверок, парные
  `malloc`/`free`.
- Не блокировать GUI-поток: никаких `furi_delay_ms` и ожиданий в
  draw/input/enter/exit и в сценах; вся работа с ФН — в воркере.
- Память ограничена (STM32WB55: 256 КБ RAM, FAP грузится в RAM целиком):
  следить за `stack_size`, лимитами `FnTextModel`, утечками; проверять
  `free`/`top` в CLI; никаких аллокаций по длине из ответа ФН.
- Байтовый порядок и кадр — только `fn_wire_*`/`fn_frame_*`; добавление команды
  без теста на golden-кадр запрещено (`docs/byte-order.md` §5).
- Коды команд (`FN_CMD*`) и поля протокола — только в `lib/fn/proto/` и
  `lib/fn/fn_ops.c`; в `ui/` их быть не должно.
- Пользовательские строки — только в `ui/fn_strings.c` (EN/RU). Основной шрифт
  без `Ё/ё`, `№`, `«»` — писать «ждет», «Отчет», без тире.
- Изменяющая команда (`60h`) требует preflight-проверки «МГМ/боевой» и явного
  подтверждения; отправляется ровно один раз.
- Не ломать release-сборку: тестовый код держать под `FN_TESTS`/`FURI_DEBUG`,
  а не включать в обычный FAP.
- При добавлении нового сервиса (`furi_record_open(RECORD_*)`) обновлять
  `requires` в `application.fam`.
- При изменении поведения/API обновлять `fap_version` в `application.fam`,
  `.catalog/CHANGELOG.md`, а при изменении протокола/UI — `docs/*.md`.
- `.specify/`, `.opencode/commands/` — инструменты Spec Kit; не считать их
  частью кода приложения и не править без запроса.
