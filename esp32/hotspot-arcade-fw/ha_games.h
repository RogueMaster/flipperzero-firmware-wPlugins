// Hotspot Arcade game engine (ESP side, real-time referee).
// Owns the player roster and the authoritative live state for the active game.
// Header-only, included exactly once by the .ino (single translation unit), so
// it may define freely. It talks to the outside world only through the sink
// functions below, which the .ino implements (WS send, UART report).
#pragma once
#include <Arduino.h>
#include "ha_json.h"
#include "ha_proto.h"

#define HA_MAX_PLAYERS 12
#define HA_NICK_LEN 20

// Nicknames are uppercased once, here at the door, so every downstream consumer
// (phone UI, Flipper roster, and the strings this engine composes like "A vs B")
// is consistent without each one having to remember. ASCII only on purpose:
// bytes >= 0x80 are UTF-8 continuation/lead bytes and are left untouched, so an
// accented or emoji nickname survives intact.
static inline void ha_upper(char* s) {
    for(; s && *s; s++)
        if(*s >= 'a' && *s <= 'z') *s -= 32;
}

// Duels (connect4 / tic-tac-toe / dots-and-boxes) share one match + challenge
// system, parameterized by the active game's kind. Only one game is active at a
// time, so all live matches are the active kind.
#define DUEL_MAX_CELLS 64 // c4 = 7x6; ttt = 3x3; reversi = 8x8
#define DUEL_MAX_MATCHES 6
#define DUEL_MAX_CHALLENGES 16
#define DOTS_W 5 // boxes across
#define DOTS_H 5 // boxes down
#define DOTS_HEDGES ((DOTS_H + 1) * DOTS_W) // horizontal edges
#define DOTS_VEDGES (DOTS_H * (DOTS_W + 1)) // vertical edges
#define DOTS_BOXES (DOTS_W * DOTS_H)

// Battleship: 1v1 on a 10x10 grid, five ships (5+4+3+3+2 = 17 cells). Its own match
// struct (like Pong), not the shared DuelMatch board, because it needs two grids per
// player and hidden fleets.
#define BS_SIZE 10
#define BS_N (BS_SIZE * BS_SIZE) // 100
#define BS_SHIPS 5
#define BS_TOTAL 17 // sum of BS_LEN, the win threshold
#define BATTLE_MAX 4 // concurrent matches

// Chess: 1v1, full FIDE rules refereed here. Squares are 0..63 with a1 = 0, b1 = 1 ...
// h8 = 63, so rank = sq >> 3 and file = sq & 7. A move is encoded as from * 64 + to.
#define CHESS_MAX 4 // concurrent matches
#define CH_HIST 154 // repetition ring: 1 + 150 halfmoves (75-move bound) + slack
#define CH_CLOCK_MS 300000UL // 5:00 per side, no increment
#define CH_MAX_MOVES 220 // legal-move buffer (theoretical max is 218)
// How a finished game ended (ChessMatch::reason).
#define CH_R_MATE 1
#define CH_R_STALEMATE 2
#define CH_R_RESIGN 3
#define CH_R_FLAG 4 // opponent's clock ran out
#define CH_R_FLAGDRAW 5 // clock ran out but the winner could not have mated (FIDE 6.9)
#define CH_R_MATERIAL 6 // dead position
#define CH_R_REP3 7 // threefold, claimed
#define CH_R_REP5 8 // fivefold, automatic
#define CH_R_MOVE50 9 // 50-move rule, claimed
#define CH_R_MOVE75 10 // 75-move rule, automatic
#define CH_R_AGREE 11 // draw by agreement
#define CH_R_LEFT 12 // opponent disconnected

#define TRIVIA_MAX_TOPICS 6
#define TRIVIA_MAX_QS 20
#define PACK_MAX_ITEMS 32 // items in a word/prompt pack (wyr/scramble/draw)
#define TRIVIA_QDUR 20 // seconds per question (safety timer)
#define TRIVIA_COUNTDOWN 3 // seconds after all-ready before the first question
#define TRIVIA_REVEAL_MS 4000 // pause on the reveal before the next question

// Character count of a UTF-8 string (counts lead bytes, skips continuation bytes).
// Content packs are UTF-8, so any language beyond ASCII (accented Latin, Cyrillic,
// Greek, ...) needs glyph-aware handling: a 2-byte letter must count as one blank in
// Draw, and Scramble must shuffle whole letters, not split them into invalid bytes.
static inline int haUtf8Len(const char* s) {
    int n = 0;
    for(; *s; s++)
        if(((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

#define DRAW_SECS 70 // per drawing round
#define DRAW_REVEAL_MS 4000 // reveal pause before the next round
#define PONG_MAX 4 // concurrent pong matches
#define PONG_WIN 5 // points to win
#define PONG_TICK_MS 33 // ~30 Hz
// Court geometry, as fractions of the canvas width. The ball must reverse when its
// EDGE meets the paddle FACE, so the contact plane is paddle thickness + ball half
// width in from the wall. Bouncing at a bare 0.05 (as this did) left the ball
// visibly short of the paddle, because the paddle only reaches 0.02 and the ball's
// edge is 0.018 ahead of its centre. web/games/pong.js draws with these same two
// numbers — change one side and the ball bounces off empty space again.
#define PONG_PAD_W 0.02f // paddle thickness
#define PONG_BALL_R 0.018f // ball half width
#define PONG_HIT_X (PONG_PAD_W + PONG_BALL_R) // left contact plane; right is 1 - this

// Whole-group "party" games (would-you-rather / scramble / reaction) share a
// lobby -> countdown -> round -> reveal -> ... -> final skeleton (see Party).
#define PARTY_COUNTDOWN 3 // seconds after all-ready before round 1
#define WYR_ROUNDS 6
#define WYR_VOTE_SECS 20 // safety timer per prompt
#define WYR_REVEAL_MS 5000
#define SCR_ROUNDS 6
#define SCR_SECS 30 // safety timer per word
#define SCR_REVEAL_MS 5000
#define REACT_ROUNDS 5
#define REACT_REVEAL_MS 4000

// Spectrum (wavelength-style): each round one player is the psychic. They see a
// hidden target on a 0..100 spectrum between two opposing words and type a clue;
// everyone else slides to guess where the clue lands. Points by closeness; the
// psychic scores by how well the guessers do, so a good clue is rewarded.
#define SPECTRUM_ROUNDS 6
#define SPECTRUM_CLUE_SECS 45 // psychic's clue window (safety timer)
#define SPECTRUM_GUESS_SECS 30 // guessers' window (safety timer)
#define SPECTRUM_REVEAL_MS 6000
#define SPECTRUM_CLUE_LEN 40

// Kiss Marry Kill: each round one player (the "chooser") secretly assigns Kiss /
// Marry / Kill to three people from the voted pack; everyone else predicts that
// assignment. A guess of a permutation of three either matches 3 positions or at
// most 1 (getting two right forces the third), so per-round scores are 0/1/3.
#define KMK_ROUNDS 6
#define KMK_CHOOSE_SECS 40 // chooser's window (safety timer)
#define KMK_GUESS_SECS 30 // guessers' window (safety timer)
#define KMK_REVEAL_MS 7000

// Secrets: each round shows a yes/no question. Everyone secretly predicts how many
// of the N joined players will answer "yes" (0..N), then secretly answers. Only the
// group's total yes-count is ever revealed, never who answered what. Two stages per
// playing round: 0 = answering yes/no, 1 = predicting the count.
#define SECRETS_ROUNDS 6
#define SECRETS_PREDICT_SECS 30 // predict window (safety timer)
#define SECRETS_ANSWER_SECS 30 // answer window (safety timer)
#define SECRETS_REVEAL_MS 5000

#define GC_ROUNDS 5
#define GC_PLAY_SECS 25 // safety deadline per color
#define GC_REVEAL_MS 6000
#define GC_SPEED_MS 12000 // speed bonus decays to 0 over this window

// Phone-initiated game-change vote: a cross-cutting proposal that sits ABOVE the active
// game. Any player can propose switching to another game; while it is pending the active
// game is paused and every OTHER player votes. This is the one sanctioned phone->host
// action, gated behind a majority of the other players (see gameVoteResolve).
#define GAMEVOTE_SECS 25 // proposal times out (treated as reject) after this

// ---- sinks implemented in the .ino ----
void haWsSendWs(uint32_t wsId, const String& msg); // to one socket (0 = no-op)
void haWsBroadcast(const String& msg); // to all connected sockets
void haUartJoin(uint8_t pid, const char* nick);
void haUartLeave(uint8_t pid);
void haUartScore(uint8_t pid, int delta, const char* reason);
void haUartEvent(const String& json);
void haUartRoundResult(const String& json);
// Human-readable trace of every identity decision (see onHello): a genuinely new
// device, or a second browser context on a phone that is already playing being
// consolidated onto its existing player. `deviceKey` is opaque here -- the .ino
// renders it, since it is the side that knows what it was made of.
void haLogJoin(uint8_t pid, uint64_t deviceKey, const char* nick, bool consolidated);

// One phone = one player. `wsId` identifies a *connection*; `deviceKey` identifies
// the *phone*, and every browser context on it (the iOS captive mini-browser, Safari,
// a second tab, a socket that came back after the screen unlocked) presents the same
// one. Keying identity on the device instead of the socket is what stops one phone
// from turning into two or three players -- see onHello() for the rebind and
// onWsDisconnect() for the stale-socket rule.
//
// The key is deliberately opaque to the engine: the .ino derives it from the station's
// MAC (falling back to its IP), and only that side knows or cares. 0 = unknown.
struct Player {
    bool used;
    uint32_t wsId; // 0 = not connected
    uint64_t deviceKey; // which phone, 0 = unknown (see onHello)
    char nick[HA_NICK_LEN];
    char avatar[8]; // emoji avatar (UTF-8), player-picked on the landing screen
    int32_t score;
};

// A phone that drops out keeps its identity for the rest of the session. When the
// socket closes the player's nick, avatar and score are parked under their device
// key; the same phone coming back -- a WiFi blip, a locked screen, a browser
// restart, a tab swiped away -- is handed all three back instead of arriving as a
// stranger on zero. Ten slots is the softAP's station cap, so a full room's worth
// of leavers fits; beyond that the stalest is evicted.
#define HA_PARKED_MAX 10
struct ParkedPlayer {
    uint64_t deviceKey; // 0 = free slot
    char nick[HA_NICK_LEN];
    char avatar[8];
    int32_t score;
    uint32_t at; // millis when parked, for evicting the stalest first
};

// Trivia content, streamed from the Flipper at session start (the packs become
// the votable topics), then owned by the ESP which orchestrates the whole game.
struct TriviaQ {
    String q;
    String o[4];
    uint8_t correct;
};
struct TriviaTopic {
    String name;
    TriviaQ qs[TRIVIA_MAX_QS];
    uint8_t qcount;
};

struct Trivia {
    uint8_t phase; // 0 lobby, 1 countdown, 2 question, 3 reveal, 4 final
    bool ready[HA_MAX_PLAYERS + 1];
    int8_t vote[HA_MAX_PLAYERS + 1]; // topic index, -1 = none
    uint32_t countdownEnd;
    int lastSec; // last countdown second broadcast
    uint8_t topic; // chosen topic index
    int qi; // current question index
    int8_t answer[HA_MAX_PLAYERS + 1];
    uint32_t answerMs[HA_MAX_PLAYERS + 1];
    int gained[HA_MAX_PLAYERS + 1]; // points earned on the current question
    int counts[4];
    uint32_t deadline; // question end
    uint32_t revealUntil;
};

struct DuelMatch {
    bool used;
    uint8_t kind; // HA_GAME_CONNECT4 / TICTACTOE / DOTS
    uint8_t a, b; // pids; a plays mark 1, b plays mark 2
    bool aIn, bIn; // still attached (not returned to lobby)
    uint8_t turn; // pid to move
    uint8_t phase; // 1 playing, 2 over
    uint8_t winner; // pid, or 0 for draw
    uint8_t first; // who moved first (rematch alternates it)
    uint8_t board[DUEL_MAX_CELLS]; // grid games (c4/ttt), row-major; 0/1/2
    uint8_t hedges[DOTS_HEDGES]; // dots: horizontal edges drawn (0/1)
    uint8_t vedges[DOTS_VEDGES]; // dots: vertical edges drawn (0/1)
    uint8_t boxes[DOTS_BOXES]; // dots: box owner (0/1/2)
    uint8_t sA, sB; // dots: box counts for a / b
};

struct DuelChallenge {
    bool used;
    uint8_t from, to;
};

// Shared word pack for scramble/draw: a set of single-word items, voted on like
// trivia topics / wyr packs. Mirrors WyrPack but with one word per item.
struct WordPack {
    String name;
    String words[PACK_MAX_ITEMS];
    uint8_t count;
};

struct DrawState {
    uint8_t phase; // 0 idle, 1 draw, 2 reveal, 3 final
    uint8_t drawer; // pid currently drawing
    uint8_t drawerSeq; // rotates the drawer
    uint16_t wordSeq; // rotates the word
    char word[24];
    int round;
    int roundsTotal; // game ends after this many rounds
    uint32_t deadline; // millis (draw end)
    uint32_t revealUntil; // millis (reveal end)
    uint8_t winner; // pid who guessed it, or 0
    WordPack packs[TRIVIA_MAX_TOPICS]; // pack cap mirrors trivia's topic cap
    uint8_t packCount;
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted (no vote strip yet; see Task 3)
    uint8_t pack; // chosen pack index (pack 0 for now, no draw vote strip)
};

// Shared lobby/ready/countdown skeleton for the whole-group party games.
// phase: 0 lobby, 1 countdown, 2 round, 3 reveal, 4 final.
struct Party {
    uint8_t phase;
    bool ready[HA_MAX_PLAYERS + 1];
    int round; // 1-based current round
    int roundsTotal;
    uint32_t countdownEnd;
    int lastSec; // last countdown second broadcast
    uint32_t deadline; // round safety deadline
    uint32_t revealUntil; // reveal end
};

// Would You Rather: a live A/B poll. Prompts come from the voted pack.
struct WyrPrompt {
    String a, b;
};
struct WyrPack {
    String name;
    WyrPrompt items[PACK_MAX_ITEMS];
    uint8_t count;
};
struct WyrState {
    Party pt;
    WyrPack packs[TRIVIA_MAX_TOPICS]; // pack cap mirrors trivia's topic cap (HA_MAX_TOPICS on the Flipper side)
    uint8_t packCount;
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack index (locked in when the round starts)
    uint8_t promptSeq; // rotates prompts across rounds within the pack
    uint8_t prompt; // current prompt index within the chosen pack
    int8_t choice[HA_MAX_PLAYERS + 1]; // A/B vote for the current prompt, -1 = none
};

// Word scramble race: everyone unscrambles the same word; fastest correct win most.
struct ScrambleState {
    Party pt;
    uint16_t wordSeq;
    char word[24]; // the answer
    char scram[24]; // shown (letters shuffled)
    bool solved[HA_MAX_PLAYERS + 1];
    uint8_t solvedCount;
    WordPack packs[TRIVIA_MAX_TOPICS]; // pack cap mirrors trivia's topic cap
    uint8_t packCount;
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack index (locked in when the round starts)
};

// Reaction duel (fastest finger): red -> (random delay) -> green; first tap wins.
// Tapping while red disqualifies you for the round.
struct ReactState {
    Party pt;
    uint32_t goAt; // millis the light turns green (phase 2)
    bool goOn; // green announced this round
    bool tapped[HA_MAX_PLAYERS + 1];
    bool dq[HA_MAX_PLAYERS + 1]; // false-started this round
    uint8_t winner; // pid, or 0
    uint32_t winMs; // winner's reaction time
};

// Guess the Color: a random swatch is shown; everyone dials in an R/G/B guess and
// submits. Points = closeness (Euclidean RGB distance) + a speed bonus that decays
// the longer you take. Closest usually wins the round; a fast submit can edge it.
struct GuessColorState {
    Party pt;
    uint8_t tr, tg, tb; // target color for the round
    uint32_t roundStart; // millis the play phase began (for speed)
    bool guessed[HA_MAX_PLAYERS + 1];
    uint8_t gr[HA_MAX_PLAYERS + 1], gg[HA_MAX_PLAYERS + 1], gb[HA_MAX_PLAYERS + 1];
    uint32_t submitMs[HA_MAX_PLAYERS + 1]; // reveal -> submit, ms
    int gained[HA_MAX_PLAYERS + 1]; // points earned this round
    uint8_t winner; // pid with the most points this round, 0 = none
};

// Spectrum: reuses WyrPack for content (each item's a=left label, b=right label)
// and the Party lobby/countdown/reveal skeleton. Within a playing round it has two
// stages: 0 = the psychic is writing the clue, 1 = everyone else is guessing.
struct SpectrumState {
    Party pt;
    WyrPack packs[TRIVIA_MAX_TOPICS]; // item.a = left word, item.b = right word
    uint8_t packCount;
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack (locked when the game starts)
    uint16_t cardSeq; // rotates the spectrum card across rounds
    uint8_t card; // current card index within the pack
    uint8_t psychic; // pid giving the clue this round
    uint8_t psychicSeq; // rotates the psychic across rounds
    uint8_t stage; // 0 clue, 1 guess
    int target; // hidden target 0..100
    char clue[SPECTRUM_CLUE_LEN]; // psychic's clue text
    int8_t guess[HA_MAX_PLAYERS + 1]; // 0..100, -1 = not guessed
    int gained[HA_MAX_PLAYERS + 1]; // points earned this round (shown on reveal)
};

// Kiss Marry Kill: reuses WordPack (a flat list of names) and the Party skeleton.
// Labels are 0 = kiss, 1 = marry, 2 = kill; each round has three people and the
// assignment is a permutation of those three labels over them.
struct KmkState {
    Party pt;
    WordPack packs[TRIVIA_MAX_TOPICS]; // each item is one person's name
    uint8_t packCount;
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack (locked when the game starts)
    uint16_t nameSeq; // advances the people picked across rounds
    uint8_t person[3]; // indices into the pack for this round's three people
    uint8_t chooser; // pid assigning K/M/K this round
    uint8_t chooserSeq; // rotates the chooser across rounds
    uint8_t stage; // 0 choose, 1 guess
    int8_t cLabel[3]; // chooser's label per person, -1 = unset
    int8_t gLabel[HA_MAX_PLAYERS + 1][3]; // each guesser's labels per person
    bool guessed[HA_MAX_PLAYERS + 1];
    int gained[HA_MAX_PLAYERS + 1]; // points earned this round (shown on reveal)
};

// Secrets: reuses WordPack (a flat list of yes/no questions) and the Party skeleton.
// Each round shows one question; players first secretly predict how many of the N
// joined players will answer "yes" (0..N), then secretly answer yes/no. Only the
// group's total yes-count is ever revealed — a player's own prediction/answer/points
// reach only that player (secretsJson gates it, like Spectrum's serializer).
struct SecretsState {
    Party pt;
    WordPack packs[TRIVIA_MAX_TOPICS]; // each item is one yes/no question
    uint8_t packCount;
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack (locked when the game starts)
    uint16_t qSeq; // rotates the question across rounds within the pack
    uint8_t question; // current question index within the pack
    uint8_t stage; // 0 answer, 1 predict
    int8_t predict[HA_MAX_PLAYERS + 1]; // each player's guessed yes-count, -1 = unset
    int8_t answer[HA_MAX_PLAYERS + 1]; // each player's yes(1)/no(0), -1 = unset
    int gained[HA_MAX_PLAYERS + 1]; // points earned this round (shown on reveal)
    int yesCount; // total yes answers this round (computed at reveal)
};

struct PongMatch {
    bool used;
    uint8_t a, b; // a = left paddle, b = right paddle
    bool aIn, bIn;
    uint8_t phase; // 1 playing, 2 over
    float bx, by, vx, vy; // ball position (0..1) + velocity per tick
    float p1, p2; // paddle centers (0..1)
    int8_t d1, d2; // paddle move dir (-1/0/1)
    uint8_t s1, s2; // scores
    uint8_t winner; // pid
};

// Battleship: a = challenger, b = opponent. Each keeps a hidden fleet grid; shots are
// recorded on the *target's* grid. battleJson never exposes an un-hit enemy ship cell.
struct BattleMatch {
    bool used;
    uint8_t a, b; // pids
    bool aIn, bIn;
    uint8_t phase; // 0 placement, 1 firing, 2 over
    uint8_t turn; // pid to fire (firing phase)
    uint8_t first; // who fired first (rematch alternates it)
    uint8_t winner; // pid, or 0
    bool readyA, readyB; // placement committed
    uint8_t fleetA[BS_N], fleetB[BS_N]; // 0 empty, else ship id 1..BS_SHIPS
    uint8_t shotOnA[BS_N], shotOnB[BS_N]; // shots landed on that grid: 0 none, 1 miss, 2 hit
    uint8_t hitsA, hitsB; // hits scored BY a / BY b (win at BS_TOTAL)
};

static const uint8_t BS_LEN[BS_SHIPS] = {5, 4, 3, 3, 2};
static const char* const BS_NAMES[BS_SHIPS] = {
    "Carrier", "Battleship", "Cruiser", "Submarine", "Destroyer"};

// ---- chess tables and piece helpers ----
// Step tables are (file, rank) deltas rather than square offsets, so every step is
// bounds-checked on both axes: a raw +-1 on the square index wraps around the board
// edge (h4 + 1 lands on a5) and would invent moves that do not exist.
static const int8_t CH_NDF[8] = {1, 2, 2, 1, -1, -2, -2, -1}; // knight
static const int8_t CH_NDR[8] = {2, 1, -1, -2, -2, -1, 1, 2};
static const int8_t CH_KDF[8] = {-1, 0, 1, -1, 1, -1, 0, 1}; // king (and the 8 neighbours)
static const int8_t CH_KDR[8] = {1, 1, 1, 0, 0, -1, -1, -1};
static const int8_t CH_SDF[8] = {1, 1, -1, -1, 1, -1, 0, 0}; // sliders: 0..3 diagonal,
static const int8_t CH_SDR[8] = {1, -1, 1, -1, 0, 0, 1, -1}; // 4..7 orthogonal

// Piece codes in ChessCore::sq are 0 empty, 1..6 white P,N,B,R,Q,K, 7..12 black.
static inline uint8_t chKind(uint8_t pc) { return pc > 6 ? (uint8_t)(pc - 6) : pc; }
static inline bool chIsWhite(uint8_t pc) { return pc >= 1 && pc <= 6; }
static inline uint8_t chSide(uint8_t pc) { return pc > 6 ? 1 : 0; } // callers check pc != 0

// Castling right lost when a corner square changes hands (the rook moving off it, or
// being captured on it). 0 for every other square.
static inline uint8_t chCornerBit(int sq) {
    return sq == 0 ? 0x02 : sq == 7 ? 0x01 : sq == 56 ? 0x08 : sq == 63 ? 0x04 : 0;
}

// Zobrist keys: 12*64 piece-square, one side-to-move, 16 castling-rights states, 8
// en-passant files. Filled once from a fixed seed (chessZobristInit) so a key means
// the same position on every boot, and in the sim.
static uint32_t ZOB[12 * 64 + 1 + 16 + 8];
static bool ZOB_READY = false;

// The position identity per FIDE 9.2: placement, side to move, castling rights and
// en-passant capturability. Everything repetition hashing has to cover, and nothing
// else: the clocks and move counters live in ChessMatch.
struct ChessCore {
    uint8_t sq[64];
    uint8_t stm; // 0 white, 1 black
    uint8_t rights; // castling: 1 = white O-O, 2 = white O-O-O, 4 = black O-O, 8 = black O-O-O
    int8_t ep; // en-passant target (the square the pawn skipped), -1 none
};

// What chessMake has to hand back so chessUnmake can restore the position exactly.
// capSq differs from the move's `to` only for an en-passant capture.
struct ChessUndo {
    uint8_t captured, capSq, rights;
    int8_t ep;
};

// Chess: a = challenger, b = opponent. `white` is a pid rather than a flag because a
// rematch swaps colors. hist[] is the repetition record: every position since the
// last irreversible move (pawn move or capture), which is what bounds it to CH_HIST.
struct ChessMatch {
    bool used;
    uint8_t a, b; // pids
    bool aIn, bIn;
    uint8_t white; // pid playing white this game
    uint8_t phase; // 1 playing, 2 over
    uint8_t winner; // pid, 0 = draw
    uint8_t reason; // CH_R_*
    ChessCore core;
    uint8_t halfmove; // plies since a pawn move/capture; 100 = 50-move claim, 150 = auto
    uint16_t fullmove;
    uint32_t clockMs[2]; // remaining ms, [0] = white, [1] = black
    uint32_t lastStamp; // millis() at game start / last completed move
    int16_t lastMove; // from * 64 + to of the move just played, -1 before the first
    uint8_t offerBy; // pid with a pending draw offer, 0 none
    uint16_t histLen;
    uint32_t hist[CH_HIST];
};

class Engine {
public:
    // The phone-client UI language, set by the host and echoed to each phone in `welcome`
    // so the client loads the matching message catalog. "" = English. Content packs are a
    // separate, Flipper-side concern (which packs get streamed).
    void setLang(const char* l) {
        strlcpy(_lang, (l && l[0]) ? l : "", sizeof(_lang));
    }

    void reset() {
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _p[i] = Player{};
        _active = HA_GAME_NONE;
        triviaClear();
        duelClear();
        drawClear();
        pongClear();
        wyrClear();
        scrambleClear();
        reactClear();
        gcClear();
        battleClear();
        spectrumClear();
        kmkClear();
        chessClear();
        secretsClear();
        gameVoteClear();
    }

    // ---- roster ----
    uint8_t pidByWs(uint32_t wsId) {
        if(!wsId) return 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _p[i].wsId == wsId) return i;
        return 0;
    }

    // The player sitting on a given device, or 0. An unknown device (key 0) never
    // matches, so those clients keep the old one-player-per-connection behaviour
    // instead of all collapsing into a single player.
    ParkedPlayer _parked[HA_PARKED_MAX] = {};

    // Park a leaving player's identity so their return can restore it.
    void parkPlayer(uint8_t pid) {
        if(!_p[pid].deviceKey) return; // nothing to recognise them by later
        int slot = -1;
        for(int i = 0; i < HA_PARKED_MAX; i++) {
            if(_parked[i].deviceKey == _p[pid].deviceKey) { slot = i; break; }
            if(!_parked[i].deviceKey && slot < 0) slot = i;
        }
        if(slot < 0) { // all taken: evict the stalest
            slot = 0;
            for(int i = 1; i < HA_PARKED_MAX; i++)
                if((int32_t)(_parked[i].at - _parked[slot].at) < 0) slot = i;
        }
        _parked[slot].deviceKey = _p[pid].deviceKey;
        strlcpy(_parked[slot].nick, _p[pid].nick, HA_NICK_LEN);
        strlcpy(_parked[slot].avatar, _p[pid].avatar, sizeof(_parked[slot].avatar));
        _parked[slot].score = _p[pid].score;
        _parked[slot].at = millis();
    }

    // Take a parked identity back out of the store (consumed, not copied).
    bool unparkPlayer(uint64_t deviceKey, uint8_t pid) {
        if(!deviceKey) return false;
        for(int i = 0; i < HA_PARKED_MAX; i++) {
            if(_parked[i].deviceKey != deviceKey) continue;
            strlcpy(_p[pid].nick, _parked[i].nick, HA_NICK_LEN);
            strlcpy(_p[pid].avatar, _parked[i].avatar, sizeof(_p[pid].avatar));
            _p[pid].score = _parked[i].score;
            _parked[i] = ParkedPlayer{};
            return true;
        }
        return false;
    }

    uint8_t pidByDevice(uint64_t deviceKey) {
        if(!deviceKey) return 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _p[i].deviceKey == deviceKey) return i;
        return 0;
    }

    void onWsDisconnect(uint32_t wsId) {
        // pidByWs() matches only a player whose CURRENT wsId is this socket, which is
        // exactly the guard the rebind needs: once a phone has moved to a new
        // connection the old socket owns nobody, so its close -- which on a locked
        // phone arrives minutes late, when TCP finally times out -- can no longer
        // take the live player down with it.
        uint8_t pid = pidByWs(wsId);
        if(!pid) return;
        anyOnLeave(pid); // forfeit any active match
        bool wasProposer = (_gvActive && pid == _gvProposer);
        parkPlayer(pid);  // keep nick/avatar/score for this phone's return
        _p[pid] = Player{};
        _gvVote[pid] = -1; // drop any pending game-change vote from the departed player
        haUartLeave(pid);
        // While a game-change vote is pending the active game is frozen, so its roster
        // handlers must not run (a leaver mustn't, say, complete a paused trivia reveal).
        if(_gvActive) {
            if(wasProposer) {
                gameVoteReject(); // the proposer left: cancel and resume the previous game
            } else {
                // Fewer "other" players can tip the tally toward approve or reject.
                if(!gameVoteResolve(millis())) pushAll(); // still pending: refresh counts
            }
            return;
        }
        triviaOnRosterChange();
        partyRosterChanged();
        pushAll();
    }

    // `deviceKey` says which phone this socket is on (0 = unknown); see the Player
    // comment for why it, not the socket, is the identity.
    void onHello(uint32_t wsId, uint64_t deviceKey, const char* nick, const char* avatar,
                 bool named) {
        uint8_t pid = pidByWs(wsId);
        // A hello on a NEW socket from a device that is already playing: the phone
        // opened the page in a second browser context (the captive mini-browser next
        // to Safari, another tab) or reconnected before the old socket's close was
        // noticed. Adopt the new connection for the existing player rather than
        // minting a second one -- pid, nick, avatar and score all stay put, and the
        // `welcome` below hands that identity to the new context, which adopts it.
        bool rebound = false;
        if(!pid) {
            pid = pidByDevice(deviceKey);
            if(pid) {
                _p[pid].wsId = wsId;
                rebound = true;
                haLogJoin(pid, deviceKey, _p[pid].nick, true);
            }
        }
        if(!pid) {
            pid = freePid();
            if(!pid) return; // full
            _p[pid].used = true;
            _p[pid].wsId = wsId;
            _p[pid].deviceKey = deviceKey;
            _p[pid].score = 0;
            // This phone played earlier and dropped out: hand back its own name,
            // avatar and score instead of starting it over at zero. A name the
            // player has just typed still wins over the restored one.
            bool restored = unparkPlayer(deviceKey, pid);
            if(!restored || (named && nick && nick[0])) {
                strlcpy(_p[pid].nick, (nick && nick[0]) ? nick : "PLAYER", HA_NICK_LEN);
                ha_upper(_p[pid].nick);
            }
            if(!restored || (named && avatar && avatar[0]))
                strlcpy(_p[pid].avatar, (avatar && avatar[0]) ? avatar : "\xF0\x9F\x99\x82", sizeof(_p[pid].avatar));
            haUartJoin(pid, _p[pid].nick);
            haLogJoin(pid, deviceKey, _p[pid].nick, restored);
        } else if(!rebound || named) {
            // Re-hello from a known socket = the player changed their name/avatar in
            // the header editor. Re-announce over UART so the Flipper's leaderboard
            // updates (player_join there updates an existing pid's nick in place).
            // A rebind is deliberately NOT this case: the second context sends
            // whatever name it happens to have saved (often a freshly generated one),
            // and letting that rename the player mid-session is the bug, not the fix.
            if(nick && nick[0]) {
                strlcpy(_p[pid].nick, nick, HA_NICK_LEN);
                ha_upper(_p[pid].nick);
                haUartJoin(pid, _p[pid].nick);
            }
            if(avatar && avatar[0]) strlcpy(_p[pid].avatar, avatar, sizeof(_p[pid].avatar));
        }
        String w = String("{\"t\":\"welcome\",\"pid\":") + pid + ",\"nick\":\"" +
                   ha_json_escape(_p[pid].nick) + "\",\"avatar\":\"" +
                   ha_json_escape(_p[pid].avatar) + "\",\"lang\":\"" + _lang + "\"}";
        haWsSendWs(wsId, w);
        // While a game-change vote is pending the active game is frozen, so its roster
        // handlers must not run here either (a join or a re-hello mid-vote would otherwise
        // mutate the frozen game, surfacing on reject/timeout). Mirrors onWsDisconnect; the
        // vote overlay still reaches the new socket via pushAll below.
        if(!_gvActive) {
            triviaOnRosterChange();
            partyRosterChanged();
        }
        pushAll();
    }

    // ---- host (Flipper) driven ----
    // A host-initiated select is authoritative and immediate: it also cancels any pending
    // phone game-change vote (gameVoteClear). Phone-initiated changes go through the vote,
    // which calls this only on approval.
    void selectGame(uint8_t id) {
        gameVoteClear();
        _active = id;
        triviaClear();
        duelClear();
        drawClear();
        pongClear();
        wyrClear();
        scrambleClear();
        reactClear();
        gcClear();
        battleClear();
        spectrumClear();
        kmkClear();
        chessClear();
        secretsClear();
        pushAll();
    }

    void resetScores() {
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used) _p[i].score = 0;
        pushAll();
    }

    // ---- trivia content streamed from the Flipper (packs -> votable topics) ----
    void triviaTopicsClear() {
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _topics[i] = TriviaTopic{};
        _topicCount = 0;
    }
    void triviaAddTopic(const char* name) {
        if(_topicCount >= TRIVIA_MAX_TOPICS) return;
        _topics[_topicCount] = TriviaTopic{};
        _topics[_topicCount].name = name;
        _topics[_topicCount].qcount = 0;
        _topicCount++;
    }
    void triviaAddQ(const char* json) {
        if(_topicCount == 0) return;
        TriviaTopic& tp = _topics[_topicCount - 1];
        if(tp.qcount >= TRIVIA_MAX_QS) return;
        TriviaQ& q = tp.qs[tp.qcount];
        char buf[200];
        q.q = ha_json_str(json, "q", buf, sizeof(buf)) ? buf : "";
        String opts[4];
        parseOptions(json, opts);
        for(int k = 0; k < 4; k++) q.o[k] = opts[k];
        int v;
        q.correct = ha_json_int(json, "c", &v) ? (uint8_t)v : 0;
        tp.qcount++;
    }

    // ---- generic content ingest ------------------------------------------------
    // The Flipper streams packs it does not understand: "Key: value" blocks, shipped
    // as JSON objects of the file's own keys. All game semantics live here, so adding
    // a content game needs a loader below and nothing on the Flipper.
    void contentClear() {
        triviaTopicsClear();
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _wyr.packs[i] = WyrPack{};
        _wyr.packCount = 0;
        // Fully reset the pack arrays -- not just packCount -- or a stale item
        // count survives a re-clear that doesn't load a replacement pack.
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _scr.packs[i] = WordPack{};
        _scr.packCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _d.packs[i] = WordPack{};
        _d.packCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _spec.packs[i] = WyrPack{};
        _spec.packCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _kmk.packs[i] = WordPack{};
        _kmk.packCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _secrets.packs[i] = WordPack{};
        _secrets.packCount = 0;
        _packGame = 0;
    }

    void contentPack(uint8_t game, const char* name) {
        _packGame = game;
        if(game == HA_GAME_TRIVIA) {
            triviaAddTopic(name);
        } else if(game == HA_GAME_WYR) {
            if(_wyr.packCount < TRIVIA_MAX_TOPICS) {
                _wyr.packs[_wyr.packCount] = WyrPack{};
                _wyr.packs[_wyr.packCount].name = name;
                _wyr.packCount++;
            }
        } else if(game == HA_GAME_SCRAMBLE) {
            if(_scr.packCount < TRIVIA_MAX_TOPICS) {
                _scr.packs[_scr.packCount] = WordPack{};
                _scr.packs[_scr.packCount].name = name;
                _scr.packCount++;
            }
        } else if(game == HA_GAME_DRAW) {
            if(_d.packCount < TRIVIA_MAX_TOPICS) {
                _d.packs[_d.packCount] = WordPack{};
                _d.packs[_d.packCount].name = name;
                _d.packCount++;
            }
        } else if(game == HA_GAME_SPECTRUM) {
            if(_spec.packCount < TRIVIA_MAX_TOPICS) {
                _spec.packs[_spec.packCount] = WyrPack{};
                _spec.packs[_spec.packCount].name = name;
                _spec.packCount++;
            }
        } else if(game == HA_GAME_KMK) {
            if(_kmk.packCount < TRIVIA_MAX_TOPICS) {
                _kmk.packs[_kmk.packCount] = WordPack{};
                _kmk.packs[_kmk.packCount].name = name;
                _kmk.packCount++;
            }
        } else if(game == HA_GAME_SECRETS) {
            if(_secrets.packCount < TRIVIA_MAX_TOPICS) {
                _secrets.packs[_secrets.packCount] = WordPack{};
                _secrets.packs[_secrets.packCount].name = name;
                _secrets.packCount++;
            }
        }
    }

    void contentItem(const char* json) {
        if(!_packGame) return; // no pack begun: nothing to attach to
        if(_packGame == HA_GAME_TRIVIA) triviaLoadItem(json);
        else if(_packGame == HA_GAME_WYR) wyrLoadItem(json);
        else if(_packGame == HA_GAME_SCRAMBLE) scrambleLoadItem(json);
        else if(_packGame == HA_GAME_DRAW) drawLoadItem(json);
        else if(_packGame == HA_GAME_SPECTRUM) spectrumLoadItem(json);
        else if(_packGame == HA_GAME_KMK) kmkLoadItem(json);
        else if(_packGame == HA_GAME_SECRETS) secretsLoadItem(json);
        // Unknown game ids are dropped on purpose: a newer Flipper must not be able
        // to corrupt an older board's state.
    }

    // Map a pack file's keys into TriviaQ. The file says {q,a,b,c,d,answer}; the
    // struct wants {q, o[4], correct}. Note "c" means option C here and the correct
    // INDEX in the struct — consuming this object raw would silently mark the wrong
    // answer, so every field is mapped explicitly.
    bool triviaLoadItem(const char* json) {
        if(_topicCount == 0) return false;
        TriviaTopic& tp = _topics[_topicCount - 1];
        if(tp.qcount >= TRIVIA_MAX_QS) return false;

        char buf[200];
        if(!ha_json_str(json, "q", buf, sizeof(buf))) return false;
        TriviaQ q;
        q.q = buf;

        static const char* keys[4] = {"a", "b", "c", "d"};
        for(int k = 0; k < 4; k++) {
            if(!ha_json_str(json, keys[k], buf, sizeof(buf))) return false; // needs all four
            q.o[k] = buf;
        }

        // "Answer: B" -> 1. Anything else is not a usable question.
        if(!ha_json_str(json, "answer", buf, sizeof(buf)) || !buf[0]) return false;
        char c = buf[0];
        if(c >= 'a' && c <= 'z') c -= 32;
        if(c < 'A' || c > 'D') return false;
        q.correct = (uint8_t)(c - 'A');

        tp.qs[tp.qcount] = q;
        tp.qcount++;
        return true;
    }

    // Map a wyr pack file's {a,b} keys into a WyrPrompt in the current pack.
    bool wyrLoadItem(const char* json) {
        if(_wyr.packCount == 0) return false;
        WyrPack& p = _wyr.packs[_wyr.packCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[128];
        if(!ha_json_str(json, "a", buf, sizeof(buf))) return false;
        String a = buf;
        if(!ha_json_str(json, "b", buf, sizeof(buf))) return false;
        p.items[p.count].a = a;
        p.items[p.count].b = buf;
        p.count++;
        return true;
    }

    // Map a spectrum pack file's {left,right} keys into the current pack, reusing
    // WyrPrompt (a = left label, b = right label).
    bool spectrumLoadItem(const char* json) {
        if(_spec.packCount == 0) return false;
        WyrPack& p = _spec.packs[_spec.packCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[128];
        if(!ha_json_str(json, "left", buf, sizeof(buf)) || !buf[0]) return false;
        String left = buf;
        if(!ha_json_str(json, "right", buf, sizeof(buf)) || !buf[0]) return false;
        p.items[p.count].a = left;
        p.items[p.count].b = buf;
        p.count++;
        return true;
    }

    // Map a scramble pack file's {word} key into the current pack.
    bool scrambleLoadItem(const char* json) {
        if(_scr.packCount == 0) return false;
        WordPack& p = _scr.packs[_scr.packCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[24];
        if(!ha_json_str(json, "word", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    // Map a Kiss Marry Kill pack file's {name} key into the current pack.
    bool kmkLoadItem(const char* json) {
        if(_kmk.packCount == 0) return false;
        WordPack& p = _kmk.packs[_kmk.packCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[40];
        if(!ha_json_str(json, "name", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    // Map a Secrets pack file's {q} key (one yes/no question) into the current pack.
    bool secretsLoadItem(const char* json) {
        if(_secrets.packCount == 0) return false;
        WordPack& p = _secrets.packs[_secrets.packCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[160];
        if(!ha_json_str(json, "q", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    // Map a draw pack file's {word} key into the current pack.
    bool drawLoadItem(const char* json) {
        if(_d.packCount == 0) return false;
        WordPack& p = _d.packs[_d.packCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[24];
        if(!ha_json_str(json, "word", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    void roundEnd() {
        if(_active == HA_GAME_TRIVIA)
            triviaClear();
        else if(isDuel(_active))
            duelClear();
        else if(_active == HA_GAME_DRAW)
            drawClear();
        else if(_active == HA_GAME_PONG)
            pongClear();
        else if(_active == HA_GAME_WYR)
            wyrClear();
        else if(_active == HA_GAME_SCRAMBLE)
            scrambleClear();
        else if(_active == HA_GAME_REACT)
            reactClear();
        else if(_active == HA_GAME_GUESSCOLOR)
            gcClear();
        else if(_active == HA_GAME_BATTLESHIP)
            battleClear();
        else if(_active == HA_GAME_SPECTRUM)
            spectrumClear();
        else if(_active == HA_GAME_KMK)
            kmkClear();
        else if(_active == HA_GAME_CHESS)
            chessClear();
        else if(_active == HA_GAME_SECRETS)
            secretsClear();
        pushAll();
    }

    // Time-based updates (trivia phases, drawing timers, pong physics). From loop().
    void tick(uint32_t now) {
        // A pending game-change vote freezes the active game: advance only its timeout.
        if(_gvActive) {
            gameVoteResolve(now);
            return;
        }
        if(_active == HA_GAME_TRIVIA)
            triviaTick(now);
        else if(_active == HA_GAME_DRAW)
            drawTick(now);
        else if(_active == HA_GAME_PONG && (now - _lastPong) >= PONG_TICK_MS) {
            _lastPong = now;
            pongTick();
        } else if(_active == HA_GAME_WYR)
            wyrTick(now);
        else if(_active == HA_GAME_SCRAMBLE)
            scrambleTick(now);
        else if(_active == HA_GAME_REACT)
            reactTick(now);
        else if(_active == HA_GAME_GUESSCOLOR)
            gcTick(now);
        else if(_active == HA_GAME_SPECTRUM)
            spectrumTick(now);
        else if(_active == HA_GAME_KMK)
            kmkTick(now);
        else if(_active == HA_GAME_CHESS)
            chessTick(now);
        else if(_active == HA_GAME_SECRETS)
            secretsTick(now);
    }

    // ---- player input (parsed WS JSON) ----
    // `deviceKey` says which phone the sending socket is on (0 = unknown). It is
    // threaded in here rather than cached in a wsId -> key table because it is needed
    // at exactly one moment -- when `hello` decides whether this is a new player or a
    // phone that is already playing -- and a table would be a second connection
    // lifecycle to keep in sync with disconnects and resets for no gain.
    void onInput(uint32_t wsId, uint64_t deviceKey, const char* json) {
        char type[20];
        if(!ha_json_str(json, "t", type, sizeof(type))) return;
        if(strcmp(type, "hello") == 0) {
            char nick[HA_NICK_LEN], avatar[8];
            ha_json_str(json, "nick", nick, sizeof(nick));
            if(!ha_json_str(json, "avatar", avatar, sizeof(avatar))) avatar[0] = '\0';
            // "named" marks a hello the player actually typed (pressed Play, or edited
            // their name) as opposed to the silent auto-rejoin a reconnecting socket
            // replays. Only a typed name may rename an existing player -- see onHello.
            int named = 0;
            ha_json_int(json, "named", &named);
            onHello(wsId, deviceKey, nick, avatar, named != 0);
            return;
        }
        if(strcmp(type, "ping") == 0) {
            haWsSendWs(wsId, "{\"t\":\"pong\"}");
            return;
        }
        uint8_t pid = pidByWs(wsId);
        if(!pid) return;
        // A pending game-change vote freezes the active game: honor only the vote itself
        // (and a player leaving); every other game intent is dropped until it resolves.
        if(_gvActive) {
            if(strcmp(type, "voteGame") == 0) {
                const char* okp = ha_json_find(json, "ok");
                voteGame(pid, okp && strncmp(okp, "true", 4) == 0);
            } else if(strcmp(type, "leaveGame") == 0) {
                anyOnLeave(pid);
                pushAll();
            }
            return;
        }
        int v;
        if(strcmp(type, "react") == 0) {
            char emoji[8];
            if(ha_json_str(json, "emoji", emoji, sizeof(emoji))) onReact(pid, emoji);
            return;
        }
        if(strcmp(type, "answer") == 0 && ha_json_int(json, "c", &v)) {
            triviaAnswer(pid, v);
            wyrAnswer(pid, v);
        } else if(strcmp(type, "ready") == 0) {
            const char* rp = ha_json_find(json, "ready");
            bool r = rp && strncmp(rp, "true", 4) == 0;
            triviaReady(pid, r);
            wyrReady(pid, r);
            scrambleReady(pid, r);
            reactReady(pid, r);
            gcReady(pid, r);
            spectrumReady(pid, r);
            kmkReady(pid, r);
            secretsReady(pid, r);
        } else if(strcmp(type, "vote") == 0 && ha_json_int(json, "topic", &v)) {
            triviaVote(pid, v);
        } else if(strcmp(type, "vote") == 0 && ha_json_int(json, "pack", &v)) {
            wyrVote(pid, v);
            scrambleVote(pid, v);
            spectrumVote(pid, v);
            kmkVote(pid, v);
            secretsVote(pid, v);
        } else if(strcmp(type, "tap") == 0) {
            reactTap(pid);
        } else if(strcmp(type, "clue") == 0) {
            char c[SPECTRUM_CLUE_LEN];
            if(ha_json_str(json, "text", c, sizeof(c))) spectrumClue(pid, c);
        } else if(strcmp(type, "slide") == 0 && ha_json_int(json, "n", &v)) {
            spectrumGuess(pid, v);
        } else if(strcmp(type, "assign") == 0) {
            int k, m, x;
            if(ha_json_int(json, "kiss", &k) && ha_json_int(json, "marry", &m) &&
               ha_json_int(json, "kill", &x))
                kmkAssign(pid, k, m, x);
        } else if(strcmp(type, "predict") == 0 && ha_json_int(json, "n", &v)) {
            secretsPredict(pid, v);
        } else if(strcmp(type, "reply") == 0 && ha_json_int(json, "v", &v)) {
            secretsReply(pid, v);
        } else if(strcmp(type, "again") == 0) {
            triviaAgain(pid);
            drawAgain(pid);
            wyrAgain(pid);
            scrambleAgain(pid);
            reactAgain(pid);
            gcAgain(pid);
            spectrumAgain(pid);
            kmkAgain(pid);
            secretsAgain(pid);
        } else if(strcmp(type, "say") == 0) {
            char t[120];
            if(ha_json_str(json, "text", t, sizeof(t))) onSay(pid, t);
        } else if(strcmp(type, "challenge") == 0 && ha_json_int(json, "to", &v)) {
            matchChallenge(pid, (uint8_t)v);
        } else if(strcmp(type, "accept") == 0 && ha_json_int(json, "from", &v)) {
            matchAccept(pid, (uint8_t)v);
        } else if(strcmp(type, "cancel") == 0) {
            duelCancel(pid);
        } else if(strcmp(type, "move") == 0 && ha_json_int(json, "n", &v)) {
            duelMove(pid, v);
        } else if(strcmp(type, "move") == 0 && _active == HA_GAME_CHESS) {
            // Chess names its squares, so its "move" carries from/to instead of the
            // duels' single "n" and falls through to here.
            int from, to, promo;
            if(!ha_json_int(json, "from", &from)) from = -1;
            if(!ha_json_int(json, "to", &to)) to = -1;
            if(!ha_json_int(json, "promo", &promo)) promo = 0;
            chessMove(pid, from, to, promo);
        } else if(strcmp(type, "rematch") == 0) {
            duelRematch(pid);
            battleRematch(pid);
            chessRematch(pid);
        } else if(strcmp(type, "paddle") == 0 && ha_json_int(json, "dir", &v)) {
            pongPaddle(pid, v);
        } else if(strcmp(type, "place") == 0) {
            battlePlace(pid, json);
        } else if(strcmp(type, "fire") == 0 && ha_json_int(json, "n", &v)) {
            battleFire(pid, v);
        } else if(strcmp(type, "resign") == 0 && _active == HA_GAME_CHESS) {
            chessResign(pid);
        } else if(strcmp(type, "draw") == 0 && _active == HA_GAME_CHESS) {
            chessDraw(pid);
        } else if(strcmp(type, "claim") == 0 && _active == HA_GAME_CHESS) {
            chessClaim(pid);
        } else if(strcmp(type, "guess") == 0) {
            // A text guess (draw/scramble) or an r/g/b color guess (guess the color).
            char g[64];
            int r, gg, b;
            if(ha_json_str(json, "text", g, sizeof(g))) {
                drawGuess(pid, g);
                scrambleGuess(pid, g);
            } else if(
                ha_json_int(json, "r", &r) && ha_json_int(json, "g", &gg) &&
                ha_json_int(json, "b", &b)) {
                gcGuess(pid, r, gg, b);
            }
        } else if(strcmp(type, "stroke") == 0) {
            drawStroke(pid, json);
        } else if(strcmp(type, "clear") == 0) {
            drawClearInk(pid);
        } else if(strcmp(type, "leaveGame") == 0) {
            anyOnLeave(pid);
            pushAll();
        } else if(strcmp(type, "proposeGame") == 0) {
            char name[24];
            if(ha_json_str(json, "game", name, sizeof(name))) proposeGame(pid, name);
        }
    }

private:
    Player _p[HA_MAX_PLAYERS + 1] = {};
    uint8_t _active = HA_GAME_NONE;
    char _lang[8] = {0}; // UI language code for the phone client, "" = English
    Trivia _t = {};
    TriviaTopic _topics[TRIVIA_MAX_TOPICS] = {};
    uint8_t _topicCount = 0;
    uint8_t _packGame = 0; // HA_GAME_* of the pack currently being streamed, 0 = none
    DuelMatch _m[DUEL_MAX_MATCHES] = {};
    DuelChallenge _c[DUEL_MAX_CHALLENGES] = {};
    DrawState _d = {};
    PongMatch _pm[PONG_MAX] = {};
    uint32_t _lastPong = 0;
    WyrState _wyr = {};
    ScrambleState _scr = {};
    ReactState _react = {};
    GuessColorState _gc = {};
    BattleMatch _bm[BATTLE_MAX] = {};
    SpectrumState _spec = {};
    KmkState _kmk = {};
    ChessMatch _cm[CHESS_MAX] = {};
    SecretsState _secrets = {};

    // Cross-cutting game-change vote (above the active game). When _gvActive, the active
    // game is frozen and every client is shown a vote overlay instead of game state.
    bool _gvActive = false;
    uint8_t _gvProposer = 0; // pid who proposed (an implicit YES)
    uint8_t _gvTarget = 0; // proposed game id
    uint32_t _gvStart = 0; // millis the proposal opened (for the timeout)
    int8_t _gvVote[HA_MAX_PLAYERS + 1] = {}; // -1 none, 0 no, 1 yes

    uint8_t freePid() {
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(!_p[i].used) return i;
        return 0;
    }
    int connectedCount() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used) n++;
        return n;
    }

    // ---------- broadcast ----------
    void pushAll() {
        // A pending game-change vote replaces all game/lobby state with the vote overlay,
        // so every client freezes its current screen and shows the modal until it resolves.
        if(_gvActive) {
            for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++) {
                if(!_p[pid].used || !_p[pid].wsId) continue;
                haWsSendWs(_p[pid].wsId, gameVoteJson(pid));
            }
            return;
        }
        String lob = lobbyJson();
        for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++) {
            if(!_p[pid].used || !_p[pid].wsId) continue;
            haWsSendWs(_p[pid].wsId, lob);
            if(_active == HA_GAME_TRIVIA)
                haWsSendWs(_p[pid].wsId, triviaJson(pid));
            else if(isDuel(_active))
                haWsSendWs(_p[pid].wsId, duelJson(pid));
            else if(_active == HA_GAME_DRAW)
                haWsSendWs(_p[pid].wsId, drawJson(pid));
            else if(_active == HA_GAME_PONG)
                haWsSendWs(_p[pid].wsId, pongJson(pid));
            else if(_active == HA_GAME_WYR)
                haWsSendWs(_p[pid].wsId, wyrJson(pid));
            else if(_active == HA_GAME_SCRAMBLE)
                haWsSendWs(_p[pid].wsId, scrambleJson(pid));
            else if(_active == HA_GAME_REACT)
                haWsSendWs(_p[pid].wsId, reactJson(pid));
            else if(_active == HA_GAME_GUESSCOLOR)
                haWsSendWs(_p[pid].wsId, gcJson(pid));
            else if(_active == HA_GAME_BATTLESHIP)
                haWsSendWs(_p[pid].wsId, battleJson(pid));
            else if(_active == HA_GAME_SPECTRUM)
                haWsSendWs(_p[pid].wsId, spectrumJson(pid));
            else if(_active == HA_GAME_KMK)
                haWsSendWs(_p[pid].wsId, kmkJson(pid));
            else if(_active == HA_GAME_CHESS)
                haWsSendWs(_p[pid].wsId, chessJson(pid));
            else if(_active == HA_GAME_SECRETS)
                haWsSendWs(_p[pid].wsId, secretsJson(pid));
        }
    }

    String playersJson() {
        String s = "[";
        bool first = true;
        for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++) {
            if(!_p[pid].used) continue;
            if(!first) s += ",";
            s += "{\"pid\":";
            s += pid;
            s += ",\"nick\":\"";
            s += ha_json_escape(_p[pid].nick);
            s += "\",\"avatar\":\"";
            s += ha_json_escape(_p[pid].avatar);
            s += "\",\"score\":";
            s += _p[pid].score;
            // In a 1v1 match (playing OR still on the over screen): don't let others
            // challenge them until they return to the lobby.
            s += ",\"busy\":";
            s += inAnyMatch(pid) ? "true" : "false";
            s += "}";
            first = false;
        }
        s += "]";
        return s;
    }

    static const char* gameName(uint8_t g) {
        switch(g) {
        case HA_GAME_TRIVIA:
            return "trivia";
        case HA_GAME_CONNECT4:
            return "connect4";
        case HA_GAME_TICTACTOE:
            return "tictactoe";
        case HA_GAME_DOTS:
            return "dots";
        case HA_GAME_DRAW:
            return "draw";
        case HA_GAME_PONG:
            return "pong";
        case HA_GAME_REACT:
            return "react";
        case HA_GAME_WYR:
            return "wyr";
        case HA_GAME_SCRAMBLE:
            return "scramble";
        case HA_GAME_REVERSI:
            return "reversi";
        case HA_GAME_GUESSCOLOR:
            return "gc";
        case HA_GAME_BATTLESHIP:
            return "bs";
        case HA_GAME_SPECTRUM:
            return "spectrum";
        case HA_GAME_KMK:
            return "kmk";
        case HA_GAME_CHESS:
            return "chess";
        case HA_GAME_SECRETS:
            return "secrets";
        default:
            return "none";
        }
    }

    String lobbyJson() {
        return String("{\"t\":\"lobby\",\"game\":\"") + gameName(_active) +
               "\",\"players\":" + playersJson() + "}";
    }

    static bool isDuel(uint8_t g) {
        return g == HA_GAME_CONNECT4 || g == HA_GAME_TICTACTOE || g == HA_GAME_DOTS ||
               g == HA_GAME_REVERSI;
    }

    // ---------- trivia (phone-driven, self-organizing) ----------
    // Pull the four strings of "o":[...] in order into opts[4].
    static void parseOptions(const char* json, String opts[4]) {
        const char* q = ha_json_find(json, "o");
        if(!q || *q != '[') return;
        q++;
        for(int k = 0; k < 4 && *q; k++) {
            while(*q == ' ' || *q == ',') q++;
            if(*q != '"') break;
            q++;
            String s;
            while(*q && *q != '"') {
                if(*q == '\\' && q[1]) {
                    q++;
                    s += *q;
                } else {
                    s += *q;
                }
                q++;
            }
            opts[k] = s;
            if(*q == '"') q++;
        }
    }

    void triviaClear() {
        _t.phase = 0; // lobby
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _t.ready[i] = false;
            _t.vote[i] = -1;
            _t.answer[i] = -1;
            _t.answerMs[i] = 0;
            _t.gained[i] = 0;
        }
        for(int k = 0; k < 4; k++) _t.counts[k] = 0;
        _t.qi = 0;
        _t.topic = 0;
        _t.lastSec = -1;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used) _p[i].score = 0;
    }

    bool triviaAllReady() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(!_t.ready[i]) return false;
        }
        return n >= 1;
    }

    bool triviaAllAnswered() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(_t.answer[i] < 0) return false;
        }
        return n >= 1;
    }

    void triviaCheckStart() {
        if(_active != HA_GAME_TRIVIA) return;
        if(_t.phase == 0 && _topicCount > 0 && triviaAllReady()) {
            _t.phase = 1; // all ready -> countdown
            // Lock in the winning topic now (votes are frozen during the
            // countdown) so the countdown shows the right name and the questions
            // come from the same topic (recomputing could break a random tie).
            _t.topic = (uint8_t)triviaWinningTopic();
            _t.countdownEnd = millis() + (uint32_t)TRIVIA_COUNTDOWN * 1000;
            _t.lastSec = -1;
        } else if(_t.phase == 1 && !triviaAllReady()) {
            _t.phase = 0; // someone unreadied / a new player joined -> cancel
        }
    }

    void triviaOnRosterChange() {
        if(_active != HA_GAME_TRIVIA) return;
        triviaCheckStart();
        if(_t.phase == 2 && triviaAllAnswered()) triviaDoReveal();
    }

    void triviaReady(uint8_t pid, bool r) {
        if(_active != HA_GAME_TRIVIA || (_t.phase != 0 && _t.phase != 1)) return;
        _t.ready[pid] = r;
        triviaCheckStart();
        pushAll();
    }

    void triviaVote(uint8_t pid, int topic) {
        if(_active != HA_GAME_TRIVIA || _t.phase != 0) return;
        if(topic < 0 || topic >= _topicCount) return;
        _t.vote[pid] = (int8_t)topic;
        pushAll();
    }

    int triviaWinningTopic() {
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _t.vote[i] >= 0 && _t.vote[i] < _topicCount) {
                votes[_t.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_topicCount);
        int best = 0;
        for(int i = 1; i < _topicCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _topicCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void triviaStartQuestion() {
        _t.phase = 2;
        _t.deadline = millis() + (uint32_t)TRIVIA_QDUR * 1000;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _t.answer[i] = -1;
            _t.answerMs[i] = 0;
            _t.gained[i] = 0;
        }
        for(int k = 0; k < 4; k++) _t.counts[k] = 0;
        pushAll();
    }

    void triviaBeginGame() {
        // _t.topic was locked in when the countdown started (triviaCheckStart).
        _t.qi = 0;
        triviaStartQuestion();
    }

    int triviaPoints(uint32_t answeredAt) {
        uint32_t start = _t.deadline - (uint32_t)TRIVIA_QDUR * 1000;
        long elapsed = (long)answeredAt - (long)start;
        long total = (long)TRIVIA_QDUR * 1000;
        if(elapsed < 0) elapsed = 0;
        if(elapsed > total) elapsed = total;
        int bonus = (int)(500L * (total - elapsed) / (total ? total : 1));
        return 500 + bonus;
    }

    void triviaAnswer(uint8_t pid, int c) {
        if(_active != HA_GAME_TRIVIA || _t.phase != 2) return;
        if(c < 0 || c > 3 || _t.answer[pid] >= 0 || millis() > _t.deadline) return;
        _t.answer[pid] = (int8_t)c;
        _t.answerMs[pid] = millis();
        _t.counts[c]++;
        if(triviaAllAnswered())
            triviaDoReveal();
        else
            pushAll();
    }

    void triviaDoReveal() {
        _t.phase = 3;
        uint8_t correct = _topics[_t.topic].qs[_t.qi].correct;
        for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++) {
            if(!_p[pid].used || _t.answer[pid] < 0) continue;
            if(_t.answer[pid] == correct) {
                int pts = triviaPoints(_t.answerMs[pid]);
                _p[pid].score += pts;
                _t.gained[pid] = pts;
                haUartScore(pid, pts, "trivia");
            }
        }
        _t.revealUntil = millis() + TRIVIA_REVEAL_MS;
        pushAll();
    }

    void triviaNext() {
        _t.qi++;
        if(_t.qi >= _topics[_t.topic].qcount) {
            _t.phase = 4; // final
            haUartRoundResult("{\"trivia\":\"final\"}");
            pushAll();
        } else {
            triviaStartQuestion();
        }
    }

    void triviaAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_TRIVIA || _t.phase != 4) return;
        triviaClear(); // back to the lobby (scores reset)
        pushAll();
    }

    void triviaTick(uint32_t now) {
        if(_t.phase == 1) { // countdown
            if(now >= _t.countdownEnd) {
                triviaBeginGame();
                return;
            }
            int secs = (int)((_t.countdownEnd - now + 999) / 1000);
            if(secs != _t.lastSec) {
                _t.lastSec = secs;
                pushAll(); // client shows the new second + plays a tick
            }
        } else if(_t.phase == 2) { // question
            if(now > _t.deadline) triviaDoReveal();
        } else if(_t.phase == 3) { // reveal
            if(now > _t.revealUntil) triviaNext();
        }
    }

    // Leaderboard: connected players sorted by score desc.
    String triviaBoard() {
        uint8_t order[HA_MAX_PLAYERS];
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used) order[n++] = i;
        for(int a = 0; a < n; a++)
            for(int b = a + 1; b < n; b++)
                if(_p[order[b]].score > _p[order[a]].score) {
                    uint8_t t = order[a];
                    order[a] = order[b];
                    order[b] = t;
                }
        String s = "[";
        for(int i = 0; i < n; i++) {
            if(i) s += ",";
            s += "{\"pid\":";
            s += order[i];
            s += ",\"nick\":\"";
            s += ha_json_escape(_p[order[i]].nick);
            s += "\",\"avatar\":\"";
            s += ha_json_escape(_p[order[i]].avatar);
            s += "\",\"score\":";
            s += _p[order[i]].score;
            s += "}";
        }
        s += "]";
        return s;
    }

    String triviaJson(uint8_t pid) {
        if(_t.phase == 0) { // lobby: ready + topic vote
            String s = "{\"t\":\"trivia\",\"phase\":\"lobby\",\"players\":[";
            bool first = true;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                if(!_p[i].used) continue;
                if(!first) s += ",";
                first = false;
                s += "{\"pid\":";
                s += i;
                s += ",\"nick\":\"";
                s += ha_json_escape(_p[i].nick);
                s += "\",\"avatar\":\"";
                s += ha_json_escape(_p[i].avatar);
                s += "\",\"ready\":";
                s += _t.ready[i] ? "true" : "false";
                s += "}";
            }
            s += "],\"topics\":[";
            int votes[TRIVIA_MAX_TOPICS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _t.vote[i] >= 0 && _t.vote[i] < _topicCount) votes[_t.vote[i]]++;
            for(int i = 0; i < _topicCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"";
                s += ha_json_escape(_topics[i].name.c_str());
                s += "\",\"votes\":";
                s += votes[i];
                s += "}";
            }
            s += "],\"myvote\":";
            s += _t.vote[pid];
            s += ",\"myready\":";
            s += _t.ready[pid] ? "true" : "false";
            s += "}";
            return s;
        }
        if(_t.phase == 1) { // countdown
            uint32_t now = millis();
            int secs = (now >= _t.countdownEnd) ? 1 : (int)((_t.countdownEnd - now + 999) / 1000);
            if(secs < 1) secs = 1;
            return String("{\"t\":\"trivia\",\"phase\":\"countdown\",\"secs\":") + secs +
                   ",\"topic\":\"" + ha_json_escape(_topics[_t.topic].name.c_str()) + "\"}";
        }
        if(_t.phase == 4) { // final
            return String("{\"t\":\"trivia\",\"phase\":\"final\",\"board\":") + triviaBoard() + "}";
        }
        // question / reveal
        TriviaTopic& tp = _topics[_t.topic];
        TriviaQ& q = tp.qs[_t.qi];
        const char* phase = (_t.phase == 3) ? "reveal" : "question";
        String s = String("{\"t\":\"trivia\",\"phase\":\"") + phase + "\",\"i\":" + _t.qi +
                   ",\"n\":" + tp.qcount + ",\"q\":\"" + ha_json_escape(q.q.c_str()) + "\",\"o\":[";
        for(int k = 0; k < 4; k++) {
            if(k) s += ",";
            s += "\"";
            s += ha_json_escape(q.o[k].c_str());
            s += "\"";
        }
        s += "],\"mine\":";
        s += _t.answer[pid];
        s += ",\"topic\":\"";
        s += ha_json_escape(tp.name.c_str());
        s += "\",\"board\":" + triviaBoard();
        if(_t.phase == 2) {
            int answered = 0;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _t.answer[i] >= 0) answered++;
            s += ",\"dur\":";
            s += TRIVIA_QDUR;
            s += ",\"deadline\":";
            s += _t.deadline;
            s += ",\"answered\":";
            s += answered;
            s += ",\"total\":";
            s += connectedCount();
        } else { // reveal
            s += ",\"correct\":";
            s += q.correct;
            s += ",\"counts\":[";
            for(int k = 0; k < 4; k++) {
                if(k) s += ",";
                s += _t.counts[k];
            }
            s += "],\"gained\":";
            s += _t.gained[pid];
        }
        s += "}";
        return s;
    }

    // ---------- duels (connect4 / tic-tac-toe / dots) ----------
    void duelClear() {
        for(int i = 0; i < DUEL_MAX_MATCHES; i++) _m[i] = DuelMatch{};
        for(int i = 0; i < DUEL_MAX_CHALLENGES; i++) _c[i] = DuelChallenge{};
    }

    static const char* kindStr(uint8_t kind) {
        return kind == HA_GAME_TICTACTOE ? "ttt" :
               kind == HA_GAME_DOTS      ? "dots" :
               kind == HA_GAME_REVERSI   ? "reversi" :
                                           "c4";
    }

    // Grid params for c4/ttt.
    static void gridParams(uint8_t kind, int& cols, int& rows, int& need, bool& gravity) {
        if(kind == HA_GAME_TICTACTOE) {
            cols = 3;
            rows = 3;
            need = 3;
            gravity = false;
        } else { // connect4
            cols = 7;
            rows = 6;
            need = 4;
            gravity = true;
        }
    }

    DuelMatch* matchOf(uint8_t pid) {
        for(int i = 0; i < DUEL_MAX_MATCHES; i++) {
            if(!_m[i].used) continue;
            if(_m[i].a == pid && _m[i].aIn) return &_m[i];
            if(_m[i].b == pid && _m[i].bIn) return &_m[i];
        }
        return nullptr;
    }

    void duelRemoveChallengesInvolving(uint8_t pid) {
        for(int i = 0; i < DUEL_MAX_CHALLENGES; i++)
            if(_c[i].used && (_c[i].from == pid || _c[i].to == pid)) _c[i] = DuelChallenge{};
    }

    // Challenge/accept are shared by all 1v1 games (duels + pong + battleship + chess).
    bool isMatchGame() {
        return isDuel(_active) || _active == HA_GAME_PONG ||
               _active == HA_GAME_BATTLESHIP || _active == HA_GAME_CHESS;
    }
    bool inAnyMatch(uint8_t pid) {
        return matchOf(pid) || pongMatchOf(pid) || battleMatchOf(pid) || chessMatchOf(pid);
    }

    void matchChallenge(uint8_t from, uint8_t to) {
        if(!isMatchGame()) return;
        if(to == from || to < 1 || to > HA_MAX_PLAYERS || !_p[to].used) return;
        if(inAnyMatch(from) || inAnyMatch(to)) return;
        // one outstanding challenge per challenger
        for(int i = 0; i < DUEL_MAX_CHALLENGES; i++)
            if(_c[i].used && _c[i].from == from) _c[i] = DuelChallenge{};
        for(int i = 0; i < DUEL_MAX_CHALLENGES; i++) {
            if(!_c[i].used) {
                _c[i] = DuelChallenge{true, from, to};
                break;
            }
        }
        if(_p[to].wsId)
            haWsSendWs(
                _p[to].wsId,
                String("{\"t\":\"toast\",\"msg\":\"") + ha_json_escape(_p[from].nick) +
                    " challenges you\"}");
        pushAll();
    }

    // Set up a fresh match between a (mark 1) and b (mark 2); `first` moves first.
    void duelStart(DuelMatch* m, uint8_t a, uint8_t b, uint8_t first) {
        *m = DuelMatch{};
        m->used = true;
        m->kind = _active;
        m->a = a;
        m->b = b;
        m->aIn = m->bIn = true;
        m->turn = first;
        m->first = first;
        m->phase = 1;
        m->winner = 0;
        if(m->kind == HA_GAME_REVERSI) {
            // 8x8 with the four center starting discs (black=1 = mark a, white=2 = mark b).
            // Standard opening: d5,e4 black; d4,e5 white. Black (challenger) moves first.
            m->board[3 * 8 + 3] = 2; // d4 white
            m->board[3 * 8 + 4] = 1; // e4 black
            m->board[4 * 8 + 3] = 1; // d5 black
            m->board[4 * 8 + 4] = 2; // e5 white
        }
    }

    void matchAccept(uint8_t pid, uint8_t from) {
        if(!isMatchGame()) return;
        bool found = false;
        for(int i = 0; i < DUEL_MAX_CHALLENGES; i++)
            if(_c[i].used && _c[i].from == from && _c[i].to == pid) found = true;
        if(!found) return;
        if(inAnyMatch(pid) || inAnyMatch(from)) return;
        if(_active == HA_GAME_PONG) {
            for(int i = 0; i < PONG_MAX; i++)
                if(!_pm[i].used) {
                    pongStart(&_pm[i], from, pid);
                    break;
                }
        } else if(_active == HA_GAME_BATTLESHIP) {
            for(int i = 0; i < BATTLE_MAX; i++)
                if(!_bm[i].used) {
                    battleStart(&_bm[i], from, pid, from); // challenger fires first
                    break;
                }
        } else if(_active == HA_GAME_CHESS) {
            for(int i = 0; i < CHESS_MAX; i++)
                if(!_cm[i].used) {
                    chessStart(&_cm[i], from, pid, from); // challenger plays white
                    break;
                }
        } else {
            for(int i = 0; i < DUEL_MAX_MATCHES; i++)
                if(!_m[i].used) {
                    duelStart(&_m[i], from, pid, from); // challenger moves first
                    break;
                }
        }
        duelRemoveChallengesInvolving(pid);
        duelRemoveChallengesInvolving(from);
        const char* key = (_active == HA_GAME_PONG)       ? "pong" :
                          (_active == HA_GAME_BATTLESHIP) ? "bs" :
                          (_active == HA_GAME_CHESS)      ? "chess" :
                                                            "duel";
        haUartEvent(
            String("{\"") + key + "\":\"" + ha_json_escape(_p[from].nick) + " vs " +
            ha_json_escape(_p[pid].nick) + "\"}");
        pushAll();
    }

    void anyOnLeave(uint8_t pid) {
        duelOnLeave(pid);
        pongOnLeave(pid);
        battleOnLeave(pid);
        chessOnLeave(pid);
    }

    void duelCancel(uint8_t pid) {
        duelRemoveChallengesInvolving(pid);
        pushAll();
    }

    // Rematch: in an over match, restart the same pairing with the first move
    // alternated. Only if the opponent is still attached.
    void duelRematch(uint8_t pid) {
        DuelMatch* m = matchOf(pid);
        if(!m || m->phase != 2) return;
        if(!m->aIn || !m->bIn) {
            // Opponent has left: there is no one to rematch. Send this player back to
            // the lobby with a note, rather than silently doing nothing.
            if(_p[pid].wsId)
                haWsSendWs(_p[pid].wsId, String("{\"t\":\"toast\",\"msg\":\"Opponent left\"}"));
            duelOnLeave(pid);
            pushAll();
            return;
        }
        uint8_t next = (m->first == m->a) ? m->b : m->a;
        duelStart(m, m->a, m->b, next);
        pushAll();
    }

    void duelMove(uint8_t pid, int n) {
        DuelMatch* m = matchOf(pid);
        if(!m || m->phase != 1 || m->turn != pid) return;
        uint8_t mark = (pid == m->a) ? 1 : 2;
        if(m->kind == HA_GAME_DOTS)
            dotsMove(m, pid, n, mark);
        else if(m->kind == HA_GAME_REVERSI)
            reversiMove(m, pid, n, mark);
        else
            gridMove(m, pid, n, mark);
        pushAll();
    }

    // ---- reversi / othello (8x8, capture by flanking) ----
    // 8 ray directions (row,col deltas), shared by the flip helpers.
    static const int* revDR() { static const int d[8] = {-1, -1, -1, 0, 0, 1, 1, 1}; return d; }
    static const int* revDC() { static const int d[8] = {-1, 0, 1, -1, 1, -1, 0, 1}; return d; }

    // How many opponent discs a move at (r,c) by `mark` would flip (0 = illegal).
    static int reversiFlips(const uint8_t* b, int r, int c, uint8_t mark) {
        if(b[r * 8 + c] != 0) return 0;
        const int *DR = revDR(), *DC = revDC();
        uint8_t opp = (mark == 1) ? 2 : 1;
        int total = 0;
        for(int d = 0; d < 8; d++) {
            int rr = r + DR[d], cc = c + DC[d], run = 0;
            while(rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && b[rr * 8 + cc] == opp) {
                rr += DR[d];
                cc += DC[d];
                run++;
            }
            if(run > 0 && rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && b[rr * 8 + cc] == mark)
                total += run;
        }
        return total;
    }

    static bool reversiHasMove(const uint8_t* b, uint8_t mark) {
        for(int i = 0; i < 64; i++)
            if(b[i] == 0 && reversiFlips(b, i / 8, i % 8, mark) > 0) return true;
        return false;
    }

    void reversiMove(DuelMatch* m, uint8_t pid, int n, uint8_t mark) {
        if(n < 0 || n >= 64) return;
        int r = n / 8, c = n % 8;
        if(reversiFlips(m->board, r, c, mark) == 0) return; // illegal
        const int *DR = revDR(), *DC = revDC();
        uint8_t opp = (mark == 1) ? 2 : 1;
        m->board[n] = mark;
        for(int d = 0; d < 8; d++) {
            int rr = r + DR[d], cc = c + DC[d], run = 0;
            while(rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && m->board[rr * 8 + cc] == opp) {
                rr += DR[d];
                cc += DC[d];
                run++;
            }
            if(run > 0 && rr >= 0 && rr < 8 && cc >= 0 && cc < 8 && m->board[rr * 8 + cc] == mark) {
                rr = r + DR[d];
                cc = c + DC[d];
                for(int s = 0; s < run; s++) {
                    m->board[rr * 8 + cc] = mark;
                    rr += DR[d];
                    cc += DC[d];
                }
            }
        }
        // Whose turn next: opponent if they can move, else same player if they can,
        // else the board is settled -> count discs and finish.
        uint8_t oppPid = (pid == m->a) ? m->b : m->a;
        if(reversiHasMove(m->board, opp))
            m->turn = oppPid;
        else if(reversiHasMove(m->board, mark))
            m->turn = pid; // opponent passes
        else
            reversiFinish(m);
    }

    void reversiFinish(DuelMatch* m) {
        int a = 0, b = 0;
        for(int i = 0; i < 64; i++) {
            if(m->board[i] == 1) a++;
            else if(m->board[i] == 2) b++;
        }
        uint8_t w = (a > b) ? m->a : (b > a) ? m->b : 0;
        duelFinish(m, w);
    }

    void gridMove(DuelMatch* m, uint8_t pid, int n, uint8_t mark) {
        int cols, rows, need;
        bool gravity;
        gridParams(m->kind, cols, rows, need, gravity);
        int row, col;
        if(gravity) {
            col = n;
            if(col < 0 || col >= cols) return;
            row = -1;
            for(int r = rows - 1; r >= 0; r--)
                if(m->board[r * cols + col] == 0) {
                    row = r;
                    break;
                }
            if(row < 0) return; // column full
        } else {
            if(n < 0 || n >= cols * rows) return;
            if(m->board[n] != 0) return; // cell taken
            row = n / cols;
            col = n % cols;
        }
        m->board[row * cols + col] = mark;
        if(gridWins(m->board, cols, rows, need, row, col, mark))
            duelFinish(m, pid);
        else if(gridFull(m->board, cols, rows))
            duelFinish(m, 0);
        else
            m->turn = (pid == m->a) ? m->b : m->a;
    }

    void dotsMove(DuelMatch* m, uint8_t pid, int n, uint8_t mark) {
        if(n < 0 || n >= DOTS_HEDGES + DOTS_VEDGES) return;
        if(n < DOTS_HEDGES) {
            if(m->hedges[n]) return;
            m->hedges[n] = 1;
        } else {
            int vi = n - DOTS_HEDGES;
            if(m->vedges[vi]) return;
            m->vedges[vi] = 1;
        }
        bool claimed = false;
        for(int r = 0; r < DOTS_H; r++)
            for(int c = 0; c < DOTS_W; c++) {
                int bi = r * DOTS_W + c;
                if(m->boxes[bi]) continue;
                if(dotsBoxComplete(m, r, c)) {
                    m->boxes[bi] = mark;
                    if(mark == 1)
                        m->sA++;
                    else
                        m->sB++;
                    claimed = true;
                }
            }
        if(m->sA + m->sB >= DOTS_BOXES) {
            uint8_t w = (m->sA > m->sB) ? m->a : (m->sB > m->sA) ? m->b : 0;
            duelFinish(m, w);
        } else if(!claimed) {
            m->turn = (pid == m->a) ? m->b : m->a; // completing a box grants another turn
        }
    }

    static bool dotsBoxComplete(const DuelMatch* m, int r, int c) {
        return m->hedges[r * DOTS_W + c] && m->hedges[(r + 1) * DOTS_W + c] &&
               m->vedges[r * (DOTS_W + 1) + c] && m->vedges[r * (DOTS_W + 1) + c + 1];
    }

    void duelFinish(DuelMatch* m, uint8_t winnerPid) {
        if(m->phase != 1) return;
        m->phase = 2;
        m->winner = winnerPid;
        uint8_t loser = (winnerPid == m->a) ? m->b : (winnerPid == m->b) ? m->a : 0;
        if(winnerPid) {
            _p[winnerPid].score += 300;
            haUartScore(winnerPid, 300, "duelwin");
            haUartRoundResult(String("{\"win\":") + winnerPid + ",\"lose\":" + loser + "}");
        } else {
            haUartRoundResult(String("{\"draw\":[") + m->a + "," + m->b + "]}");
        }
    }

    // A player returns to lobby or disconnects. Forfeit a live match to opponent.
    void duelOnLeave(uint8_t pid) {
        DuelMatch* m = matchOf(pid);
        if(!m) return;
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        if(m->phase == 1) duelFinish(m, opp); // forfeit
        if(pid == m->a) m->aIn = false;
        if(pid == m->b) m->bIn = false;
        if(!m->aIn && !m->bIn) *m = DuelMatch{}; // both gone: free the slot
    }

    static bool gridFull(const uint8_t* b, int cols, int rows) {
        for(int i = 0; i < cols * rows; i++)
            if(b[i] == 0) return false;
        return true;
    }

    static bool
        gridWins(const uint8_t* b, int cols, int rows, int need, int row, int col, uint8_t disc) {
        static const int dr[4] = {0, 1, 1, 1};
        static const int dc[4] = {1, 0, 1, -1};
        for(int d = 0; d < 4; d++) {
            int cnt = 1;
            for(int s = 1; s < need; s++) {
                int r = row + dr[d] * s, c = col + dc[d] * s;
                if(r < 0 || r >= rows || c < 0 || c >= cols) break;
                if(b[r * cols + c] != disc) break;
                cnt++;
            }
            for(int s = 1; s < need; s++) {
                int r = row - dr[d] * s, c = col - dc[d] * s;
                if(r < 0 || r >= rows || c < 0 || c >= cols) break;
                if(b[r * cols + c] != disc) break;
                cnt++;
            }
            if(cnt >= need) return true;
        }
        return false;
    }

    String duelChallengesJson() {
        String s = "[";
        bool first = true;
        for(int i = 0; i < DUEL_MAX_CHALLENGES; i++) {
            if(!_c[i].used) continue;
            if(!first) s += ",";
            s += "{\"from\":";
            s += _c[i].from;
            s += ",\"to\":";
            s += _c[i].to;
            s += "}";
            first = false;
        }
        s += "]";
        return s;
    }

    static String intArray(const uint8_t* a, int n) {
        String s = "[";
        for(int i = 0; i < n; i++) {
            if(i) s += ",";
            s += a[i];
        }
        s += "]";
        return s;
    }

    String duelJson(uint8_t pid) {
        DuelMatch* m = matchOf(pid);
        const char* kind = kindStr(_active);
        if(!m) {
            return String("{\"t\":\"duel\",\"kind\":\"") + kind +
                   "\",\"phase\":\"lobby\",\"challenges\":" + duelChallengesJson() + "}";
        }
        kind = kindStr(m->kind);
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        uint8_t me = (pid == m->a) ? 1 : 2;
        const char* phase = (m->phase == 2) ? "over" : "playing";
        String s = String("{\"t\":\"duel\",\"kind\":\"") + kind + "\",\"phase\":\"" + phase +
                   "\",\"turn\":" + m->turn + ",\"me\":" + me + ",\"you\":" + pid + ",\"opp\":\"" +
                   ha_json_escape(_p[opp].nick) + "\"";
        if(m->kind == HA_GAME_DOTS) {
            s += ",\"w\":";
            s += DOTS_W;
            s += ",\"h\":";
            s += DOTS_H;
            s += ",\"hedges\":" + intArray(m->hedges, DOTS_HEDGES);
            s += ",\"vedges\":" + intArray(m->vedges, DOTS_VEDGES);
            s += ",\"boxes\":" + intArray(m->boxes, DOTS_BOXES);
            s += ",\"sme\":";
            s += (me == 1) ? m->sA : m->sB;
            s += ",\"sopp\":";
            s += (me == 1) ? m->sB : m->sA;
        } else if(m->kind == HA_GAME_REVERSI) {
            int cA = 0, cB = 0;
            for(int i = 0; i < 64; i++) {
                if(m->board[i] == 1) cA++;
                else if(m->board[i] == 2) cB++;
            }
            s += ",\"cols\":8,\"rows\":8";
            s += ",\"board\":" + intArray(m->board, 64);
            s += ",\"sme\":";
            s += (me == 1) ? cA : cB;
            s += ",\"sopp\":";
            s += (me == 1) ? cB : cA;
            // Legal moves for the player to move, so the client can highlight them.
            s += ",\"valid\":[";
            if(m->phase == 1 && m->turn == pid) {
                bool f = true;
                for(int i = 0; i < 64; i++)
                    if(m->board[i] == 0 && reversiFlips(m->board, i / 8, i % 8, me) > 0) {
                        if(!f) s += ",";
                        s += i;
                        f = false;
                    }
            }
            s += "]";
        } else {
            int cols, rows, need;
            bool gravity;
            gridParams(m->kind, cols, rows, need, gravity);
            s += ",\"cols\":";
            s += cols;
            s += ",\"rows\":";
            s += rows;
            s += ",\"need\":";
            s += need;
            s += ",\"gravity\":";
            s += gravity ? "true" : "false";
            s += ",\"board\":" + intArray(m->board, cols * rows);
        }
        if(m->phase == 2) {
            const char* r = (m->winner == 0) ? "draw" : (m->winner == pid) ? "win" : "lose";
            s += ",\"result\":\"";
            s += r;
            s += "\"";
        }
        s += "}";
        return s;
    }

    // ---------- drawing + guessing ----------
    // Reset round state only -- packs/packCount are content, streamed once at
    // session start, and must survive selectGame()/again clearing round state
    // (mirrors wyrClear/scrambleClear, which likewise leave their packs alone).
    void drawClear() {
        _d.phase = 0;
        _d.drawer = 0;
        _d.drawerSeq = 0;
        _d.wordSeq = 0;
        _d.word[0] = '\0';
        _d.round = 0;
        _d.roundsTotal = 0;
        _d.deadline = 0;
        _d.revealUntil = 0;
        _d.winner = 0;
        _d.pack = 0; // no draw vote strip yet (see Task 3): always pack 0
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _d.vote[i] = -1;
    }

    void drawAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_DRAW || _d.phase != 3) return;
        drawClear(); // back to idle; the tick restarts once 2+ players are present
        pushAll();
    }

    // Lobby / general chat: relay a player's line to everyone as a chat message.
    void onSay(uint8_t pid, const char* text) {
        if(!text[0]) return;
        haWsBroadcast(
            String("{\"t\":\"chat\",\"nick\":\"") + ha_json_escape(_p[pid].nick) + "\",\"text\":\"" +
            ha_json_escape(text) + "\"}");
        // Also surface it on the Flipper console so the host can follow the lobby chat.
        haUartEvent(String("{\"chat\":\"") + ha_json_escape(_p[pid].nick) + ": " +
                    ha_json_escape(text) + "\"}");
    }

    // Emoji reaction. Goes to whoever shares your screen: your opponent if you are
    // in a 1v1 match, otherwise everyone else who is also un-matched. In the lobby
    // and in every whole-group game nobody is in a match, so that second case is
    // "everyone" and the behaviour there is unchanged. Without this, six concurrent
    // duels spray emoji at each other about games nobody else can see.
    //
    // matchOf/pongMatchOf both gate on aIn/bIn, so a player who has returned to the
    // lobby from a finished match correctly counts as un-matched.
    //
    // The sender is always included: the client renders nothing locally and waits
    // for this echo, so dropping the sender would hide your own reaction from you.
    //
    // Uses type "emoji" so it never collides with the reaction-duel game's
    // {t:"react",phase} state messages.
    void onReact(uint8_t pid, const char* emoji) {
        if(!emoji[0]) return;
        String msg = String("{\"t\":\"emoji\",\"pid\":") + pid + ",\"nick\":\"" +
                     ha_json_escape(_p[pid].nick) + "\",\"avatar\":\"" +
                     ha_json_escape(_p[pid].avatar) + "\",\"emoji\":\"" +
                     ha_json_escape(emoji) + "\"}";
        DuelMatch* dm = matchOf(pid);
        PongMatch* pm = dm ? nullptr : pongMatchOf(pid);
        BattleMatch* bm = (dm || pm) ? nullptr : battleMatchOf(pid);
        ChessMatch* cm = (dm || pm || bm) ? nullptr : chessMatchOf(pid);
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_p[i].wsId) continue;
            bool peer;
            if(dm)
                peer = (i == dm->a || i == dm->b);
            else if(pm)
                peer = (i == pm->a || i == pm->b);
            else if(bm)
                peer = (i == bm->a || i == bm->b);
            else if(cm)
                peer = (i == cm->a || i == cm->b);
            else
                peer = !inAnyMatch(i); // lobby / whole-group: reaches everyone not in a match
            if(peer) haWsSendWs(_p[i].wsId, msg);
        }
    }

    void drawStart(uint32_t now) {
        if(_d.packCount == 0) return; // no pack streamed: refuse to start a round
        int used = connectedCount();
        if(used < 2) {
            _d.phase = 0;
            pushAll();
            return;
        }
        if(_d.round == 0) { // fresh game: everyone draws once (capped), scores reset
            _d.roundsTotal = used < 6 ? used : 6;
            if(_d.roundsTotal < 2) _d.roundsTotal = 2;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used) _p[i].score = 0;
        }
        if(_d.round >= _d.roundsTotal) { // played them all -> final scoreboard
            _d.phase = 3;
            haUartRoundResult("{\"draw\":\"final\"}");
            pushAll();
            return;
        }
        _d.drawerSeq++;
        int target = _d.drawerSeq % used, i = 0;
        uint8_t drawer = 0;
        for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++)
            if(_p[pid].used) {
                if(i == target) {
                    drawer = pid;
                    break;
                }
                i++;
            }
        if(!drawer) {
            _d.phase = 0;
            return;
        }
        WordPack& dp = _d.packs[_d.pack];
        if(dp.count == 0) { // empty pack: nothing to draw, end the game
            _d.phase = 3;
            haUartRoundResult("{\"draw\":\"final\"}");
            pushAll();
            return;
        }
        _d.drawer = drawer;
        strlcpy(_d.word, dp.words[_d.wordSeq % dp.count].c_str(), sizeof(_d.word));
        _d.wordSeq++;
        _d.phase = 1;
        _d.round++;
        _d.winner = 0;
        _d.deadline = now + (uint32_t)DRAW_SECS * 1000;
        haWsBroadcast("{\"t\":\"ink\",\"clear\":true}");
        pushAll();
        haUartEvent(String("{\"draw\":\"") + ha_json_escape(_p[drawer].nick) + " drawing\"}");
    }

    void drawReveal(uint32_t now, uint8_t winner) {
        _d.phase = 2;
        _d.winner = winner;
        _d.revealUntil = now + DRAW_REVEAL_MS;
        pushAll();
    }

    void drawTick(uint32_t now) {
        if(_d.phase == 0) {
            if(connectedCount() >= 2) drawStart(now);
        } else if(_d.phase == 1) {
            if(!_p[_d.drawer].used || now > _d.deadline) drawReveal(now, 0);
        } else if(_d.phase == 2) {
            if(now > _d.revealUntil) drawStart(now);
        }
    }

    static bool wordMatch(const char* a, const char* b) {
        while(*a == ' ') a++;
        while(*b == ' ') b++;
        while(*a && *b) {
            char ca = *a, cb = *b;
            if(ca >= 'A' && ca <= 'Z') ca += 32;
            if(cb >= 'A' && cb <= 'Z') cb += 32;
            if(ca != cb) return false;
            a++;
            b++;
        }
        while(*a == ' ') a++;
        return *a == '\0' && *b == '\0';
    }

    void drawGuess(uint8_t pid, const char* text) {
        if(_active != HA_GAME_DRAW || _d.phase != 1 || pid == _d.drawer) return;
        if(wordMatch(text, _d.word)) {
            _p[pid].score += 200;
            haUartScore(pid, 200, "draw");
            if(_p[_d.drawer].used) {
                _p[_d.drawer].score += 100;
                haUartScore(_d.drawer, 100, "drawn");
            }
            haUartRoundResult(String("{\"draw\":\"") + ha_json_escape(_p[pid].nick) + " got it\"}");
            drawReveal(millis(), pid);
        } else {
            haWsBroadcast(
                String("{\"t\":\"chat\",\"nick\":\"") + ha_json_escape(_p[pid].nick) +
                "\",\"text\":\"" + ha_json_escape(text) + "\"}");
        }
    }

    static bool jsonNum(const char* s, const char* key, char* out, size_t n) {
        const char* q = ha_json_find(s, key);
        if(!q) return false;
        size_t i = 0;
        while(*q && (isdigit((unsigned char)*q) || *q == '.' || *q == '-' || *q == '+' ||
                     *q == 'e' || *q == 'E') &&
              i < n - 1)
            out[i++] = *q++;
        out[i] = '\0';
        return i > 0;
    }

    // Relay the drawer's stroke to every other client as an "ink" message.
    void drawStroke(uint8_t pid, const char* json) {
        if(_active != HA_GAME_DRAW || _d.phase != 1 || pid != _d.drawer) return;
        String ink = "{\"t\":\"ink\"";
        static const char* keys[4] = {"x0", "y0", "x1", "y1"};
        char num[16];
        for(int k = 0; k < 4; k++)
            if(jsonNum(json, keys[k], num, sizeof(num))) {
                ink += ",\"";
                ink += keys[k];
                ink += "\":";
                ink += num;
            }
        ink += "}";
        for(uint8_t p = 1; p <= HA_MAX_PLAYERS; p++)
            if(_p[p].used && _p[p].wsId && p != _d.drawer) haWsSendWs(_p[p].wsId, ink);
    }

    void drawClearInk(uint8_t pid) {
        if(_active != HA_GAME_DRAW || pid != _d.drawer) return;
        for(uint8_t p = 1; p <= HA_MAX_PLAYERS; p++)
            if(_p[p].used && _p[p].wsId && p != _d.drawer)
                haWsSendWs(_p[p].wsId, "{\"t\":\"ink\",\"clear\":true}");
    }

    String drawJson(uint8_t pid) {
        if(_d.phase == 3) { // final scoreboard
            return String("{\"t\":\"draw\",\"phase\":\"final\",\"board\":") + triviaBoard() + "}";
        }
        String s = "{\"t\":\"draw\",\"phase\":\"";
        s += _d.phase == 1 ? "draw" : _d.phase == 2 ? "reveal" : "idle";
        s += "\"";
        if(_d.phase != 0) {
            s += ",\"round\":";
            s += _d.round;
            s += ",\"rounds\":";
            s += _d.roundsTotal;
            if(_d.phase == 2) {
                s += ",\"word\":\"";
                s += ha_json_escape(_d.word);
                s += "\",\"winner\":";
                if(_d.winner)
                    s += _d.winner;
                else
                    s += "null";
            } else {
                // draw phase: everyone gets the round deadline for a countdown
                s += ",\"deadline\":";
                s += _d.deadline;
                s += ",\"dur\":";
                s += DRAW_SECS;
                if(pid == _d.drawer) {
                    s += ",\"role\":\"drawer\",\"word\":\"";
                    s += ha_json_escape(_d.word);
                    s += "\",\"drawer\":";
                    s += _d.drawer;
                } else {
                    s += ",\"role\":\"guesser\",\"len\":";
                    s += haUtf8Len(_d.word); // characters, not bytes: one blank per letter
                    s += ",\"drawer\":\"";
                    s += ha_json_escape(_p[_d.drawer].nick);
                    s += "\"";
                }
            }
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- pong ----------
    void pongClear() {
        for(int i = 0; i < PONG_MAX; i++) _pm[i] = PongMatch{};
    }

    PongMatch* pongMatchOf(uint8_t pid) {
        for(int i = 0; i < PONG_MAX; i++) {
            if(!_pm[i].used) continue;
            if(_pm[i].a == pid && _pm[i].aIn) return &_pm[i];
            if(_pm[i].b == pid && _pm[i].bIn) return &_pm[i];
        }
        return nullptr;
    }

    void pongServe(PongMatch* m, int dir) {
        m->bx = 0.5f;
        m->by = 0.5f;
        m->p1 = 0.5f;
        m->p2 = 0.5f;
        m->vx = dir > 0 ? 0.018f : -0.018f;
        m->vy = 0.010f;
        m->d1 = 0;
        m->d2 = 0;
    }

    void pongStart(PongMatch* m, uint8_t a, uint8_t b) {
        *m = PongMatch{};
        m->used = true;
        m->a = a;
        m->b = b;
        m->aIn = m->bIn = true;
        m->phase = 1;
        pongServe(m, 1);
    }

    void pongPaddle(uint8_t pid, int dir) {
        PongMatch* m = pongMatchOf(pid);
        if(!m || m->phase != 1) return;
        if(dir < -1) dir = -1;
        if(dir > 1) dir = 1;
        if(pid == m->a)
            m->d1 = (int8_t)dir;
        else
            m->d2 = (int8_t)dir;
    }

    void pongFinish(PongMatch* m, uint8_t winner) {
        if(m->phase != 1) return;
        m->phase = 2;
        m->winner = winner;
        uint8_t loser = (winner == m->a) ? m->b : m->a;
        _p[winner].score += 300;
        haUartScore(winner, 300, "pongwin");
        haUartRoundResult(String("{\"win\":") + winner + ",\"lose\":" + loser + "}");
    }

    void pongOnLeave(uint8_t pid) {
        PongMatch* m = pongMatchOf(pid);
        if(!m) return;
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        if(m->phase == 1) pongFinish(m, opp);
        if(pid == m->a) m->aIn = false;
        if(pid == m->b) m->bIn = false;
        if(!m->aIn && !m->bIn) *m = PongMatch{};
    }

    void pongTick() {
        const float PADHALF = 0.11f, PSPEED = 0.03f;
        for(int i = 0; i < PONG_MAX; i++) {
            PongMatch* m = &_pm[i];
            if(!m->used || m->phase != 1) continue;
            m->p1 += m->d1 * PSPEED;
            m->p2 += m->d2 * PSPEED;
            if(m->p1 < PADHALF) m->p1 = PADHALF;
            if(m->p1 > 1 - PADHALF) m->p1 = 1 - PADHALF;
            if(m->p2 < PADHALF) m->p2 = PADHALF;
            if(m->p2 > 1 - PADHALF) m->p2 = 1 - PADHALF;
            m->bx += m->vx;
            m->by += m->vy;
            if(m->by < 0) {
                m->by = 0;
                m->vy = -m->vy;
            }
            if(m->by > 1) {
                m->by = 1;
                m->vy = -m->vy;
            }
            if(m->bx <= PONG_HIT_X) {
                if(fabsf(m->by - m->p1) <= PADHALF) {
                    m->bx = PONG_HIT_X;
                    m->vx = -m->vx;
                    m->vy += (m->by - m->p1) * 0.05f;
                } else {
                    m->s2++;
                    if(m->s2 >= PONG_WIN)
                        pongFinish(m, m->b);
                    else
                        pongServe(m, 1);
                }
            }
            if(m->phase == 1 && m->bx >= 1.0f - PONG_HIT_X) {
                if(fabsf(m->by - m->p2) <= PADHALF) {
                    m->bx = 1.0f - PONG_HIT_X;
                    m->vx = -m->vx;
                    m->vy += (m->by - m->p2) * 0.05f;
                } else {
                    m->s1++;
                    if(m->s1 >= PONG_WIN)
                        pongFinish(m, m->a);
                    else
                        pongServe(m, -1);
                }
            }
            if(_p[m->a].wsId) haWsSendWs(_p[m->a].wsId, pongJson(m->a));
            if(_p[m->b].wsId) haWsSendWs(_p[m->b].wsId, pongJson(m->b));
        }
    }

    static String pongF(float v) {
        int iv = (int)(v * 1000.0f + 0.5f);
        if(iv < 0) iv = 0;
        if(iv > 1000) iv = 1000;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d.%03d", iv / 1000, iv % 1000);
        return String(buf);
    }

    String pongJson(uint8_t pid) {
        PongMatch* m = pongMatchOf(pid);
        if(!m)
            return String("{\"t\":\"pong\",\"phase\":\"lobby\",\"challenges\":") +
                   duelChallengesJson() + "}";
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        uint8_t me = (pid == m->a) ? 1 : 2;
        String s = "{\"t\":\"pong\",\"phase\":\"";
        s += (m->phase == 2) ? "over" : "playing";
        s += "\",\"you\":";
        s += pid;
        s += ",\"me\":";
        s += me;
        s += ",\"opp\":\"";
        s += ha_json_escape(_p[opp].nick);
        s += "\",\"ball\":{\"x\":" + pongF(m->bx) + ",\"y\":" + pongF(m->by) + "}";
        s += ",\"p1\":" + pongF(m->p1) + ",\"p2\":" + pongF(m->p2);
        s += ",\"s1\":";
        s += m->s1;
        s += ",\"s2\":";
        s += m->s2;
        if(m->phase == 2) {
            const char* r = (m->winner == pid) ? "win" : "lose";
            s += ",\"result\":\"";
            s += r;
            s += "\"";
        }
        s += "}";
        return s;
    }

    // ================= whole-group party games ==================
    // Shared lobby/ready/countdown helpers on a Party sub-state.
    void partyClear(Party& pt) {
        pt.phase = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) pt.ready[i] = false;
        pt.round = 0;
        pt.roundsTotal = 0;
        pt.countdownEnd = 0;
        pt.lastSec = -1;
        pt.deadline = 0;
        pt.revealUntil = 0;
    }

    bool partyAllReady(const Party& pt) {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(!pt.ready[i]) return false;
        }
        return n >= 1;
    }

    // Players list with per-player ready flags, for the party lobby screens.
    String partyPlayersJson(const Party& pt) {
        String s = "[";
        bool first = true;
        for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++) {
            if(!_p[pid].used) continue;
            if(!first) s += ",";
            first = false;
            s += "{\"pid\":";
            s += pid;
            s += ",\"nick\":\"";
            s += ha_json_escape(_p[pid].nick);
            s += "\",\"avatar\":\"";
            s += ha_json_escape(_p[pid].avatar);
            s += "\",\"ready\":";
            s += pt.ready[pid] ? "true" : "false";
            s += "}";
        }
        s += "]";
        return s;
    }

    // Broadcast pushAll once per second while a countdown ticks; returns true at zero.
    bool partyCountdownDone(Party& pt, uint32_t now) {
        if((int32_t)(pt.countdownEnd - now) <= 0) return true;
        int sec = (int)((pt.countdownEnd - now + 999) / 1000);
        if(sec != pt.lastSec) {
            pt.lastSec = sec;
            pushAll();
        }
        return false;
    }

    int partyCountdownSec(const Party& pt) {
        uint32_t now = millis();
        if((int32_t)(pt.countdownEnd - now) <= 0) return 0;
        return (int)((pt.countdownEnd - now + 999) / 1000);
    }

    void resetScoresAll() {
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used) _p[i].score = 0;
    }

    // A join/leave can complete a vote/round or cancel a pending start.
    void partyRosterChanged() {
        if(_active == HA_GAME_WYR)
            wyrCheckStart();
        else if(_active == HA_GAME_SCRAMBLE)
            scrambleCheckStart();
        else if(_active == HA_GAME_REACT)
            reactCheckStart();
        else if(_active == HA_GAME_GUESSCOLOR)
            gcCheckStart();
        else if(_active == HA_GAME_SPECTRUM)
            spectrumCheckStart();
        else if(_active == HA_GAME_KMK)
            kmkCheckStart();
        else if(_active == HA_GAME_SECRETS)
            secretsCheckStart();
    }

    // ---------- would you rather (live A/B poll) ----------
    // Which pack wins the pre-round vote, mirroring triviaWinningTopic(): most
    // votes wins, ties broken at random, and an untallied vote (total == 0)
    // picks uniformly at random among all packs. Guard packCount == 0 so an
    // empty game (no packs streamed yet) never indexes out of range.
    int wyrWinningPack() {
        if(_wyr.packCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _wyr.vote[i] >= 0 && _wyr.vote[i] < _wyr.packCount) {
                votes[_wyr.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_wyr.packCount);
        int best = 0;
        for(int i = 1; i < _wyr.packCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _wyr.packCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void wyrClear() {
        partyClear(_wyr.pt);
        _wyr.prompt = 0;
        _wyr.pack = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _wyr.vote[i] = -1;
            _wyr.choice[i] = -1;
        }
    }

    void wyrReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_WYR) return;
        if(_wyr.pt.phase != 0 && _wyr.pt.phase != 4) return;
        if(_wyr.pt.phase == 4 && val) wyrClear(); // ready from the final screen -> new game
        _wyr.pt.ready[pid] = val;
        wyrCheckStart();
        pushAll();
    }

    void wyrVote(uint8_t pid, int pack) {
        if(_active != HA_GAME_WYR || _wyr.pt.phase != 0) return;
        if(pack < 0 || pack >= _wyr.packCount) return;
        _wyr.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void wyrCheckStart() {
        Party& pt = _wyr.pt;
        if(pt.phase == 0 && _wyr.packCount > 0 && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !partyAllReady(pt)) {
            pt.phase = 0;
        }
    }

    bool wyrAllVoted() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(_wyr.choice[i] < 0) return false;
        }
        return n >= 1;
    }

    void wyrNextPrompt(uint32_t now) {
        Party& pt = _wyr.pt;
        if(pt.round >= WYR_ROUNDS) {
            pt.phase = 4; // final
            pushAll();
            return;
        }
        WyrPack& pk = _wyr.packs[_wyr.pack];
        if(pk.count == 0) { // empty pack: nothing to play, end the game
            pt.phase = 4;
            pushAll();
            return;
        }
        pt.round++;
        _wyr.prompt = _wyr.promptSeq % pk.count;
        _wyr.promptSeq++;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _wyr.choice[i] = -1;
        pt.phase = 2;
        pt.deadline = now + (uint32_t)WYR_VOTE_SECS * 1000;
        pushAll();
    }

    void wyrAnswer(uint8_t pid, int c) {
        if(_active != HA_GAME_WYR || _wyr.pt.phase != 2) return;
        if(c != 0 && c != 1) return;
        _wyr.choice[pid] = (int8_t)c;
        if(wyrAllVoted()) wyrReveal(millis());
        else pushAll();
    }

    void wyrReveal(uint32_t now) {
        _wyr.pt.phase = 3;
        _wyr.pt.revealUntil = now + WYR_REVEAL_MS;
        pushAll();
    }

    void wyrAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_WYR || _wyr.pt.phase != 4) return;
        wyrClear();
        pushAll();
    }

    void wyrTick(uint32_t now) {
        Party& pt = _wyr.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                // Lock in the winning pack now (votes are frozen during the
                // countdown), mirroring trivia's topic lock.
                _wyr.pack = (uint8_t)wyrWinningPack();
                wyrNextPrompt(now);
            }
        } else if(pt.phase == 2) {
            if(now > pt.deadline || wyrAllVoted()) wyrReveal(now);
        } else if(pt.phase == 3) {
            if(now > pt.revealUntil) wyrNextPrompt(now);
        }
    }

    String wyrJson(uint8_t pid) {
        Party& pt = _wyr.pt;
        if(pt.phase == 0) {
            String s = String("{\"t\":\"wyr\",\"phase\":\"lobby\",\"you\":") + pid +
                       ",\"players\":" + partyPlayersJson(pt);
            s += ",\"packs\":[";
            int votes[TRIVIA_MAX_TOPICS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _wyr.vote[i] >= 0 && _wyr.vote[i] < _wyr.packCount) votes[_wyr.vote[i]]++;
            for(int i = 0; i < _wyr.packCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_wyr.packs[i].name.c_str()) + "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_wyr.vote[pid]);
            s += "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"wyr\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"wyr\",\"phase\":\"final\",\"you\":") + pid + "}";
        WyrPack& pk = _wyr.packs[_wyr.pack];
        const char* a = pk.items[_wyr.prompt].a.c_str();
        const char* b = pk.items[_wyr.prompt].b.c_str();
        int cA = 0, cB = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(_wyr.choice[i] == 0) cA++;
            else if(_wyr.choice[i] == 1) cB++;
        }
        String s = String("{\"t\":\"wyr\",\"phase\":\"") + (pt.phase == 3 ? "reveal" : "vote") +
                   "\",\"round\":" + pt.round + ",\"rounds\":" + WYR_ROUNDS + ",\"a\":\"" +
                   ha_json_escape(a) + "\",\"b\":\"" + ha_json_escape(b) + "\",\"myvote\":" +
                   _wyr.choice[pid] + ",\"counts\":[" + cA + "," + cB + "]";
        if(pt.phase == 2) { // asking: count down the vote window
            s += ",\"deadline\":";
            s += pt.deadline;
            s += ",\"dur\":";
            s += WYR_VOTE_SECS;
        } else if(pt.phase == 3) { // results: count down to the next prompt
            s += ",\"deadline\":";
            s += pt.revealUntil;
            s += ",\"dur\":";
            s += (WYR_REVEAL_MS / 1000);
        }
        s += "}";
        return s;
    }

    // ---------- word scramble race ----------
    // Which pack wins the pre-round vote, mirroring wyrWinningPack(): most votes
    // wins, ties broken at random, an untallied vote (total == 0) picks uniformly
    // at random among all packs. Guard packCount == 0 so an empty game never
    // indexes out of range.
    int scrambleWinningPack() {
        if(_scr.packCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _scr.vote[i] >= 0 && _scr.vote[i] < _scr.packCount) {
                votes[_scr.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_scr.packCount);
        int best = 0;
        for(int i = 1; i < _scr.packCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _scr.packCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    // Shuffle src into dst (NUL-terminated); retry a few times so it differs from src.
    // Shuffles by UTF-8 character, not byte: a multi-byte letter (an accented Latin
    // char, or any non-Latin script) moves as one glyph instead of splitting into
    // continuation bytes that render as garbage.
    void scrambleMake(char* dst, const char* src) {
        // Slice src into glyphs: offset + byte-length of each UTF-8 character.
        const char* off[24];
        uint8_t glen[24];
        int n = 0;
        for(const char* p = src; *p && n < 23;) {
            unsigned char c = (unsigned char)*p;
            int l = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            off[n] = p;
            glen[n] = (uint8_t)l;
            n++;
            p += l;
        }
        for(int attempt = 0; attempt < 8; attempt++) {
            int idx[24];
            for(int i = 0; i < n; i++) idx[i] = i;
            for(int i = n - 1; i > 0; i--) {
                int j = (int)(esp_random() % (uint32_t)(i + 1));
                int t = idx[i];
                idx[i] = idx[j];
                idx[j] = t;
            }
            char* o = dst;
            for(int i = 0; i < n; i++) {
                memcpy(o, off[idx[i]], glen[idx[i]]);
                o += glen[idx[i]];
            }
            *o = '\0';
            if(n < 2 || strcmp(dst, src) != 0) return;
        }
    }

    void scrambleClear() {
        partyClear(_scr.pt);
        _scr.word[0] = '\0';
        _scr.scram[0] = '\0';
        _scr.solvedCount = 0;
        _scr.pack = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _scr.solved[i] = false;
            _scr.vote[i] = -1;
        }
    }

    void scrambleReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_SCRAMBLE) return;
        if(_scr.pt.phase != 0 && _scr.pt.phase != 4) return;
        if(_scr.pt.phase == 4 && val) scrambleClear();
        _scr.pt.ready[pid] = val;
        scrambleCheckStart();
        pushAll();
    }

    void scrambleVote(uint8_t pid, int pack) {
        if(_active != HA_GAME_SCRAMBLE || _scr.pt.phase != 0) return;
        if(pack < 0 || pack >= _scr.packCount) return;
        _scr.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void scrambleCheckStart() {
        if(_scr.packCount == 0) return;
        Party& pt = _scr.pt;
        if(pt.phase == 0 && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !partyAllReady(pt)) {
            pt.phase = 0;
        }
    }

    bool scrambleAllSolved() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(!_scr.solved[i]) return false;
        }
        return n >= 1;
    }

    void scrambleNextWord(uint32_t now) {
        Party& pt = _scr.pt;
        if(pt.round >= SCR_ROUNDS) {
            pt.phase = 4;
            haUartRoundResult("{\"scramble\":\"final\"}");
            pushAll();
            return;
        }
        WordPack& p = _scr.packs[_scr.pack];
        if(p.count == 0) { // empty pack: nothing to play, end the game
            pt.phase = 4;
            haUartRoundResult("{\"scramble\":\"final\"}");
            pushAll();
            return;
        }
        pt.round++;
        strlcpy(_scr.word, p.words[_scr.wordSeq % p.count].c_str(), sizeof(_scr.word));
        _scr.wordSeq++;
        scrambleMake(_scr.scram, _scr.word);
        _scr.solvedCount = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _scr.solved[i] = false;
        pt.phase = 2;
        pt.deadline = now + (uint32_t)SCR_SECS * 1000;
        pushAll();
    }

    void scrambleGuess(uint8_t pid, const char* text) {
        if(_active != HA_GAME_SCRAMBLE || _scr.pt.phase != 2) return;
        if(_scr.solved[pid]) return;
        if(!wordMatch(text, _scr.word)) return;
        _scr.solved[pid] = true;
        int pts = (_scr.solvedCount == 0) ? 200 :
                  (_scr.solvedCount == 1) ? 120 :
                  (_scr.solvedCount == 2) ? 80 :
                                            40;
        _scr.solvedCount++;
        _p[pid].score += pts;
        haUartScore(pid, pts, "scramble");
        haWsBroadcast(
            String("{\"t\":\"chat\",\"nick\":\"") + ha_json_escape(_p[pid].nick) +
            "\",\"text\":\"solved it!\"}");
        if(scrambleAllSolved()) scrambleReveal(millis());
        else pushAll();
    }

    void scrambleReveal(uint32_t now) {
        _scr.pt.phase = 3;
        _scr.pt.revealUntil = now + SCR_REVEAL_MS;
        pushAll();
    }

    void scrambleAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_SCRAMBLE || _scr.pt.phase != 4) return;
        scrambleClear();
        pushAll();
    }

    void scrambleTick(uint32_t now) {
        Party& pt = _scr.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                resetScoresAll();
                // Lock in the winning pack now (votes are frozen during the
                // countdown), mirroring WYR's pack lock.
                _scr.pack = (uint8_t)scrambleWinningPack();
                scrambleNextWord(now);
            }
        } else if(pt.phase == 2) {
            if(now > pt.deadline || scrambleAllSolved()) scrambleReveal(now);
        } else if(pt.phase == 3) {
            if(now > pt.revealUntil) scrambleNextWord(now);
        }
    }

    String scrambleJson(uint8_t pid) {
        Party& pt = _scr.pt;
        if(pt.phase == 0) {
            String s = String("{\"t\":\"scramble\",\"phase\":\"lobby\",\"you\":") + pid +
                       ",\"players\":" + partyPlayersJson(pt);
            s += ",\"packs\":[";
            int votes[TRIVIA_MAX_TOPICS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _scr.vote[i] >= 0 && _scr.vote[i] < _scr.packCount) votes[_scr.vote[i]]++;
            for(int i = 0; i < _scr.packCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_scr.packs[i].name.c_str()) + "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_scr.vote[pid]);
            s += "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"scramble\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"scramble\",\"phase\":\"final\",\"board\":") + triviaBoard() +
                   "}";
        String s = String("{\"t\":\"scramble\",\"phase\":\"") + (pt.phase == 3 ? "reveal" : "play") +
                   "\",\"round\":" + pt.round + ",\"rounds\":" + SCR_ROUNDS;
        if(pt.phase == 3) {
            s += ",\"word\":\"";
            s += ha_json_escape(_scr.word);
            s += "\"";
        } else {
            s += ",\"scram\":\"";
            s += ha_json_escape(_scr.scram);
            s += "\",\"len\":";
            s += (int)strlen(_scr.word);
            s += ",\"solved\":";
            s += _scr.solved[pid] ? "true" : "false";
            s += ",\"deadline\":";
            s += pt.deadline;
            s += ",\"dur\":";
            s += SCR_SECS;
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- reaction duel (fastest finger) ----------
    void reactClear() {
        partyClear(_react.pt);
        _react.goAt = 0;
        _react.goOn = false;
        _react.winner = 0;
        _react.winMs = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _react.tapped[i] = false;
            _react.dq[i] = false;
        }
    }

    void reactReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_REACT) return;
        if(_react.pt.phase != 0 && _react.pt.phase != 4) return;
        if(_react.pt.phase == 4 && val) reactClear();
        _react.pt.ready[pid] = val;
        reactCheckStart();
        pushAll();
    }

    void reactCheckStart() {
        Party& pt = _react.pt;
        if(pt.phase == 0 && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !partyAllReady(pt)) {
            pt.phase = 0;
        }
    }

    // Everyone has either tapped (green) or false-started (dq) -> round is settled.
    bool reactAllResolved() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(!_react.tapped[i] && !_react.dq[i]) return false;
        }
        return n >= 1;
    }

    void reactArm(uint32_t now) {
        Party& pt = _react.pt;
        if(pt.round >= REACT_ROUNDS) {
            pt.phase = 4;
            haUartRoundResult("{\"react\":\"final\"}");
            pushAll();
            return;
        }
        pt.round++;
        _react.goOn = false;
        _react.winner = 0;
        _react.winMs = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _react.tapped[i] = false;
            _react.dq[i] = false;
        }
        _react.goAt = now + 2000 + (esp_random() % 3000); // 2-5 s of red
        pt.phase = 2; // armed
        pushAll();
    }

    void reactTap(uint8_t pid) {
        if(_active != HA_GAME_REACT || _react.pt.phase != 2) return;
        if(_react.tapped[pid] || _react.dq[pid]) return;
        uint32_t now = millis();
        if(now < _react.goAt) { // tapped while red -> false start
            _react.dq[pid] = true;
            if(reactAllResolved()) reactReveal(now);
            else pushAll();
            return;
        }
        _react.tapped[pid] = true;
        if(_react.winner == 0) {
            _react.winner = pid;
            _react.winMs = now - _react.goAt;
            _p[pid].score += 200;
            haUartScore(pid, 200, "react");
            reactReveal(now); // first valid tap ends the round
        } else {
            pushAll();
        }
    }

    void reactReveal(uint32_t now) {
        _react.pt.phase = 3;
        _react.pt.revealUntil = now + REACT_REVEAL_MS;
        pushAll();
    }

    void reactAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_REACT || _react.pt.phase != 4) return;
        reactClear();
        pushAll();
    }

    void reactTick(uint32_t now) {
        Party& pt = _react.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                resetScoresAll();
                reactArm(now);
            }
        } else if(pt.phase == 2) {
            if(!_react.goOn && now >= _react.goAt) {
                _react.goOn = true; // red -> green: push so clients light up
                pushAll();
            }
            // nobody tapped for a while after green -> reveal with no winner
            if(_react.goOn && _react.winner == 0 && now > _react.goAt + 6000) reactReveal(now);
        } else if(pt.phase == 3) {
            if(now > pt.revealUntil) reactArm(now);
        }
    }

    String reactJson(uint8_t pid) {
        Party& pt = _react.pt;
        if(pt.phase == 0)
            return String("{\"t\":\"react\",\"phase\":\"lobby\",\"you\":") + pid +
                   ",\"players\":" + partyPlayersJson(pt) + "}";
        if(pt.phase == 1)
            return String("{\"t\":\"react\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"react\",\"phase\":\"final\",\"board\":") + triviaBoard() + "}";
        if(pt.phase == 2) {
            String s = String("{\"t\":\"react\",\"phase\":\"armed\",\"round\":") + pt.round +
                       ",\"rounds\":" + REACT_ROUNDS + ",\"light\":\"" +
                       (_react.goOn ? "go" : "wait") + "\",\"dq\":" +
                       (_react.dq[pid] ? "true" : "false") + ",\"tapped\":" +
                       (_react.tapped[pid] ? "true" : "false") + ",\"scores\":" + playersJson() +
                       "}";
            return s;
        }
        // reveal
        String s = String("{\"t\":\"react\",\"phase\":\"reveal\",\"round\":") + pt.round +
                   ",\"rounds\":" + REACT_ROUNDS;
        if(_react.winner) {
            s += ",\"winner\":\"";
            s += ha_json_escape(_p[_react.winner].nick);
            s += "\",\"ms\":";
            s += _react.winMs;
            s += ",\"iwon\":";
            s += (_react.winner == pid) ? "true" : "false";
        } else {
            s += ",\"winner\":null";
        }
        s += ",\"dq\":";
        s += _react.dq[pid] ? "true" : "false";
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- guess the color (closest RGB + speed) ----------
    void gcClear() {
        partyClear(_gc.pt);
        _gc.tr = _gc.tg = _gc.tb = 0;
        _gc.roundStart = 0;
        _gc.winner = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _gc.guessed[i] = false;
            _gc.gained[i] = 0;
            _gc.submitMs[i] = 0;
            _gc.gr[i] = _gc.gg[i] = _gc.gb[i] = 0;
        }
    }

    void gcReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_GUESSCOLOR) return;
        if(_gc.pt.phase != 0 && _gc.pt.phase != 4) return;
        if(_gc.pt.phase == 4 && val) gcClear();
        _gc.pt.ready[pid] = val;
        gcCheckStart();
        pushAll();
    }

    void gcCheckStart() {
        Party& pt = _gc.pt;
        if(pt.phase == 0 && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !partyAllReady(pt)) {
            pt.phase = 0;
        }
    }

    // Everyone present has submitted -> round is settled.
    bool gcAllGuessed() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(!_gc.guessed[i]) return false;
        }
        return n >= 1;
    }

    void gcStartRound(uint32_t now) {
        Party& pt = _gc.pt;
        if(pt.round >= GC_ROUNDS) {
            pt.phase = 4;
            haUartRoundResult("{\"gc\":\"final\"}");
            pushAll();
            return;
        }
        pt.round++;
        _gc.tr = esp_random() % 256;
        _gc.tg = esp_random() % 256;
        _gc.tb = esp_random() % 256;
        _gc.winner = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _gc.guessed[i] = false;
            _gc.gained[i] = 0;
            _gc.submitMs[i] = 0;
        }
        _gc.roundStart = now;
        pt.deadline = now + (uint32_t)GC_PLAY_SECS * 1000;
        pt.phase = 2;
        pushAll();
    }

    void gcGuess(uint8_t pid, int r, int g, int b) {
        if(_active != HA_GAME_GUESSCOLOR || _gc.pt.phase != 2) return;
        if(_gc.guessed[pid]) return;
        if(r < 0) r = 0;
        if(r > 255) r = 255;
        if(g < 0) g = 0;
        if(g > 255) g = 255;
        if(b < 0) b = 0;
        if(b > 255) b = 255;
        _gc.gr[pid] = (uint8_t)r;
        _gc.gg[pid] = (uint8_t)g;
        _gc.gb[pid] = (uint8_t)b;
        _gc.guessed[pid] = true;
        uint32_t now = millis();
        _gc.submitMs[pid] = (now >= _gc.roundStart) ? (now - _gc.roundStart) : 0;
        if(gcAllGuessed()) gcReveal(now);
        else pushAll();
    }

    void gcReveal(uint32_t now) {
        _gc.winner = 0;
        int bestPts = -1;
        uint32_t bestMs = 0xFFFFFFFF;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(!_gc.guessed[i]) {
                _gc.gained[i] = 0;
                continue;
            }
            int dr = (int)_gc.gr[i] - _gc.tr, dg = (int)_gc.gg[i] - _gc.tg,
                db = (int)_gc.gb[i] - _gc.tb;
            float dist = sqrtf((float)(dr * dr + dg * dg + db * db)); // 0..441.67
            int closeness = (int)(200.0f * (1.0f - dist / 441.673f) + 0.5f);
            if(closeness < 0) closeness = 0;
            float sf = 1.0f - (float)_gc.submitMs[i] / (float)GC_SPEED_MS;
            if(sf < 0) sf = 0;
            int speed = (int)(100.0f * sf + 0.5f);
            int pts = (int)((closeness + speed) / 30.0f + 0.5f); // rescale 0..300 -> 0..10
            if(pts > 10) pts = 10;
            _gc.gained[i] = pts;
            _p[i].score += pts;
            haUartScore(i, pts, "gc");
            if(pts > bestPts || (pts == bestPts && _gc.submitMs[i] < bestMs)) {
                bestPts = pts;
                bestMs = _gc.submitMs[i];
                _gc.winner = i;
            }
        }
        _gc.pt.phase = 3;
        _gc.pt.revealUntil = now + GC_REVEAL_MS;
        pushAll();
    }

    void gcAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_GUESSCOLOR || _gc.pt.phase != 4) return;
        gcClear();
        pushAll();
    }

    void gcTick(uint32_t now) {
        Party& pt = _gc.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                resetScoresAll();
                gcStartRound(now);
            }
        } else if(pt.phase == 2) {
            if(now > pt.deadline) gcReveal(now);
        } else if(pt.phase == 3) {
            if(now > pt.revealUntil) gcStartRound(now);
        }
    }

    String gcJson(uint8_t pid) {
        Party& pt = _gc.pt;
        if(pt.phase == 0)
            return String("{\"t\":\"gc\",\"phase\":\"lobby\",\"you\":") + pid +
                   ",\"players\":" + partyPlayersJson(pt) + "}";
        if(pt.phase == 1)
            return String("{\"t\":\"gc\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"gc\",\"phase\":\"final\",\"board\":") + triviaBoard() + "}";
        char color[8];
        snprintf(color, sizeof(color), "#%02X%02X%02X", _gc.tr, _gc.tg, _gc.tb);
        if(pt.phase == 2)
            return String("{\"t\":\"gc\",\"phase\":\"play\",\"round\":") + pt.round +
                   ",\"rounds\":" + GC_ROUNDS + ",\"color\":\"" + color + "\",\"submitted\":" +
                   (_gc.guessed[pid] ? "true" : "false") + ",\"scores\":" + playersJson() + "}";
        // reveal
        String s = String("{\"t\":\"gc\",\"phase\":\"reveal\",\"round\":") + pt.round +
                   ",\"rounds\":" + GC_ROUNDS + ",\"r\":" + _gc.tr + ",\"g\":" + _gc.tg +
                   ",\"b\":" + _gc.tb + ",\"color\":\"" + color + "\"";
        // Every player's guess, so the reveal can compare them side by side.
        s += ",\"guesses\":[";
        bool gfirst = true;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_gc.guessed[i]) continue;
            int dr = (int)_gc.gr[i] - _gc.tr, dg = (int)_gc.gg[i] - _gc.tg,
                db = (int)_gc.gb[i] - _gc.tb;
            int dist = (int)(sqrtf((float)(dr * dr + dg * dg + db * db)) + 0.5f);
            char gcol[8];
            snprintf(gcol, sizeof(gcol), "#%02X%02X%02X", _gc.gr[i], _gc.gg[i], _gc.gb[i]);
            if(!gfirst) s += ",";
            s += "{\"pid\":" + String(i) + ",\"nick\":\"" + ha_json_escape(_p[i].nick) +
                 "\",\"color\":\"" + gcol + "\",\"dist\":" + dist + ",\"points\":" + _gc.gained[i] + "}";
            gfirst = false;
        }
        s += "]";
        if(_gc.winner) {
            s += ",\"winner\":\"";
            s += ha_json_escape(_p[_gc.winner].nick);
            s += "\",\"winnerPid\":";
            s += _gc.winner;
            s += ",\"iwon\":";
            s += (_gc.winner == pid) ? "true" : "false";
        } else {
            s += ",\"winner\":null";
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- battleship (1v1, hidden fleets) ----------
    void battleClear() {
        for(int i = 0; i < BATTLE_MAX; i++) _bm[i] = BattleMatch{};
    }

    BattleMatch* battleMatchOf(uint8_t pid) {
        for(int i = 0; i < BATTLE_MAX; i++) {
            if(!_bm[i].used) continue;
            if(_bm[i].a == pid && _bm[i].aIn) return &_bm[i];
            if(_bm[i].b == pid && _bm[i].bIn) return &_bm[i];
        }
        return nullptr;
    }

    void battleStart(BattleMatch* m, uint8_t a, uint8_t b, uint8_t first) {
        *m = BattleMatch{};
        m->used = true;
        m->a = a;
        m->b = b;
        m->aIn = m->bIn = true;
        m->phase = 0; // placement
        m->first = first;
        m->turn = first;
        m->winner = 0;
    }

    void battleFinish(BattleMatch* m, uint8_t winner) {
        m->phase = 2;
        m->winner = winner;
    }

    // Parse one base-10 int from `p`, advancing past it. Own parser (no strtol, which
    // the off-target Arduino shim doesn't provide).
    static bool bsReadInt(const char*& p, int& out) {
        while(*p == ' ') p++;
        bool neg = (*p == '-');
        if(neg) p++;
        if(*p < '0' || *p > '9') return false;
        int v = 0;
        while(*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
        out = neg ? -v : v;
        return true;
    }

    // ships is "r,c,d;r,c,d;..." in fixed ship order; d=0 horizontal, d=1 vertical.
    void battlePlace(uint8_t pid, const char* json) {
        BattleMatch* m = battleMatchOf(pid);
        if(!m || m->phase != 0) return;
        char buf[96];
        if(!ha_json_str(json, "ships", buf, sizeof(buf))) return;
        uint8_t fleet[BS_N];
        memset(fleet, 0, sizeof(fleet));
        const char* p = buf;
        for(uint8_t s = 0; s < BS_SHIPS; s++) {
            int r, c, d;
            if(!bsReadInt(p, r)) return;
            if(*p == ',') p++;
            if(!bsReadInt(p, c)) return;
            if(*p == ',') p++;
            if(!bsReadInt(p, d)) return;
            if(*p == ';') p++;
            for(uint8_t k = 0; k < BS_LEN[s]; k++) {
                int rr = r + (d ? (int)k : 0), cc = c + (d ? 0 : (int)k);
                if(rr < 0 || rr >= BS_SIZE || cc < 0 || cc >= BS_SIZE) return; // out of bounds
                int idx = rr * BS_SIZE + cc;
                if(fleet[idx]) return; // overlap
                fleet[idx] = s + 1; // ship id 1..BS_SHIPS
            }
        }
        uint8_t* myFleet = (pid == m->a) ? m->fleetA : m->fleetB;
        memcpy(myFleet, fleet, sizeof(fleet));
        if(pid == m->a)
            m->readyA = true;
        else
            m->readyB = true;
        if(m->readyA && m->readyB) {
            m->phase = 1; // both placed -> firing
            m->turn = m->first;
        }
        pushAll();
    }

    bool battleShipSunk(const uint8_t* fleet, const uint8_t* shot, uint8_t shipId) {
        for(int i = 0; i < BS_N; i++)
            if(fleet[i] == shipId && shot[i] != 2) return false;
        return true;
    }

    int battleShipsLeft(const uint8_t* fleet, const uint8_t* shot) {
        int left = 0;
        for(uint8_t s = 1; s <= BS_SHIPS; s++)
            if(!battleShipSunk(fleet, shot, s)) left++;
        return left;
    }

    void battleFire(uint8_t pid, int n) {
        BattleMatch* m = battleMatchOf(pid);
        if(!m || m->phase != 1 || m->turn != pid) return;
        if(n < 0 || n >= BS_N) return;
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        uint8_t* oppFleet = (pid == m->a) ? m->fleetB : m->fleetA;
        uint8_t* shotOnOpp = (pid == m->a) ? m->shotOnB : m->shotOnA;
        if(shotOnOpp[n]) return; // already fired here
        bool hit = oppFleet[n] != 0;
        shotOnOpp[n] = hit ? 2 : 1;
        if(!hit) {
            m->turn = opp; // miss passes the turn
            pushAll();
            return;
        }
        if(pid == m->a)
            m->hitsA++;
        else
            m->hitsB++;
        uint8_t hits = (pid == m->a) ? m->hitsA : m->hitsB;
        if(battleShipSunk(oppFleet, shotOnOpp, oppFleet[n])) {
            const char* name = BS_NAMES[oppFleet[n] - 1];
            if(_p[pid].wsId)
                haWsSendWs(
                    _p[pid].wsId,
                    String("{\"t\":\"toast\",\"msg\":\"You sank their ") + name + "!\"}");
            if(_p[opp].wsId)
                haWsSendWs(
                    _p[opp].wsId,
                    String("{\"t\":\"toast\",\"msg\":\"Your ") + name + " was sunk!\"}");
        }
        if(hits >= BS_TOTAL) battleFinish(m, pid); // all enemy ships down
        // else: a hit keeps the turn (shoot again)
        pushAll();
    }

    void battleRematch(uint8_t pid) {
        BattleMatch* m = battleMatchOf(pid);
        if(!m || m->phase != 2) return;
        if(!m->aIn || !m->bIn) {
            if(_p[pid].wsId)
                haWsSendWs(_p[pid].wsId, String("{\"t\":\"toast\",\"msg\":\"Opponent left\"}"));
            battleOnLeave(pid);
            pushAll();
            return;
        }
        uint8_t next = (m->first == m->a) ? m->b : m->a; // alternate who fires first
        battleStart(m, m->a, m->b, next);
        pushAll();
    }

    void battleOnLeave(uint8_t pid) {
        BattleMatch* m = battleMatchOf(pid);
        if(!m) return;
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        if(m->phase == 0 || m->phase == 1) battleFinish(m, opp); // forfeit
        if(pid == m->a) m->aIn = false;
        if(pid == m->b) m->bIn = false;
        if(!m->aIn && !m->bIn) *m = BattleMatch{}; // both gone: free the slot
    }

    String battleCells(const uint8_t* v) {
        String s = "[";
        for(int i = 0; i < BS_N; i++) {
            if(i) s += ",";
            s += v[i];
        }
        s += "]";
        return s;
    }

    String battleJson(uint8_t pid) {
        BattleMatch* m = battleMatchOf(pid);
        if(!m)
            return String("{\"t\":\"bs\",\"phase\":\"lobby\",\"challenges\":") +
                   duelChallengesJson() + "}";
        uint8_t me = (pid == m->a) ? 1 : 2;
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        if(m->phase == 0) {
            bool ready = (pid == m->a) ? m->readyA : m->readyB;
            bool oppReady = (pid == m->a) ? m->readyB : m->readyA;
            return String("{\"t\":\"bs\",\"phase\":\"place\",\"you\":") + pid + ",\"me\":" + me +
                   ",\"opp\":\"" + ha_json_escape(_p[opp].nick) + "\",\"ready\":" +
                   (ready ? "true" : "false") + ",\"oppReady\":" +
                   (oppReady ? "true" : "false") + "}";
        }
        // firing / over: build the two grids from this player's perspective
        uint8_t* fleetSelf = (pid == m->a) ? m->fleetA : m->fleetB;
        uint8_t* shotOnSelf = (pid == m->a) ? m->shotOnA : m->shotOnB; // opponent's shots on me
        uint8_t* oppFleet = (pid == m->a) ? m->fleetB : m->fleetA;
        uint8_t* shotOnOpp = (pid == m->a) ? m->shotOnB : m->shotOnA; // my shots on them
        uint8_t mine[BS_N], track[BS_N];
        for(int i = 0; i < BS_N; i++) {
            uint8_t sh = shotOnSelf[i]; // 0 none, 1 miss, 2 hit
            mine[i] = (sh == 2) ? 3 : (sh == 1) ? 2 : (fleetSelf[i] ? 1 : 0);
            uint8_t st = shotOnOpp[i];
            // hidden info: only read oppFleet where I've already hit (st == 2)
            track[i] = (st == 2) ? (battleShipSunk(oppFleet, shotOnOpp, oppFleet[i]) ? 3 : 2) : st;
        }
        int myShips = battleShipsLeft(fleetSelf, shotOnSelf);
        int oppShips = battleShipsLeft(oppFleet, shotOnOpp);
        String s = "{\"t\":\"bs\",\"phase\":\"";
        s += (m->phase == 2) ? "over" : "fire";
        s += "\",\"you\":";
        s += pid;
        s += ",\"me\":";
        s += me;
        s += ",\"opp\":\"" + ha_json_escape(_p[opp].nick) + "\"";
        s += ",\"turn\":";
        s += m->turn;
        s += ",\"yourTurn\":";
        s += (m->turn == pid) ? "true" : "false";
        s += ",\"myShips\":";
        s += myShips;
        s += ",\"oppShips\":";
        s += oppShips;
        s += ",\"mine\":" + battleCells(mine);
        s += ",\"track\":" + battleCells(track);
        if(m->phase == 2) {
            s += ",\"result\":\"";
            s += (m->winner == pid) ? "win" : "lose";
            s += "\",\"oppFleet\":" + battleCells(oppFleet); // reveal at game end
        }
        s += "}";
        return s;
    }

    // ---------- chess (rules core) ----------
    // Pure rules, no match state: everything below takes a ChessCore and is safe to
    // call on a scratch copy. Move generation is plain mailbox scanning: a few thousand
    // ops per move at human speed, which is nothing next to the WS traffic.

    static void chPush(uint16_t* out, int& n, int from, int to) {
        if(n < CH_MAX_MOVES) out[n++] = (uint16_t)(from * 64 + to);
    }

    // Is `sq` attacked by any piece of `bySide` (0 white, 1 black)?
    static bool chessAttacked(const ChessCore& c, int sq, uint8_t bySide) {
        int f = sq & 7, r = sq >> 3;
        uint8_t base = bySide ? 6 : 0; // white pieces are 1..6, black 7..12
        for(int i = 0; i < 8; i++) { // knights
            int ff = f + CH_NDF[i], rr = r + CH_NDR[i];
            if(ff < 0 || ff > 7 || rr < 0 || rr > 7) continue;
            if(c.sq[rr * 8 + ff] == base + 2) return true;
        }
        for(int i = 0; i < 8; i++) { // king
            int ff = f + CH_KDF[i], rr = r + CH_KDR[i];
            if(ff < 0 || ff > 7 || rr < 0 || rr > 7) continue;
            if(c.sq[rr * 8 + ff] == base + 6) return true;
        }
        // Pawns attack forwards, so an attacker stands one rank *behind* `sq`.
        int pr = bySide ? r + 1 : r - 1;
        if(pr >= 0 && pr <= 7) {
            if(f > 0 && c.sq[pr * 8 + f - 1] == base + 1) return true;
            if(f < 7 && c.sq[pr * 8 + f + 1] == base + 1) return true;
        }
        for(int d = 0; d < 8; d++) { // sliders: bishop/queen diagonally, rook/queen straight
            int ff = f + CH_SDF[d], rr = r + CH_SDR[d];
            while(ff >= 0 && ff <= 7 && rr >= 0 && rr <= 7) {
                uint8_t pc = c.sq[rr * 8 + ff];
                if(pc) {
                    uint8_t k = chKind(pc);
                    if(chSide(pc) == bySide && (k == 5 || k == (d < 4 ? 3 : 4))) return true;
                    break; // first piece on the ray blocks it
                }
                ff += CH_SDF[d];
                rr += CH_SDR[d];
            }
        }
        return false;
    }

    // Where is `side`'s king? -1 if it has none (only loaded test positions can).
    static int chessKingSq(const ChessCore& c, uint8_t side) {
        uint8_t king = side ? 12 : 6;
        for(int i = 0; i < 64; i++)
            if(c.sq[i] == king) return i;
        return -1;
    }

    static bool chessInCheck(const ChessCore& c) {
        int ks = chessKingSq(c, c.stm);
        return ks >= 0 && chessAttacked(c, ks, c.stm ^ 1);
    }

    // Every pseudo-legal move for c.stm (own king may end up attacked; chessGenLegal
    // filters that). Promotions emit ONE entry per from/to, because the promotion piece
    // is supplied at apply time and cannot change whether the move is legal.
    static int chessGenPseudo(const ChessCore& c, uint16_t* out) {
        int n = 0;
        uint8_t me = c.stm, opp = me ^ 1;
        for(int from = 0; from < 64; from++) {
            uint8_t pc = c.sq[from];
            if(!pc || chSide(pc) != me) continue;
            int f = from & 7, r = from >> 3;
            uint8_t k = chKind(pc);
            if(k == 1) { // pawn
                int dir = me ? -1 : 1, home = me ? 6 : 1, r1 = r + dir;
                if(r1 < 0 || r1 > 7) continue; // an unpromoted pawn on the last rank
                if(!c.sq[r1 * 8 + f]) {
                    chPush(out, n, from, r1 * 8 + f);
                    int r2 = r + 2 * dir; // double push needs both squares empty
                    if(r == home && !c.sq[r2 * 8 + f]) chPush(out, n, from, r2 * 8 + f);
                }
                for(int df = -1; df <= 1; df += 2) {
                    int ff = f + df;
                    if(ff < 0 || ff > 7) continue;
                    int to = r1 * 8 + ff;
                    uint8_t t = c.sq[to];
                    if(t) {
                        if(chSide(t) == opp) chPush(out, n, from, to);
                    } else if(to == (int)c.ep) {
                        chPush(out, n, from, to); // en passant
                    }
                }
            } else if(k == 2 || k == 6) { // knight, king: one step per direction
                const int8_t* sdf = (k == 2) ? CH_NDF : CH_KDF;
                const int8_t* sdr = (k == 2) ? CH_NDR : CH_KDR;
                for(int i = 0; i < 8; i++) {
                    int ff = f + sdf[i], rr = r + sdr[i];
                    if(ff < 0 || ff > 7 || rr < 0 || rr > 7) continue;
                    int to = rr * 8 + ff;
                    uint8_t t = c.sq[to];
                    if(t && chSide(t) == me) continue;
                    chPush(out, n, from, to);
                }
            } else { // bishop rays 0..3, rook rays 4..7, queen all eight
                int d0 = (k == 4) ? 4 : 0, d1 = (k == 3) ? 4 : 8;
                for(int d = d0; d < d1; d++) {
                    int ff = f + CH_SDF[d], rr = r + CH_SDR[d];
                    while(ff >= 0 && ff <= 7 && rr >= 0 && rr <= 7) {
                        int to = rr * 8 + ff;
                        uint8_t t = c.sq[to];
                        if(t && chSide(t) == me) break;
                        chPush(out, n, from, to);
                        if(t) break; // captured: the ray stops here
                        ff += CH_SDF[d];
                        rr += CH_SDR[d];
                    }
                }
            }
        }
        // Castling is validated in full here, so it never reaches the make/unmake
        // filter: right present, rook actually home (defensive, for loaded positions),
        // the path clear, and the king neither in check nor crossing an attacked square.
        int home = me ? 60 : 4, base = me ? 56 : 0;
        uint8_t rook = me ? 10 : 4, bitK = me ? 4 : 1, bitQ = me ? 8 : 2;
        if(c.sq[home] == (uint8_t)(me ? 12 : 6) && (c.rights & (bitK | bitQ)) &&
           !chessAttacked(c, home, opp)) {
            if((c.rights & bitK) && c.sq[base + 7] == rook && !c.sq[base + 5] &&
               !c.sq[base + 6] && !chessAttacked(c, base + 5, opp) &&
               !chessAttacked(c, base + 6, opp))
                chPush(out, n, home, base + 6);
            if((c.rights & bitQ) && c.sq[base] == rook && !c.sq[base + 1] &&
               !c.sq[base + 2] && !c.sq[base + 3] && !chessAttacked(c, base + 3, opp) &&
               !chessAttacked(c, base + 2, opp))
                chPush(out, n, home, base + 2); // b1/b8 may be attacked, only crossed by the rook
        }
        return n;
    }

    // Apply a move and record what chessUnmake needs. `promo` is a WHITE piece code
    // (2 = N, 3 = B, 4 = R, 5 = Q) and is only meaningful for a pawn reaching the last
    // rank; callers pass 0 otherwise, which chessUnmake relies on. Returns true when
    // the move was irreversible (pawn move or capture), on which the caller resets the
    // halfmove clock and the repetition history.
    static bool chessMake(ChessCore& c, int from, int to, uint8_t promo, ChessUndo& u) {
        uint8_t pc = c.sq[from], k = chKind(pc), me = chSide(pc);
        u.rights = c.rights;
        u.ep = c.ep;
        u.capSq = (uint8_t)to;
        u.captured = c.sq[to];
        bool irreversible = (k == 1) || (u.captured != 0);
        if(k == 1 && to == (int)c.ep && !u.captured) { // en passant: victim sits beside `to`
            u.capSq = (uint8_t)(me ? to + 8 : to - 8);
            u.captured = c.sq[u.capSq];
            c.sq[u.capSq] = 0;
        }
        c.sq[to] = pc;
        c.sq[from] = 0;
        if(k == 1 && (to >> 3) == (me ? 0 : 7) && promo >= 2 && promo <= 5)
            c.sq[to] = (uint8_t)(promo + (me ? 6 : 0));
        if(k == 6 && from == (me ? 60 : 4)) { // castling also hops the rook over the king
            if(to == from + 2) {
                c.sq[from + 1] = c.sq[from + 3];
                c.sq[from + 3] = 0;
            } else if(to == from - 2) {
                c.sq[from - 1] = c.sq[from - 4];
                c.sq[from - 4] = 0;
            }
        }
        if(k == 6) c.rights &= (uint8_t)~(me ? 0x0C : 0x03); // king moved: both rights gone
        c.rights &= (uint8_t)~chCornerBit(from);
        if(u.captured) c.rights &= (uint8_t)~chCornerBit(u.capSq);
        c.ep = (k == 1 && (to - from == 16 || from - to == 16)) ? (int8_t)((from + to) / 2) : -1;
        c.stm ^= 1;
        return irreversible;
    }

    // Exact inverse of chessMake, given the same from/to/promo and its ChessUndo.
    static void chessUnmake(ChessCore& c, int from, int to, uint8_t promo,
                            const ChessUndo& u) {
        c.stm ^= 1; // back to the side that moved
        uint8_t me = c.stm, pc = c.sq[to];
        if(promo >= 2 && promo <= 5) pc = me ? 7 : 1; // demote: only a pawn could promote
        c.sq[from] = pc;
        c.sq[to] = 0;
        if(u.captured) c.sq[u.capSq] = u.captured; // capSq != to for en passant
        if(chKind(pc) == 6 && from == (me ? 60 : 4)) { // un-hop the castling rook
            if(to == from + 2) {
                c.sq[from + 3] = c.sq[from + 1];
                c.sq[from + 1] = 0;
            } else if(to == from - 2) {
                c.sq[from - 4] = c.sq[from - 1];
                c.sq[from - 1] = 0;
            }
        }
        c.rights = u.rights;
        c.ep = u.ep;
    }

    // Pseudo-legal moves that do not leave the mover's own king attacked. Pins and
    // en-passant discovered checks fall out of the make/test/unmake for free.
    static int chessGenLegal(const ChessCore& c, uint16_t* out) {
        uint16_t ps[CH_MAX_MOVES];
        int np = chessGenPseudo(c, ps), n = 0;
        ChessCore w = c;
        uint8_t me = c.stm;
        for(int i = 0; i < np; i++) {
            int from = ps[i] >> 6, to = ps[i] & 63;
            ChessUndo u;
            chessMake(w, from, to, 0, u);
            int ks = chessKingSq(w, me);
            if(ks < 0 || !chessAttacked(w, ks, me ^ 1)) out[n++] = ps[i];
            chessUnmake(w, from, to, 0, u);
        }
        return n;
    }

    // FIDE 6.9: on a flag fall the opponent only wins if they *could* mate by some
    // series of legal moves (the helpmate test, not "can force"). So K+N vs K+N is a
    // win on time, but K+N vs a bare king is a draw.
    static bool chessCanMateVs(const ChessCore& c, uint8_t side) {
        int minors = 0, heavy = 0, oppPieces = 0;
        for(int i = 0; i < 64; i++) {
            uint8_t pc = c.sq[i];
            if(!pc || chKind(pc) == 6) continue;
            if(chSide(pc) != side)
                oppPieces++;
            else if(chKind(pc) == 2 || chKind(pc) == 3)
                minors++;
            else
                heavy++; // pawn, rook or queen: a mate always exists
        }
        if(heavy) return true;
        if(minors == 0) return false; // bare king
        return !(minors == 1 && oppPieces == 0); // lone minor vs lone king cannot mate
    }

    // Dead position (FIDE 5.2.2): no legal sequence at all reaches a mate, so the game
    // is drawn the instant it arises. The standard material subset: K vs K, K+minor vs
    // K, and any number of bishops as long as they all stand on one square color.
    // K+N vs K+N is NOT dead: it has helpmates.
    static bool chessDeadPosition(const ChessCore& c) {
        int knights = 0, bishops = 0, color = -1;
        for(int i = 0; i < 64; i++) {
            uint8_t pc = c.sq[i];
            if(!pc) continue;
            uint8_t k = chKind(pc);
            if(k == 6) continue;
            if(k == 2) {
                knights++;
            } else if(k == 3) {
                int sc = (((i >> 3) + (i & 7)) & 1);
                if(color < 0)
                    color = sc;
                else if(color != sc)
                    return false; // bishops on both colors can mate
                bishops++;
            } else {
                return false; // pawn, rook or queen
            }
        }
        if(knights + bishops <= 1) return true; // K vs K, or a single minor
        return knights == 0; // bishops only, and the loop proved they share a color
    }

    // splitmix32 over a fixed seed: the Zobrist keys are the same on every boot without
    // spending 3 KB of flash on a stored table.
    static void chessZobristInit() {
        if(ZOB_READY) return;
        uint32_t s = 0x9E3779B9UL;
        for(unsigned i = 0; i < sizeof(ZOB) / sizeof(ZOB[0]); i++) {
            s += 0x9E3779B9UL;
            uint32_t z = s;
            z = (z ^ (z >> 16)) * 0x85EBCA6BUL;
            z = (z ^ (z >> 13)) * 0xC2B2AE35UL;
            ZOB[i] = z ^ (z >> 16);
        }
        ZOB_READY = true;
    }

    // Can the side to move actually capture en passant here? FIDE 9.2 compares the
    // *possible moves*, not the bare ep square, so a hash that always folds in the ep
    // file reports two identical positions as different and repetition never triggers.
    static bool chessEpLegal(const ChessCore& c) {
        if(c.ep < 0) return false;
        int to = c.ep, f = to & 7, pr = (to >> 3) + (c.stm ? 1 : -1);
        if(pr < 0 || pr > 7) return false;
        uint8_t pawn = c.stm ? 7 : 1;
        ChessCore w = c;
        for(int df = -1; df <= 1; df += 2) {
            int ff = f + df;
            if(ff < 0 || ff > 7) continue;
            int from = pr * 8 + ff;
            if(w.sq[from] != pawn) continue;
            ChessUndo u;
            chessMake(w, from, to, 0, u);
            int ks = chessKingSq(w, c.stm);
            bool ok = (ks < 0) || !chessAttacked(w, ks, c.stm ^ 1);
            chessUnmake(w, from, to, 0, u);
            if(ok) return true;
        }
        return false;
    }

    // Position key for repetition detection, recomputed from scratch (a 64-square scan
    // once per move; incremental updating would buy nothing at this rate).
    static uint32_t chessHash(const ChessCore& c) {
        chessZobristInit();
        uint32_t h = 0;
        for(int i = 0; i < 64; i++)
            if(c.sq[i]) h ^= ZOB[(c.sq[i] - 1) * 64 + i];
        if(c.stm) h ^= ZOB[768];
        h ^= ZOB[769 + (c.rights & 15)];
        if(chessEpLegal(c)) h ^= ZOB[785 + (c.ep & 7)];
        return h;
    }

#ifdef HA_CHESS_TEST
public:
    // Move-path enumeration, the standard yardstick for a move generator: the number
    // of distinct legal move sequences of the given length. Test builds only.
    static uint32_t chessPerft(ChessCore& c, int depth) {
        if(depth <= 0) return 1;
        uint16_t mv[CH_MAX_MOVES];
        int n = chessGenLegal(c, mv);
        uint32_t total = 0;
        for(int i = 0; i < n; i++) {
            int from = mv[i] >> 6, to = mv[i] & 63;
            ChessUndo u;
            // Perft counts each promotion piece as its own move, so expand the single
            // entry our encoding generates back into the four choices.
            if(chKind(c.sq[from]) == 1 && (to >> 3) == (c.stm ? 0 : 7)) {
                for(uint8_t p = 2; p <= 5; p++) {
                    chessMake(c, from, to, p, u);
                    total += chessPerft(c, depth - 1);
                    chessUnmake(c, from, to, p, u);
                }
            } else {
                chessMake(c, from, to, 0, u);
                total += chessPerft(c, depth - 1);
                chessUnmake(c, from, to, 0, u);
            }
        }
        return total;
    }

    // board64 is exactly 64 chars, index 0 = a1 .. 63 = h8, FEN letters (uppercase =
    // white) and '.' for an empty square. False on a bad length or character.
    static bool chessLoadCore(ChessCore& c, const char* board64, uint8_t stm,
                              uint8_t rights, int8_t ep) {
        if(!board64 || strlen(board64) != 64) return false;
        const char* codes = "PNBRQKpnbrqk";
        ChessCore t = ChessCore{};
        for(int i = 0; i < 64; i++) {
            if(board64[i] == '.') continue;
            const char* p = strchr(codes, board64[i]);
            if(!p) return false;
            t.sq[i] = (uint8_t)(p - codes + 1);
        }
        t.stm = stm ? 1 : 0;
        t.rights = rights & 15;
        t.ep = ep;
        c = t;
        return true;
    }
private:
#endif

    // ---------- chess (match) ----------
    // Lifecycle, clocks and serialization around the rules core above. Same shape as
    // battleship: one slot per live pairing, freed when both players have detached.
    void chessClear() {
        for(int i = 0; i < CHESS_MAX; i++) _cm[i] = ChessMatch{};
    }

    ChessMatch* chessMatchOf(uint8_t pid) {
        for(int i = 0; i < CHESS_MAX; i++) {
            if(!_cm[i].used) continue;
            if(_cm[i].a == pid && _cm[i].aIn) return &_cm[i];
            if(_cm[i].b == pid && _cm[i].bIn) return &_cm[i];
        }
        return nullptr;
    }

    // Colors are per game, not per seat: a rematch swaps them, so every side/pid
    // translation goes through m->white rather than through a/b.
    static uint8_t chessSideOf(const ChessMatch* m, uint8_t pid) {
        return pid == m->white ? 0 : 1;
    }
    static uint8_t chessPidOf(const ChessMatch* m, uint8_t side) {
        return side ? ((m->white == m->a) ? m->b : m->a) : m->white;
    }
    static uint8_t chessTurnPid(const ChessMatch* m) { return chessPidOf(m, m->core.stm); }

    void chessStart(ChessMatch* m, uint8_t a, uint8_t b, uint8_t whitePid) {
        *m = ChessMatch{};
        m->used = true;
        m->a = a;
        m->b = b;
        m->aIn = m->bIn = true;
        m->white = whitePid;
        m->phase = 1;
        m->winner = 0;
        static const uint8_t back[8] = {4, 2, 3, 5, 6, 3, 2, 4}; // R N B Q K B N R
        for(int f = 0; f < 8; f++) {
            m->core.sq[f] = back[f]; // a1..h1
            m->core.sq[8 + f] = 1; // white pawns
            m->core.sq[48 + f] = 7; // black pawns
            m->core.sq[56 + f] = (uint8_t)(back[f] + 6); // a8..h8
        }
        m->core.stm = 0;
        m->core.rights = 15;
        m->core.ep = -1;
        m->halfmove = 0;
        m->fullmove = 1;
        m->clockMs[0] = m->clockMs[1] = CH_CLOCK_MS;
        m->lastStamp = millis();
        m->lastMove = -1;
        m->offerBy = 0;
        chessZobristInit();
        m->hist[0] = chessHash(m->core);
        m->histLen = 1;
    }

    // Scoring copies duelFinish (battleship's finish forgot it): 300 to the winner,
    // nothing on a draw, and the result goes up the UART either way. winnerPid 0 = draw.
    void chessFinish(ChessMatch* m, uint8_t winnerPid, uint8_t reason) {
        if(m->phase != 1) return;
        m->phase = 2;
        m->winner = winnerPid;
        m->reason = reason;
        uint8_t loser = (winnerPid == m->a) ? m->b : (winnerPid == m->b) ? m->a : 0;
        if(winnerPid) {
            _p[winnerPid].score += 300;
            haUartScore(winnerPid, 300, "chesswin");
            haUartRoundResult(String("{\"win\":") + winnerPid + ",\"lose\":" + loser + "}");
        } else {
            haUartRoundResult(String("{\"draw\":[") + m->a + "," + m->b + "]}");
        }
    }

    // The flag falls for the side to move. FIDE 6.9: the opponent only wins if they
    // could still mate by SOME legal sequence, otherwise the game is drawn.
    void chessFlagFall(ChessMatch* m) {
        uint8_t side = m->core.stm, opp = (uint8_t)(side ^ 1);
        m->clockMs[side] = 0;
        if(chessCanMateVs(m->core, opp))
            chessFinish(m, chessPidOf(m, opp), CH_R_FLAG);
        else
            chessFinish(m, 0, CH_R_FLAGDRAW);
    }

    // How often the position now on the board has occurred, counting this occurrence.
    static int chessRepCount(const ChessMatch* m) {
        uint32_t h = chessHash(m->core);
        int n = 0;
        for(uint16_t i = 0; i < m->histLen; i++)
            if(m->hist[i] == h) n++;
        return n;
    }

    void chessMove(uint8_t pid, int from, int to, int promo) {
        ChessMatch* m = chessMatchOf(pid);
        if(!m || m->phase != 1 || chessTurnPid(m) != pid) return;
        uint8_t stm = m->core.stm;
        uint32_t now = millis(), elapsed = now - m->lastStamp;
        if(elapsed >= m->clockMs[stm]) { // the move arrived after the flag fell: ignore it
            chessFlagFall(m);
            pushAll();
            return;
        }
        if(from < 0 || from > 63 || to < 0 || to > 63) return;
        // Never hand chessMake a move it did not generate: its castling branch hops the
        // rook on any e1-g1/c1 king move without re-checking, so a spoofed one corrupts
        // the board.
        uint16_t mv[CH_MAX_MOVES];
        int n = chessGenLegal(m->core, mv);
        bool legal = false;
        for(int i = 0; i < n; i++)
            if(mv[i] == (uint16_t)(from * 64 + to)) legal = true;
        if(!legal) return;
        // A pawn reaching the last rank must name a promotion piece, and nothing else may.
        bool isPromo = chKind(m->core.sq[from]) == 1 && (to >> 3) == (stm ? 0 : 7);
        if(isPromo != (promo >= 2 && promo <= 5)) return;

        m->clockMs[stm] -= elapsed;
        m->lastStamp = now;
        ChessUndo u;
        bool irrev = chessMake(m->core, from, to, isPromo ? (uint8_t)promo : 0, u);
        m->lastMove = (int16_t)(from * 64 + to);
        m->offerBy = 0; // a pending draw offer lapses once a move is played
        if(stm) m->fullmove++;
        if(irrev) { // a pawn move or capture can never repeat: the record starts over
            m->halfmove = 0;
            m->histLen = 0;
        } else {
            m->halfmove++;
        }
        if(m->histLen < CH_HIST) m->hist[m->histLen++] = chessHash(m->core);

        // Checkmate outranks the automatic counters (FIDE 9.6.2): a mating move ends the
        // game even when it also completes the 75-move or fivefold count.
        uint16_t reply[CH_MAX_MOVES];
        bool stuck = chessGenLegal(m->core, reply) == 0; // no reply: mate or stalemate
        if(stuck && chessInCheck(m->core))
            chessFinish(m, pid, CH_R_MATE);
        else if(stuck)
            chessFinish(m, 0, CH_R_STALEMATE);
        else if(chessDeadPosition(m->core))
            chessFinish(m, 0, CH_R_MATERIAL);
        else if(m->halfmove >= 150)
            chessFinish(m, 0, CH_R_MOVE75);
        else if(chessRepCount(m) >= 5)
            chessFinish(m, 0, CH_R_REP5);
        pushAll();
    }

    void chessResign(uint8_t pid) {
        ChessMatch* m = chessMatchOf(pid);
        if(!m || m->phase != 1) return;
        chessFinish(m, (pid == m->a) ? m->b : m->a, CH_R_RESIGN);
        pushAll();
    }

    // Offer a draw, or accept the one already on the table.
    void chessDraw(uint8_t pid) {
        ChessMatch* m = chessMatchOf(pid);
        if(!m || m->phase != 1) return;
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        if(m->offerBy == pid) return;
        if(m->offerBy == opp) {
            chessFinish(m, 0, CH_R_AGREE);
        } else {
            m->offerBy = pid;
            if(_p[opp].wsId)
                haWsSendWs(
                    _p[opp].wsId,
                    String("{\"t\":\"toast\",\"msg\":\"") + ha_json_escape(_p[pid].nick) +
                        " offers a draw\"}");
        }
        pushAll();
    }

    // Threefold and the 50-move rule are claims, not automatic: only the player to move
    // may make them, and only while the count actually stands.
    void chessClaim(uint8_t pid) {
        ChessMatch* m = chessMatchOf(pid);
        if(!m || m->phase != 1 || chessTurnPid(m) != pid) return;
        if(chessRepCount(m) >= 3)
            chessFinish(m, 0, CH_R_REP3);
        else if(m->halfmove >= 100)
            chessFinish(m, 0, CH_R_MOVE50);
        else
            return; // nothing to claim
        pushAll();
    }

    void chessRematch(uint8_t pid) {
        ChessMatch* m = chessMatchOf(pid);
        if(!m || m->phase != 2) return;
        if(!m->aIn || !m->bIn) {
            if(_p[pid].wsId)
                haWsSendWs(_p[pid].wsId, String("{\"t\":\"toast\",\"msg\":\"Opponent left\"}"));
            chessOnLeave(pid);
            pushAll();
            return;
        }
        uint8_t next = (m->white == m->a) ? m->b : m->a; // colors swap every game
        chessStart(m, m->a, m->b, next);
        pushAll();
    }

    void chessOnLeave(uint8_t pid) {
        ChessMatch* m = chessMatchOf(pid);
        if(!m) return;
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        if(m->phase == 1) chessFinish(m, opp, CH_R_LEFT); // forfeit
        if(pid == m->a) m->aIn = false;
        if(pid == m->b) m->bIn = false;
        if(!m->aIn && !m->bIn) *m = ChessMatch{}; // both gone: free the slot
    }

    // The only game whose state changes with no input at all. Nothing is pushed unless a
    // flag actually fell: the phones count the running clock down from `deadline`.
    void chessTick(uint32_t now) {
        bool ended = false;
        for(int i = 0; i < CHESS_MAX; i++) {
            ChessMatch* m = &_cm[i];
            if(!m->used || m->phase != 1) continue;
            if((now - m->lastStamp) < m->clockMs[m->core.stm]) continue;
            chessFlagFall(m);
            ended = true;
        }
        if(ended) pushAll();
    }

    // 64 chars, index 0 = a1: FEN letters, uppercase white, '.' empty.
    static String chessBoardStr(const ChessCore& c) {
        char b[65];
        for(int i = 0; i < 64; i++) b[i] = ".PNBRQKpnbrqk"[c.sq[i]];
        b[64] = '\0';
        return String(b);
    }

    static const char* chessReasonStr(uint8_t r) {
        switch(r) {
        case CH_R_MATE:
            return "mate";
        case CH_R_STALEMATE:
            return "stalemate";
        case CH_R_RESIGN:
            return "resign";
        case CH_R_FLAG:
            return "flag";
        case CH_R_FLAGDRAW:
            return "flagdraw";
        case CH_R_MATERIAL:
            return "material";
        case CH_R_REP3:
            return "rep3";
        case CH_R_REP5:
            return "rep5";
        case CH_R_MOVE50:
            return "move50";
        case CH_R_MOVE75:
            return "move75";
        case CH_R_AGREE:
            return "agree";
        case CH_R_LEFT:
            return "left";
        }
        return "";
    }

    String chessJson(uint8_t pid) {
        ChessMatch* m = chessMatchOf(pid);
        if(!m)
            return String("{\"t\":\"chess\",\"phase\":\"lobby\",\"challenges\":") +
                   duelChallengesJson() + "}";
        uint8_t opp = (pid == m->a) ? m->b : m->a;
        uint8_t stm = m->core.stm, turn = chessTurnPid(m);
        bool yourTurn = (turn == pid);
        // One clock reading for the whole message, so `run` and `deadline` agree. The
        // running clock freezes once the game is over -- the over screen is not a place
        // to watch time tick away.
        uint32_t now = millis(), rem = m->clockMs[stm];
        if(m->phase == 1) {
            uint32_t spent = now - m->lastStamp;
            rem -= (spent < rem) ? spent : rem;
        }
        String s = "{\"t\":\"chess\",\"phase\":\"";
        s += (m->phase == 2) ? "over" : "playing";
        s += "\",\"you\":";
        s += pid;
        s += ",\"opp\":\"" + ha_json_escape(_p[opp].nick) + "\"";
        s += ",\"white\":";
        s += (chessSideOf(m, pid) == 0) ? "true" : "false";
        s += ",\"turn\":";
        s += turn;
        s += ",\"yourTurn\":";
        s += yourTurn ? "true" : "false";
        s += ",\"board\":\"" + chessBoardStr(m->core) + "\"";
        if(m->phase == 1) { // the mover's own legal moves; nobody else's are anyone's business
            s += ",\"moves\":[";
            if(yourTurn) {
                uint16_t mv[CH_MAX_MOVES];
                int n = chessGenLegal(m->core, mv);
                for(int i = 0; i < n; i++) {
                    if(i) s += ",";
                    s += (int)mv[i];
                }
            }
            s += "]";
        }
        s += ",\"check\":";
        s += chessInCheck(m->core) ? "true" : "false";
        s += ",\"last\":";
        s += (int)m->lastMove;
        s += ",\"deadline\":";
        s += (unsigned long)(now + rem);
        s += ",\"run\":";
        s += (unsigned long)rem;
        s += ",\"oms\":";
        s += (unsigned long)m->clockMs[stm ^ 1];
        s += ",\"wtm\":";
        s += (stm == 0) ? "true" : "false";
        if(m->phase == 1) {
            s += ",\"claim3\":";
            s += (yourTurn && chessRepCount(m) >= 3) ? "true" : "false";
            s += ",\"claim50\":";
            s += (yourTurn && m->halfmove >= 100) ? "true" : "false";
        }
        s += ",\"offer\":";
        s += m->offerBy;
        if(m->phase == 2) {
            s += ",\"result\":\"";
            s += !m->winner ? "draw" : (m->winner == pid) ? "win" : "lose";
            s += "\",\"reason\":\"";
            s += chessReasonStr(m->reason);
            s += "\"";
        }
        s += "}";
        return s;
    }

#ifdef HA_CHESS_TEST
public:
    // Test-only: overwrite match slot 0's position after a normal challenge/accept, so
    // a scenario can set up a specific board without walking through the opening moves.
    // Requires slot 0 to already hold a live game (_cm[0].used && phase == 1).
    void chessTestLoad(const char* board64, int stm, int rights, int ep, int halfmove,
                        uint32_t wms, uint32_t bms) {
        if(!_cm[0].used || _cm[0].phase != 1) return;
        if(!chessLoadCore(_cm[0].core, board64, (uint8_t)stm, (uint8_t)rights, (int8_t)ep))
            return;
        _cm[0].halfmove = (uint8_t)halfmove;
        _cm[0].clockMs[0] = wms;
        _cm[0].clockMs[1] = bms;
        _cm[0].lastStamp = millis();
        _cm[0].offerBy = 0;
        _cm[0].lastMove = -1;
        _cm[0].hist[0] = chessHash(_cm[0].core);
        _cm[0].histLen = 1;
        pushAll();
    }

    // Loads a scratch position (no match involved) and runs perft on it.
    static uint32_t chessTestPerft(const char* board64, int stm, int rights, int ep,
                                    int depth) {
        ChessCore c{};
        if(!chessLoadCore(c, board64, (uint8_t)stm, (uint8_t)rights, (int8_t)ep)) return 0;
        return chessPerft(c, depth);
    }
private:
#endif

    // ---------- spectrum (wavelength-style guessing) ----------
    // Which pack wins the pre-round vote; identical policy to wyrWinningPack().
    int spectrumWinningPack() {
        if(_spec.packCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _spec.vote[i] >= 0 && _spec.vote[i] < _spec.packCount) {
                votes[_spec.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_spec.packCount);
        int best = 0;
        for(int i = 1; i < _spec.packCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _spec.packCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void spectrumClear() {
        partyClear(_spec.pt);
        _spec.pack = 0;
        _spec.card = 0;
        _spec.cardSeq = 0;
        _spec.psychic = 0;
        _spec.psychicSeq = 0;
        _spec.stage = 0;
        _spec.target = 0;
        _spec.clue[0] = '\0';
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _spec.vote[i] = -1;
            _spec.guess[i] = -1;
            _spec.gained[i] = 0;
        }
    }

    void spectrumReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_SPECTRUM) return;
        if(_spec.pt.phase != 0 && _spec.pt.phase != 4) return;
        if(_spec.pt.phase == 4 && val) spectrumClear(); // ready from final -> new game
        _spec.pt.ready[pid] = val;
        spectrumCheckStart();
        pushAll();
    }

    void spectrumVote(uint8_t pid, int pack) {
        if(_active != HA_GAME_SPECTRUM || _spec.pt.phase != 0) return;
        if(pack < 0 || pack >= _spec.packCount) return;
        _spec.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void spectrumCheckStart() {
        if(_spec.packCount == 0) return;
        Party& pt = _spec.pt;
        if(pt.phase == 0 && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !partyAllReady(pt)) {
            pt.phase = 0;
        }
    }

    // The psychic rotates across rounds: the (psychicSeq mod N)-th connected player.
    uint8_t spectrumPickPsychic() {
        int n = connectedCount();
        if(n <= 0) return 0;
        int want = _spec.psychicSeq % n;
        int seen = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(seen == want) return i;
            seen++;
        }
        return 0;
    }

    void spectrumNextRound(uint32_t now) {
        Party& pt = _spec.pt;
        WyrPack& pk = _spec.packs[_spec.pack];
        if(pt.round >= SPECTRUM_ROUNDS || pk.count == 0) {
            pt.phase = 4; // final
            pushAll();
            return;
        }
        pt.round++;
        _spec.psychic = spectrumPickPsychic();
        _spec.psychicSeq++;
        _spec.card = (uint8_t)(_spec.cardSeq % pk.count);
        _spec.cardSeq++;
        _spec.target = 5 + (int)random(91); // 5..95, avoid the very edges
        _spec.stage = 0; // clue first
        _spec.clue[0] = '\0';
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _spec.guess[i] = -1;
            _spec.gained[i] = 0;
        }
        pt.deadline = now + (uint32_t)SPECTRUM_CLUE_SECS * 1000;
        pt.phase = 2;
        pushAll();
    }

    void spectrumClue(uint8_t pid, const char* text) {
        if(_active != HA_GAME_SPECTRUM || _spec.pt.phase != 2 || _spec.stage != 0) return;
        if(pid != _spec.psychic) return;
        strlcpy(_spec.clue, text, sizeof(_spec.clue));
        _spec.stage = 1; // move to guessing
        _spec.pt.deadline = millis() + (uint32_t)SPECTRUM_GUESS_SECS * 1000;
        haUartEvent(String("{\"draw\":\"") + ha_json_escape(_p[pid].nick) + ": " +
                    ha_json_escape(_spec.clue) + "\"}");
        pushAll();
    }

    bool spectrumAllGuessed() {
        int guessers = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || i == _spec.psychic) continue;
            guessers++;
            if(_spec.guess[i] < 0) return false;
        }
        return guessers >= 1;
    }

    void spectrumGuess(uint8_t pid, int val) {
        if(_active != HA_GAME_SPECTRUM || _spec.pt.phase != 2 || _spec.stage != 1) return;
        if(pid == _spec.psychic) return; // the clue-giver doesn't guess
        if(val < 0) val = 0;
        if(val > 100) val = 100;
        _spec.guess[pid] = (int8_t)val;
        if(spectrumAllGuessed()) spectrumReveal(millis());
        else pushAll();
    }

    // Points by closeness of the guess (0..100) to the hidden target (0..100).
    // A tight bullseye (±2) for landing right on it, then two 5-wide rings,
    // matching the dial's three scoring wedges exactly.
    static int spectrumPoints(int target, int guess) {
        int d = target - guess;
        if(d < 0) d = -d;
        if(d <= 2) return 4;
        if(d <= 7) return 3;
        if(d <= 12) return 2;
        return 0;
    }

    void spectrumReveal(uint32_t now) {
        int sum = 0, guessers = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || i == _spec.psychic || _spec.guess[i] < 0) continue;
            int pts = spectrumPoints(_spec.target, _spec.guess[i]);
            _spec.gained[i] = pts;
            _p[i].score += pts;
            if(pts) haUartScore(i, pts, "spectrum");
            sum += pts;
            guessers++;
        }
        // The psychic scores by how well the group did: the average guesser score,
        // so a clue that lands everyone near the target is worth the most.
        if(_spec.psychic && guessers > 0) {
            int avg = (sum + guessers / 2) / guessers;
            _spec.gained[_spec.psychic] = avg;
            _p[_spec.psychic].score += avg;
            if(avg) haUartScore(_spec.psychic, avg, "clue");
        }
        haUartRoundResult(String("{\"spectrum\":\"round ") + _spec.pt.round + "\"}");
        _spec.pt.phase = 3;
        _spec.pt.revealUntil = now + SPECTRUM_REVEAL_MS;
        pushAll();
    }

    void spectrumAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_SPECTRUM || _spec.pt.phase != 4) return;
        spectrumClear();
        pushAll();
    }

    void spectrumTick(uint32_t now) {
        Party& pt = _spec.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                _spec.pack = (uint8_t)spectrumWinningPack();
                _spec.psychicSeq = 0;
                _spec.cardSeq = 0;
                spectrumNextRound(now);
            }
        } else if(pt.phase == 2) {
            if(_spec.stage == 0) {
                // Clue window expired with no clue: move on to guessing anyway so a
                // silent/absent psychic can't stall the game.
                if((int32_t)(now - pt.deadline) >= 0) {
                    _spec.stage = 1;
                    pt.deadline = now + (uint32_t)SPECTRUM_GUESS_SECS * 1000;
                    pushAll();
                }
            } else {
                if((int32_t)(now - pt.deadline) >= 0 || spectrumAllGuessed())
                    spectrumReveal(now);
            }
        } else if(pt.phase == 3) {
            if((int32_t)(now - pt.revealUntil) >= 0) spectrumNextRound(now);
        }
    }

    String spectrumJson(uint8_t pid) {
        Party& pt = _spec.pt;
        if(pt.phase == 0) {
            String s = String("{\"t\":\"spectrum\",\"phase\":\"lobby\",\"you\":") + pid +
                       ",\"players\":" + partyPlayersJson(pt);
            s += ",\"packs\":[";
            int votes[TRIVIA_MAX_TOPICS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _spec.vote[i] >= 0 && _spec.vote[i] < _spec.packCount)
                    votes[_spec.vote[i]]++;
            for(int i = 0; i < _spec.packCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_spec.packs[i].name.c_str()) +
                     "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_spec.vote[pid]) + "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"spectrum\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"spectrum\",\"phase\":\"final\",\"board\":") + triviaBoard() +
                   "}";

        WyrPack& pk = _spec.packs[_spec.pack];
        const char* left = pk.items[_spec.card].a.c_str();
        const char* right = pk.items[_spec.card].b.c_str();
        bool mePsychic = (pid == _spec.psychic);
        bool reveal = (pt.phase == 3);
        const char* stage = reveal ? "reveal" : (_spec.stage == 0 ? "clue" : "guess");

        String s = String("{\"t\":\"spectrum\",\"phase\":\"play\",\"stage\":\"") + stage +
                   "\",\"round\":" + pt.round + ",\"rounds\":" + SPECTRUM_ROUNDS + ",\"left\":\"" +
                   ha_json_escape(left) + "\",\"right\":\"" + ha_json_escape(right) +
                   "\",\"psychic\":\"" + ha_json_escape(_p[_spec.psychic].nick) +
                   "\",\"iam\":" + (mePsychic ? "true" : "false");
        // The psychic sees the target during the clue stage; on reveal everyone does.
        if(reveal || mePsychic) {
            s += ",\"target\":";
            s += _spec.target;
        }
        if(_spec.stage == 1 || reveal) {
            s += ",\"clue\":\"";
            s += ha_json_escape(_spec.clue);
            s += "\"";
        }
        if(!mePsychic) {
            s += ",\"myguess\":";
            s += (int)_spec.guess[pid];
        }
        if(reveal) {
            s += ",\"guesses\":[";
            bool first = true;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                if(!_p[i].used || i == _spec.psychic || _spec.guess[i] < 0) continue;
                if(!first) s += ",";
                first = false;
                s += "{\"nick\":\"" + ha_json_escape(_p[i].nick) + "\",\"g\":" +
                     (int)_spec.guess[i] + ",\"pts\":" + _spec.gained[i] + "}";
            }
            s += "]";
            s += ",\"mygain\":";
            s += _spec.gained[pid];
            s += ",\"deadline\":";
            s += pt.revealUntil;
            s += ",\"dur\":";
            s += (SPECTRUM_REVEAL_MS / 1000);
        } else {
            s += ",\"deadline\":";
            s += pt.deadline;
            s += ",\"dur\":";
            s += (_spec.stage == 0 ? SPECTRUM_CLUE_SECS : SPECTRUM_GUESS_SECS);
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- Kiss Marry Kill (predict a player's picks) ----------
    int kmkWinningPack() {
        if(_kmk.packCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _kmk.vote[i] >= 0 && _kmk.vote[i] < _kmk.packCount) {
                votes[_kmk.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_kmk.packCount);
        int best = 0;
        for(int i = 1; i < _kmk.packCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _kmk.packCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void kmkClear() {
        partyClear(_kmk.pt);
        _kmk.pack = 0;
        _kmk.nameSeq = 0;
        _kmk.chooser = 0;
        _kmk.chooserSeq = 0;
        _kmk.stage = 0;
        for(int i = 0; i < 3; i++) {
            _kmk.person[i] = 0;
            _kmk.cLabel[i] = -1;
        }
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _kmk.vote[i] = -1;
            _kmk.guessed[i] = false;
            _kmk.gained[i] = 0;
            for(int j = 0; j < 3; j++) _kmk.gLabel[i][j] = -1;
        }
    }

    void kmkReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_KMK) return;
        if(_kmk.pt.phase != 0 && _kmk.pt.phase != 4) return;
        if(_kmk.pt.phase == 4 && val) kmkClear();
        _kmk.pt.ready[pid] = val;
        kmkCheckStart();
        pushAll();
    }

    void kmkVote(uint8_t pid, int pack) {
        if(_active != HA_GAME_KMK || _kmk.pt.phase != 0) return;
        if(pack < 0 || pack >= _kmk.packCount) return;
        _kmk.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void kmkCheckStart() {
        if(_kmk.packCount == 0) return;
        Party& pt = _kmk.pt;
        if(pt.phase == 0 && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !partyAllReady(pt)) {
            pt.phase = 0;
        }
    }

    // The chooser rotates: the (chooserSeq mod N)-th connected player.
    uint8_t kmkPickChooser() {
        int n = connectedCount();
        if(n <= 0) return 0;
        int want = _kmk.chooserSeq % n, seen = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(seen == want) return i;
            seen++;
        }
        return 0;
    }

    void kmkNextRound(uint32_t now) {
        Party& pt = _kmk.pt;
        WordPack& pk = _kmk.packs[_kmk.pack];
        if(pt.round >= KMK_ROUNDS || pk.count < 3) {
            pt.phase = 4; // final (need at least three names to play)
            pushAll();
            return;
        }
        pt.round++;
        _kmk.chooser = kmkPickChooser();
        _kmk.chooserSeq++;
        // three distinct people, walking the pack from a rotating offset
        uint8_t base = (uint8_t)(_kmk.nameSeq % pk.count);
        _kmk.nameSeq += 3;
        _kmk.person[0] = base;
        _kmk.person[1] = (uint8_t)((base + 1 + random(pk.count - 2)) % pk.count);
        do {
            _kmk.person[2] = (uint8_t)(random(pk.count));
        } while(_kmk.person[2] == _kmk.person[0] || _kmk.person[2] == _kmk.person[1]);
        _kmk.stage = 0; // chooser assigns first
        for(int i = 0; i < 3; i++) _kmk.cLabel[i] = -1;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _kmk.guessed[i] = false;
            _kmk.gained[i] = 0;
            for(int j = 0; j < 3; j++) _kmk.gLabel[i][j] = -1;
        }
        pt.deadline = now + (uint32_t)KMK_CHOOSE_SECS * 1000;
        pt.phase = 2;
        pushAll();
    }

    // kiss/marry/kill are person indices 0..2; build a per-person label array
    // (0 kiss, 1 marry, 2 kill). Returns false unless it's a valid permutation.
    static bool kmkToLabels(int kiss, int marry, int kill, int8_t out[3]) {
        int a[3] = {kiss, marry, kill};
        for(int i = 0; i < 3; i++)
            if(a[i] < 0 || a[i] > 2) return false;
        if(kiss == marry || kiss == kill || marry == kill) return false;
        out[kiss] = 0;
        out[marry] = 1;
        out[kill] = 2;
        return true;
    }

    bool kmkAllGuessed() {
        int guessers = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || i == _kmk.chooser) continue;
            guessers++;
            if(!_kmk.guessed[i]) return false;
        }
        return guessers >= 1;
    }

    void kmkAssign(uint8_t pid, int kiss, int marry, int kill) {
        if(_active != HA_GAME_KMK || _kmk.pt.phase != 2) return;
        int8_t labels[3];
        if(!kmkToLabels(kiss, marry, kill, labels)) return;
        if(_kmk.stage == 0) {
            if(pid != _kmk.chooser) return; // only the chooser sets the secret
            for(int i = 0; i < 3; i++) _kmk.cLabel[i] = labels[i];
            _kmk.stage = 1;
            _kmk.pt.deadline = millis() + (uint32_t)KMK_GUESS_SECS * 1000;
            haUartEvent(String("{\"draw\":\"") + ha_json_escape(_p[pid].nick) + " has decided\"}");
            pushAll();
        } else {
            if(pid == _kmk.chooser) return; // the chooser doesn't guess
            for(int i = 0; i < 3; i++) _kmk.gLabel[pid][i] = labels[i];
            _kmk.guessed[pid] = true;
            if(kmkAllGuessed()) kmkReveal(millis());
            else pushAll();
        }
    }

    void kmkReveal(uint32_t now) {
        int sum = 0, guessers = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || i == _kmk.chooser || !_kmk.guessed[i]) continue;
            int hit = 0;
            for(int j = 0; j < 3; j++)
                if(_kmk.gLabel[i][j] == _kmk.cLabel[j]) hit++;
            _kmk.gained[i] = hit; // 0, 1 or 3 (two right forces the third)
            _p[i].score += hit;
            if(hit) haUartScore(i, hit, "kmk");
            sum += hit;
            guessers++;
        }
        if(_kmk.chooser && guessers > 0) {
            int avg = (sum + guessers / 2) / guessers;
            _kmk.gained[_kmk.chooser] = avg;
            _p[_kmk.chooser].score += avg;
            if(avg) haUartScore(_kmk.chooser, avg, "kmk");
        }
        haUartRoundResult(String("{\"kmk\":\"round ") + _kmk.pt.round + "\"}");
        _kmk.pt.phase = 3;
        _kmk.pt.revealUntil = now + KMK_REVEAL_MS;
        pushAll();
    }

    void kmkAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_KMK || _kmk.pt.phase != 4) return;
        kmkClear();
        pushAll();
    }

    void kmkTick(uint32_t now) {
        Party& pt = _kmk.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                _kmk.pack = (uint8_t)kmkWinningPack();
                _kmk.chooserSeq = 0;
                _kmk.nameSeq = 0;
                kmkNextRound(now);
            }
        } else if(pt.phase == 2) {
            if(_kmk.stage == 0) {
                if((int32_t)(now - pt.deadline) >= 0) { // chooser stalled: pick for them
                    _kmk.cLabel[0] = 0;
                    _kmk.cLabel[1] = 1;
                    _kmk.cLabel[2] = 2;
                    _kmk.stage = 1;
                    pt.deadline = now + (uint32_t)KMK_GUESS_SECS * 1000;
                    pushAll();
                }
            } else {
                if((int32_t)(now - pt.deadline) >= 0 || kmkAllGuessed()) kmkReveal(now);
            }
        } else if(pt.phase == 3) {
            if((int32_t)(now - pt.revealUntil) >= 0) kmkNextRound(now);
        }
    }

    // Emit a player's K/M/K labels for the three people as an array of 0/1/2/-1.
    String kmkLabelsJson(const int8_t* lab) {
        String s = "[";
        for(int i = 0; i < 3; i++) {
            if(i) s += ",";
            s += (int)lab[i];
        }
        s += "]";
        return s;
    }

    String kmkJson(uint8_t pid) {
        Party& pt = _kmk.pt;
        if(pt.phase == 0) {
            String s = String("{\"t\":\"kmk\",\"phase\":\"lobby\",\"you\":") + pid +
                       ",\"players\":" + partyPlayersJson(pt);
            s += ",\"packs\":[";
            int votes[TRIVIA_MAX_TOPICS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _kmk.vote[i] >= 0 && _kmk.vote[i] < _kmk.packCount)
                    votes[_kmk.vote[i]]++;
            for(int i = 0; i < _kmk.packCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_kmk.packs[i].name.c_str()) +
                     "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_kmk.vote[pid]) + "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"kmk\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"kmk\",\"phase\":\"final\",\"board\":") + triviaBoard() + "}";

        WordPack& pk = _kmk.packs[_kmk.pack];
        bool me = (pid == _kmk.chooser);
        bool reveal = (pt.phase == 3);
        const char* stage = reveal ? "reveal" : (_kmk.stage == 0 ? "choose" : "guess");

        String s = String("{\"t\":\"kmk\",\"phase\":\"play\",\"stage\":\"") + stage +
                   "\",\"round\":" + pt.round + ",\"rounds\":" + KMK_ROUNDS + ",\"chooser\":\"" +
                   ha_json_escape(_p[_kmk.chooser].nick) + "\",\"iam\":" + (me ? "true" : "false") +
                   ",\"people\":[";
        for(int i = 0; i < 3; i++) {
            if(i) s += ",";
            s += "\"" + ha_json_escape(pk.words[_kmk.person[i]].c_str()) + "\"";
        }
        s += "]";
        // The chooser sees their own picks during the guess stage; on reveal everyone
        // sees the chooser's actual assignment.
        if(reveal || (me && _kmk.stage == 1))
            s += ",\"answer\":" + kmkLabelsJson(_kmk.cLabel);
        if(!me) s += ",\"mine\":" + kmkLabelsJson(_kmk.gLabel[pid]);
        if(reveal) {
            s += ",\"guesses\":[";
            bool first = true;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                if(!_p[i].used || i == _kmk.chooser || !_kmk.guessed[i]) continue;
                if(!first) s += ",";
                first = false;
                s += "{\"nick\":\"" + ha_json_escape(_p[i].nick) + "\",\"pick\":" +
                     kmkLabelsJson(_kmk.gLabel[i]) + ",\"pts\":" + _kmk.gained[i] + "}";
            }
            s += "],\"mygain\":" + String(_kmk.gained[pid]);
            s += ",\"deadline\":" + String(pt.revealUntil) + ",\"dur\":" +
                 String(KMK_REVEAL_MS / 1000);
        } else {
            s += ",\"deadline\":" + String(pt.deadline) + ",\"dur\":" +
                 String(_kmk.stage == 0 ? KMK_CHOOSE_SECS : KMK_GUESS_SECS);
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- Secrets (hidden yes/no vote + prediction) ----------
    // Which pack wins the pre-round vote; identical policy to wyrWinningPack().
    int secretsWinningPack() {
        if(_secrets.packCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _secrets.vote[i] >= 0 && _secrets.vote[i] < _secrets.packCount) {
                votes[_secrets.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_secrets.packCount);
        int best = 0;
        for(int i = 1; i < _secrets.packCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _secrets.packCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void secretsClear() {
        partyClear(_secrets.pt);
        _secrets.pack = 0;
        _secrets.question = 0;
        _secrets.qSeq = 0;
        _secrets.stage = 0;
        _secrets.yesCount = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _secrets.vote[i] = -1;
            _secrets.predict[i] = -1;
            _secrets.answer[i] = -1;
            _secrets.gained[i] = 0;
        }
    }

    void secretsReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_SECRETS) return;
        if(_secrets.pt.phase != 0 && _secrets.pt.phase != 4) return;
        if(_secrets.pt.phase == 4 && val) secretsClear(); // ready from final -> new game
        _secrets.pt.ready[pid] = val;
        secretsCheckStart();
        pushAll();
    }

    void secretsVote(uint8_t pid, int pack) {
        if(_active != HA_GAME_SECRETS || _secrets.pt.phase != 0) return;
        if(pack < 0 || pack >= _secrets.packCount) return;
        _secrets.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void secretsCheckStart() {
        if(_secrets.packCount == 0) return;
        Party& pt = _secrets.pt;
        if(pt.phase == 0 && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !partyAllReady(pt)) {
            pt.phase = 0;
        }
    }

    bool secretsAllPredicted() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(_secrets.predict[i] < 0) return false;
        }
        return n >= 1;
    }

    bool secretsAllAnswered() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            n++;
            if(_secrets.answer[i] < 0) return false;
        }
        return n >= 1;
    }

    void secretsNextRound(uint32_t now) {
        Party& pt = _secrets.pt;
        WordPack& pk = _secrets.packs[_secrets.pack];
        if(pt.round >= SECRETS_ROUNDS || pk.count == 0) {
            pt.phase = 4; // final
            pushAll();
            return;
        }
        pt.round++;
        _secrets.question = (uint8_t)(_secrets.qSeq % pk.count);
        _secrets.qSeq++;
        _secrets.stage = 0; // answer first, then predict
        _secrets.yesCount = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _secrets.predict[i] = -1;
            _secrets.answer[i] = -1;
            _secrets.gained[i] = 0;
        }
        pt.deadline = now + (uint32_t)SECRETS_ANSWER_SECS * 1000;
        pt.phase = 2;
        pushAll();
    }

    void secretsToPredict(uint32_t now) {
        _secrets.stage = 1; // answers are in; now guess how many said yes
        _secrets.pt.deadline = now + (uint32_t)SECRETS_PREDICT_SECS * 1000;
        pushAll();
    }

    void secretsReply(uint8_t pid, int v) {
        if(_active != HA_GAME_SECRETS || _secrets.pt.phase != 2 || _secrets.stage != 0) return;
        if(v != 0 && v != 1) return;
        _secrets.answer[pid] = (int8_t)v;
        if(secretsAllAnswered()) secretsToPredict(millis());
        else pushAll();
    }

    void secretsPredict(uint8_t pid, int n) {
        if(_active != HA_GAME_SECRETS || _secrets.pt.phase != 2 || _secrets.stage != 1) return;
        int cap = connectedCount(); // predictions range 0..N (N = joined players)
        if(n < 0) n = 0;
        if(n > cap) n = cap;
        _secrets.predict[pid] = (int8_t)n;
        if(secretsAllPredicted()) secretsReveal(millis());
        else pushAll();
    }

    // Score per player: an exact prediction of the group yes-count earns 1, otherwise 0.
    // A player who never predicted (predict < 0) scores nothing.
    void secretsReveal(uint32_t now) {
        int yes = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _secrets.answer[i] == 1) yes++;
        _secrets.yesCount = yes;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            int pred = _secrets.predict[i];
            // Exact guesses only. Rewarding "off by one" as well made the reveal
            // fiddly to read (two kinds of winner, two point values) for very little
            // play value, so a prediction either nails the group's yes-count or it
            // scores nothing.
            int pts = (pred >= 0 && pred == yes) ? 1 : 0;
            _secrets.gained[i] = pts;
            if(pts) {
                _p[i].score += pts;
                haUartScore(i, pts, "secrets");
            }
        }
        haUartRoundResult(String("{\"secrets\":\"round ") + _secrets.pt.round + "\"}");
        _secrets.pt.phase = 3;
        _secrets.pt.revealUntil = now + SECRETS_REVEAL_MS;
        pushAll();
    }

    void secretsAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_SECRETS || _secrets.pt.phase != 4) return;
        secretsClear();
        pushAll();
    }

    void secretsTick(uint32_t now) {
        Party& pt = _secrets.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                _secrets.pack = (uint8_t)secretsWinningPack();
                _secrets.qSeq = 0;
                secretsNextRound(now);
            }
        } else if(pt.phase == 2) {
            if(_secrets.stage == 0) {
                // Answer window expired: move to predicting anyway so a silent player
                // can't stall the round (a missing answer just counts as no).
                if((int32_t)(now - pt.deadline) >= 0 || secretsAllAnswered())
                    secretsToPredict(now);
            } else {
                // Predict window expired: reveal anyway (missing predictions score 0).
                if((int32_t)(now - pt.deadline) >= 0 || secretsAllPredicted())
                    secretsReveal(now);
            }
        } else if(pt.phase == 3) {
            if((int32_t)(now - pt.revealUntil) >= 0) secretsNextRound(now);
        }
    }

    // Anonymity is enforced here. A round runs answer -> predict -> reveal. Each player's
    // individual yes/no ANSWER is never serialized to anyone, in any phase — only the group
    // yes-count, and only on reveal. Predictions are guesses about the group (not personal),
    // so at reveal every player's prediction + points are exposed in "guesses"; before then
    // only the player's own prediction/answer and aggregate progress counts leave this method.
    String secretsJson(uint8_t pid) {
        Party& pt = _secrets.pt;
        if(pt.phase == 0) {
            String s = String("{\"t\":\"secrets\",\"phase\":\"lobby\",\"you\":") + pid +
                       ",\"players\":" + partyPlayersJson(pt);
            s += ",\"packs\":[";
            int votes[TRIVIA_MAX_TOPICS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _secrets.vote[i] >= 0 && _secrets.vote[i] < _secrets.packCount)
                    votes[_secrets.vote[i]]++;
            for(int i = 0; i < _secrets.packCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_secrets.packs[i].name.c_str()) +
                     "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_secrets.vote[pid]) + "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"secrets\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"secrets\",\"phase\":\"final\",\"board\":") + triviaBoard() +
                   "}";

        WordPack& pk = _secrets.packs[_secrets.pack];
        const char* q = pk.words[_secrets.question].c_str();
        int total = connectedCount(); // number of players (also the predict upper bound)
        bool reveal = (pt.phase == 3);
        const char* phase = reveal ? "reveal" : (_secrets.stage == 0 ? "answer" : "predict");
        // Aggregate progress only: how many have locked in the current step (answers while
        // answering, predictions while predicting). This never exposes an individual's pick.
        int locked = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(!reveal && _secrets.stage == 0) {
                if(_secrets.answer[i] >= 0) locked++;
            } else if(_secrets.predict[i] >= 0)
                locked++;
        }

        String s = String("{\"t\":\"secrets\",\"phase\":\"") + phase + "\",\"round\":" +
                   pt.round + ",\"rounds\":" + SECRETS_ROUNDS + ",\"n\":" + total +
                   ",\"q\":\"" + ha_json_escape(q) + "\",\"locked\":" + locked +
                   ",\"total\":" + total;
        // Your own prediction/answer are yours to see; nobody else's.
        s += ",\"myprediction\":";
        s += (int)_secrets.predict[pid];
        s += ",\"myanswer\":";
        s += (int)_secrets.answer[pid];
        if(reveal) {
            // Only the group total is revealed, never who answered what. Predictions are
            // guesses about the group, so every player's prediction + points are listed.
            s += ",\"yes\":";
            s += _secrets.yesCount;
            s += ",\"guesses\":[";
            bool first = true;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                if(!_p[i].used) continue;
                if(!first) s += ",";
                first = false;
                // pid too: the reveal marks *your* row, and nicknames can collide.
                s += "{\"pid\":" + String((int)i) + ",\"nick\":\"" +
                     ha_json_escape(_p[i].nick) + "\",\"n\":" + (int)_secrets.predict[i] +
                     ",\"pts\":" + _secrets.gained[i] + "}";
            }
            s += "]";
            s += ",\"mygain\":";
            s += _secrets.gained[pid];
            s += ",\"deadline\":";
            s += pt.revealUntil;
            s += ",\"dur\":";
            s += (SECRETS_REVEAL_MS / 1000);
        } else {
            s += ",\"deadline\":";
            s += pt.deadline;
            s += ",\"dur\":";
            s += (_secrets.stage == 0 ? SECRETS_ANSWER_SECS : SECRETS_PREDICT_SECS);
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- game-change vote (cross-cutting, above the active game) ----------
    // Name -> id, the inverse of gameName(). "none" is a legitimate target (back to the
    // plain lobby), so HA_GAME_NONE can't double as the not-found marker: returns -1 for
    // an unknown name instead.
    static int gameIdByName(const char* name) {
        if(!name || !name[0]) return -1;
        for(uint8_t id = HA_GAME_NONE; id <= HA_GAME_SECRETS; id++)
            if(strcmp(gameName(id), name) == 0) return (int)id;
        return -1;
    }

    void gameVoteClear() {
        _gvActive = false;
        _gvProposer = 0;
        _gvTarget = 0;
        _gvStart = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _gvVote[i] = -1;
    }

    // A player proposes switching the active game. Only one proposal at a time, and only to
    // a different, valid target -- which includes "none", i.e. back to the plain lobby. The
    // proposer counts as an implicit YES. This is the single sanctioned phone->host action;
    // a host-initiated select still bypasses the vote.
    void proposeGame(uint8_t pid, const char* name) {
        if(_gvActive) return; // one proposal at a time
        int id = gameIdByName(name);
        if(id < 0 || (uint8_t)id == _active) return; // unknown, or already the active game
        _gvActive = true;
        _gvProposer = pid;
        _gvTarget = (uint8_t)id;
        _gvStart = millis();
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _gvVote[i] = -1;
        _gvVote[pid] = 1; // the proposer is an implicit YES
        if(!gameVoteResolve(millis())) pushAll(); // resolves at once if the proposer is alone
    }

    void voteGame(uint8_t pid, bool ok) {
        if(!_gvActive) return;
        if(pid == _gvProposer) {
            // The proposer's YES is implicit, so an OK from them means nothing -- but a NO is
            // how they withdraw: cancel the proposal and resume the frozen game at once.
            if(!ok) gameVoteReject();
            return;
        }
        _gvVote[pid] = ok ? 1 : 0;
        if(!gameVoteResolve(millis())) pushAll();
    }

    // Resolve the pending vote. Approve on a strict majority of the OTHER players (the
    // proposer excluded), or immediately if the proposer is the only player. Reject as soon
    // as that majority is impossible, or on timeout. Returns true if it resolved (having
    // already pushed the resulting state), false if the proposal is still open.
    bool gameVoteResolve(uint32_t now) {
        if(!_gvActive) return false;
        int others = 0, yes = 0, no = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || i == _gvProposer) continue;
            others++;
            if(_gvVote[i] == 1) yes++;
            else if(_gvVote[i] == 0) no++;
        }
        if(others <= 0 || yes * 2 > others) { // proposer alone, or a strict majority says yes
            gameVoteApprove();
            return true;
        }
        if(no * 2 >= others || // approval is now impossible ...
           (int32_t)(now - _gvStart) >= (int32_t)(GAMEVOTE_SECS * 1000)) { // ... or timed out
            gameVoteReject();
            return true;
        }
        return false;
    }

    void gameVoteApprove() {
        uint8_t target = _gvTarget;
        haUartEvent(String("{\"gamevote\":\"approved\",\"game\":\"") + gameName(target) + "\"}");
        gameVoteClear();
        selectGame(target); // resets to the target game's lobby and pushAll()s
    }

    void gameVoteReject() {
        gameVoteClear();
        pushAll(); // resume the frozen game (its state was left untouched)
    }

    String gameVoteJson(uint8_t pid) {
        int yes = 0, no = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(_gvVote[i] == 1) yes++; // includes the proposer's implicit YES
            else if(_gvVote[i] == 0) no++;
        }
        int others = connectedCount() - 1;
        if(others < 0) others = 0;
        int need = others > 0 ? (others / 2 + 1) : 0; // yes votes needed from the others
        const char* name = gameName(_gvTarget);
        // The voters' line leads with the proposer's avatar, so it ships alongside the nick.
        String s = String("{\"t\":\"gamevote\",\"proposer\":\"") +
                   ha_json_escape(_p[_gvProposer].nick) + "\",\"avatar\":\"" +
                   ha_json_escape(_p[_gvProposer].avatar) + "\",\"game\":\"" + name +
                   "\",\"label\":\"" + name + "\",\"yes\":" + yes + ",\"no\":" + no +
                   ",\"others\":" + others + ",\"need\":" + need + ",\"youproposed\":" +
                   (pid == _gvProposer ? "true" : "false") + ",\"youvoted\":" +
                   (_gvVote[pid] >= 0 ? "true" : "false") + "}";
        return s;
    }
};
