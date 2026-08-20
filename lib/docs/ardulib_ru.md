# Документация ArduboyLIB {#ardulib}

ArduboyLIB — это общая библиотека для создания игр в стиле Arduino/Arduboy на Flipper Zero. Она предоставляет общую среду выполнения (`arduboy_app`) и API, совместимый с Arduboy, чтобы код вашей игры мог оставаться в классическом стиле Arduino.

## Быстрый старт

### 1. Добавьте ArduboyLIB в проект

**Вариант A: Git Submodule**
```bash
git submodule add https://github.com/apfxtech/ArduboyLIB.git lib
git submodule update --init --recursive
```

**Вариант B: Прямое клонирование**
```bash
git clone https://github.com/apfxtech/ArduboyLIB.git lib
```

### 2. Настройте application.fam

```python
App(
    appid="your_app_id",
    name="Your Game",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="arduboy_app",
    requires=["gui"],
    sources=["main.cpp", "lib/scr/*.cpp", "game/*.cpp"],
)
```

### 3. Создайте main.cpp

```cpp
#include "lib/Arduboy2.h"
#include "lib/runtime.h"

#define TARGET_FRAMERATE 60

void setup() {
    arduboy.setFrameRate(TARGET_FRAMERATE);
    // Ваш код инициализации
}

void loop() {
    if(!arduboy.nextFrame()) return;
    arduboy.pollButtons();
    
    // Логика игры и отрисовка
    arduboy.clear();
    // ... отрисовка здесь ...
    arduboy.display();
}
```

## Флаги компиляции

Определите эти флаги в начале `main.cpp` **перед** подключением библиотек.

- **`ARDULIB_RANDOM_LEGACY`** — Использовать устаревший псевдослучайный генератор, совместимый с Arduboy, вместо Flipper HAL RNG.

- **`ARDULIB_USE_ATM`** — Включить встроенную интеграцию ATM из среды выполнения (init/idle/deinit), чтобы код игры мог оставаться в стиле Arduino без функций движка.

- **`ARDULIB_USE_TONES`** — Включить поддержку воспроизведения `ArduboyTones`. Без этого флага API `ArduboyTones` остаётся доступным как пустой заглушкой совместимости и не запускает поток воспроизведения тонов.

- **`ARDULIB_USE_FX`** — Включить поддержку FX (внешняя флеш-память) для игр, использующих FX-чип для дополнительного хранения. Этот флаг активирует класс `FX` для чтения игровых данных и файлов сохранений из внешней флеш-памяти.

- **`ARDULIB_SWAP_AB`** — Поменять местами флаги кнопок A и B. При определении `A_BUTTON` становится `0x20`, а `B_BUTTON` становится `0x10`. Полезно для игр, которые ожидают другую раскладку кнопок.

- **`ARDULIB_USE_VIEW_PORT`** {#view_port_flag} — Переключает среду выполнения с устаревшего режима framebuffer на режим ViewPort для отрисовки на экране и ввода кнопок.

    > **Важно:** Этот флаг требует ручной локальной сборки. Автосборщики (GitHub Workflow, flipper.lab) не смогут собрать приложение из-за более строгой проверки кода. Это временная мера до публикации нового API дисплея разработчиками прошивок.

    **Сравнение режимов:**

    | Функция | Legacy режим (по умолчанию) | ViewPort режим |
    |---------|----------------------------|----------------|
    | Метод сборки | Любой (авто/ручной) | Только ручная сборка |
    | Стабильность ввода | Менее стабилен | Более надёжный |
    | Отрисовка дисплея | Framebuffer callback | ViewPort callback |
    | Используемый API | Публичный API | Внутренний API |

    **Когда использовать:**
    - Используйте **legacy режим** (по умолчанию) для совместимости с автосборщиками (GitHub Workflow, flipper.lab)
    - Используйте **режим ViewPort** для более стабильной обработки ввода при локальной сборке

## Обзор API

### Основные функции

- `arduboy.begin()` - Инициализация библиотеки
- `arduboy.setFrameRate(fps)` - Установка целевой частоты кадров
- `arduboy.nextFrame()` - Контроль темпа кадров
- `arduboy.pollButtons()` - Обновление состояния кнопок
- `arduboy.display()` - Вывод буфера на экран

### Ввод кнопок

```cpp
arduboy.pressed(UP_BUTTON)      // Сейчас нажато
arduboy.justPressed(A_BUTTON)   // Нажато в этом кадре
arduboy.justReleased(B_BUTTON)  // Отпущено в этом кадре
```

### Отрисовка

```cpp
arduboy.clear()                          // Очистить буфер
arduboy.drawPixel(x, y, color)           // Нарисовать пиксель
arduboy.drawLine(x0, y0, x1, y1, color)  // Нарисовать линию
arduboy.fillRect(x, y, w, h, color)      // Заполненный прямоугольник
arduboy.drawBitmap(x, y, bitmap, w, h)   // Нарисовать bitmap
```

## Проекты, использующие ArduboyLIB

- [Arduventure](https://github.com/apfxtech/FlipperArduventure)
- [Catacombs of the Damned](https://github.com/apfxtech/FlipperCatacombs)
- [Wolfenstein](https://github.com/apfxtech/FlipperWolfenstein)
- [Mystic Balloon](https://github.com/apfxtech/FlipperMysticBalloon)
- [ArduGolf](https://github.com/apfxtech/FlipperGolf)
- [ArduDriver](https://github.com/apfxtech/FlipperDrivin)
- [MicroTD](https://github.com/apfxtech/FlipperTowerDefense.git)
- [MicroCity](https://github.com/apfxtech/FlipperMicroCity.git)
- [Minecraft](https://github.com/apfxtech/FlipperMinecraft.git)

## Благодарности

**Оригинал:** MrBlinky - [Arduboy homemade package](https://github.com/MrBlinky/Arduboy-homemade-package)

**Лицензия:** CC0 1.0 Universal
