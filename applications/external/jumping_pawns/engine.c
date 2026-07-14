#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>
#include "engine.h"

void find_ai_jumps(
    JumpingPawnsModel* model,
    int board[11][6],
    int from_x,
    int from_y,
    int x,
    int y,
    int player_value,
    AIMoveStructure* move_list,
    size_t* move_count,
    size_t max_moves,
    bool visited[11][6]) {
    int dx[] = {0, 0, -1, 1}; // directions: up, down, left, right
    int dy[] = {-1, 1, 0, 0};

    visited[y][x] = true;

    for(int dir = 0; dir < 4; dir++) {
        int first_pawn_x = x + dx[dir];
        int first_pawn_y = y + dy[dir];

        // Must start with a pawn in this direction
        if(first_pawn_x < 0 || first_pawn_x >= 6 || first_pawn_y < 0 || first_pawn_y >= 11 ||
           board[first_pawn_y][first_pawn_x] == 0 || board[first_pawn_y][first_pawn_x] == 3) {
            continue;
        }

        // Step past contiguous pawns
        int next_x = first_pawn_x + dx[dir];
        int next_y = first_pawn_y + dy[dir];
        while(next_x >= 0 && next_x < 6 && next_y >= 0 && next_y < 11 &&
              board[next_y][next_x] != 0 && board[next_y][next_x] != 3) {
            next_x += dx[dir];
            next_y += dy[dir];
        }

        // Check landing square
        int landing_x = next_x;
        int landing_y = next_y;

        // Must jump at least one pawn and land in bounds on empty square
        if((abs(landing_x - x) <= 1 && abs(landing_y - y) <= 1) || landing_x < 0 ||
           landing_x >= 6 || landing_y < 0 || landing_y >= 11 ||
           board[landing_y][landing_x] != 0 || visited[landing_y][landing_x]) {
            continue;
        }

        // Temporarily remove jumped pawns for this direction
        int jumped_pawns[11][6] = {0};
        int temp_x = first_pawn_x;
        int temp_y = first_pawn_y;
        while(temp_x >= 0 && temp_x < 6 && temp_y >= 0 && temp_y < 11 &&
              board[temp_y][temp_x] != 0 && board[temp_y][temp_x] != 3) {
            jumped_pawns[temp_y][temp_x] = board[temp_y][temp_x];
            board[temp_y][temp_x] = 0;
            temp_x += dx[dir];
            temp_y += dy[dir];
        }

        // Store the move
        if(*move_count < max_moves) {
            move_list[(*move_count)++] = (AIMoveStructure){
                .from_x = from_x,
                .from_y = from_y,
                .to_x = landing_x,
                .to_y = landing_y,
            };
        }

        // Recurse from the new landing position
        find_ai_jumps(
            model,
            board,
            from_x,
            from_y,
            landing_x,
            landing_y,
            player_value,
            move_list,
            move_count,
            max_moves,
            visited);

        // Restore jumped pawns
        temp_x = first_pawn_x;
        temp_y = first_pawn_y;
        while(temp_x >= 0 && temp_x < 6 && temp_y >= 0 && temp_y < 11 &&
              jumped_pawns[temp_y][temp_x] != 0) {
            board[temp_y][temp_x] = jumped_pawns[temp_y][temp_x];
            temp_x += dx[dir];
            temp_y += dy[dir];
        }
    }

    visited[y][x] = false;
}

size_t generate_legal_ai_moves(
    JumpingPawnsModel* model,
    int player_value,
    AIMoveStructure* move_list,
    size_t max_moves) {
    size_t move_count = 0;

    for(int sy = 0; sy < 11; sy++) {
        for(int sx = 0; sx < 6; sx++) {
            if(model->board_state[sy][sx] != player_value) continue;

            bool visited[11][6] = {false};
            find_ai_jumps(
                model,
                model->board_state,
                sx,
                sy,
                sx,
                sy,
                player_value,
                move_list,
                &move_count,
                max_moves,
                visited);
        }
    }

    return move_count;
}

int count_pawn_moves(JumpingPawnsModel* model, int y, int x) {
    int move_count = 0;
    const int dx[] = {0, 0, -1, 1};
    const int dy[] = {-1, 1, 0, 0};

    for(int dir = 0; dir < 4; dir++) {
        int nx = x + dx[dir];
        int ny = y + dy[dir];

        // Adjacent simple move
        if(ny >= 0 && ny < 11 && nx >= 0 && nx < 6 && model->board_state[ny][nx] == 0) {
            move_count++;
        }

        // Check jump over clump
        int cx = nx;
        int cy = ny;
        bool found_clump = false;
        while(cx >= 0 && cx < 6 && cy >= 0 && cy < 11 && model->board_state[cy][cx] != 0 &&
              model->board_state[cy][cx] != 3) {
            cx += dx[dir];
            cy += dy[dir];
            found_clump = true;
        }
        if(found_clump && cx >= 0 && cx < 6 && cy >= 0 && cy < 11 &&
           model->board_state[cy][cx] == 0) {
            move_count++;
        }
    }
    return move_count;
}

int count_forward_moves(JumpingPawnsModel* model, int y, int x, uint8_t piece) {
    int moves = 0;
    // Assuming AI is moving down (y+)
    static const int dirs_ai[4][2] = {{1, 0}, {1, 1}, {1, -1}, {-1, 0}};
    static const int dirs_op[4][2] = {{-1, 0}, {-1, 1}, {-1, -1}, {1, 0}};
    const int(*dirs)[2] = (piece == 2) ? dirs_ai : dirs_op;
    for(int i = 0; i < 3; i++) { // only check forward & diagonals
        int ny = y + dirs[i][0];
        int nx = x + dirs[i][1];
        if(ny >= 0 && ny < 11 && nx >= 0 && nx < 6) {
            if(model->board_state[ny][nx] == 0) moves++;
        }
    }
    return moves;
}

int evaluate_board(JumpingPawnsModel* model) {
    int score = 0;
    int isolation_penalty = 15;
    int short_l_penalty = 10;
    int pair_penalty = 10;

#define IN_BOUNDS(_y, _x) ((_y) >= 0 && (_y) < 11 && (_x) >= 0 && (_x) < 6)

    // Detect late game
    int total_pawns = 0;
    for(int yy = 0; yy < 11; yy++) {
        for(int xx = 0; xx < 6; xx++) {
            if(model->board_state[yy][xx] == 1 || model->board_state[yy][xx] == 2) total_pawns++;
        }
    }
    bool late_game = total_pawns < 10;

    for(int y = 0; y < 11; y++) {
        for(int x = 0; x < 6; x++) {
            uint8_t piece = model->board_state[y][x];
            if(piece != 1 && piece != 2) continue;

            // --- Forward progress scoring ---
            if(piece == 2) {
                score += (y * y * 2); // stronger forward emphasis

                if(y == 8) score += 50; // strong final approach bonus
                if(y >= 9) score += 200; // finishing rows

                // Very strong penalty for staying back
                if(y <= 1)
                    score -= 50; // don't sit on starting 2 rows
                else if(y == 2)
                    score -= 20; // smaller penalty for staying in 3rd row

                // Bonus for leaving start zone
                if(y > 0 && y <= 3) score += 10; // bonus for 4th row or greater

                if(late_game && y >= 3 && y <= 7)
                    score -=
                        30; // mid-board camping late game -- needs to be implemented sooner(?)
            } else {
                int dist = 10 - y;
                score -= (dist * dist * 2);
                if(y <= 1) score -= 30;
                if(y >= 8) score += 10;
            }

            // --- Neighbor connectivity ---
            // Helper vars
            int left = (x > 0 && model->board_state[y][x - 1] == piece);
            int right = (x < 5 && model->board_state[y][x + 1] == piece);
            int up = (y > 0 && model->board_state[y - 1][x] == piece);
            int down = (y < 10 && model->board_state[y + 1][x] == piece);

            int connections = left + right + up + down;
            if(connections >= 2) {
                if(piece == 2)
                    score += 3;
                else
                    score -= 3;
            }

            // --- Isolation penalty ---
            if(!left && !right && !up && !down) {
                int pen = isolation_penalty;
                // No isolation penalty for AI pawns in start zone
                if(piece == 2 && y <= 2) pen = 0;
                if(late_game && piece == 2) pen /= 2;
                if(piece == 2)
                    score -= pen;
                else
                    score += pen;
            }

            // --- Horizontal pair isolation ---
            if(right) {
                if(x == 0 || model->board_state[y][x - 1] != piece) {
                    if((x + 2 >= 6) || model->board_state[y][x + 2] != piece) {
                        int isolated = 1;
                        int coords[2][2] = {{y, x}, {y, x + 1}};
                        for(int i = 0; i < 2 && isolated; i++) {
                            int cy = coords[i][0], cx = coords[i][1];
                            const int ny[4] = {cy - 1, cy + 1, cy, cx};
                            const int nx[4] = {cx, cx, cx - 1, cx + 1};
                            for(int n = 0; n < 4; n++) {
                                int ny_ = ny[n], nx_ = nx[n];
                                if(!IN_BOUNDS(ny_, nx_)) continue;
                                if((ny_ == coords[0][0] && nx_ == coords[0][1]) ||
                                   (ny_ == coords[1][0] && nx_ == coords[1][1]))
                                    continue;
                                if(model->board_state[ny_][nx_] == piece) {
                                    isolated = 0;
                                    break;
                                }
                            }
                        }
                        if(isolated) {
                            if(piece == 2)
                                score -= pair_penalty;
                            else
                                score += pair_penalty;
                        }
                    }
                }
            }

            // --- Short L check ---
            int is_l_isolated(int coords[3][2]) {
                for(int i = 0; i < 3; i++) {
                    int cy = coords[i][0], cx = coords[i][1];
                    const int ny[4] = {cy - 1, cy + 1, cy, cy};
                    const int nx[4] = {cx, cx, cx - 1, cx + 1};
                    for(int n = 0; n < 4; n++) {
                        int ny_ = ny[n], nx_ = nx[n];
                        if(!IN_BOUNDS(ny_, nx_)) continue;
                        int skip = 0;
                        for(int k = 0; k < 3; k++) {
                            if(coords[k][0] == ny_ && coords[k][1] == nx_) {
                                skip = 1;
                                break;
                            }
                        }
                        if(skip) continue;
                        if(model->board_state[ny_][nx_] == piece) return 0;
                    }
                }
                return 1;
            }

            if(right && y > 0) {
                if(model->board_state[y - 1][x] == piece &&
                   model->board_state[y - 1][x + 1] != piece) {
                    int l_coords[3][2] = {{y, x}, {y, x + 1}, {y - 1, x}};
                    if(is_l_isolated(l_coords)) {
                        if(piece == 2)
                            score -= short_l_penalty;
                        else
                            score += short_l_penalty;
                    }
                }
                if(y < 10 && model->board_state[y + 1][x] == piece &&
                   model->board_state[y + 1][x + 1] != piece) {
                    int l_coords[3][2] = {{y, x}, {y, x + 1}, {y + 1, x}};
                    if(is_l_isolated(l_coords)) {
                        if(piece == 2)
                            score -= short_l_penalty;
                        else
                            score += short_l_penalty;
                    }
                }
            }

            if(down && x > 0) {
                if(model->board_state[y][x - 1] == piece &&
                   model->board_state[y + 1][x - 1] != piece) {
                    int l_coords[3][2] = {{y, x}, {y + 1, x}, {y, x - 1}};
                    if(is_l_isolated(l_coords)) {
                        if(piece == 2)
                            score -= short_l_penalty;
                        else
                            score += short_l_penalty;
                    }
                }
                if(x < 5 && model->board_state[y][x + 1] == piece &&
                   model->board_state[y + 1][x + 1] != piece) {
                    int l_coords[3][2] = {{y, x}, {y + 1, x}, {y, x + 1}};
                    if(is_l_isolated(l_coords)) {
                        if(piece == 2)
                            score -= short_l_penalty;
                        else
                            score += short_l_penalty;
                    }
                }
            }

            // --- Forward-biased mobility ---
            int mobility = count_pawn_moves(model, y, x);
            int forward_moves = count_forward_moves(model, y, x, piece);
            int sideways_moves = mobility - forward_moves;
            if(mobility == 0) {
                if(piece == 2)
                    score -= 20;
                else
                    score += 20;
            } else {
                int forward_weight = (y <= 3) ? 6 : 5; // early game prefers forward more
                if(piece == 2)
                    score += (forward_moves * forward_weight) + (sideways_moves * 1);
                else
                    score -= (forward_moves * forward_weight) + (sideways_moves * 1);
            }
        }
    }

#undef IN_BOUNDS
    return score;
}

void copy_model(JumpingPawnsModel* dest, JumpingPawnsModel* src) {
    for(int y = 0; y < 11; y++) {
        for(int x = 0; x < 6; x++) {
            dest->board_state[y][x] = src->board_state[y][x];
        }
    }
}

int minimax(JumpingPawnsModel* model, int depth, bool maximizingPlayer) {
    if(depth == 0) {
        return evaluate_board(model);
    }

    AIMoveStructure moves[64];
    size_t move_count = generate_legal_ai_moves(model, maximizingPlayer ? 2 : 1, moves, 64);

    if(move_count == 0) {
        return evaluate_board(model);
    }

    int best_score = maximizingPlayer ? -10000 : 10000;

    for(size_t i = 0; i < move_count; i++) {
        JumpingPawnsModel temp;
        copy_model(&temp, model);
        apply_ai_move(&temp, moves[i], maximizingPlayer ? 2 : 1);

        int score = minimax(&temp, depth - 1, !maximizingPlayer);

        if(maximizingPlayer) {
            if(score > best_score) best_score = score;
        } else {
            if(score < best_score) best_score = score;
        }
    }

    return best_score;
}

void apply_ai_move(JumpingPawnsModel* model, AIMoveStructure move, int player_value) {
    model->board_state[move.from_y][move.from_x] = 0;
    model->board_state[move.to_y][move.to_x] = player_value;
}

void ai_move(JumpingPawnsModel* model) {
    AIMoveStructure moves[64];
    size_t count = generate_legal_ai_moves(model, 2, moves, 64);
    if(count == 0) return;
    if(model->game_over) return;

    int depth = model->difficulty_level + 1;

    // --- Endgame detection ---
    int ai_goal_pawns = 0;
    int player_goal_pawns = 0;

    for(int y = 0; y < 11; y++) {
        for(int x = 0; x < 6; x++) {
            if(model->board_state[y][x] == 2 && (y == 9 || y == 10)) ai_goal_pawns++;
            if(model->board_state[y][x] == 1 && (y == 0 || y == 1)) player_goal_pawns++;
        }
    }

    // If 10 pawns are in final 2 rows, increase depth if Easy mode is enabled
    if(ai_goal_pawns >= 10 || player_goal_pawns >= 10) {
        if(depth < 3) {
            depth = 3;
        }
    }

    int best_score = -10000;
    AIMoveStructure best_move = moves[0];

    for(size_t i = 0; i < count; i++) {
        JumpingPawnsModel temp;
        copy_model(&temp, model);
        apply_ai_move(&temp, moves[i], 2);
        int score = minimax(&temp, depth, false);

        if(score > best_score) {
            best_score = score;
            best_move = moves[i];
        }
    }

    model->is_ai_thinking = false;
    apply_ai_move(model, best_move, 2);
}
