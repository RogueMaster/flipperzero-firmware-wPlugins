#pragma once
#include <vector>
#include "vec2.h"
#include <gui/canvas.h> // for Canvas*

#include "signals.h"

#define DEF_BALL_RADIUS   20
#define DEF_BUMPER_RADIUS 40
#define DEF_BUMPER_BOUNCE 1.0f
#define DEF_FLIPPER_SIZE  120
#define DEF_RAIL_BOUNCE   0.9f
#define DEF_TURBO_RADIUS  20
#define DEF_TURBO_BOOST   5

#define ARC_TANGENT_RESTITUTION 1.0f
#define ARC_NORMAL_RESTITUTION  0.8f

// A dynamic, moveable object with acceleration
class Object {
public:
    Object(const Vec2& p_, float r_);
    virtual ~Object() = default;

    // Verlet data
    Vec2 p; // position
    Vec2 prev_p; // previous position
    Vec2 a;
    float r;

    bool physical; // is this a real object that can be hit?
    float bounce; // < 1 dampens, > 1 adds power
    bool fixed; // should this move?
    int score; // incremental score for hitting this

    void update(float dt); // updates position
    inline void accelerate(const Vec2& da) {
        a += da;
    }
    inline void add_velocity(const Vec2& v, float dt) {
        prev_p -= v * dt;
    }

    virtual void draw(Canvas* canvas) = 0;
};

class Ball : public Object {
public:
    Ball(const Vec2& p_ = Vec2(), float r_ = DEF_BALL_RADIUS)
        : Object(p_, r_) {
    }
    void draw(Canvas* canvas);
};

class Flipper {
public:
    enum Side {
        LEFT,
        RIGHT
    };

    Flipper(const Vec2& p_, Side side, size_t size_ = DEF_FLIPPER_SIZE);

    void draw(Canvas* canvas);
    void update(float dt); // updates position to new position
    bool collide(Ball& ball);

    Vec2 get_tip() const;

    Vec2 p;
    Side side;
    size_t size;
    float r;

    float rest_angle;
    float max_rotation;
    float sign;
    float omega; // angular velocity

    float rotation;
    float current_omega;

    bool powered; // is this flipper being activated? i.e. is keypad pressed?

    int score;
    void (*notification)(void* app);
};

// A static object that never moves and can be any shape
class FixedObject {
public:
    FixedObject()
        : bounce(1.0f)
        , physical(true)
        , hidden(false)
        , score(0)
        , tx_id(INVALID_ID)
        , rx_id(INVALID_ID)
        , tx_type(SignalType::ALL)
        , notification(nullptr) {
    }
    virtual ~FixedObject() = default;

    float bounce;
    bool physical; // interacts with ball vs table decoration
    bool hidden; // do not draw
    int score;
    int tx_id;
    int rx_id;
    SignalType tx_type;

    void (*notification)(void* app);

    struct {
        bool physical;
        bool hidden;
    } saved;

    virtual void draw(Canvas* canvas) = 0;
    virtual bool collide(Ball& ball) = 0;
    virtual void reset_animation() {};
    virtual void step_animation() {};

    virtual void signal_receive();
    virtual void signal_send();

    virtual void save_state();
    virtual void reset_state();
};

class Polygon : public FixedObject {
public:
    Polygon()
        : FixedObject() {};

    std::vector<Vec2> points;
    std::vector<Vec2> normals;

    void draw(Canvas* canvas);
    bool collide(Ball& ball);
    void add_point(const Vec2& np) {
        points.push_back(np);
    }
    void finalize();
};

class Portal : public FixedObject {
public:
    Portal(const Vec2& a1_, const Vec2& a2_, const Vec2& b1_, const Vec2& b2_)
        : FixedObject()
        , a1(a1_)
        , a2(a2_)
        , b1(b1_)
        , b2(b2_) {
        score = 200;
    }
    Vec2 a1, a2; // portal 'a'
    Vec2 b1, b2; // portal 'b'
    Vec2 na, nb; // normals
    Vec2 au, bu; // unit vectors
    float amag, bmag; // length of portals
    bool bidirectional{true}; // TODO: ehhh?

    Vec2 enter_p; // where we entered portal
    size_t decay{0}; // used for animation

    void draw(Canvas* canvas);
    bool collide(Ball& ball);
    void reset_animation();
    void step_animation();
    void finalize();
};

class Arc : public FixedObject {
public:
    enum Surface {
        OUTSIDE,
        INSIDE,
        BOTH
    };

    Arc(const Vec2& p_,
        float r_,
        float s_ = 0,
        float e_ = (float)M_PI * 2,
        Surface surf_ = OUTSIDE);

    Vec2 p;
    float r;
    float start;
    float end;
    Surface surface;
    void draw(Canvas* canvas);
    bool collide(Ball& ball);
};

class Bumper : public Arc {
public:
    Bumper(const Vec2& p_, float r_);

    size_t decay;

    void draw(Canvas* canvas);
    void reset_animation();
    void step_animation();
};

class Plunger : public Object {
public:
    Plunger(const Vec2& p_);

    void draw(Canvas* canvas);

    int size; // how tall is it
    int compression; // how much is it pulled back?
};

// Simply displays a letter after a rollover
class Rollover : public FixedObject {
public:
    Rollover(const Vec2& p_, char c_)
        : FixedObject()
        , p(p_) {
        c[0] = c_;
        c[1] = '\0';
        score = 400;
    }

    Vec2 p;
    char c[2];
    bool activated{false};

    void draw(Canvas* canvas);
    bool collide(Ball& ball);

    void signal_receive();
    void signal_send();

    void reset_state();
};

class Turbo : public FixedObject {
public:
    Turbo(const Vec2& p_, float angle_, float boost_, float radius_)
        : FixedObject()
        , p(p_)
        , angle(angle_)
        , boost(boost_)
        , r(radius_) {
        // Our boost direction
        dir = Vec2(cosf(angle), -sinf(angle));

        // define the points of the chevrons at the 0 angle
        chevron_1[0] = Vec2(p.x, p.y - r);
        chevron_1[1] = Vec2(p.x + r, p.y);
        chevron_1[2] = Vec2(p.x, p.y + r);

        chevron_2[0] = Vec2(p.x - r, p.y - r);
        chevron_2[1] = Vec2(p.x, p.y);
        chevron_2[2] = Vec2(p.x - r, p.y + r);

        // rotate the chevrons to the correct angle
        for(size_t i = 0; i < 3; i++) {
            Vec2& v = chevron_1[i];
            Vec2 d = v - p;
            v.x = p.x + d.x * cosf(angle) - d.y * sinf(angle);
            v.y = p.y + d.x * -sinf(angle) + d.y * -cosf(angle);
        }
        for(size_t i = 0; i < 3; i++) {
            Vec2& v = chevron_2[i];
            Vec2 d = v - p;
            v.x = p.x + d.x * cosf(angle) - d.y * sinf(angle);
            v.y = p.y + d.x * -sinf(angle) + d.y * -cosf(angle);
        }
    }

    Vec2 p;
    float angle;
    float boost;
    float r;

    Vec2 dir; // unit normal of turbo direction

    Vec2 chevron_1[3];
    Vec2 chevron_2[3];

    void draw(Canvas* canvas);
    bool collide(Ball& ball);
};

// Visual item only - chase of dots in one direction
// AXIS-ALIGNED!
class Chaser : public Polygon {
public:
    enum Style {
        SIMPLE,
        SLASH
    };

    Chaser(const Vec2& p1, const Vec2& p2, size_t gap_ = 8, size_t speed_ = 3, Style style_ = SIMPLE)
        : Polygon()
        , tick(0)
        , offset(0)
        , gap(gap_)
        , speed(speed_)
        , style(style_) {
        physical = false;
        points.push_back(p1);
        points.push_back(p2);
    }

    size_t tick;
    size_t offset;
    size_t gap;
    size_t speed;
    Style style;

    void draw(Canvas* canvas);
    void step_animation();
};
