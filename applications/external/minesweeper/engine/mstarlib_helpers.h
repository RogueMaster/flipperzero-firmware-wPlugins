#ifndef MSTARLIB_HELPERS_H
#define MSTARLIB_HELPERS_H

#include "m-deque.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/** We can use this Point struct for the 2d position for the minesweeper game.
  * We define the necessary functions needed for this user defined type
  * so that we can make use of M*LIB's Red Black Tree and Double Ended
  * Queue for DFS and other algorithms for our MineSweeper game
  */

typedef struct {
    uint8_t x, y;
} Point;

typedef Point Point_t[1];

static inline void pointobj_init(Point_t dest) {
    dest->x = 0;
    dest->y = 0;
}

static inline void pointobj_clear(Point_t dest) {
    (void)dest;
}

static inline void pointobj_init_set(Point_t dest, const Point_t source) {
    dest->x = source->x;
    dest->y = source->y;
}

static inline void pointobj_set(Point_t dest, const Point_t source) {
    dest->x = source->x;
    dest->y = source->y;
}

static inline void pointobj_set_point(Point_t dest, Point value) {
    dest->x = value.x;
    dest->y = value.y;
}

static inline void pointobj_init_set_point(Point_t dest, Point val) {
    dest->x = val.x;
    dest->y = val.y;
}

static inline bool pointobj_equal_p(const Point_t a, const Point_t b) {
    return a->x == b->x && a->y == b->y;
}

static inline int pointobj_cmp(const Point_t a, const Point_t b) {
    if(a->x != b->x) return (a->x < b->x) ? -1 : 1;
    if(a->y != b->y) return (a->y < b->y) ? -1 : 1;
    return 0;
}

// This is the oplist needed to define the rb tree and dequeue and uses the/
// above functions.
#define POINT_OPLIST              \
    (TYPE(Point_t),               \
     INIT(pointobj_init),         \
     INIT_SET(pointobj_init_set), \
     SET(pointobj_set),           \
     CLEAR(pointobj_clear),       \
     EQUAL(pointobj_equal_p),     \
     CMP(pointobj_cmp))

// Example Macro defining the DEQ for Point that will be used as a double ended queue
DEQUE_DEF(point_deq, Point_t, POINT_OPLIST)

// Helper to convert the Point_t type to Point
static inline Point pointobj_get_point(const Point_t z) {
    return *z;
}

static inline size_t point_visited_bitset_bytes(uint16_t tile_count) {
    return (size_t)(tile_count + 7u) / 8u;
}

static inline void point_visited_clear(uint8_t* visited, uint16_t tile_count) {
    const size_t bytes = point_visited_bitset_bytes(tile_count);
    for(size_t i = 0; i < bytes; ++i) {
        visited[i] = 0u;
    }
}

static inline bool point_visited_test(const uint8_t* visited, uint16_t tile_index) {
    return (visited[tile_index >> 3u] & (uint8_t)(1u << (tile_index & 0x07u))) != 0u;
}

static inline void point_visited_set(uint8_t* visited, uint16_t tile_index) {
    visited[tile_index >> 3u] |= (uint8_t)(1u << (tile_index & 0x07u));
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MSTARLIB_HELPERS_H
