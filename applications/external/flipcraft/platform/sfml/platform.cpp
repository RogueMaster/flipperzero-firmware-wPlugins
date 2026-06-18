#include "platform.h"

#include "../../game/game.h"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <new>
#include <thread>

namespace flipcraft {
namespace platform {

static constexpr unsigned SCALE = 4;
static constexpr unsigned WINDOW_W = SCREEN_WIDTH * SCALE;
static constexpr unsigned WINDOW_H = SCREEN_HEIGHT * SCALE;
static constexpr std::chrono::milliseconds GAME_TICK_DELAY{33};

struct SfmlFile {
    std::fstream stream;
};

static void* fsOpen(void*, const char* path, FileMode mode) {
    auto* file = new(std::nothrow) SfmlFile();
    if(!file) return nullptr;

    std::ios::openmode open_mode = std::ios::binary;
    switch(mode) {
    case FileMode::Read:
        open_mode |= std::ios::in;
        break;
    case FileMode::WriteTruncate:
        open_mode |= std::ios::out | std::ios::trunc;
        if(auto parent = std::filesystem::path(path).parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        break;
    case FileMode::ReadWriteExisting:
        open_mode |= std::ios::in | std::ios::out;
        break;
    }

    file->stream.open(path, open_mode);
    if(!file->stream.is_open()) {
        delete file;
        return nullptr;
    }
    return file;
}

static void fsClose(void*, void* handle) {
    auto* file = reinterpret_cast<SfmlFile*>(handle);
    if(!file) return;
    file->stream.close();
    delete file;
}

static bool fsSeek(void*, void* handle, uint32_t offset) {
    auto* file = reinterpret_cast<SfmlFile*>(handle);
    if(!file) return false;
    file->stream.clear();
    file->stream.seekg(offset, std::ios::beg);
    file->stream.seekp(offset, std::ios::beg);
    return !file->stream.fail();
}

static size_t fsRead(void*, void* handle, void* data, size_t size) {
    auto* file = reinterpret_cast<SfmlFile*>(handle);
    if(!file) return 0;
    file->stream.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<size_t>(file->stream.gcount());
}

static size_t fsWrite(void*, void* handle, const void* data, size_t size) {
    auto* file = reinterpret_cast<SfmlFile*>(handle);
    if(!file) return 0;
    file->stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return file->stream.fail() ? 0 : size;
}

static uint32_t fsSize(void*, void* handle) {
    auto* file = reinterpret_cast<SfmlFile*>(handle);
    if(!file) return 0;
    file->stream.clear();
    auto pos = file->stream.tellg();
    file->stream.seekg(0, std::ios::end);
    uint32_t size = static_cast<uint32_t>(file->stream.tellg());
    file->stream.seekg(pos);
    file->stream.seekp(pos);
    return size;
}

static void fsSync(void*, void* handle) {
    auto* file = reinterpret_cast<SfmlFile*>(handle);
    if(file) file->stream.flush();
}

struct KeyboardState {
    bool w = false;
    bool a = false;
    bool s = false;
    bool d = false;
    bool lshift = false;
    bool rshift = false;
    bool lcontrol = false;
    bool rcontrol = false;
};

static bool keyDown(sf::Keyboard::Key key) {
    return sf::Keyboard::isKeyPressed(key);
}

static void setKey(KeyboardState& keys, sf::Keyboard::Key code, bool pressed) {
    switch(code) {
    case sf::Keyboard::Key::W:
        keys.w = pressed;
        break;
    case sf::Keyboard::Key::A:
        keys.a = pressed;
        break;
    case sf::Keyboard::Key::S:
        keys.s = pressed;
        break;
    case sf::Keyboard::Key::D:
        keys.d = pressed;
        break;
    case sf::Keyboard::Key::LShift:
        keys.lshift = pressed;
        break;
    case sf::Keyboard::Key::RShift:
        keys.rshift = pressed;
        break;
    case sf::Keyboard::Key::LControl:
        keys.lcontrol = pressed;
        break;
    case sf::Keyboard::Key::RControl:
        keys.rcontrol = pressed;
        break;
    default:
        break;
    }
}

static Input buildInput(Game& game, const KeyboardState& keys, const sf::Event* event) {
    Input in{};
    bool in_menu = game.screenId != SCR_PLAY;

    if(in_menu) {
        if(const auto* key = event ? event->getIf<sf::Event::KeyPressed>() : nullptr) {
            if(key->code == sf::Keyboard::Key::W) in.navY = 1;
            if(key->code == sf::Keyboard::Key::S) in.navY = -1;
            if(key->code == sf::Keyboard::Key::A) in.navX = -1;
            if(key->code == sf::Keyboard::Key::D) in.navX = 1;
            if(key->code == sf::Keyboard::Key::Space || key->code == sf::Keyboard::Key::Enter) {
                in.menuSelect = true;
            }
            if(key->code == sf::Keyboard::Key::LShift || key->code == sf::Keyboard::Key::RShift) {
                in.openInventory = true;
            }
        }
    } else {
        if(keys.w || keyDown(sf::Keyboard::Key::W)) in.forward = 8;
        if(keys.s || keyDown(sf::Keyboard::Key::S)) in.forward = -8;
        if(keys.a || keyDown(sf::Keyboard::Key::A)) in.turn = 1;
        if(keys.d || keyDown(sf::Keyboard::Key::D)) in.turn = -1;
        in.crouch = keys.lshift || keys.rshift ||
                    keyDown(sf::Keyboard::Key::LShift) ||
                    keyDown(sf::Keyboard::Key::RShift);
        in.breakPressed = keys.lcontrol || keys.rcontrol ||
                          keyDown(sf::Keyboard::Key::LControl) ||
                          keyDown(sf::Keyboard::Key::RControl);

        if(const auto* key = event ? event->getIf<sf::Event::KeyPressed>() : nullptr) {
            if(key->code == sf::Keyboard::Key::Space) in.jump = true;
            if(key->code == sf::Keyboard::Key::E || key->code == sf::Keyboard::Key::Tab) {
                in.openInventory = true;
            }
            if(key->code == sf::Keyboard::Key::F || key->code == sf::Keyboard::Key::Enter) {
                in.placePressed = true;
            }
            if(key->code == sf::Keyboard::Key::Up) in.pitch = 1;
            if(key->code == sf::Keyboard::Key::Down) in.pitch = -1;
            if(key->code == sf::Keyboard::Key::Q || key->code == sf::Keyboard::Key::Left) {
                in.slotScroll = -1;
            }
            if(key->code == sf::Keyboard::Key::R || key->code == sf::Keyboard::Key::Right) {
                in.slotScroll = 1;
            }
        }
    }

    return in;
}

static void drawFramebuffer(sf::Image& image, const Framebuffer& fb) {
    for(unsigned y = 0; y < SCREEN_HEIGHT; ++y) {
        for(unsigned x = 0; x < SCREEN_WIDTH; ++x) {
            image.setPixel({x, y}, fb.px[y][x] ? sf::Color::Black : sf::Color::White);
        }
    }
}

int32_t run(Game& game) {
    static const FileSystem files = {
        nullptr,
        fsOpen,
        fsClose,
        fsSeek,
        fsRead,
        fsWrite,
        fsSize,
        fsSync,
    };
    GameConfig config = {&files, "platform/sfml/world.fcw"};
    if(!game.setup(config)) return -1;
    game.frame(Input{});

    sf::RenderWindow window(sf::VideoMode({WINDOW_W, WINDOW_H}), "Flipcraft");
    window.setVerticalSyncEnabled(false);

    const sf::Vector2u screen_size{SCREEN_WIDTH, SCREEN_HEIGHT};
    sf::Image image(screen_size, sf::Color::White);
    sf::Texture texture(screen_size);
    sf::Sprite sprite(texture);
    sprite.setScale({static_cast<float>(SCALE), static_cast<float>(SCALE)});

    KeyboardState keys;
    while(window.isOpen()) {
        bool had_event_input = false;
        Input event_input{};

        while(const std::optional event = window.pollEvent()) {
            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if(key) setKey(keys, key->code, true);
            if(const auto* released = event->getIf<sf::Event::KeyReleased>()) {
                setKey(keys, released->code, false);
            }

            if(event->is<sf::Event::Closed>() ||
               (key && key->code == sf::Keyboard::Key::Escape)) {
                window.close();
                break;
            }

            Input next = buildInput(game, keys, &*event);
            event_input.forward = next.forward ? next.forward : event_input.forward;
            event_input.turn = next.turn ? next.turn : event_input.turn;
            event_input.pitch = next.pitch ? next.pitch : event_input.pitch;
            event_input.slotScroll = next.slotScroll ? next.slotScroll : event_input.slotScroll;
            event_input.jump = event_input.jump || next.jump;
            event_input.crouch = event_input.crouch || next.crouch;
            event_input.breakPressed = event_input.breakPressed || next.breakPressed;
            event_input.placePressed = event_input.placePressed || next.placePressed;
            event_input.openInventory = event_input.openInventory || next.openInventory;
            event_input.navX = next.navX ? next.navX : event_input.navX;
            event_input.navY = next.navY ? next.navY : event_input.navY;
            event_input.menuSelect = event_input.menuSelect || next.menuSelect;
            event_input.distribute = event_input.distribute || next.distribute;
            had_event_input = true;
        }

        if(!window.isOpen()) break;

        Input held_input = buildInput(game, keys, nullptr);
        Input in = held_input;
        if(had_event_input) {
            in.forward = event_input.forward ? event_input.forward : in.forward;
            in.turn = event_input.turn ? event_input.turn : in.turn;
            in.pitch = event_input.pitch ? event_input.pitch : in.pitch;
            in.slotScroll = event_input.slotScroll ? event_input.slotScroll : in.slotScroll;
            in.jump = event_input.jump || in.jump;
            in.crouch = event_input.crouch || in.crouch;
            in.breakPressed = event_input.breakPressed || in.breakPressed;
            in.placePressed = event_input.placePressed || in.placePressed;
            in.openInventory = event_input.openInventory || in.openInventory;
            in.navX = event_input.navX ? event_input.navX : in.navX;
            in.navY = event_input.navY ? event_input.navY : in.navY;
            in.menuSelect = event_input.menuSelect || in.menuSelect;
        }

        game.frame(in);

        drawFramebuffer(image, game.fb);
        texture.update(image);
        window.clear(sf::Color::White);
        window.draw(sprite);
        window.display();

        std::this_thread::sleep_for(GAME_TICK_DELAY);
    }

    game.shutdown();
    return 0;
}

}
}
