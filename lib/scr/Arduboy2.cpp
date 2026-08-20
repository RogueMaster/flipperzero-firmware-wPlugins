#include "../Arduboy2.h"
#include "../runtime.h"

// Глобальный объект arduboy - единственный экземпляр для всех игр
Arduboy2Base arduboy;
Sprites sprites;

const uint8_t Arduboy2Base::font5x7_[] PROGMEM = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x5B, 0x4F, 0x5B, 0x3E, 0x3E, 0x6B, 0x4F, 0x6B, 0x3E, 0x1C,
            0x3E, 0x7C, 0x3E, 0x1C, 0x18, 0x3C, 0x7E, 0x3C, 0x18, 0x1C, 0x57, 0x7D, 0x57, 0x1C, 0x1C, 0x5E,
            0x7F, 0x5E, 0x1C, 0x00, 0x18, 0x3C, 0x18, 0x00, 0xFF, 0xE7, 0xC3, 0xE7, 0xFF, 0x00, 0x18, 0x24,
            0x18, 0x00, 0xFF, 0xE7, 0xDB, 0xE7, 0xFF, 0x30, 0x48, 0x3A, 0x06, 0x0E, 0x26, 0x29, 0x79, 0x29,
            0x26, 0x40, 0x7F, 0x05, 0x05, 0x07, 0x40, 0x7F, 0x05, 0x25, 0x3F, 0x5A, 0x3C, 0xE7, 0x3C, 0x5A,
            0x7F, 0x3E, 0x1C, 0x1C, 0x08, 0x08, 0x1C, 0x1C, 0x3E, 0x7F, 0x14, 0x22, 0x7F, 0x22, 0x14, 0x5F,
            0x5F, 0x00, 0x5F, 0x5F, 0x06, 0x09, 0x7F, 0x01, 0x7F, 0x00, 0x66, 0x89, 0x95, 0x6A, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x94, 0xA2, 0xFF, 0xA2, 0x94, 0x08, 0x04, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x7E,
            0x20, 0x10, 0x08, 0x08, 0x2A, 0x1C, 0x08, 0x08, 0x1C, 0x2A, 0x08, 0x08, 0x1E, 0x10, 0x10, 0x10,
            0x10, 0x0C, 0x1E, 0x0C, 0x1E, 0x0C, 0x30, 0x38, 0x3E, 0x38, 0x30, 0x06, 0x0E, 0x3E, 0x0E, 0x06,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00, 0x14,
            0x7F, 0x14, 0x7F, 0x14, 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x23, 0x13, 0x08, 0x64, 0x62, 0x36, 0x49,
            0x56, 0x20, 0x50, 0x00, 0x08, 0x07, 0x03, 0x00, 0x00, 0x1C, 0x22, 0x41, 0x00, 0x00, 0x41, 0x22,
            0x1C, 0x00, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x80, 0x70, 0x30,
            0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x60, 0x60, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02,
            0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00, 0x72, 0x49, 0x49, 0x49, 0x46, 0x21,
            0x41, 0x49, 0x4D, 0x33, 0x18, 0x14, 0x12, 0x7F, 0x10, 0x27, 0x45, 0x45, 0x45, 0x39, 0x3C, 0x4A,
            0x49, 0x49, 0x31, 0x41, 0x21, 0x11, 0x09, 0x07, 0x36, 0x49, 0x49, 0x49, 0x36, 0x46, 0x49, 0x49,
            0x29, 0x1E, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x40, 0x34, 0x00, 0x00, 0x00, 0x08, 0x14, 0x22,
            0x41, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x41, 0x22, 0x14, 0x08, 0x02, 0x01, 0x59, 0x09, 0x06,
            0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x3E,
            0x41, 0x41, 0x41, 0x22, 0x7F, 0x41, 0x41, 0x41, 0x3E, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x7F, 0x09,
            0x09, 0x09, 0x01, 0x3E, 0x41, 0x41, 0x51, 0x73, 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x41, 0x7F,
            0x41, 0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, 0x7F, 0x08, 0x14, 0x22, 0x41, 0x7F, 0x40, 0x40, 0x40,
            0x40, 0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x3E, 0x41, 0x41, 0x41, 0x3E,
            0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x7F, 0x09, 0x19, 0x29, 0x46, 0x26,
            0x49, 0x49, 0x49, 0x32, 0x03, 0x01, 0x7F, 0x01, 0x03, 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x1F, 0x20,
            0x40, 0x20, 0x1F, 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x63, 0x14, 0x08, 0x14, 0x63, 0x03, 0x04, 0x78,
            0x04, 0x03, 0x61, 0x59, 0x49, 0x4D, 0x43, 0x00, 0x7F, 0x41, 0x41, 0x41, 0x02, 0x04, 0x08, 0x10,
            0x20, 0x00, 0x41, 0x41, 0x41, 0x7F, 0x04, 0x02, 0x01, 0x02, 0x04, 0x40, 0x40, 0x40, 0x40, 0x40,
            0x00, 0x03, 0x07, 0x08, 0x00, 0x20, 0x54, 0x54, 0x78, 0x40, 0x7F, 0x28, 0x44, 0x44, 0x38, 0x38,
            0x44, 0x44, 0x44, 0x28, 0x38, 0x44, 0x44, 0x28, 0x7F, 0x38, 0x54, 0x54, 0x54, 0x18, 0x00, 0x08,
            0x7E, 0x09, 0x02, 0x18, 0xA4, 0xA4, 0x9C, 0x78, 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x44, 0x7D,
            0x40, 0x00, 0x20, 0x40, 0x40, 0x3D, 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00, 0x00, 0x41, 0x7F, 0x40,
            0x00, 0x7C, 0x04, 0x78, 0x04, 0x78, 0x7C, 0x08, 0x04, 0x04, 0x78, 0x38, 0x44, 0x44, 0x44, 0x38,
            0xFC, 0x18, 0x24, 0x24, 0x18, 0x18, 0x24, 0x24, 0x18, 0xFC, 0x7C, 0x08, 0x04, 0x04, 0x08, 0x48,
            0x54, 0x54, 0x54, 0x24, 0x04, 0x04, 0x3F, 0x44, 0x24, 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x1C, 0x20,
            0x40, 0x20, 0x1C, 0x3C, 0x40, 0x30, 0x40, 0x3C, 0x44, 0x28, 0x10, 0x28, 0x44, 0x4C, 0x90, 0x90,
            0x90, 0x7C, 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00, 0x08, 0x36, 0x41, 0x00, 0x00, 0x00, 0x77, 0x00,
            0x00, 0x00, 0x41, 0x36, 0x08, 0x00, 0x02, 0x01, 0x02, 0x04, 0x02, 0x3C, 0x26, 0x23, 0x26, 0x3C,
};


// ============================================================================
// BeepPin1 - legacy compatibility
// ============================================================================

void BeepPin1::begin() {
    tones_.begin();
}

void BeepPin1::timer() {
}

uint16_t BeepPin1::freq(uint16_t f) const {
    return f;
}

void BeepPin1::tone(uint16_t frequency, uint16_t duration_frames) {
    if(duration_frames == 0) {
        tones_.noTone();
        return;
    }

    // Legacy BeepPin1 uses frame-based durations, typically at 60 FPS.
    uint32_t duration_ms = ((uint32_t)duration_frames * 1000u + 30u) / 60u;
    if(duration_ms == 0) duration_ms = 1;
    tones_.tone(frequency, (uint16_t)duration_ms);
}

// ============================================================================
// Sprites - API совместимости с оригинальным Arduboy2
// ============================================================================

Arduboy2Base* Sprites::ab_ = nullptr;

void Sprites::setArduboy(Arduboy2Base* a) {
    ab_ = a;
}

void Sprites::drawOverwrite(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
    if(!ab_ || !bmp) return;
    ab_->drawSolidBitmapFrame(x, y, bmp, frame);
}

void Sprites::drawSelfMasked(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
    if(!ab_ || !bmp) return;
    ab_->drawBitmapFrame(x, y, bmp, frame);
}

void Sprites::drawErase(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
    if(!ab_ || !bmp) return;
    ab_->eraseBitmapFrame(x, y, bmp, frame);
}

void Sprites::drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame) {
    if(!ab_) return;
    ab_->drawPlusMask(x, y, plusmask, frame);
}

// ============================================================================
// Arduboy2Base - начало
// ============================================================================

void Arduboy2Base::begin(
    uint8_t* screen_buffer,
    volatile uint8_t* input_state,
    volatile uint8_t* input_press_latch,
    FuriMutex* game_mutex,
    volatile bool* exit_requested) {
    // Прямая инициализация без InputContext/InputRuntime мостов
    sBuffer = screen_buffer;
    input_state_ = input_state;
    input_press_latch_ = input_press_latch;
    game_mutex_ = game_mutex;
    exit_requested_ = exit_requested;
    external_timing_ = false;
    frame_count_ = 0;
    last_frame_ms_ = 0;
    resetInputState();
    audio.begin();
}

void Arduboy2Base::begin() {
    // Пустая версия для совместимости
}

void Arduboy2Base::exitToBootloader() {
    if(exit_requested_) {
        *exit_requested_ = true;
    }
}

bool Arduboy2Base::collide(Point point, Rect rect) {
    return (point.x >= rect.x) && (point.x < rect.x + rect.width) && (point.y >= rect.y) &&
           (point.y < rect.y + rect.height);
}

bool Arduboy2Base::collide(Rect rect1, Rect rect2) {
    return !(
        rect2.x >= rect1.x + rect1.width || rect2.x + rect2.width <= rect1.x ||
        rect2.y >= rect1.y + rect1.height || rect2.y + rect2.height <= rect1.y);
}

void Arduboy2Base::FlipperInputCallback(const InputEvent* event, void* ctx_ptr) {
    // Прямой доступ к arduboy.input_state_ и arduboy.input_press_latch_
    // без InputContext/InputRuntime мостов
    if(!event || !ctx_ptr) return;
    Arduboy2Base* arduboy_ptr = (Arduboy2Base*)ctx_ptr;

    const uint8_t bit = FlipperInputMaskFromKey_(event->key);
    if(!bit) return;

    if(event->type == InputTypePress) {
        if(arduboy_ptr->input_press_latch_) {
            (void)__atomic_fetch_or((uint8_t*)arduboy_ptr->input_press_latch_, bit, __ATOMIC_RELAXED);
        }
        if(arduboy_ptr->input_state_) {
            (void)__atomic_fetch_or((uint8_t*)arduboy_ptr->input_state_, bit, __ATOMIC_RELAXED);
        }
    } else if(event->type == InputTypeRepeat) {
        // Repeat keeps the held state alive, but must not retrigger justPressed edges.
        if(arduboy_ptr->input_state_) {
            (void)__atomic_fetch_or((uint8_t*)arduboy_ptr->input_state_, bit, __ATOMIC_RELAXED);
        }
    } else if(event->type == InputTypeRelease) {
        if(arduboy_ptr->input_state_) {
            (void)__atomic_fetch_and((uint8_t*)arduboy_ptr->input_state_, (uint8_t)~bit, __ATOMIC_RELAXED);
        }
    }
}

void Arduboy2Base::FlipperInputClearMask(uint8_t input_mask, void* ctx_ptr) {
    // Прямой доступ без InputContext
    if(!input_mask || !ctx_ptr) return;
    Arduboy2Base* arduboy_ptr = (Arduboy2Base*)ctx_ptr;

    if(arduboy_ptr->input_press_latch_) {
        (void)__atomic_fetch_and((uint8_t*)arduboy_ptr->input_press_latch_, (uint8_t)~input_mask, __ATOMIC_RELAXED);
    }
    if(arduboy_ptr->input_state_) {
        (void)__atomic_fetch_and((uint8_t*)arduboy_ptr->input_state_, (uint8_t)~input_mask, __ATOMIC_RELAXED);
    }
}

void Arduboy2Base::boot() {
}
void Arduboy2Base::bootLogo() {
}
void Arduboy2Base::bootLogoSpritesSelfMasked() {
}

void Arduboy2Base::setFrameRate(uint8_t fps) {
    if(fps == 0) fps = 30;
    uint32_t d = 1000u / fps;
    frame_duration_ms_ = d ? d : 1u;
}

bool Arduboy2Base::nextFrame() {
    if(external_timing_) {
        frame_count_++;
        return true;
    }
    const uint32_t now = millis();
    if((uint32_t)(now - last_frame_ms_) < frame_duration_ms_) return false;
    last_frame_ms_ = now;
    frame_count_++;
    return true;
}

bool Arduboy2Base::everyXFrames(uint8_t n) const {
    if(n == 0) return false;
    return (frame_count_ % n) == 0;
}

void Arduboy2Base::pollButtons() {
    // Прямое чтение из input_state_ и input_press_latch_ без InputContext
    prev_buttons_ = cur_buttons_;
    uint8_t in = 0;
    uint8_t press = 0;
    uint8_t release = 0;

    // input_state_ хранит текущее состояние кнопок
    // input_press_latch_ хранит нажатия с последнего сброса (edge detection)
    if(input_state_) {
        in = __atomic_load_n((uint8_t*)input_state_, __ATOMIC_RELAXED);
    }
    if(input_press_latch_) {
        // Сбрасываем latch после чтения для edge detection
        press = __atomic_exchange_n((uint8_t*)input_press_latch_, 0, __ATOMIC_RELAXED);
    }

    cur_buttons_ = mapInputToArduboyMask_(in);
    press_edges_ = mapInputToArduboyMask_(press);
    release_edges_ = mapInputToArduboyMask_(release);
}

void Arduboy2Base::clearButtonState() {
    prev_buttons_ = 0;
    cur_buttons_ = 0;
    press_edges_ = 0;
    release_edges_ = 0;
}

void Arduboy2Base::resetInputState() {
    // Прямой сброс без InputContext
    if(input_state_) {
        (void)__atomic_store_n((uint8_t*)input_state_, 0, __ATOMIC_RELAXED);
    }
    if(input_press_latch_) {
        (void)__atomic_store_n((uint8_t*)input_press_latch_, 0, __ATOMIC_RELAXED);
    }
    clearButtonState();
}

bool Arduboy2Base::pressed(uint8_t mask) const {
    return (cur_buttons_ & mask) != 0;
}
bool Arduboy2Base::notPressed(uint8_t mask) const {
    return (cur_buttons_ & mask) == 0;
}
bool Arduboy2Base::justPressed(uint8_t mask) const {
    return ((press_edges_ & mask) != 0) ||
           (((cur_buttons_ & mask) != 0) && ((prev_buttons_ & mask) == 0));
}
bool Arduboy2Base::justReleased(uint8_t mask) const {
    return ((release_edges_ & mask) != 0) ||
           (((cur_buttons_ & mask) == 0) && ((prev_buttons_ & mask) != 0));
}

uint8_t Arduboy2Base::justPressedButtons() const {
    return (uint8_t)(press_edges_ ? press_edges_ : (cur_buttons_ & (uint8_t)~prev_buttons_));
}

uint8_t Arduboy2Base::pressedButtons() const {
    return cur_buttons_;
}

void Arduboy2Base::resetFrameCount() {
    frame_offset_ = -(int32_t)frameCount();
}

uint16_t Arduboy2Base::getFrameCount() const {
    return (uint16_t)((int32_t)frameCount() + frame_offset_);
}

void Arduboy2Base::setFrameCount(uint16_t val) const {
    frame_offset_ = (int32_t)val - (int32_t)frameCount();
}

uint8_t Arduboy2Base::getFrameCount(uint8_t mod, int8_t offset) const {
    if(mod == 0) return 0;
    return (uint8_t)((getFrameCount() + offset) % mod);
}

bool Arduboy2Base::getFrameCountHalf(uint8_t mod) const {
    return getFrameCount(mod) > (mod / 2);
}

bool Arduboy2Base::isFrameCount(uint8_t mod) const {
    if(mod == 0) return false;
    return (getFrameCount() % mod) == 0;
}

bool Arduboy2Base::isFrameCount(uint8_t mod, uint8_t val) const {
    if(mod == 0) return false;
    return (getFrameCount() % mod) == val;
}

uint16_t rnd = 0xACE1;

uint8_t Arduboy2Base::randomLFSR(uint8_t min, uint8_t max) {
    if(max <= min) return min;
    uint16_t r = rnd;
    r ^= (uint16_t)millis();
    (r & 1) ? r = (r >> 1) ^ 0xB400 : r >>= 1;
    rnd = r;
    return (uint8_t)(r % (max - min) + min);
}

uint8_t* Arduboy2Base::getBuffer() {
    return sBuffer;
}
const uint8_t* Arduboy2Base::getBuffer() const {
    return sBuffer;
}

void Arduboy2Base::clear() {
    fillScreen(0);
}

void Arduboy2Base::fillScreen(uint8_t color) {
    if(!sBuffer) return;
    memset(sBuffer, color ? 0xFF : 0x00, (WIDTH * HEIGHT) / 8);
}

void Arduboy2Base::display() {
    rt_display(false);
}

void Arduboy2Base::display(bool clear) {
    rt_display(clear);
}

void Arduboy2Base::invert(bool invert) {
    // Прямой вызов функции из runtime.cpp для инверсии экрана
    // Объявление здесь для устранения зависимости от runtime.h
    extern void arduboy_screen_invert(bool);
    arduboy_screen_invert(invert);
}

void Arduboy2Base::drawPixel(int16_t x, int16_t y, uint8_t color) {
    uint16_t ux = (uint16_t)x;
    uint16_t uy = (uint16_t)y;

    if(!sBuffer || ux >= WIDTH || uy >= HEIGHT) return;

    uint8_t* dst = sBuffer + ux + (uint16_t)((uy >> 3) * WIDTH);
    uint8_t mask = (uint8_t)(1u << (uy & 7));

    uint8_t v = *dst;
    v = (uint8_t)((v & (uint8_t)~mask) | ((uint8_t)-(uint8_t)(color != 0) & mask));
    *dst = v;
}

void Arduboy2Base::drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color) {
    if(!sBuffer || w <= 0) return;
    if((uint16_t)y >= HEIGHT) return;

    if(x < 0) {
        w = (int16_t)(w + x);
        x = 0;
    }
    if(w <= 0 || x >= WIDTH) return;
    if((int32_t)x + (int32_t)w > WIDTH) {
        w = (int16_t)(WIDTH - x);
    }

    const uint8_t mask = (uint8_t)(1u << (y & 7));
    uint8_t* dst = sBuffer + x + (uint16_t)((y >> 3) * WIDTH);
    if(color) {
        for(int16_t i = 0; i < w; ++i) {
            dst[i] |= mask;
        }
    } else {
        const uint8_t inv_mask = (uint8_t)~mask;
        for(int16_t i = 0; i < w; ++i) {
            dst[i] &= inv_mask;
        }
    }
}

void Arduboy2Base::drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color) {
    if(!sBuffer || h <= 0) return;
    if((uint16_t)x >= WIDTH) return;

    int32_t y0 = y;
    int32_t y1 = (int32_t)y + (int32_t)h;
    if(y0 < 0) y0 = 0;
    if(y1 > HEIGHT) y1 = HEIGHT;
    if(y0 >= y1) return;

    const int16_t first_row = (int16_t)((int16_t)y0 >> 3);
    const int16_t last_row = (int16_t)(((int16_t)(y1 - 1)) >> 3);
    uint8_t* dst = sBuffer + x + first_row * WIDTH;

    if(first_row == last_row) {
        const uint8_t first_mask = (uint8_t)(0xFFu << ((uint8_t)y0 & 7));
        const uint8_t last_mask = (uint8_t)(0xFFu >> (7 - (((uint8_t)(y1 - 1)) & 7)));
        const uint8_t mask = (uint8_t)(first_mask & last_mask);
        if(color) {
            *dst |= mask;
        } else {
            *dst &= (uint8_t)~mask;
        }
        return;
    }

    const uint8_t first_mask = (uint8_t)(0xFFu << ((uint8_t)y0 & 7));
    if(color) {
        *dst |= first_mask;
    } else {
        *dst &= (uint8_t)~first_mask;
    }
    dst += WIDTH;

    for(int16_t row = (int16_t)(first_row + 1); row < last_row; ++row, dst += WIDTH) {
        *dst = color ? 0xFFu : 0x00u;
    }

    const uint8_t last_mask = (uint8_t)(0xFFu >> (7 - (((uint8_t)(y1 - 1)) & 7)));
    if(color) {
        *dst |= last_mask;
    } else {
        *dst &= (uint8_t)~last_mask;
    }
}

void Arduboy2Base::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    if(w <= 0 || h <= 0) return;
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, (int16_t)(y + h - 1), w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine((int16_t)(x + w - 1), y, h, color);
}

void Arduboy2Base::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    if(!sBuffer || w <= 0 || h <= 0) return;

    int32_t x0 = x;
    int32_t y0 = y;
    int32_t x1 = (int32_t)x + (int32_t)w;
    int32_t y1 = (int32_t)y + (int32_t)h;

    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    if(x1 > WIDTH) x1 = WIDTH;
    if(y1 > HEIGHT) y1 = HEIGHT;
    if(x0 >= x1 || y0 >= y1) return;

    const int16_t clipped_h = (int16_t)(y1 - y0);
    for(int16_t xx = (int16_t)x0; xx < (int16_t)x1; ++xx) {
        drawFastVLine(xx, (int16_t)y0, clipped_h, color);
    }
}

void Arduboy2Base::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    int16_t dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = -((y0 < y1) ? (y1 - y0) : (y0 - y1));
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (int16_t)(dx + dy);

    while(true) {
        drawPixel(x0, y0, color);
        if(x0 == x1 && y0 == y1) break;

        int16_t e2 = (int16_t)(2 * err);
        if(e2 >= dy) {
            err = (int16_t)(err + dy);
            x0 = (int16_t)(x0 + sx);
        }
        if(e2 <= dx) {
            err = (int16_t)(err + dx);
            y0 = (int16_t)(y0 + sy);
        }
    }
}

void Arduboy2Base::fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
    if(r < 0) return;
    drawFastVLine(x0, (int16_t)(y0 - r), (int16_t)(2 * r + 1), color);

    int16_t x = 0;
    int16_t y = r;
    int16_t d = (int16_t)(1 - r);

    while(y > x) {
        ++x;
        if(d < 0) {
            d = (int16_t)(d + 2 * x + 1);
        } else {
            --y;
            d = (int16_t)(d + 2 * (x - y) + 1);
        }

        drawFastVLine((int16_t)(x0 + x), (int16_t)(y0 - y), (int16_t)(2 * y + 1), color);
        drawFastVLine((int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)(2 * y + 1), color);
        drawFastVLine((int16_t)(x0 + y), (int16_t)(y0 - x), (int16_t)(2 * x + 1), color);
        drawFastVLine((int16_t)(x0 - y), (int16_t)(y0 - x), (int16_t)(2 * x + 1), color);
    }
}

void Arduboy2Base::drawBitmap(
    int16_t x,
    int16_t y,
    const uint8_t* bitmap,
    int16_t w,
    int16_t h,
    uint8_t color) {
    if(!bitmap || !sBuffer || w <= 0 || h <= 0) return;
    if(color) {
        blitSelfMasked_(x, y, bitmap, w, h);
    } else {
        blitErase_(x, y, bitmap, w, h);
    }
}

void Arduboy2Base::setCursor(int16_t x, int16_t y) {
    cursor_x_ = x;
    cursor_y_ = y;
}

void Arduboy2Base::setTextColor(uint8_t color) {
    text_color_ = color;
}

void Arduboy2Base::setTextBackground(uint8_t color) {
    text_bg_ = color;
}

size_t Arduboy2Base::write(uint8_t c) {
    if(c == '\r') return 1;
    if(c == '\n') {
        cursor_x_ = 0;
        cursor_y_ += 8;
        return 1;
    }

    if(c > 127) c = '?';
    if(c < 32) c = 32;

    const uint16_t glyph_offset = (uint16_t)c * 5U;
    for(uint8_t col = 0; col < 5; ++col) {
        const uint8_t line = pgm_read_byte(font5x7_ + glyph_offset + col);
        for(uint8_t row = 0; row < 8; ++row) {
            if(line & (uint8_t)(1U << row)) {
                drawPixel(cursor_x_ + col, cursor_y_ + row, text_color_);
            } else {
                drawPixel(cursor_x_ + col, cursor_y_ + row, text_bg_);
            }
        }
    }

    for(uint8_t row = 0; row < 8; ++row) {
        drawPixel(cursor_x_ + 5, cursor_y_ + row, text_bg_);
    }

    cursor_x_ += 6;
    return 1;
}

void Arduboy2Base::print(const char* text) {
    if(!text) return;
    while(*text) {
        write((uint8_t)*text++);
    }
}

void Arduboy2Base::print(uint8_t value) {
    printUnsigned_(value);
}

void Arduboy2Base::print(int value) {
    if(value < 0) {
        write((uint8_t)'-');
        const unsigned long abs_value = (unsigned long)(-(long)value);
        printUnsigned_(abs_value);
    } else {
        printUnsigned_((unsigned long)value);
    }
}

void Arduboy2Base::print(unsigned long value) {
    printUnsigned_(value);
}

void Arduboy2Base::drawBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
    if(!bmp || !sBuffer) return;
    const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
    const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
    if(!isSpriteVisible_(x, y, w, h)) return;
    const int16_t pages = (h + 7) >> 3;
    const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
    const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
    blitSelfMasked_(x, y, data, w, h);
}

void Arduboy2Base::drawSolidBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
    if(!bmp || !sBuffer) return;
    const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
    const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
    if(!isSpriteVisible_(x, y, w, h)) return;
    const int16_t pages = (h + 7) >> 3;
    const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
    const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
    blitOverwrite_(x, y, data, w, h);
}

void Arduboy2Base::eraseBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
    if(!bmp || !sBuffer) return;
    const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
    const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
    if(!isSpriteVisible_(x, y, w, h)) return;
    const int16_t pages = (h + 7) >> 3;
    const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
    const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
    blitErase_(x, y, data, w, h);
}

void Arduboy2Base::drawSolidBitmapData(int16_t x, int16_t y, const uint8_t* data, uint8_t w, uint8_t h) {
    if(!data || !sBuffer) return;
    blitOverwrite_(x, y, data, (int16_t)w, (int16_t)h);
}

void Arduboy2Base::drawSelfMaskedData(int16_t x, int16_t y, const uint8_t* data, uint8_t w, uint8_t h) {
    if(!data || !sBuffer) return;
    blitSelfMasked_(x, y, data, (int16_t)w, (int16_t)h);
}

void Arduboy2Base::drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame) {
    if(!plusmask || !sBuffer) return;
    const int16_t w = (int16_t)pgm_read_byte(plusmask + 0);
    const int16_t h = (int16_t)pgm_read_byte(plusmask + 1);
    if(!isSpriteVisible_(x, y, w, h)) return;
    const int16_t pages = (h + 7) >> 3;
    const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
    const uint8_t* data = plusmask + 2 + (uint32_t)frame * (uint32_t)frame_size * 2u;
    blitPlusMask_(x, y, data, w, h);
}

void Arduboy2Base::drawPlusMaskData(int16_t x, int16_t y, const uint8_t* data, uint8_t w, uint8_t h) {
    if(!data || !sBuffer) return;
    blitPlusMask_(x, y, data, (int16_t)w, (int16_t)h);
}

void Arduboy2Base::drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
    if(r <= 0) return;
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - x;

    while(x >= y) {
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 - y, y0 - x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 + x, y0 - y, color);

        y++;
        if(err < 0) {
            err += (int16_t)(2 * y + 1);
        } else {
            x--;
            err += (int16_t)(2 * (y - x) + 1);
        }
    }
}

void Arduboy2Base::drawSprite(
    int16_t x,
    int16_t y,
    const uint8_t* bmp,
    const uint8_t* mask,
    uint8_t frame,
    uint8_t mask_frame) {
    if(!bmp || !sBuffer) return;

    const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
    const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
    if(!isSpriteVisible_(x, y, w, h)) return;
    const int16_t pages = (h + 7) >> 3;
    const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;

    if(mask) {
        const uint8_t* sprite_data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
        const uint8_t* mask_data = mask + (uint32_t)mask_frame * (uint32_t)frame_size;
        blitExternalMask_(x, y, sprite_data, mask_data, w, h);
    } else {
        const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size * 2u;
        blitPlusMask_(x, y, data, w, h);
    }
}

void Arduboy2Base::setRGBled(uint8_t r, uint8_t g, uint8_t b) {
    NotificationApp* n = (NotificationApp*)furi_record_open(RECORD_NOTIFICATION);
    if(!n) return;

    notification_message(n, r ? &sequence_set_red_255 : &sequence_reset_red);
    notification_message(n, g ? &sequence_set_green_255 : &sequence_reset_green);
    notification_message(n, b ? &sequence_set_blue_255 : &sequence_reset_blue);

    furi_record_close(RECORD_NOTIFICATION);
}

void Arduboy2Base::setRGBled(uint8_t color, uint8_t val) {
    const uint8_t r = (color == RED_LED) ? val : 0;
    const uint8_t g = (color == GREEN_LED) ? val : 0;
    const uint8_t b = (color == BLUE_LED) ? val : 0;
    setRGBled(r, g, b);
}

void Arduboy2Base::expectLoadDelay() {
    last_frame_ms_ = millis();
}

uint32_t Arduboy2Base::frameCount() const {
    return frame_count_;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

uint8_t Arduboy2Base::FlipperInputMaskFromKey_(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return INPUT_UP;
    case InputKeyDown:
        return INPUT_DOWN;
    case InputKeyLeft:
        return INPUT_LEFT;
    case InputKeyRight:
        return INPUT_RIGHT;
    case InputKeyOk:
        return INPUT_B;
    case InputKeyBack:
        return INPUT_A;
    default:
        return 0;
    }
}

uint8_t Arduboy2Base::page_mask_(int16_t p, int16_t pages, int16_t h) {
    if(p != pages - 1) return 0xFF;
    const int16_t rem = (h & 7);
    if(rem == 0) return 0xFF;
    return (uint8_t)((1u << rem) - 1u);
}

bool Arduboy2Base::isSpriteVisible_(int16_t x, int16_t y, int16_t w, int16_t h) {
    if(w <= 0 || h <= 0) return false;
    const int32_t x2 = (int32_t)x + (int32_t)w;
    const int32_t y2 = (int32_t)y + (int32_t)h;
    return (x < WIDTH) && (y < HEIGHT) && (x2 > 0) && (y2 > 0);
}

bool Arduboy2Base::clipVisibleColumns_(int16_t x, int16_t w, int16_t& start, int16_t& end) {
    if(w <= 0) return false;
    const int32_t x2 = (int32_t)x + (int32_t)w;
    if(x >= WIDTH || x2 <= 0) return false;
    start = (x < 0) ? (int16_t)(-x) : 0;
    end = (x2 > WIDTH) ? (int16_t)(WIDTH - x) : w;
    return start < end;
}

void Arduboy2Base::buildPageMasks_(uint8_t* page_masks, int16_t pages, int16_t h) {
    for(int16_t p = 0; p < pages; ++p) {
        page_masks[p] = page_mask_(p, pages, h);
    }
}

void Arduboy2Base::blitSelfMasked_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h) {
    if(!isSpriteVisible_(x, y, w, h)) return;

    int16_t col_start = 0;
    int16_t col_end = 0;
    if(!clipVisibleColumns_(x, w, col_start, col_end)) return;

    const int16_t yOffset = (int16_t)(y & 7);
    const int16_t sRow = (int16_t)(y >> 3);
    const int16_t pages = (h + 7) >> 3;
    const int16_t maxRow = (HEIGHT >> 3);
    uint8_t page_masks[32];
    buildPageMasks_(page_masks, pages, h);

    for(int16_t i = col_start; i < col_end; i++) {
        const int16_t sx = (int16_t)(x + i);
        uint8_t* dst_col = sBuffer + sx;

        const uint8_t* col = src + i;
        if(yOffset == 0) {
            int16_t row = sRow;
            int16_t p = 0;

            if(row < 0) {
                p = (int16_t)(-row);
                if(p >= pages) continue;
                row = 0;
                col += (int32_t)p * w;
            }
            if(row >= maxRow) continue;

            int16_t p_end = pages;
            if(row + (p_end - p) > maxRow) {
                p_end = (int16_t)(p + (maxRow - row));
            }

            uint8_t* dst = dst_col + row * WIDTH;
            for(; p < p_end; ++p, col += w, dst += WIDTH) {
                const uint8_t b = (uint8_t)(pgm_read_byte(col) & page_masks[p]);
                *dst |= b;
            }
        } else {
            for(int16_t p = 0; p < pages; p++, col += w) {
                const uint8_t b = (uint8_t)(pgm_read_byte(col) & page_masks[p]);
                if(!b) continue;

                const int16_t row = (int16_t)(sRow + p);
                const uint8_t lo = (uint8_t)(b << yOffset);
                const uint8_t hi = (uint8_t)(b >> (8 - yOffset));

                if((uint16_t)row < (uint16_t)maxRow) {
                    dst_col[row * WIDTH] |= lo;
                }
                const int16_t row2 = (int16_t)(row + 1);
                if((uint16_t)row2 < (uint16_t)maxRow) {
                    dst_col[row2 * WIDTH] |= hi;
                }
            }
        }
    }
}

void Arduboy2Base::blitErase_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h) {
    if(!isSpriteVisible_(x, y, w, h)) return;

    int16_t col_start = 0;
    int16_t col_end = 0;
    if(!clipVisibleColumns_(x, w, col_start, col_end)) return;

    const int16_t yOffset = (int16_t)(y & 7);
    const int16_t sRow = (int16_t)(y >> 3);
    const int16_t pages = (h + 7) >> 3;
    const int16_t maxRow = (HEIGHT >> 3);
    uint8_t page_masks[32];
    buildPageMasks_(page_masks, pages, h);

    for(int16_t i = col_start; i < col_end; i++) {
        const int16_t sx = (int16_t)(x + i);
        uint8_t* dst_col = sBuffer + sx;

        const uint8_t* col = src + i;
        if(yOffset == 0) {
            int16_t row = sRow;
            int16_t p = 0;

            if(row < 0) {
                p = (int16_t)(-row);
                if(p >= pages) continue;
                row = 0;
                col += (int32_t)p * w;
            }
            if(row >= maxRow) continue;

            int16_t p_end = pages;
            if(row + (p_end - p) > maxRow) {
                p_end = (int16_t)(p + (maxRow - row));
            }

            uint8_t* dst = dst_col + row * WIDTH;
            for(; p < p_end; ++p, col += w, dst += WIDTH) {
                const uint8_t b = (uint8_t)(pgm_read_byte(col) & page_masks[p]);
                *dst &= (uint8_t)~b;
            }
        } else {
            for(int16_t p = 0; p < pages; p++, col += w) {
                const uint8_t b = (uint8_t)(pgm_read_byte(col) & page_masks[p]);
                if(!b) continue;

                const int16_t row = (int16_t)(sRow + p);
                const uint8_t lo = (uint8_t)(b << yOffset);
                const uint8_t hi = (uint8_t)(b >> (8 - yOffset));

                if((uint16_t)row < (uint16_t)maxRow) {
                    dst_col[row * WIDTH] &= (uint8_t)~lo;
                }
                const int16_t row2 = (int16_t)(row + 1);
                if((uint16_t)row2 < (uint16_t)maxRow) {
                    dst_col[row2 * WIDTH] &= (uint8_t)~hi;
                }
            }
        }
    }
}

void Arduboy2Base::blitOverwrite_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h) {
    if(!isSpriteVisible_(x, y, w, h)) return;

    int16_t col_start = 0;
    int16_t col_end = 0;
    if(!clipVisibleColumns_(x, w, col_start, col_end)) return;

    const int16_t yOffset = (int16_t)(y & 7);
    const int16_t sRow = (int16_t)(y >> 3);
    const int16_t pages = (h + 7) >> 3;
    const int16_t maxRow = (HEIGHT >> 3);
    uint8_t page_masks[32];
    buildPageMasks_(page_masks, pages, h);

    for(int16_t i = col_start; i < col_end; i++) {
        const int16_t sx = (int16_t)(x + i);
        uint8_t* dst_col = sBuffer + sx;

        const uint8_t* col = src + i;
        if(yOffset == 0) {
            int16_t row = sRow;
            int16_t p = 0;

            if(row < 0) {
                p = (int16_t)(-row);
                if(p >= pages) continue;
                row = 0;
                col += (int32_t)p * w;
            }
            if(row >= maxRow) continue;

            int16_t p_end = pages;
            if(row + (p_end - p) > maxRow) {
                p_end = (int16_t)(p + (maxRow - row));
            }

            uint8_t* dst = dst_col + row * WIDTH;
            for(; p < p_end; ++p, col += w, dst += WIDTH) {
                *dst = (uint8_t)(pgm_read_byte(col) & page_masks[p]);
            }
        } else {
            for(int16_t p = 0; p < pages; p++, col += w) {
                uint8_t b = pgm_read_byte(col);
                const uint8_t srcMask = page_masks[p];
                b &= srcMask;

                const int16_t row = (int16_t)(sRow + p);

                const uint8_t lo = (uint8_t)(b << yOffset);
                const uint8_t hi = (uint8_t)(b >> (8 - yOffset));
                const uint8_t maskLo = (uint8_t)(srcMask << yOffset);
                const uint8_t maskHi = (uint8_t)(srcMask >> (8 - yOffset));

                if((uint16_t)row < (uint16_t)maxRow) {
                    uint8_t* dst = &dst_col[row * WIDTH];
                    *dst = (uint8_t)((*dst & (uint8_t)~maskLo) | (lo & maskLo));
                }
                const int16_t row2 = (int16_t)(row + 1);
                if((uint16_t)row2 < (uint16_t)maxRow) {
                    uint8_t* dst2 = &dst_col[row2 * WIDTH];
                    *dst2 = (uint8_t)((*dst2 & (uint8_t)~maskHi) | (hi & maskHi));
                }
            }
        }
    }
}

void Arduboy2Base::blitPlusMask_(int16_t x, int16_t y, const uint8_t* srcPairs, int16_t w, int16_t h) {
    if(!isSpriteVisible_(x, y, w, h)) return;

    int16_t col_start = 0;
    int16_t col_end = 0;
    if(!clipVisibleColumns_(x, w, col_start, col_end)) return;

    const int16_t yOffset = (int16_t)(y & 7);
    const int16_t sRow = (int16_t)(y >> 3);
    const int16_t pages = (h + 7) >> 3;
    const int16_t maxRow = (HEIGHT >> 3);
    uint8_t page_masks[32];
    buildPageMasks_(page_masks, pages, h);

    for(int16_t i = col_start; i < col_end; i++) {
        const int16_t sx = (int16_t)(x + i);
        uint8_t* dst_col = sBuffer + sx;
        const uint8_t* col = srcPairs + (uint16_t)i * 2u;

        if(yOffset == 0) {
            int16_t row = sRow;
            int16_t p = 0;

            if(row < 0) {
                p = (int16_t)(-row);
                if(p >= pages) continue;
                row = 0;
                col += (int32_t)p * w * 2;
            }
            if(row >= maxRow) continue;

            int16_t p_end = pages;
            if(row + (p_end - p) > maxRow) {
                p_end = (int16_t)(p + (maxRow - row));
            }

            uint8_t* dst = dst_col + row * WIDTH;
            for(; p < p_end; ++p, col += (int32_t)w * 2, dst += WIDTH) {
                uint8_t s = pgm_read_byte(col + 0);
                uint8_t m = pgm_read_byte(col + 1);
                const uint8_t srcMask = page_masks[p];
                s &= srcMask;
                m &= srcMask;
                *dst = (uint8_t)((*dst & (uint8_t)~m) | (s & m));
            }
        } else {
            for(int16_t p = 0; p < pages; p++, col += (int32_t)w * 2) {
                uint8_t s = pgm_read_byte(col + 0);
                uint8_t m = pgm_read_byte(col + 1);
                const uint8_t srcMask = page_masks[p];
                s &= srcMask;
                m &= srcMask;

                const int16_t row = (int16_t)(sRow + p);
                const uint8_t slo = (uint8_t)(s << yOffset);
                const uint8_t shi = (uint8_t)(s >> (8 - yOffset));
                const uint8_t mlo = (uint8_t)(m << yOffset);
                const uint8_t mhi = (uint8_t)(m >> (8 - yOffset));

                if((uint16_t)row < (uint16_t)maxRow) {
                    uint8_t* dst = &dst_col[row * WIDTH];
                    *dst = (uint8_t)((*dst & (uint8_t)~mlo) | (slo & mlo));
                }
                const int16_t row2 = (int16_t)(row + 1);
                if((uint16_t)row2 < (uint16_t)maxRow) {
                    uint8_t* dst2 = &dst_col[row2 * WIDTH];
                    *dst2 = (uint8_t)((*dst2 & (uint8_t)~mhi) | (shi & mhi));
                }
            }
        }
    }
}

void Arduboy2Base::blitExternalMask_(
    int16_t x,
    int16_t y,
    const uint8_t* sprite,
    const uint8_t* mask,
    int16_t w,
    int16_t h) {
    if(!isSpriteVisible_(x, y, w, h)) return;

    int16_t col_start = 0;
    int16_t col_end = 0;
    if(!clipVisibleColumns_(x, w, col_start, col_end)) return;

    const int16_t yOffset = (int16_t)(y & 7);
    const int16_t sRow = (int16_t)(y >> 3);
    const int16_t pages = (h + 7) >> 3;
    const int16_t maxRow = (HEIGHT >> 3);
    uint8_t page_masks[32];
    buildPageMasks_(page_masks, pages, h);

    for(int16_t i = col_start; i < col_end; i++) {
        const int16_t sx = (int16_t)(x + i);
        uint8_t* dst_col = sBuffer + sx;

        const uint8_t* scol = sprite + i;
        const uint8_t* mcol = mask + i;

        if(yOffset == 0) {
            int16_t row = sRow;
            int16_t p = 0;

            if(row < 0) {
                p = (int16_t)(-row);
                if(p >= pages) continue;
                row = 0;
                scol += (int32_t)p * w;
                mcol += (int32_t)p * w;
            }
            if(row >= maxRow) continue;

            int16_t p_end = pages;
            if(row + (p_end - p) > maxRow) {
                p_end = (int16_t)(p + (maxRow - row));
            }

            uint8_t* dst = dst_col + row * WIDTH;
            for(; p < p_end; ++p, scol += w, mcol += w, dst += WIDTH) {
                uint8_t s = pgm_read_byte(scol);
                uint8_t m = pgm_read_byte(mcol);
                const uint8_t srcMask = page_masks[p];
                s &= srcMask;
                m &= srcMask;
                *dst = (uint8_t)((*dst & (uint8_t)~m) | (s & m));
            }
        } else {
            for(int16_t p = 0; p < pages; p++, scol += w, mcol += w) {
                uint8_t s = pgm_read_byte(scol);
                uint8_t m = pgm_read_byte(mcol);

                const uint8_t srcMask = page_masks[p];
                s &= srcMask;
                m &= srcMask;

                const int16_t row = (int16_t)(sRow + p);

                const uint8_t slo = (uint8_t)(s << yOffset);
                const uint8_t shi = (uint8_t)(s >> (8 - yOffset));
                const uint8_t mlo = (uint8_t)(m << yOffset);
                const uint8_t mhi = (uint8_t)(m >> (8 - yOffset));

                if((uint16_t)row < (uint16_t)maxRow) {
                    uint8_t* dst = &dst_col[row * WIDTH];
                    *dst = (uint8_t)((*dst & (uint8_t)~mlo) | (slo & mlo));
                }
                const int16_t row2 = (int16_t)(row + 1);
                if((uint16_t)row2 < (uint16_t)maxRow) {
                    uint8_t* dst2 = &dst_col[row2 * WIDTH];
                    *dst2 = (uint8_t)((*dst2 & (uint8_t)~mhi) | (shi & mhi));
                }
            }
        }
    }
}

uint8_t Arduboy2Base::mapInputToArduboyMask_(uint8_t in) {
    uint8_t out = 0;
    if(in & INPUT_UP) out |= UP_BUTTON;
    if(in & INPUT_DOWN) out |= DOWN_BUTTON;
    if(in & INPUT_LEFT) out |= LEFT_BUTTON;
    if(in & INPUT_RIGHT) out |= RIGHT_BUTTON;
#ifdef ARDULIB_SWAP_AB
    if(in & INPUT_B) out |= A_BUTTON;
    if(in & INPUT_A) out |= B_BUTTON;
#else
    if(in & INPUT_B) out |= B_BUTTON;
    if(in & INPUT_A) out |= A_BUTTON;
#endif
    return out;
}

void Arduboy2Base::printUnsigned_(unsigned long value) {
    char tmp[10];
    uint8_t len = 0;
    do {
        tmp[len++] = (char)('0' + (value % 10ul));
        value /= 10ul;
    } while(value && (len < sizeof(tmp)));

    while(len) {
        write((uint8_t)tmp[--len]);
    }
}
