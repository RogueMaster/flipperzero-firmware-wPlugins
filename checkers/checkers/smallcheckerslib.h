#ifndef SMALLCHECKERSLIB_H
#define SMALLCHECKERSLIB_H

/**
  @file smallcheckerslib.h

  Small single-header C99 checkers (draughts) library for Flipper Zero.
  Drop-in replacement for smallcheckerslib.h with the same SCL_ API surface
  used by flipcheckers_scene_1.c.

  Board layout: squares 0-63, same as chess (a1=0, h1=7, a8=56, h8=63).
  Only dark squares are used (standard checkers).

  Pieces (same char scheme as chess lib):
    'p' = black man   'P' = white man
    'k' = black king  'K' = white king
    '.' = empty
*/

#include <stdint.h>
#include <string.h>

// ---- compatibility stubs (used by flipcheckers_scene_1.c defines) ----
#ifndef SCL_DEBUG_AI
#define SCL_DEBUG_AI 0
#endif
#ifndef SCL_960_CASTLING
#define SCL_960_CASTLING 0
#endif
#ifndef SCL_EVALUATION_FUNCTION
#define SCL_EVALUATION_FUNCTION SCL_boardEvaluateStatic
#endif

// ---- constants ----
#define SCL_BOARD_SQUARES         64
#define SCL_BOARD_PICTURE_WIDTH   64
#define SCL_FEN_MAX_LENGTH        96
#define SCL_VALUE_PAWN            100

// Unused byte indices – kept so the model struct compiles
#define SCL_BOARD_ENPASSANT_CASTLE_BYTE 64
#define SCL_BOARD_MOVE_COUNT_BYTE       65
#define SCL_BOARD_PLY_BYTE              66
#define SCL_BOARD_MULTIJUMP_BYTE        67  // 255 = no multi-jump in progress, else = square of jumping piece
#define SCL_BOARD_STATE_SIZE            72

// Game states
#define SCL_GAME_STATE_PLAYING        0
#define SCL_GAME_STATE_WHITE_WIN      1
#define SCL_GAME_STATE_BLACK_WIN      2
#define SCL_GAME_STATE_DRAW_STALEMATE 3
#define SCL_GAME_STATE_DRAW_REPETITION 4
#define SCL_GAME_STATE_DRAW_DEAD      5
#define SCL_GAME_STATE_DRAW           6
#define SCL_GAME_STATE_DRAW_50        7

// Phase (unused but referenced in comments)
#define SCL_PHASE_OPENING  0
#define SCL_PHASE_ENDGAME  2

// ---- types ----
typedef uint8_t SCL_Board[SCL_BOARD_STATE_SIZE];
typedef uint8_t SCL_SquareSet[8]; // 64-bit bitmask

// Record: stores up to 200 half-moves (from, to pairs)
#define SCL_RECORD_MAX_MOVES 200
typedef struct {
    uint8_t from[SCL_RECORD_MAX_MOVES];
    uint8_t to[SCL_RECORD_MAX_MOVES];
    uint16_t len;
} SCL_RecordT;
typedef SCL_RecordT SCL_Record[1];

typedef struct {
    SCL_Board board;
    SCL_Record record;
    uint16_t ply;
    uint8_t state;
    // undo stack (store board snapshots)
    SCL_Board undoStack[SCL_RECORD_MAX_MOVES];
} SCL_Game;

typedef void (*SCL_PutPixelFunction)(uint8_t pixel, uint16_t index);
typedef uint8_t (*SCL_RandomFunction)(void);

// ---- SquareSet helpers ----
static inline void SCL_squareSetClear(SCL_SquareSet s) {
    for(int i = 0; i < 8; i++) s[i] = 0;
}

static inline void SCL_squareSetAdd(SCL_SquareSet s, uint8_t sq) {
    if(sq < 64) s[sq / 8] |= (1 << (sq % 8));
}

static inline uint8_t SCL_squareSetContains(const SCL_SquareSet s, uint8_t sq) {
    if(sq >= 64) return 0;
    return (s[sq / 8] >> (sq % 8)) & 1;
}

#define SCL_SQUARE_SET_EMPTY {0,0,0,0,0,0,0,0}

#define SCL_SQUARE_SET_ITERATE_BEGIN(set) \
    for(uint8_t iteratedSquare = 0; iteratedSquare < 64; iteratedSquare++) { \
        if(SCL_squareSetContains(set, iteratedSquare)) {

#define SCL_SQUARE_SET_ITERATE_END \
        } \
    }

// ---- piece helpers ----
static inline uint8_t SCL_pieceIsWhite(char p) {
    return p == 'P' || p == 'K';
}

static inline char SCL_pieceToColor(char p, uint8_t white) {
    if(white) return (p == 'p' || p == 'P') ? 'P' : 'K';
    return (p == 'p' || p == 'P') ? 'p' : 'k';
}

static inline uint8_t SCL_boardWhitesTurn(const SCL_Board board) {
    return board[SCL_BOARD_PLY_BYTE] & 1;
}

// ---- random ----
static uint16_t _scl_rng = 1;

static inline void SCL_randomBetterSeed(uint16_t seed) {
    _scl_rng = seed ? seed : 1;
}

static inline uint8_t SCL_randomBetter(void) {
    _scl_rng ^= _scl_rng << 7;
    _scl_rng ^= _scl_rng >> 9;
    _scl_rng ^= _scl_rng << 8;
    return (uint8_t)(_scl_rng & 0xff);
}

// ---- must-jump rule flag ----
// Set to 0 to allow optional captures (free-play mode)
// Set to 1 (default) to enforce mandatory capture rule
static uint8_t SCL_mustJump = 1;

static inline void SCL_setMustJump(uint8_t val) {
    SCL_mustJump = val;
}
// In standard checkers only dark squares are used.
// Square numbering: sq = row*8 + col. Dark square when (row+col) is odd... 
// Actually we treat ALL squares as valid board positions for the cursor,
// but only place pieces on dark squares (row+col odd).
static inline uint8_t SCL_isDarkSquare(uint8_t sq) {
    uint8_t row = sq / 8;
    uint8_t col = sq % 8;
    return (row + col) % 2 == 1;
}

// ---- board initialisation ----
// Board layout: row 0 = bottom (white/red side), row 7 = top (black side)
// Dark squares used: (row+col) % 2 == 1
// PLY_BYTE at index 66 = position [2] of the 8 extra bytes
// Set PLY_BYTE=1 means white's turn first
#define SCL_BOARD_START_STATE_INIT \
    {'.','P','.','P','.','P','.','P', \
     'P','.','P','.','P','.','P','.', \
     '.','P','.','P','.','P','.','P', \
     '.','.','.','.','.','.','.','.', \
     '.','.','.','.','.','.','.','.', \
     'p','.','p','.','p','.','p','.', \
     '.','p','.','p','.','p','.','p', \
     'p','.','p','.','p','.','p','.', \
     0,0,1,255,0,0,0,0}  /* extra bytes 64-71; [2]=PLY_BYTE=1(white starts), [3]=MULTIJUMP=none */

static const SCL_Board SCL_BOARD_START_STATE_VAL = SCL_BOARD_START_STATE_INIT;

// This macro expands to an initialiser for a local SCL_Board variable
#define SCL_BOARD_START_STATE SCL_BOARD_START_STATE_INIT

static inline void SCL_boardInit(SCL_Board board) {
    const SCL_Board init = SCL_BOARD_START_STATE_INIT;
    memcpy(board, init, sizeof(SCL_Board));
}

// ---- move generation ----
// A "move" in checkers: simple diagonal step, or one/multi-hop capture.
// For simplicity we generate only single-step and single-jump moves.
// Direction: white moves up (increasing row), black moves down (decreasing row).
// Kings move both directions.

typedef struct {
    uint8_t from;
    uint8_t to;
    uint8_t captured; // 255 = no capture
    uint8_t capturedPiece;
} SCL_Move;

#define SCL_MAX_MOVES 64

// Returns 1 if the piece at sq can make at least one capture right now.
static uint8_t _scl_canCaptureFrom(const SCL_Board board, uint8_t sq) {
    char p = board[sq];
    if(p == '.') return 0;
    uint8_t white = SCL_pieceIsWhite(p);
    uint8_t isKing = (p == 'K' || p == 'k');
    uint8_t row = sq / 8, col = sq % 8;
    uint8_t maxDirs = isKing ? 4 : 2;
    for(uint8_t d = 0; d < maxDirs; d++) {
        int8_t dr, dc;
        if(!isKing) {
            dr = white ? 1 : -1;
            dc = (d == 0) ? 1 : -1;
        } else {
            int8_t dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
            dr = dirs[d][0]; dc = dirs[d][1];
        }
        int8_t mr = (int8_t)row + dr, mc = (int8_t)col + dc;
        int8_t lr = mr + dr,          lc = mc + dc;
        if(mr < 0 || mr > 7 || mc < 0 || mc > 7) continue;
        if(lr < 0 || lr > 7 || lc < 0 || lc > 7) continue;
        uint8_t mid  = (uint8_t)mr * 8 + (uint8_t)mc;
        uint8_t land = (uint8_t)lr * 8 + (uint8_t)lc;
        char midP = board[mid];
        if(midP == '.') continue;
        if((uint8_t)SCL_pieceIsWhite(midP) == white) continue;
        if(board[land] != '.') continue;
        return 1;
    }
    return 0;
}

static uint8_t _scl_genMoves(const SCL_Board board, SCL_Move* moves) {
    uint8_t count = 0;
    uint8_t white = SCL_boardWhitesTurn(board);
    uint8_t multiJumpSq = board[SCL_BOARD_MULTIJUMP_BYTE];
    // First pass: find captures (mandatory in checkers)
    // If mid multi-jump, only generate captures for the jumping piece.
    uint8_t hasCaptures = 0;
    for(uint8_t sq = 0; sq < 64 && count < SCL_MAX_MOVES; sq++) {
        if(multiJumpSq != 255 && sq != multiJumpSq) continue;
        char p = board[sq];
        if(p == '.') continue;
        if((uint8_t)SCL_pieceIsWhite(p) != white) continue;
        uint8_t isKing = (p == 'K' || p == 'k');
        uint8_t row = sq / 8, col = sq % 8;
        int8_t dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
        int8_t maxDirs = isKing ? 4 : 2;
        for(int8_t d = 0; d < maxDirs; d++) {
            int8_t dr, dc;
            if(!isKing) {
                dr = white ? 1 : -1;
                dc = (d == 0) ? 1 : -1;
            } else {
                dr = dirs[d][0];
                dc = dirs[d][1];
            }
            int8_t mr = (int8_t)row + dr;
            int8_t mc = (int8_t)col + dc;
            int8_t lr = mr + dr;
            int8_t lc = mc + dc;
            if(mr < 0 || mr > 7 || mc < 0 || mc > 7) continue;
            if(lr < 0 || lr > 7 || lc < 0 || lc > 7) continue;
            uint8_t mid = (uint8_t)mr * 8 + (uint8_t)mc;
            uint8_t land = (uint8_t)lr * 8 + (uint8_t)lc;
            char midP = board[mid];
            if(midP == '.') continue;
            if((uint8_t)SCL_pieceIsWhite(midP) == white) continue;
            if(board[land] != '.') continue;
            moves[count].from = sq;
            moves[count].to = land;
            moves[count].captured = mid;
            moves[count].capturedPiece = (uint8_t)midP;
            count++;
            hasCaptures = 1;
        }
    }
    if(hasCaptures && SCL_mustJump) return count;
    if(multiJumpSq != 255) return count; // mid-jump: no simple moves allowed
    // Second pass: simple moves
    for(uint8_t sq = 0; sq < 64 && count < SCL_MAX_MOVES; sq++) {
        char p = board[sq];
        if(p == '.') continue;
        if((uint8_t)SCL_pieceIsWhite(p) != white) continue;
        uint8_t isKing = (p == 'K' || p == 'k');
        uint8_t row = sq / 8, col = sq % 8;
        int8_t maxDirs = isKing ? 4 : 2;
        for(int8_t d = 0; d < maxDirs; d++) {
            int8_t dr, dc;
            if(!isKing) {
                dr = white ? 1 : -1;
                dc = (d == 0) ? 1 : -1;
            } else {
                int8_t dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
                dr = dirs[d][0]; dc = dirs[d][1];
            }
            int8_t nr = (int8_t)row + dr;
            int8_t nc = (int8_t)col + dc;
            if(nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
            uint8_t dest = (uint8_t)nr * 8 + (uint8_t)nc;
            if(board[dest] != '.') continue;
            moves[count].from = sq;
            moves[count].to = dest;
            moves[count].captured = 255;
            moves[count].capturedPiece = '.';
            count++;
        }
    }
    return count;
}

static inline uint8_t SCL_boardMoveIsLegal(const SCL_Board board, uint8_t from, uint8_t to) {
    SCL_Move moves[SCL_MAX_MOVES];
    uint8_t n = _scl_genMoves(board, moves);
    for(uint8_t i = 0; i < n; i++) {
        if(moves[i].from == from && moves[i].to == to) return 1;
    }
    return 0;
}

static inline void SCL_boardGetMoves(const SCL_Board board, uint8_t from, SCL_SquareSet dest) {
    SCL_squareSetClear(dest);
    SCL_Move moves[SCL_MAX_MOVES];
    uint8_t n = _scl_genMoves(board, moves);
    for(uint8_t i = 0; i < n; i++) {
        if(moves[i].from == from) SCL_squareSetAdd(dest, moves[i].to);
    }
}

// Apply a move to the board (in-place).
// For captures, if the landing piece can jump again the turn is NOT flipped and
// SCL_BOARD_MULTIJUMP_BYTE is set to the landing square so the next genMoves call
// restricts moves to that piece only. The turn flips only when the jump chain ends.
static inline void _scl_applyMove(SCL_Board board, uint8_t from, uint8_t to) {
    char p = board[from];
    board[to] = p;
    board[from] = '.';
    int8_t fr = from / 8, fc = from % 8;
    int8_t tr = to / 8,   tc = to % 8;
    int8_t dr = tr - fr,  dc = tc - fc;
    if(dr == 2 || dr == -2) {
        // Capture: remove the jumped piece
        uint8_t mid = (uint8_t)((fr + dr/2) * 8 + (fc + dc/2));
        board[mid] = '.';
        // Promotion (reaching the back rank ends the jump chain per standard rules)
        uint8_t toRow = to / 8;
        uint8_t promoted = 0;
        if(p == 'P' && toRow == 7) { board[to] = 'K'; promoted = 1; }
        if(p == 'p' && toRow == 0) { board[to] = 'k'; promoted = 1; }
        // Continue multi-jump if more captures exist and piece wasn't just promoted
        if(!promoted && _scl_canCaptureFrom(board, to)) {
            board[SCL_BOARD_MULTIJUMP_BYTE] = to; // stay on same player's turn
            return;
        }
    } else {
        // Simple move: handle promotion
        uint8_t toRow = to / 8;
        if(p == 'P' && toRow == 7) board[to] = 'K';
        if(p == 'p' && toRow == 0) board[to] = 'k';
    }
    // End of turn (no more jumps, or simple move)
    board[SCL_BOARD_MULTIJUMP_BYTE] = 255;
    board[SCL_BOARD_PLY_BYTE] ^= 1;
}

// ---- evaluation ----
static inline int16_t SCL_boardEvaluateStatic(const SCL_Board board) {
    int16_t val = 0;
    for(uint8_t i = 0; i < 64; i++) {
        char p = board[i];
        if(p == 'P') val += SCL_VALUE_PAWN;
        else if(p == 'K') val += SCL_VALUE_PAWN * 3;
        else if(p == 'p') val -= SCL_VALUE_PAWN;
        else if(p == 'k') val -= SCL_VALUE_PAWN * 3;
    }
    return val;
}

// ---- game state check ----
static inline uint8_t _scl_checkState(const SCL_Board board) {
    SCL_Move moves[SCL_MAX_MOVES];
    uint8_t n = _scl_genMoves(board, moves);
    uint8_t white = SCL_boardWhitesTurn(board);
    if(n == 0) {
        return white ? SCL_GAME_STATE_BLACK_WIN : SCL_GAME_STATE_WHITE_WIN;
    }
    // Check if any pieces of current side remain
    uint8_t whitePieces = 0, blackPieces = 0;
    for(uint8_t i = 0; i < 64; i++) {
        char p = board[i];
        if(p == 'P' || p == 'K') whitePieces++;
        else if(p == 'p' || p == 'k') blackPieces++;
    }
    if(whitePieces == 0) return SCL_GAME_STATE_BLACK_WIN;
    if(blackPieces == 0) return SCL_GAME_STATE_WHITE_WIN;
    return SCL_GAME_STATE_PLAYING;
}

// ---- game functions ----
static inline void SCL_gameInit(SCL_Game* game, const SCL_Board startState) {
    memcpy(game->board, startState, sizeof(SCL_Board));
    game->record[0].len = 0;
    game->ply = 0;
    game->state = SCL_GAME_STATE_PLAYING;
}

static inline void SCL_gameMakeMove(SCL_Game* game, uint8_t from, uint8_t to, char promote) {
    (void)promote;
    if(game->ply < SCL_RECORD_MAX_MOVES) {
        game->record[0].from[game->ply] = from;
        game->record[0].to[game->ply] = to;
        memcpy(game->undoStack[game->ply], game->board, sizeof(SCL_Board));
    }
    _scl_applyMove(game->board, from, to);
    game->ply++;
    game->state = _scl_checkState(game->board);
}

static inline void SCL_gameUndoMove(SCL_Game* game) {
    if(game->ply == 0) return;
    game->ply--;
    memcpy(game->board, game->undoStack[game->ply], sizeof(SCL_Board));
    game->state = SCL_GAME_STATE_PLAYING;
}

static inline void SCL_gameGetRepetiotionMove(SCL_Game* game, uint8_t* s0, uint8_t* s1) {
    (void)game; *s0 = 255; *s1 = 255;
}

// ---- AI ----
static int16_t _scl_minimax(SCL_Board board, uint8_t depth, int16_t alpha, int16_t beta, uint8_t maxing) {
    if(depth == 0) return SCL_boardEvaluateStatic(board);
    uint8_t state = _scl_checkState(board);
    if(state == SCL_GAME_STATE_WHITE_WIN) return 30000;
    if(state == SCL_GAME_STATE_BLACK_WIN) return -30000;
    if(state != SCL_GAME_STATE_PLAYING) return 0;

    SCL_Move moves[SCL_MAX_MOVES];
    uint8_t n = _scl_genMoves(board, moves);
    if(n == 0) return maxing ? -30000 : 30000;

    if(maxing) {
        int16_t best = -30000;
        for(uint8_t i = 0; i < n; i++) {
            SCL_Board next;
            memcpy(next, board, sizeof(SCL_Board));
            _scl_applyMove(next, moves[i].from, moves[i].to);
            int16_t v = _scl_minimax(next, depth - 1, alpha, beta, 0);
            if(v > best) best = v;
            if(v > alpha) alpha = v;
            if(beta <= alpha) break;
        }
        return best;
    } else {
        int16_t best = 30000;
        for(uint8_t i = 0; i < n; i++) {
            SCL_Board next;
            memcpy(next, board, sizeof(SCL_Board));
            _scl_applyMove(next, moves[i].from, moves[i].to);
            int16_t v = _scl_minimax(next, depth - 1, alpha, beta, 1);
            if(v < best) best = v;
            if(v < beta) beta = v;
            if(beta <= alpha) break;
        }
        return best;
    }
}

static inline int16_t SCL_getAIMove(
    SCL_Board board,
    uint8_t depth,
    uint8_t extraDepth,
    uint8_t endgameDepth,
    void* evalFn,
    SCL_RandomFunction randFn,
    uint8_t randomness,
    uint8_t rs0,
    uint8_t rs1,
    uint8_t* s0,
    uint8_t* s1,
    char* prom) {
    (void)extraDepth; (void)endgameDepth; (void)evalFn;
    (void)rs0; (void)rs1; (void)prom;

    SCL_Move moves[SCL_MAX_MOVES];
    uint8_t n = _scl_genMoves(board, moves);
    if(n == 0) { *s0 = 255; *s1 = 255; return 0; }

    uint8_t white = SCL_boardWhitesTurn(board);
    int16_t bestVal = white ? -30000 : 30000;
    uint8_t bestIdx = 0;
    uint8_t bestCount = 0;

    for(uint8_t i = 0; i < n; i++) {
        SCL_Board next;
        memcpy(next, board, sizeof(SCL_Board));
        _scl_applyMove(next, moves[i].from, moves[i].to);
        int16_t v = _scl_minimax(next, depth > 0 ? depth - 1 : 0, -30000, 30000, !white);
        if(white ? (v > bestVal) : (v < bestVal)) {
            bestVal = v;
            bestIdx = i;
            bestCount = 1;
        } else if(v == bestVal) {
            bestCount++;
            if(randomness && randFn && (randFn() % bestCount == 0)) {
                bestIdx = i;
            }
        }
    }

    *s0 = moves[bestIdx].from;
    *s1 = moves[bestIdx].to;
    return bestVal;
}

// ---- string/FEN helpers (stubbed for compatibility) ----
static inline char* SCL_squareToString(uint8_t square, char* string) {
    string[0] = 'a' + (square % 8);
    string[1] = '1' + (square / 8);
    return string;
}

static inline void SCL_moveToString(
    const SCL_Board board,
    uint8_t from,
    uint8_t to,
    char promote,
    char* string) {
    (void)board; (void)promote;
    SCL_squareToString(from, string);
    SCL_squareToString(to, string + 2);
    string[4] = 0;
}

static inline uint8_t SCL_boardToFEN(const SCL_Board board, char* fen) {
    uint8_t pos = 0;
    for(uint8_t i = 0; i < 64 && pos < SCL_FEN_MAX_LENGTH - 2; i++) {
        fen[pos++] = board[i];
    }
    fen[pos++] = SCL_boardWhitesTurn(board) ? 'w' : 'b';
    fen[pos] = 0;
    return pos;
}

static inline void SCL_boardFromFEN(SCL_Board board, const char* fen) {
    SCL_boardInit(board);
    if(!fen || fen[0] == 0) return;
    for(uint8_t i = 0; i < 64; i++) {
        if(fen[i] == 0) break;
        board[i] = fen[i];
    }
    if(fen[64] == 'b') board[SCL_BOARD_PLY_BYTE] = 0; // black = 0 (black goes first would be 0... or 1)
    // We treat white=1, black=0 in PLY_BYTE. Default is white starts.
}

static inline void SCL_recordGetMove(
    SCL_Record record,
    uint16_t ply,
    uint8_t* s0,
    uint8_t* s1,
    char* p) {
    if(ply < record[0].len) {
        *s0 = record[0].from[ply];
        *s1 = record[0].to[ply];
    } else {
        *s0 = 0; *s1 = 0;
    }
    if(p) *p = 0;
}

static inline void SCL_recordFromPGN(SCL_Record record, const char* pgn) {
    (void)pgn;
    record[0].len = 0;
}

static inline void SCL_recordApply(SCL_Record record, SCL_Board board, uint16_t step) {
    (void)record; (void)board; (void)step;
}

static inline void SCL_printPGN(SCL_Record r, void* putCharFunc, SCL_Board initialState) {
    (void)r; (void)putCharFunc; (void)initialState;
}

static inline uint32_t SCL_boardHash32(const SCL_Board board) {
    uint32_t h = 0;
    for(uint8_t i = 0; i < 64; i++) h = h * 31 + (uint8_t)board[i];
    return h;
}

static inline uint8_t SCL_boardCheck(const SCL_Board board, uint8_t white) {
    (void)board; (void)white;
    return 0; // no "check" in checkers
}

static inline uint8_t SCL_boardEstimatePhase(const SCL_Board board) {
    uint8_t total = 0;
    for(uint8_t i = 0; i < 64; i++) {
        char p = board[i];
        if(p != '.') total++;
    }
    if(total <= 8) return SCL_PHASE_ENDGAME;
    return 1; // midgame
}

// ---- board drawing ----
// Draw a 64x64 1-bit board image.
// Each square is 8x8 pixels.
// White squares: filled (1=lit). Dark squares: piece drawn or empty.

// Simple 6-row sprites for each piece type (6 bits wide, centered in 8-bit byte)
// Pieces are 6x6 pixels, centered in the 8x8 square (1 pixel padding each side)
// Man (circle), King (circle with crown notches)
static const uint8_t _scl_spriteMan[6]   = {0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C};
static const uint8_t _scl_spriteManB[6]  = {0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C};
static const uint8_t _scl_spriteKing[6]  = {0x7E, 0x5A, 0x7E, 0x7E, 0x5A, 0x7E};
static const uint8_t _scl_spriteKingB[6] = {0x7E, 0x5A, 0x42, 0x42, 0x5A, 0x7E};

static inline void SCL_drawBoard(
    SCL_Board board,
    SCL_PutPixelFunction putPixel,
    uint8_t selectedSquare,
    SCL_SquareSet highlightSquares,
    uint8_t blackDown) {

    uint16_t n = 0;
    for(uint8_t screenRow = 0; screenRow < 8; screenRow++) {
        for(uint8_t pixelRow = 0; pixelRow < 8; pixelRow++) {
            for(uint8_t screenCol = 0; screenCol < 8; screenCol++) {
                // Map screen position to board square
                uint8_t boardRow = blackDown ? screenRow : (7 - screenRow);
                uint8_t boardCol = blackDown ? (7 - screenCol) : screenCol;
                uint8_t sq = boardRow * 8 + boardCol;

                char piece = board[sq];
                uint8_t isDark = SCL_isDarkSquare(sq);
                uint8_t isSelected = (sq == selectedSquare);
                uint8_t isHighlighted = SCL_squareSetContains(highlightSquares, sq);

                uint8_t byte;
                if(!isDark) {
                    // Light square: checkerboard fill
                    byte = (pixelRow % 2) ? 0xAA : 0x55;
                } else if(piece == '.') {
                    byte = 0x00; // empty dark square
                } else {
                    uint8_t isWhite = SCL_pieceIsWhite(piece);
                    uint8_t isKingPiece = (piece == 'K' || piece == 'k');
                    if(pixelRow == 0 || pixelRow == 7) {
                        byte = 0x00; // 1-pixel vertical padding for 6x6 piece
                    } else if(isKingPiece) {
                        byte = isWhite ? _scl_spriteKingB[pixelRow - 1] : _scl_spriteKing[pixelRow - 1];
                    } else {
                        byte = isWhite ? _scl_spriteManB[pixelRow - 1] : _scl_spriteMan[pixelRow - 1];
                    }
                }

                if(isHighlighted && isDark) {
                    if(pixelRow == 0 || pixelRow == 7) byte |= 0xFF;
                    else byte |= 0x81;
                }
                if(isSelected) {
                    if(pixelRow <= 1 || pixelRow >= 6) byte = 0xFF;
                    else byte |= 0xC3;
                }

                // Output 8 pixels
                for(uint8_t bit = 0; bit < 8; bit++) {
                    uint8_t pixel = (byte & (0x80 >> bit)) ? 0 : 1;
                    putPixel(pixel, n);
                    n++;
                }
            }
        }
    }
}

#endif // SMALLCHECKERSLIB_H
