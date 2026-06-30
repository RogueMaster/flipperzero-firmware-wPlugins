#pragma once
#include <cstddef>

// Lightweight callback structs
// Each holds a plain function pointer and a void* context (for captured state).

// Forward declarations
class Entity;
class Game;
class Draw;
class Level;

struct CallbackVoid
{
    void (*fn)(void *) = nullptr;
    void *ctx = nullptr;
    void operator()() const
    {
        if (fn)
            fn(ctx);
    }
    explicit operator bool() const { return fn != nullptr; }
};

struct CallbackEntityGame
{
    void (*fn)(Entity *, Game *, void *) = nullptr;
    void *ctx = nullptr;
    CallbackEntityGame() = default;
    CallbackEntityGame(std::nullptr_t) : fn(nullptr), ctx(nullptr) {}
    void operator()(Entity *e, Game *g) const
    {
        if (fn)
            fn(e, g, ctx);
    }
    explicit operator bool() const { return fn != nullptr; }
};

struct CallbackEntityDrawGame
{
    void (*fn)(Entity *, Draw *, Game *, void *) = nullptr;
    void *ctx = nullptr;
    CallbackEntityDrawGame() = default;
    CallbackEntityDrawGame(std::nullptr_t) : fn(nullptr), ctx(nullptr) {}
    void operator()(Entity *e, Draw *d, Game *g) const
    {
        if (fn)
            fn(e, d, g, ctx);
    }
    explicit operator bool() const { return fn != nullptr; }
};

struct CallbackEntityEntityGame
{
    void (*fn)(Entity *, Entity *, Game *, void *) = nullptr;
    void *ctx = nullptr;
    CallbackEntityEntityGame() = default;
    CallbackEntityEntityGame(std::nullptr_t) : fn(nullptr), ctx(nullptr) {}
    void operator()(Entity *e1, Entity *e2, Game *g) const
    {
        if (fn)
            fn(e1, e2, g, ctx);
    }
    explicit operator bool() const { return fn != nullptr; }
};

struct CallbackLevel
{
    void (*fn)(Level &, void *) = nullptr;
    void *ctx = nullptr;
    void operator()(Level &l) const
    {
        if (fn)
            fn(l, ctx);
    }
    explicit operator bool() const { return fn != nullptr; }
};
