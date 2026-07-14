#include <stdio.h>

typedef struct {
    int from_x;
    int from_y;
    int to_x;
    int to_y;
} AIMoveStructure;

typedef struct {
    char* ai_or_player;
    char* whose_turn;
    int selected_x;
    int selected_y;
    int last_calculated_piece_x;
    int last_calculated_piece_y;
    bool is_ai_thinking;
    bool led_notifications;
    bool vibro_notifications;
    bool game_over;
    bool player_1_win;
    bool player_2_win;
    int difficulty_level;
    int board_state[11][6];
} JumpingPawnsModel;

// generates a list of legal moves the engine can make
size_t generate_legal_ai_moves(
    JumpingPawnsModel* model,
    int player_value,
    AIMoveStructure* move_list,
    size_t max_moves);
// actually make the move
void apply_ai_move(JumpingPawnsModel* model, AIMoveStructure move, int player_value);
// calls both funcs along with some other stuff
void ai_move(JumpingPawnsModel* model);
