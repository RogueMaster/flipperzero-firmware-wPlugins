#include <toolbox/dir_walk.h>
#include <toolbox/path.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/args.h>

#include "pinball0.h"
#include "graphics.h"
#include "table.h"
// #include "notifications.h"

// Table defaults
#define LIVES     3
#define LIVES_POS Vec2(20, 20)

void Lives::draw(Canvas* canvas) {
    // we don't draw the last one, as it's in play!
    constexpr float r = 20;
    if(display && value > 0) {
        float x = p.x;
        float y = p.y;
        float x_off = alignment == Align::Horizontal ? (2 * r) + r : 0;
        float y_off = alignment == Align::Vertical ? (2 * r) + r : 0;
        for(auto l = 0; l < value - 1; x += x_off, y += y_off, l++) {
            gfx_draw_disc(canvas, x + r, y + r, 20);
        }
    }
}

void Score::draw(Canvas* canvas) {
    if(display) {
        char buf[32];
        snprintf(buf, 32, "%d", value);
        gfx_draw_str(canvas, p.x, p.y, AlignRight, AlignTop, buf);
    }
}

Table::Table()
    : game_over(false)
    , balls_released(false)
    , plunger(nullptr)
    , tilt_detect_enabled(true)
    , last_bump(furi_get_tick())
    , bump_count(0) {
}

Table::~Table() {
    for(size_t i = 0; i < objects.size(); i++) {
        delete objects[i];
    }
    if(plunger != nullptr) {
        delete plunger;
    }
}

void Table::draw(Canvas* canvas) {
    lives.draw(canvas);

    // da balls
    for(auto& b : balls) {
        b.draw(canvas);
    }

    // loop through objects on the table and draw them
    for(auto& o : objects) {
        o->draw(canvas);
    }

    // now draw flippers
    for(auto& f : flippers) {
        f.draw(canvas);
    }

    // is there a plunger in the house?
    if(plunger) {
        plunger->draw(canvas);
    }

    score.draw(canvas);
}

Table* table_init_table_select(void* ctx) {
    UNUSED(ctx);
    Table* table = new Table();

    table->balls.push_back(Ball(Vec2(20, 880), 35));
    table->balls.back().add_velocity(Vec2(7, 0), .10f);
    table->balls.push_back(Ball(Vec2(610, 920), 30));
    table->balls.back().add_velocity(Vec2(-8, 0), .10f);
    table->balls.push_back(Ball(Vec2(250, 980), 20));
    table->balls.back().add_velocity(Vec2(10, 0), .10f);

    table->balls_released = true;

    Polygon* new_rail = new Polygon();
    new_rail->add_point({-1, 840});
    new_rail->add_point({-1, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({-1, 1280});
    new_rail->add_point({640, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({640, 1280});
    new_rail->add_point({640, 840});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    int gap = 8;
    int speed = 3;
    float top = 20;
    // right side
    table->objects.push_back(new Chaser(Vec2(32, top), Vec2(62, top), gap, speed));
    table->objects.push_back(new Chaser(Vec2(62, top), Vec2(62, 84), gap, speed));
    table->objects.push_back(new Chaser(Vec2(62, 84), Vec2(32, 84), gap, speed));

    // left side
    table->objects.push_back(new Chaser(Vec2(32, top), Vec2(1, top), gap, speed));
    table->objects.push_back(new Chaser(Vec2(1, top), Vec2(1, 84), gap, speed));
    table->objects.push_back(new Chaser(Vec2(1, 84), Vec2(32, 84), gap, speed));

    return table;
}

Table* table_init_table_error(void* ctx) {
    UNUSED(ctx);
    // PinballApp* pb = (PinballApp*)ctx;
    Table* table = new Table();

    table->balls.push_back(Ball(Vec2(20, 880), 30));
    table->balls.back().add_velocity(Vec2(7, 0), .10f);
    // table->balls.push_back(Ball(Vec2(610, 920), 30));
    // table->balls.back().add_velocity(Vec2(-8, 0), .10f);
    // table->balls.push_back(Ball(Vec2(250, 980), 20));
    // table->balls.back().add_velocity(Vec2(10, 0), .10f);

    table->balls_released = true;

    Polygon* new_rail = new Polygon();
    new_rail->add_point({-1, 840});
    new_rail->add_point({-1, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({-1, 1280});
    new_rail->add_point({640, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({640, 1280});
    new_rail->add_point({640, 840});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    int gap = 8;
    int speed = 3;
    float top = 20;

    table->objects.push_back(new Chaser(Vec2(2, top), Vec2(61, top), gap, speed, Chaser::SLASH));
    table->objects.push_back(new Chaser(Vec2(2, top), Vec2(2, 84), gap, speed, Chaser::SLASH));
    table->objects.push_back(new Chaser(Vec2(2, 84), Vec2(61, 84), gap, speed, Chaser::SLASH));
    table->objects.push_back(new Chaser(Vec2(61, top), Vec2(61, 84), gap, speed, Chaser::SLASH));

    return table;
}

Table* table_init_table_settings(void* ctx) {
    UNUSED(ctx);
    Table* table = new Table();

    // table->balls.push_back(Ball(Vec2(20, 880), 10));
    // table->balls.back().add_velocity(Vec2(7, 0), .10f);
    // table->balls.push_back(Ball(Vec2(610, 920), 10));
    // table->balls.back().add_velocity(Vec2(-8, 0), .10f);
    // table->balls.push_back(Ball(Vec2(250, 980), 10));
    // table->balls.back().add_velocity(Vec2(10, 0), .10f);

    table->balls_released = true;

    Polygon* new_rail = new Polygon();
    new_rail->add_point({-1, 840});
    new_rail->add_point({-1, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({-1, 1280});
    new_rail->add_point({640, 1280});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    new_rail = new Polygon();
    new_rail->add_point({640, 1280});
    new_rail->add_point({640, 840});
    new_rail->finalize();
    new_rail->hidden = true;
    table->objects.push_back(new_rail);

    return table;
}

bool table_load_table(void* ctx, size_t index) {
    PinballApp* pb = (PinballApp*)ctx;

    // read the index'th file in pb->table_list and allocate
    FURI_LOG_I(TAG, "Loading table %u", index);

    // if there's already a table loaded, free it
    if(pb->table) {
        delete pb->table;
        pb->table = nullptr;
    }

    switch(index) {
    case TABLE_SELECT:
        pb->table = table_init_table_select(ctx);
        break;
    case TABLE_ERROR:
        pb->table = table_init_table_error(ctx);
        break;
    case TABLE_SETTINGS:
        pb->table = table_init_table_settings(ctx);
        break;
    default:
        pb->table = table_load_table_from_file(pb, index - TABLE_INDEX_OFFSET);
        break;
    }
    return pb->table != NULL;
}
