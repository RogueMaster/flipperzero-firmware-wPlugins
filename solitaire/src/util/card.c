#include "card.h"
#include "../../assets.h"
#include "helpers.h"

static RenderSettings default_render = DEFAULT_RENDER;

static Buffer* letters[] = {
    (Buffer*)&sprite_2,
    (Buffer*)&sprite_3,
    (Buffer*)&sprite_4,
    (Buffer*)&sprite_5,
    (Buffer*)&sprite_6,
    (Buffer*)&sprite_7,
    (Buffer*)&sprite_8,
    (Buffer*)&sprite_9,
    (Buffer*)&sprite_10,
    (Buffer*)&sprite_J,
    (Buffer*)&sprite_Q,
    (Buffer*)&sprite_K,
    (Buffer*)&sprite_A,
};

static Buffer* suits[] = {
    (Buffer*)&sprite_hearths,
    (Buffer*)&sprite_spades,
    (Buffer*)&sprite_diamonds,
    (Buffer*)&sprite_clubs};

static Buffer* backSide = (Buffer*)&sprite_pattern_big;

void card_render_front(
    Card* c,
    int16_t x,
    int16_t y,
    bool selected,
    Buffer* buffer,
    uint8_t size_limit) {
    uint8_t height = y + fmin(size_limit, 22);

    buffer_draw_rbox(buffer, x, y, x + 16, height, White);
    buffer_draw_rbox_frame(buffer, x, y, x + 16, height, Black);

    Vector p = (Vector){(float)x + 6, (float)y + 5};
    buffer_draw_all(buffer, letters[c->value], &p, 0);

    p = (Vector){(float)x + 12, (float)y + 5};
    buffer_draw_all(buffer, suits[c->suit], &p, 0);

    if(size_limit > 8) {
        p = (Vector){(float)x + 10, (float)y + 16};
        buffer_draw_all(buffer, letters[c->value], &p, M_PI);
        p = (Vector){(float)x + 4, (float)y + 16};
        buffer_draw_all(buffer, suits[c->suit], &p, M_PI);
    }
    if(selected) {
        buffer_draw_box(buffer, x, y, x + 17, height + 1, Flip);
    }
}

void card_render_slot(int16_t x, int16_t y, bool selected, Buffer* buffer) {
    buffer_draw_rbox(buffer, x, y, x + 17, y + 23, Black);
    buffer_draw_rbox_frame(buffer, x + 2, y + 2, x + 14, y + 20, White);
    if(selected) buffer_draw_rbox(buffer, x + 1, y + 1, x + 16, y + 22, Flip);
}

void card_render_back(int16_t x, int16_t y, bool selected, Buffer* buffer, uint8_t size_limit) {
    uint8_t height = y + fmin(size_limit, 22);

    buffer_draw_rbox(buffer, x + 1, y + 1, x + 16, height, White);
    buffer_draw_rbox_frame(buffer, x, y, x + 16, height, Black);
    Vector pos = (Vector){(float)x + 9, (float)y + 11};
    check_pointer(buffer);
    check_pointer(backSide);
    check_pointer(&pos);
    check_pointer(&default_render);
    buffer_draw(buffer, backSide, &pos, 15, (int)fmin(size_limit, 22), 0, &default_render);
    if(selected) {
        buffer_draw_box(buffer, x, y, x + 17, height + 1, Flip);
    }
}

void card_try_render(
    Card* c,
    int16_t x,
    int16_t y,
    bool selected,
    Buffer* buffer,
    uint8_t size_limit) {
    if(c) {
        if(c->exposed)
            card_render_front(c, x, y, selected, buffer, size_limit);
        else
            card_render_back(x, y, selected, buffer, size_limit);
    } else {
        card_render_slot(x, y, selected, buffer);
    }
}

bool card_test_foundation(Card* data, Card* target) {
    if(!target || (target->value == -1 && target->suit == data->suit)) {
        return data->value == ACE;
    }
    return target->suit == data->suit && ((target->value + 1) % 13 == data->value % 13);
}

bool card_test_column(Card* data, Card* target) {
    if(!target) return data->value == KING;
    return target->suit % 2 == (data->suit + 1) % 2 && (data->value + 1) == target->value;
}

List* deck_generate(uint8_t deck_count) {
    List* deck = list_make();
    int cards_count = 52 * deck_count;
    uint8_t cards[cards_count];
    for(int i = 0; i < cards_count; i++)
        cards[i] = i % 52;
    srand(curr_time());

    //reorder
    for(int i = 0; i < cards_count; i++) {
        int r = i + (rand() % (cards_count - i));
        uint8_t card = cards[i];
        cards[i] = cards[r];
        cards[r] = card;
    }

    //Init deck list
    for(int i = 0; i < cards_count; i++) {
        Card* c = malloc(sizeof(Card));
        c->value = cards[i] % 13;
        c->suit = cards[i] / 13;
        c->exposed = false;
        list_push_back(c, deck);
    }

    return deck;
}

void deck_free(List* deck) {
    list_free(deck);
}

void deck_render_vertical(List* deck, uint8_t x, uint8_t y, int8_t selected, Buffer* buffer) {
    check_pointer(deck);
    check_pointer(buffer);
    uint8_t loop_end = deck->count;
    int8_t selection = loop_end - selected;
    uint8_t loop_start = MAX(loop_end - 4, 0);
    uint8_t position = 0;
    int8_t first_non_flipped;
    Card* first_non_flipped_card = deck_first_non_flipped(deck, &first_non_flipped);

    bool had_top = false;
    bool showDark = selection >= 0;

    if(first_non_flipped <= loop_start && selection != first_non_flipped &&
       first_non_flipped_card) {
        // Draw a card back if it is not the first card
        if(first_non_flipped > 0) {
            card_render_back(x, y + position, false, buffer, 5);
            // Increment loop start index and position
            position += 4;
            loop_start++;
            had_top = true;
        }

        // Draw the front side of the first non-flipped card
        card_try_render(
            first_non_flipped_card, x, y + position, false, buffer, deck->count == 1 ? 22 : 9);

        position += 8;
        loop_start++; // Increment loop start index
    }

    // Draw the selected card with adjusted visibility
    if(loop_start > selection) {
        if(!had_top && first_non_flipped > 0) {
            card_render_back(x, y + position, false, buffer, 5);
            position += 4;
            loop_start++;
        }

        Card* selected_card = (Card*)list_peek_index(deck, selection);
        check_pointer(selected_card);
        // Draw the front side of the selected card
        card_try_render(selected_card, x, y + position, showDark, buffer, 9);
        position += 8;
        loop_start++; // Increment loop start index
    }

    int height = 5;
    ListItem* curr = list_get_index(deck, loop_start);
    for(uint8_t i = loop_start; i < loop_end; i++) {
        check_pointer(curr);
        if(!curr) break;

        if(i >= loop_start && i < loop_end) {
            height = 5;
            if((i + 1) == loop_end)
                height = 22;
            else if(i == selection || i == first_non_flipped)
                height = 9;
            Card* c = (Card*)curr->data;
            check_pointer(c);
            card_try_render(c, x, y + position, i == selection && showDark, buffer, height);
            if(i == selection || i == first_non_flipped) position += 4;
            position += 4;
        }
        curr = curr->next;
    }
}

void deck_render(
    List* deck,
    DeckType type,
    int16_t x,
    int16_t y,
    int8_t selected,
    bool draw_empty,
    Buffer* buffer) {
    switch(type) {
    case Normal:
        card_try_render(list_peek_back(deck), x, y, selected == 1, buffer, 22);
        break;
    case Vertical:
        if(deck && deck->count > 0)
            deck_render_vertical(deck, x, y, selected, buffer);
        else if(draw_empty)
            card_render_slot(x, y, selected == 1, buffer);
        break;
    case Pile:
        break;
    }
}

Card* deck_first_non_flipped(List* deck, int8_t* index) {
    ListItem* curr = deck->head;
    for(int8_t i = 0; i < (int8_t)deck->count; i++) {
        if(!curr->data) break;
        Card* card = (Card*)curr->data;
        if(card->exposed) {
            (*index) = i;
            return card;
        }
        curr = curr->next;
    }
    (*index) = -1;
    return NULL;
}
