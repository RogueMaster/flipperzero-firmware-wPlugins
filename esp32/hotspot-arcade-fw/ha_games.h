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

#define TRIVIA_MAX_TOPICS 6
#define TRIVIA_MAX_QS 20
#define PACK_MAX_ITEMS 32 // items in a word/prompt pack (wyr/scramble/draw)
#define TRIVIA_QDUR 20 // seconds per question (safety timer)
#define TRIVIA_COUNTDOWN 3 // seconds after all-ready before the first question
#define TRIVIA_REVEAL_MS 4000 // pause on the reveal before the next question

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

#define GC_ROUNDS 5
#define GC_PLAY_SECS 25 // safety deadline per color
#define GC_REVEAL_MS 6000
#define GC_SPEED_MS 12000 // speed bonus decays to 0 over this window

// ---- sinks implemented in the .ino ----
void haWsSendWs(uint32_t wsId, const String& msg); // to one socket (0 = no-op)
void haWsBroadcast(const String& msg); // to all connected sockets
void haUartJoin(uint8_t pid, const char* nick);
void haUartLeave(uint8_t pid);
void haUartScore(uint8_t pid, int delta, const char* reason);
void haUartEvent(const String& json);
void haUartRoundResult(const String& json);

struct Player {
    bool used;
    uint32_t wsId; // 0 = not connected
    char nick[HA_NICK_LEN];
    char avatar[8]; // emoji avatar (UTF-8), player-picked on the landing screen
    int32_t score;
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

class Engine {
public:
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
    }

    // ---- roster ----
    uint8_t pidByWs(uint32_t wsId) {
        if(!wsId) return 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _p[i].wsId == wsId) return i;
        return 0;
    }

    void onWsDisconnect(uint32_t wsId) {
        uint8_t pid = pidByWs(wsId);
        if(!pid) return;
        anyOnLeave(pid); // forfeit any active match
        _p[pid] = Player{};
        haUartLeave(pid);
        triviaOnRosterChange();
        partyRosterChanged();
        pushAll();
    }

    void onHello(uint32_t wsId, const char* nick, const char* avatar) {
        uint8_t pid = pidByWs(wsId);
        if(!pid) {
            pid = freePid();
            if(!pid) return; // full
            _p[pid].used = true;
            _p[pid].wsId = wsId;
            _p[pid].score = 0;
            strlcpy(_p[pid].nick, (nick && nick[0]) ? nick : "PLAYER", HA_NICK_LEN);
            ha_upper(_p[pid].nick);
            strlcpy(_p[pid].avatar, (avatar && avatar[0]) ? avatar : "\xF0\x9F\x99\x82", sizeof(_p[pid].avatar));
            haUartJoin(pid, _p[pid].nick);
        } else {
            // Re-hello from a known socket = the player changed their name/avatar in
            // the header editor. Re-announce over UART so the Flipper's leaderboard
            // updates (player_join there updates an existing pid's nick in place).
            if(nick && nick[0]) {
                strlcpy(_p[pid].nick, nick, HA_NICK_LEN);
                ha_upper(_p[pid].nick);
                haUartJoin(pid, _p[pid].nick);
            }
            if(avatar && avatar[0]) strlcpy(_p[pid].avatar, avatar, sizeof(_p[pid].avatar));
        }
        String w = String("{\"t\":\"welcome\",\"pid\":") + pid + ",\"nick\":\"" +
                   ha_json_escape(_p[pid].nick) + "\"}";
        haWsSendWs(wsId, w);
        triviaOnRosterChange();
        partyRosterChanged();
        pushAll();
    }

    // ---- host (Flipper) driven ----
    void selectGame(uint8_t id) {
        _active = id;
        triviaClear();
        duelClear();
        drawClear();
        pongClear();
        wyrClear();
        scrambleClear();
        reactClear();
        spectrumClear();
        kmkClear();
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
        pushAll();
    }

    // Time-based updates (trivia phases, drawing timers, pong physics). From loop().
    void tick(uint32_t now) {
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
    }

    // ---- player input (parsed WS JSON) ----
    void onInput(uint32_t wsId, const char* json) {
        char type[20];
        if(!ha_json_str(json, "t", type, sizeof(type))) return;
        if(strcmp(type, "hello") == 0) {
            char nick[HA_NICK_LEN], avatar[8];
            ha_json_str(json, "nick", nick, sizeof(nick));
            if(!ha_json_str(json, "avatar", avatar, sizeof(avatar))) avatar[0] = '\0';
            onHello(wsId, nick, avatar);
            return;
        }
        if(strcmp(type, "ping") == 0) {
            haWsSendWs(wsId, "{\"t\":\"pong\"}");
            return;
        }
        uint8_t pid = pidByWs(wsId);
        if(!pid) return;
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
        } else if(strcmp(type, "vote") == 0 && ha_json_int(json, "topic", &v)) {
            triviaVote(pid, v);
        } else if(strcmp(type, "vote") == 0 && ha_json_int(json, "pack", &v)) {
            wyrVote(pid, v);
            scrambleVote(pid, v);
            spectrumVote(pid, v);
            kmkVote(pid, v);
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
        } else if(strcmp(type, "again") == 0) {
            triviaAgain(pid);
            drawAgain(pid);
            wyrAgain(pid);
            scrambleAgain(pid);
            reactAgain(pid);
            gcAgain(pid);
            spectrumAgain(pid);
            kmkAgain(pid);
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
        } else if(strcmp(type, "rematch") == 0) {
            duelRematch(pid);
            battleRematch(pid);
        } else if(strcmp(type, "paddle") == 0 && ha_json_int(json, "dir", &v)) {
            pongPaddle(pid, v);
        } else if(strcmp(type, "place") == 0) {
            battlePlace(pid, json);
        } else if(strcmp(type, "fire") == 0 && ha_json_int(json, "n", &v)) {
            battleFire(pid, v);
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
        }
    }

private:
    Player _p[HA_MAX_PLAYERS + 1] = {};
    uint8_t _active = HA_GAME_NONE;
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

    // Challenge/accept are shared by all 1v1 games (duels + pong + battleship).
    bool isMatchGame() {
        return isDuel(_active) || _active == HA_GAME_PONG || _active == HA_GAME_BATTLESHIP;
    }
    bool inAnyMatch(uint8_t pid) {
        return matchOf(pid) || pongMatchOf(pid) || battleMatchOf(pid);
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
        } else {
            for(int i = 0; i < DUEL_MAX_MATCHES; i++)
                if(!_m[i].used) {
                    duelStart(&_m[i], from, pid, from); // challenger moves first
                    break;
                }
        }
        duelRemoveChallengesInvolving(pid);
        duelRemoveChallengesInvolving(from);
        const char* key = (_active == HA_GAME_PONG)      ? "pong" :
                          (_active == HA_GAME_BATTLESHIP) ? "bs" :
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
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_p[i].wsId) continue;
            bool peer;
            if(dm)
                peer = (i == dm->a || i == dm->b);
            else if(pm)
                peer = (i == pm->a || i == pm->b);
            else if(bm)
                peer = (i == bm->a || i == bm->b);
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
                    s += (int)strlen(_d.word);
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
    void scrambleMake(char* dst, const char* src) {
        int len = (int)strlen(src);
        for(int attempt = 0; attempt < 8; attempt++) {
            strlcpy(dst, src, 24);
            for(int i = len - 1; i > 0; i--) {
                int j = (int)(esp_random() % (uint32_t)(i + 1));
                char t = dst[i];
                dst[i] = dst[j];
                dst[j] = t;
            }
            if(len < 2 || strcmp(dst, src) != 0) return;
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
};
