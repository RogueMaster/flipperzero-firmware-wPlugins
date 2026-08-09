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

#define TRIVIA_MAX_TOPICS 8 // raised from 6 (v19): ~47 KB freed by moving the web bundle to flash
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

// Fill the Blank (inspired by Cards Against Humanity; the shipped cards are our own):
// a prompt card with a blank, everyone but the rotating Czar plays one answer card from
// their hand face down, the played answers are shuffled and shown anonymously, and the
// Czar picks the winner for a point.
//
// Sizing: a pack here is two decks, not one, so this does NOT reuse PACK_MAX_ITEMS (32)
// or TRIVIA_MAX_TOPICS (6) — the answer deck needs well over 32 entries, but a judging
// game only ever plays one pack per session and the lobby vote strip stays readable at
// three, so the pack cap drops instead of the deck growing. The prompt deck only needs
// FB_ROUNDS entries plus slack (24); the answer deck has to keep every hand full
// (12 players x 6 = 72 cards out at the extreme, which is why the pile reshuffles used
// cards back in — see fillblankRefillDeck). 3 x (1 name + 24 + 56) = 243 Strings, about
// 2.9 KB of static state on the ESP (12 bytes per String) — less than WyrState's
// 6 x 65 = 390 (~4.7 KB), and ~3 KB cheaper than a 6-pack cap would have been.
#define FB_MAX_PACKS 3
#define FB_MAX_PROMPTS 24
#define FB_MAX_ANSWERS 56
#define FB_HAND 6 // answer cards held by each non-Czar player
#define FB_ROUNDS 6
#define FB_MIN_PLAYERS 2 // a Czar plus one answer; the deck pads the pile to FB_MIN_PILE
// With two players the Czar judges a single real submission, which would be no choice at
// all -- and worse, a certainty about who wrote it. Pad the pile with the deck's own
// anonymous cards up to this many, so the Czar always picks blind from a real spread.
#define FB_MIN_PILE 3
#define FB_PLAY_SECS 45 // submission window (safety timer)
#define FB_PICK_SECS 30 // the Czar's judging window (safety timer)
#define FB_REVEAL_MS 6000
// Every judging round also carries one answer card drawn at random from the deck, judged
// blind alongside the players'. It is marked in the pile by this sentinel author pid (0 is
// never a real pid) and scores nobody if the Czar picks it -- the deck beat the room.
#define FB_DECK_PID 0
// Real submissions cap at HA_MAX_PLAYERS - 1 (the Czar never plays), plus the deck's card.
#define FB_MAX_SUBS (HA_MAX_PLAYERS + 1)

// Werewolf: hidden-role social deduction. Roles are dealt at the start and live
// only on the ESP; the per-player serializer is what keeps them secret, so read
// wwJson() as the rulebook for who may know what. Phases run themselves on
// timers, so one distracted player can never stall the room -- anyone who does
// not act inside the window is simply skipped.
#define WW_MIN_PLAYERS 5 // fewer than this and the roles don't work
#define WW_DOCTOR_MIN 6 // a doctor joins the deal from this many players up
#define WW_QUIET_NIGHT_MAX 6 // at or below this many, night one takes nobody
#define WW_ROLES_SECS 12 // private "here is your role" window before night 1
// The night is a FIXED window and never ends early. A night that ended as soon
// as every special role had acted would leak how many are still alive -- the
// room would learn "that was quick, the seer must be dead". The day may end
// early, but only on a hammer (a strict majority), which is public anyway.
#define WW_NIGHT_SECS 60
// The day scales with the room: 60s of preamble plus 20s of airtime per living
// player, clamped. Eight alive lands at 220s, seven at 200s.
#define WW_DAY_BASE 60
#define WW_DAY_PER 20
#define WW_DAY_MIN 90
#define WW_DAY_MAX 240
#define WW_ANNOUNCE_MS 8000 // dawn (night result) / dusk (vote result) pause
#define WW_MAX_LOG 16 // nights kept for the end-of-game summary
// Roles. 0 = not in this game (joined mid-game, or left): a spectator.
#define WW_VILLAGER 1
#define WW_WOLF 2
#define WW_SEER 3
#define WW_DOCTOR 4
// Sub-phases inside Party::phase 2 (playing).
#define WW_S_ROLES 0
#define WW_S_NIGHT 1
#define WW_S_DAWN 2
#define WW_S_DAY 3
#define WW_S_DUSK 4
// How a night ended. The room must be able to tell these apart: with a doctor in
// play, "the attack was blocked" and "the wolves never hunted" are very
// different pieces of information.
#define WW_D_KILLED 0
#define WW_D_SAVED 1 // the doctor was shielding the wolves' target
#define WW_D_QUIET 2 // no wolf named a valid target
#define WW_D_NOKILL 3 // the small-table first night: no hunt at all

// Spyfall: everyone at the table shares a secret location and holds a role there,
// except one player -- the spy -- who is told neither. The room questions each other
// out loud; the phones are only the referee. The round ends when the talk timer runs
// out (everyone votes for the spy) or the moment the spy calls the location.
//
// Its content is bigger per entry than the one-line items PACK_MAX_ITEMS was sized for
// (a location carries several roles), so Spyfall declares its own caps rather than
// borrowing trivia's 6 packs x 32 items. Worst case is 3 x 14 x (1 name + 6 roles) =
// 294 Arduino Strings of static state, roughly 4.7 KB of String headers plus ~5 KB of
// heap for the text -- comfortably UNDER Would You Rather's 6 x 32 x 2 = 384 headers,
// so an ESP32-S2 pays no more for Spyfall than for a pack game it already runs.
// The round is driven by players PRESSING things, not by a clock: "I know the location"
// (spy only) and "I know the spy" (everyone, the spy included) end it at any moment. The
// six-minute clock is only the fallback, and running it out does not end the round -- it
// starts a round-robin nomination that the table has to actually resolve.
#define SPYFALL_MAX_PACKS 3
#define SPYFALL_MAX_LOCS 14
#define SPYFALL_MAX_ROLES 6
#define SPYFALL_ROUNDS 4 // a talking round is long, so fewer of them than the other party games
#define SPYFALL_MIN_PLAYERS 3 // two players make the spy trivially obvious
#define SPYFALL_CARD_SECS 30 // safety: a phone left in a pocket can't hold up the deal
#define SPYFALL_TALK_SECS 360 // 6 minutes of questioning (confirmed on hardware: leave it)
#define SPYFALL_HUSH_MS 4000 // "Time's up. Stop discussing!" beat before nominations
#define SPYFALL_NOM_SECS 30 // one player's turn to nominate
#define SPYFALL_POLL_SECS 20 // the "are you in?" window on a nomination
#define SPYFALL_REVEAL_MS 9000
// How a round ended (SpyfallState::outcome), and who it scores. Everything is worth
// exactly 1 point, so the shared leaderboard stays flat across all of them.
#define SPYFALL_OUT_CAUGHT 1 // the spy was named: the non-spies score
#define SPYFALL_OUT_ESCAPED 2 // an innocent was condemned, or nobody pinned the spy: spy scores
#define SPYFALL_OUT_SOLVED 3 // the spy called the location right: the spy scores
#define SPYFALL_OUT_FAILED 4 // the spy called it wrong: the non-spies score
#define SPYFALL_OUT_ABORT 5 // the spy left (or the table shrank): nobody scores

// Frankendraw (exquisite corpse): everyone starts a sheet and draws the head; the
// sheets then rotate one seat per round so the torso and the legs come from two other
// hands, and a drawer only ever sees a thin sliver of the panel above theirs.
//
// The sheet is a 0..255 square in both axes (FD_UNIT), split into FD_PANELS equal
// bands of FD_BAND -- 255 = 3 * 85 exactly, so the bands need no rounding. Strokes are
// stored quantised to those units: four bytes a segment, which is what keeps a whole
// gallery in a fixed block of .bss instead of the heap (see FrankenState).
//
// FD_PANEL_STROKES is the whole memory story: HA_MAX_PLAYERS * FD_PANELS * it * 4 bytes.
// At 192 that is 12 * 3 * 192 * 4 = 27.6 KB, up from 9 KB at the 64 it shipped with --
// 64 turned out to be about a third of one drawing. 192 is picked to stay comfortable on
// the smallest supported board: the ESP32-S2 has 320 KB of SRAM, most of which goes to
// WiFi/lwIP and the heap-held web bundle, and 28 KB of static state is a few per cent of
// it. Boards with more RAM (the C5, WROOM, a Cardputer host) have far more headroom, but
// this is one constant, shared, and sized for the tightest one. Raising it also grows the
// gallery message; that is why the picture is broadcast once per creature rather than
// pasted into every per-player push (see fdShowSheet).
#define FD_PANELS 3 // head, torso, legs
#define FD_UNIT 255 // sheet coordinate range, both axes
#define FD_BAND 85 // FD_UNIT / FD_PANELS: one panel's height
#define FD_OVERLAP 7 // sliver of the panel above that the next drawer sees (~8% of a band)
#define FD_PANEL_STROKES 192 // segments per panel; at the cap the pen stops (see fdStroke)
#define FD_MIN_PLAYERS 3 // a sheet has to pass through three different hands
#define FD_DRAW_SECS 75 // per panel (safety timer; "done" from everyone ends it early)
#define FD_SHOW_MS 5000 // gallery: how long each finished creature is on screen
#define FD_FINALE_MS 8000 // the winning creature, shown again at the end
#define FD_VOTE_POINTS 100 // per net thumb-up, to each of the sheet's three contributors

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
// Finished artwork, for the host to keep. Called once with HA_ART_BEGIN, then once per
// line segment with HA_ART_STROKE, then once with HA_ART_END -- a picture is streamed
// as it is handed over, never buffered, so this costs the engine no RAM at all. The
// Flipper turns the stream into one SVG file per sheet on its SD card.
void haUartArt(uint8_t op, const String& json);

struct Player {
    bool used;
    bool bot; // an engine-run fill player (no socket, no device); see botSync()
    uint32_t wsId; // 0 = not connected
    uint64_t deviceKey; // which phone, 0 = unknown (see onHello)
    char nick[HA_NICK_LEN];
    char avatar[8]; // emoji avatar (UTF-8), player-picked on the landing screen
    int32_t score;
};

// How many bot seats the testing switch may fill. Werewolf's minimum of 5 is the
// deepest hole a two-person room can be in, so 4 covers every game from one real
// player up; more would just crowd the roster.
#define HA_BOT_MAX 4

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
    // Content packs live in Engine::_dPacks / _dPackCount, kept out of the game-state union
    // (they hold Strings and are streamed for every game up front, so they stay resident).
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
    // Content packs live in Engine::_wyrPacks / _wyrPackCount, kept out of the game-state union.
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack index (locked in when the round starts)
    uint8_t promptSeq; // rotates prompts across rounds within the pack
    uint8_t prompt; // current prompt index within the chosen pack
    int8_t choice[HA_MAX_PLAYERS + 1]; // A/B vote for the current prompt, -1 = none
    // Per-round A/B split, latched at reveal, for the final "how much did we agree"
    // chart. The client cannot reconstruct this from what it saw: a phone that joined
    // late (or reloaded) never received the earlier rounds, so the engine has to carry
    // the history into the final payload.
    uint8_t splitA[WYR_ROUNDS], splitB[WYR_ROUNDS];
    uint8_t splitCount; // rounds latched so far (<= WYR_ROUNDS)
};

// Word scramble race: everyone unscrambles the same word; fastest correct win most.
struct ScrambleState {
    Party pt;
    uint16_t wordSeq;
    char word[24]; // the answer
    char scram[24]; // shown (letters shuffled)
    bool solved[HA_MAX_PLAYERS + 1];
    uint8_t solvedCount;
    // Content packs live in Engine::_scrPacks / _scrPackCount, kept out of the game-state union.
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
    // Content packs live in Engine::_specPacks / _specPackCount, kept out of the game-state union.
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
    // Content packs live in Engine::_kmkPacks / _kmkPackCount, kept out of the game-state union.
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
    // Content packs live in Engine::_secretsPacks / _secretsPackCount, kept out of the game-state union.
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

// Fill the Blank content: one pack is two decks. A pack file's items carry either a
// `P` key (a prompt card, containing the _____ blank) or an `A` key (an answer card),
// and fillblankLoadItem files each into the matching list.
struct FillBlankPack {
    String name;
    String prompts[FB_MAX_PROMPTS];
    String answers[FB_MAX_ANSWERS];
    uint8_t pcount;
    uint8_t acount;
};

// Fill the Blank: the Party skeleton plus a per-player hand and a shuffled submission
// pile. Within a playing round: stage 0 = everyone but the Czar plays a card face down,
// stage 1 = the shuffled submissions are shown anonymously and only the Czar may pick.
struct FillBlankState {
    Party pt;
    // Content packs live in Engine::_fbPacks / _fbPackCount, kept out of the game-state union.
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack (locked when the game starts)
    uint16_t promptSeq; // rotates the prompt card across rounds
    uint8_t prompt; // current prompt index within the pack
    uint8_t czar; // pid judging this round
    uint8_t czarSeq; // rotates the Czar across rounds
    uint8_t stage; // 0 play, 1 judge
    // Answer draw pile: a shuffled permutation of answer indices, consumed by `drawNext`.
    // When it runs dry the used cards are shuffled back in (minus what is still in hand).
    uint8_t deck[FB_MAX_ANSWERS];
    uint8_t deckLen;
    uint8_t drawNext;
    int8_t hand[HA_MAX_PLAYERS + 1][FB_HAND]; // answer index per slot, -1 = empty
    bool inRound[HA_MAX_PLAYERS + 1]; // was here when the round dealt (mid-game joiners wait)
    // Hand slot played this round, -1 = none. The card STAYS in the hand (the client marks
    // it and greys the rest); it is discarded and redrawn when the next round deals, so a
    // player can always see what they committed to.
    int8_t played[HA_MAX_PLAYERS + 1];
    // Submission pile, shuffled before judging. `subPid` never leaves the ESP before the
    // Czar has picked; fillblankJson only emits the card text until then.
    // subPid == FB_DECK_PID marks the deck's own random card, which is judged like any
    // other but scores nobody. One extra slot for it on top of the real submissions.
    uint8_t subPid[FB_MAX_SUBS];
    uint8_t subCard[FB_MAX_SUBS];
    uint8_t subCount;
    int8_t picked; // index into the shuffled pile the Czar chose, -1 = none
    uint8_t winner; // pid who won the round, 0 = none (deck won, or round aborted)
    bool deckWon; // the Czar picked the deck's card: nobody scores
    uint8_t czarGain; // 1 when the Czar picked a real player's card, else 0
};

// Werewolf: hidden roles on the shared Party skeleton. No content packs -- the
// roles are code. Party::round counts nights (day 1 follows night 1), and
// Party::phase 2 is subdivided by `stage` (WW_S_*) into roles -> night -> dawn ->
// day -> dusk -> night -> ...
//
// EVERY field below is secret by default. wwJson() is the only place a role
// leaves the engine, and it applies exactly one rule (wwMaySeeRole): you always
// see your own role, werewolves see each other, a dead player's role is public,
// and at game end everything opens up.
// One line of the end-of-game summary: what each night and the day after it did.
struct WwDay {
    uint8_t victim; // pid taken that night, 0 = nobody
    uint8_t kind; // WW_D_*, why nobody died when victim is 0
    uint8_t lynched; // pid voted out that day, 0 = nobody
};

struct WerewolfState {
    Party pt;
    uint8_t stage; // WW_S_*
    uint8_t role[HA_MAX_PLAYERS + 1]; // WW_VILLAGER / WOLF / SEER / DOCTOR, 0 = spectator
    bool alive[HA_MAX_PLAYERS + 1];
    bool revealed[HA_MAX_PLAYERS + 1]; // role is public (died, or the game ended)
    int8_t kill[HA_MAX_PLAYERS + 1]; // a wolf's night target pid, -1 = not picked
    int8_t accuse[HA_MAX_PLAYERS + 1]; // a player's day vote pid, -1 = not voted
    uint8_t seer; // the seer's pid this game, 0 = none left
    uint8_t seerTarget; // who the seer checked this night, 0 = nobody yet
    bool seerResult; // ...and whether they are a werewolf. Seer's payload only.
    uint8_t doctor; // the doctor's pid this game, 0 = none dealt / none left
    uint8_t docTarget; // who the doctor is shielding tonight. Doctor's payload only.
    uint8_t docLast; // and last night's shield, which may not be repeated
    uint8_t dealt; // how many players were dealt in (drives the quiet first night)
    uint8_t victim; // pid the wolves took last night, 0 = nobody died
    uint8_t dawnKind; // WW_D_*: how last night actually ended
    uint8_t lynched; // pid the village voted out today, 0 = nobody
    uint8_t winner; // 0 undecided, WW_VILLAGER = village, WW_WOLF = wolves
    WwDay log[WW_MAX_LOG];
    uint8_t logN;
};

// Spyfall content: one location and the handful of roles played at it. Its own pack
// type (not WordPack/WyrPack) because an entry is a name plus a list, and its own
// caps -- see the SPYFALL_* block above for the memory reasoning.
struct SpyLoc {
    String name;
    String roles[SPYFALL_MAX_ROLES];
    uint8_t roleCount;
};
struct SpyPack {
    String name;
    SpyLoc locs[SPYFALL_MAX_LOCS];
    uint8_t count;
};

// Spyfall: reuses the Party lobby/countdown/reveal skeleton. A playing round walks
// stage 0 (everyone reads their card and taps OK -- the clock only starts once they
// have) -> stage 1 (six minutes of questioning, ended at any moment by a button) ->
// stage 2 (the clock ran out: a round-robin nomination the table must resolve).
// Everything secret lives here and is filtered per player in spyfallJson() -- the
// location index is never serialized, only the resolved name, and only to someone
// allowed to see it.
struct SpyfallState {
    Party pt;
    // Content packs live in Engine::_sfPacks / _sfPackCount, kept out of the game-state union.
    int8_t vote[HA_MAX_PLAYERS + 1]; // pack index, -1 = not voted
    uint8_t pack; // chosen pack (locked when the game starts)
    uint16_t locSeq; // rotates the location across rounds
    uint8_t loc; // this round's location index within the chosen pack
    uint8_t spy; // pid of the spy this round
    uint8_t spySeq; // rotates the spy across rounds
    uint8_t stage; // 0 card, 1 talk, 2 nominate
    uint8_t nomStage; // within stage 2: 0 hush, 1 pick, 2 poll
    bool inRound[HA_MAX_PLAYERS + 1]; // dealt in at round start; joiners wait it out
    int8_t role[HA_MAX_PLAYERS + 1]; // role index at the location, -1 = spy / not dealt in
    bool seen[HA_MAX_PLAYERS + 1]; // acknowledged their card (stage 0)
    bool spent[HA_MAX_PLAYERS + 1]; // burnt their one "I know the spy" this round
    bool nominated[HA_MAX_PLAYERS + 1]; // has taken their round-robin turn
    uint8_t nominator; // whose turn it is to nominate, 0 = none yet
    uint8_t nominee; // who they nominated, 0 = still choosing
    int8_t agree[HA_MAX_PLAYERS + 1]; // poll answer: -1 unanswered, 0 no, 1 in
    uint8_t missBy[HA_MAX_PLAYERS]; // failed accusations this round: who pressed...
    uint8_t missOf[HA_MAX_PLAYERS]; // ...and who they got wrong
    uint8_t missCount;
    uint8_t outcome; // SPYFALL_OUT_*, set on reveal
    int8_t called; // location index the spy called, -1 = they never did
    uint8_t blamed; // pid the round ended on, 0 = nobody was pinned
    int gained[HA_MAX_PLAYERS + 1]; // points earned this round (shown on reveal)
};

// Frankendraw: one line segment of a panel, quantised to the 0..255 sheet grid.
struct FdStroke {
    uint8_t x0, y0, x1, y1;
};

// One sheet: three panels, each drawn by a different player. `who` keeps a copy of the
// contributor's nickname because a player can disconnect before the gallery runs, and
// the reveal (and the saved SVG) must still credit them.
struct FdSheet {
    uint8_t by[FD_PANELS]; // pid that drew each panel, 0 = nobody did
    char who[FD_PANELS][HA_NICK_LEN];
    uint8_t n[FD_PANELS]; // segments stored in each panel
    FdStroke s[FD_PANELS][FD_PANEL_STROKES];
};

// Frankendraw state. Sized for the worst case up front (HA_MAX_PLAYERS sheets of
// FD_PANELS * FD_PANEL_STROKES segments, see the constants above) and never grown: the
// ESP32-S2 has no room for an open-ended per-drawing buffer, so a panel's budget is
// fixed, the drawer is shown it running out, and finished sheets leave RAM through the
// artwork sink as they are shown.
//
// `seat` freezes the table order when the game starts. In round r (1-based), seat k
// holds sheet (k + seats - (r-1)) % seats -- one seat of rotation per round, so with
// seats >= FD_MIN_PLAYERS every sheet passes through three different hands.
struct FrankenState {
    Party pt;
    uint8_t seat[HA_MAX_PLAYERS]; // seat index -> pid, frozen at game start
    uint8_t seats; // seats == sheets in play
    // The per-sheet stroke store (~28 KB: HA_MAX_PLAYERS sheets, each FD_PANELS panels of
    // FD_PANEL_STROKES segments) is NOT held here -- it would sit in static DRAM and it is what
    // overflowed the ESP32-S2's dram0 segment. It lives in Engine::_fdSheets, allocated at
    // runtime (PSRAM when the board has it, plain heap otherwise) only while Frankendraw is the
    // active game. Indexed identically: _fdSheets[sheet].
    bool done[HA_MAX_PLAYERS + 1]; // tapped Next for this panel
    uint8_t stage; // phase 3: 0 gallery walk, 1 finale
    uint8_t show; // which sheet is on screen
    int8_t thumb[HA_MAX_PLAYERS + 1][HA_MAX_PLAYERS]; // per player, per sheet: -1/0/+1
    int8_t artSent[HA_MAX_PLAYERS + 1]; // last sheet whose picture that socket has, -1 none
    uint8_t best; // winning sheet after the tally
    int bestNet; // its thumbs up minus thumbs down
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
        gsZero();          // all game runtime state back to zero (no active game left to re-default)
        challengesClear(); // shared 1v1 challenge list is outside the union -> clear it here
        gameVoteClear();
        fdSheetsFree();    // no game active after a reset -> release the stroke store
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

    // The current game, advertised in the PING beacon so the Flipper can mirror it -- this
    // reflects phone-vote changes reliably even when a one-off EVENT wouldn't reach it.
    uint8_t activeGame() const { return _active; }

    // ---- host (Flipper) driven ----
    // A host-initiated select is authoritative and immediate: it also cancels any pending
    // phone game-change vote (gameVoteClear). Phone-initiated changes go through the vote,
    // which calls this only on approval.
    void selectGame(uint8_t id) {
        gameVoteClear();
        _active = id;
        gsZero();          // wipe every game's bytes; only the incoming game's clear sets defaults
        challengesClear(); // shared 1v1 challenge list is outside the union -> clear it here
        // Trivia's clear zeroed scores on every switch; now that only the incoming clear runs,
        // do it here so switching to any game still resets the scoreboard (see triviaClear).
        resetScoresAll();
        // Hold the ~28 KB stroke store only while Frankendraw is the active game. Ensure it
        // before fdClear() runs (inside dispatchClear) so its sheet wipe has a buffer.
        if(id == HA_GAME_FRANKENDRAW)
            fdSheetsEnsure();
        else
            fdSheetsFree();
        dispatchClear(id); // reset only the incoming game to its lobby
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
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _wyrPacks[i] = WyrPack{};
        _wyrPackCount = 0;
        // Fully reset the pack arrays -- not just packCount -- or a stale item
        // count survives a re-clear that doesn't load a replacement pack.
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _scrPacks[i] = WordPack{};
        _scrPackCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _dPacks[i] = WordPack{};
        _dPackCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _specPacks[i] = WyrPack{};
        _specPackCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _kmkPacks[i] = WordPack{};
        _kmkPackCount = 0;
        for(int i = 0; i < TRIVIA_MAX_TOPICS; i++) _secretsPacks[i] = WordPack{};
        _secretsPackCount = 0;
        for(int i = 0; i < FB_MAX_PACKS; i++) _fbPacks[i] = FillBlankPack{};
        _fbPackCount = 0;
        for(int i = 0; i < SPYFALL_MAX_PACKS; i++) _sfPacks[i] = SpyPack{};
        _sfPackCount = 0;
        _packGame = 0;
    }

    void contentPack(uint8_t game, const char* name) {
        _packGame = game;
        if(game == HA_GAME_TRIVIA) {
            triviaAddTopic(name);
        } else if(game == HA_GAME_WYR) {
            if(_wyrPackCount < TRIVIA_MAX_TOPICS) {
                _wyrPacks[_wyrPackCount] = WyrPack{};
                _wyrPacks[_wyrPackCount].name = name;
                _wyrPackCount++;
            }
        } else if(game == HA_GAME_SCRAMBLE) {
            if(_scrPackCount < TRIVIA_MAX_TOPICS) {
                _scrPacks[_scrPackCount] = WordPack{};
                _scrPacks[_scrPackCount].name = name;
                _scrPackCount++;
            }
        } else if(game == HA_GAME_DRAW) {
            if(_dPackCount < TRIVIA_MAX_TOPICS) {
                _dPacks[_dPackCount] = WordPack{};
                _dPacks[_dPackCount].name = name;
                _dPackCount++;
            }
        } else if(game == HA_GAME_SPECTRUM) {
            if(_specPackCount < TRIVIA_MAX_TOPICS) {
                _specPacks[_specPackCount] = WyrPack{};
                _specPacks[_specPackCount].name = name;
                _specPackCount++;
            }
        } else if(game == HA_GAME_KMK) {
            if(_kmkPackCount < TRIVIA_MAX_TOPICS) {
                _kmkPacks[_kmkPackCount] = WordPack{};
                _kmkPacks[_kmkPackCount].name = name;
                _kmkPackCount++;
            }
        } else if(game == HA_GAME_SECRETS) {
            if(_secretsPackCount < TRIVIA_MAX_TOPICS) {
                _secretsPacks[_secretsPackCount] = WordPack{};
                _secretsPacks[_secretsPackCount].name = name;
                _secretsPackCount++;
            }
        } else if(game == HA_GAME_FILLBLANK) {
            if(_fbPackCount < FB_MAX_PACKS) {
                _fbPacks[_fbPackCount] = FillBlankPack{};
                _fbPacks[_fbPackCount].name = name;
                _fbPackCount++;
            }
        } else if(game == HA_GAME_SPYFALL) {
            if(_sfPackCount < SPYFALL_MAX_PACKS) {
                _sfPacks[_sfPackCount] = SpyPack{};
                _sfPacks[_sfPackCount].name = name;
                _sfPackCount++;
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
        else if(_packGame == HA_GAME_FILLBLANK) fillblankLoadItem(json);
        else if(_packGame == HA_GAME_SPYFALL) spyfallLoadItem(json);
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
        if(_wyrPackCount == 0) return false;
        WyrPack& p = _wyrPacks[_wyrPackCount - 1];
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
        if(_specPackCount == 0) return false;
        WyrPack& p = _specPacks[_specPackCount - 1];
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
        if(_scrPackCount == 0) return false;
        WordPack& p = _scrPacks[_scrPackCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[24];
        if(!ha_json_str(json, "word", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    // Map a Kiss Marry Kill pack file's {name} key into the current pack.
    bool kmkLoadItem(const char* json) {
        if(_kmkPackCount == 0) return false;
        WordPack& p = _kmkPacks[_kmkPackCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[40];
        if(!ha_json_str(json, "name", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    // Map a Secrets pack file's {q} key (one yes/no question) into the current pack.
    bool secretsLoadItem(const char* json) {
        if(_secretsPackCount == 0) return false;
        WordPack& p = _secretsPacks[_secretsPackCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[160];
        if(!ha_json_str(json, "q", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    // Map a Fill the Blank pack file's item into the current pack. Each block carries
    // EITHER a `P` key (a prompt card, which should contain the _____ blank) or an `A`
    // key (an answer card); the two go into separate decks of the same pack.
    bool fillblankLoadItem(const char* json) {
        if(_fbPackCount == 0) return false;
        FillBlankPack& p = _fbPacks[_fbPackCount - 1];
        char buf[128];
        if(ha_json_str(json, "p", buf, sizeof(buf)) && buf[0]) {
            if(p.pcount >= FB_MAX_PROMPTS) return false;
            p.prompts[p.pcount++] = buf;
            return true;
        }
        if(ha_json_str(json, "a", buf, sizeof(buf)) && buf[0]) {
            if(p.acount >= FB_MAX_ANSWERS) return false;
            p.answers[p.acount++] = buf;
            return true;
        }
        return false;
    }

    // Map a spyfall pack block into one location: a "Loc:" line plus one "R:" line per
    // role played there. The Flipper ships every line of a block as its own JSON pair
    // without interpreting it, so the role lines arrive as the SAME key repeated --
    // ha_json_str() would only ever see the first, hence ha_json_str_nth() to walk them
    // in file order. Extra roles beyond SPYFALL_MAX_ROLES are dropped, and a location
    // with no roles at all is rejected (there'd be nothing to hand the players).
    bool spyfallLoadItem(const char* json) {
        if(_sfPackCount == 0) return false;
        SpyPack& p = _sfPacks[_sfPackCount - 1];
        if(p.count >= SPYFALL_MAX_LOCS) return false;
        char buf[64];
        if(!ha_json_str(json, "loc", buf, sizeof(buf)) || !buf[0]) return false;
        SpyLoc loc;
        loc.name = buf;
        loc.roleCount = 0;
        for(int i = 0; i < SPYFALL_MAX_ROLES; i++) {
            if(!ha_json_str_nth(json, "r", i, buf, sizeof(buf)) || !buf[0]) break;
            loc.roles[loc.roleCount++] = buf;
        }
        if(loc.roleCount == 0) return false;
        p.locs[p.count] = loc;
        p.count++;
        return true;
    }

    // Map a draw pack file's {word} key into the current pack.
    bool drawLoadItem(const char* json) {
        if(_dPackCount == 0) return false;
        WordPack& p = _dPacks[_dPackCount - 1];
        if(p.count >= PACK_MAX_ITEMS) return false;
        char buf[24];
        if(!ha_json_str(json, "word", buf, sizeof(buf)) || !buf[0]) return false;
        p.words[p.count++] = buf;
        return true;
    }

    // Reset only game `id` to its lobby. This is the union-safe clear: it touches exactly the
    // one game whose state is live, so it is the only clear selectGame()/roundEnd() run.
    void dispatchClear(uint8_t id) {
        if(id == HA_GAME_TRIVIA)
            triviaClear();
        else if(isDuel(id))
            duelClear();
        else if(id == HA_GAME_DRAW)
            drawClear();
        else if(id == HA_GAME_PONG)
            pongClear();
        else if(id == HA_GAME_WYR)
            wyrClear();
        else if(id == HA_GAME_SCRAMBLE)
            scrambleClear();
        else if(id == HA_GAME_REACT)
            reactClear();
        else if(id == HA_GAME_GUESSCOLOR)
            gcClear();
        else if(id == HA_GAME_BATTLESHIP)
            battleClear();
        else if(id == HA_GAME_SPECTRUM)
            spectrumClear();
        else if(id == HA_GAME_KMK)
            kmkClear();
        else if(id == HA_GAME_CHESS)
            chessClear();
        else if(id == HA_GAME_SECRETS)
            secretsClear();
        else if(id == HA_GAME_FILLBLANK)
            fillblankClear();
        else if(id == HA_GAME_WEREWOLF)
            wwClear();
        else if(id == HA_GAME_SPYFALL)
            spyfallClear();
        else if(id == HA_GAME_FRANKENDRAW)
            fdClear();
    }

    void roundEnd() {
        dispatchClear(_active);
        pushAll();
    }

    // Time-based updates (trivia phases, drawing timers, pong physics). From loop().
    void tick(uint32_t now) {
        // A pending game-change vote freezes the active game: advance only its timeout.
        if(_gvActive) {
            gameVoteResolve(now);
            return;
        }
        botSync(now); // keep the bot seats (testing switch) matched, and let them act
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
        else if(_active == HA_GAME_FILLBLANK)
            fillblankTick(now);
        else if(_active == HA_GAME_WEREWOLF)
            wwTick(now);
        else if(_active == HA_GAME_SPYFALL)
            spyfallTick(now);
        else if(_active == HA_GAME_FRANKENDRAW)
            fdTick(now);
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
            // A phone can send a stable client id it stores itself (localStorage). When
            // present it becomes the device key, so restoring a returning player survives
            // iOS Wi-Fi MAC randomization -- a "forget network" or OS change gives a new MAC,
            // which would otherwise look like a brand-new phone with no score. Falls back to
            // the MAC-derived key when the phone sends no cid (older clients).
            char cid[24];
            if(ha_json_str(json, "cid", cid, sizeof(cid)) && cid[0]) {
                uint64_t h = 1469598103934665603ULL; // FNV-1a 64
                for(const char* p = cid; *p; p++) {
                    h ^= (uint8_t)*p;
                    h *= 1099511628211ULL;
                }
                if(h) deviceKey = h; // never 0 (0 means "unknown")
            }
            onHello(wsId, deviceKey, nick, avatar, named != 0);
            return;
        }
        // Debug: let a two-person room reach the games that need more. Any player may
        // flip it -- this is a testing aid, not a permission system -- and everyone sees
        // the new state on the next push so the switch cannot disagree with the host.
        // Flipping it on fills the missing seats with bots (botSync), not merely
        // silences the check: these games size their round on who is actually present.
        if(strcmp(type, "minoverride") == 0) {
            const char* v = ha_json_find(json, "on");
            _minOverride = v && strncmp(v, "true", 4) == 0;
            botSync(millis()); // seats appear/vanish now, and its pushAll shows them
            pushAll();
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
            fillblankReady(pid, r);
            wwReady(pid, r);
            spyfallReady(pid, r);
            fdReady(pid, r);
        } else if(strcmp(type, "vote") == 0 && ha_json_int(json, "topic", &v)) {
            triviaVote(pid, v);
        } else if(strcmp(type, "vote") == 0 && ha_json_int(json, "pack", &v)) {
            wyrVote(pid, v);
            scrambleVote(pid, v);
            spectrumVote(pid, v);
            kmkVote(pid, v);
            secretsVote(pid, v);
            fillblankVote(pid, v);
            spyfallVote(pid, v);
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
        } else if(strcmp(type, "play") == 0 && ha_json_int(json, "card", &v)) {
            fillblankPlay(pid, v);
        } else if(strcmp(type, "pick") == 0 && ha_json_int(json, "i", &v)) {
            fillblankPick(pid, v);
        } else if(strcmp(type, "kill") == 0 && ha_json_int(json, "n", &v)) {
            wwKill(pid, v); // werewolf's night target
        } else if(strcmp(type, "see") == 0 && ha_json_int(json, "n", &v)) {
            wwSee(pid, v); // seer's night check
        } else if(strcmp(type, "guard") == 0 && ha_json_int(json, "n", &v)) {
            wwGuard(pid, v); // doctor's night shield
        } else if(strcmp(type, "accuse") == 0 && ha_json_int(json, "n", &v)) {
            wwAccuse(pid, v); // day vote
        } else if(strcmp(type, "seen") == 0) {
            spyfallSeen(pid);
        } else if(strcmp(type, "accuse") == 0 && ha_json_int(json, "pid", &v)) {
            spyfallAccuse(pid, v);
        } else if(strcmp(type, "solve") == 0 && ha_json_int(json, "loc", &v)) {
            spyfallSolve(pid, v);
        } else if(strcmp(type, "nominate") == 0 && ha_json_int(json, "pid", &v)) {
            spyfallNominate(pid, v);
        } else if(strcmp(type, "agree") == 0) {
            const char* ap = ha_json_find(json, "in");
            spyfallAgree(pid, ap && strncmp(ap, "true", 4) == 0);
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
            fillblankAgain(pid);
            wwAgain(pid);
            spyfallAgain(pid);
            fdAgain(pid);
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
            fdStroke(pid, json);
        } else if(strcmp(type, "clear") == 0) {
            drawClearInk(pid);
        } else if(strcmp(type, "done") == 0) {
            fdDone(pid);
        } else if(strcmp(type, "undo") == 0) {
            fdUndo(pid);
        } else if(strcmp(type, "thumb") == 0) {
            int sheet, val;
            if(ha_json_int(json, "sheet", &sheet) && ha_json_int(json, "v", &val))
                fdThumb(pid, sheet, val);
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
    // ---- always-resident state (kept OUT of the per-game union below) ----
    TriviaTopic _topics[TRIVIA_MAX_TOPICS] = {}; // trivia's content (its runtime state _t is in the union)
    uint8_t _topicCount = 0;
    uint8_t _packGame = 0; // HA_GAME_* of the pack currently being streamed, 0 = none
    // The eight content games' packs, lifted out of their state structs. They hold Strings (so
    // they cannot live in the POD union) and are streamed for every game up front regardless of
    // which one is active, so they must stay resident. Each game's runtime state is in the union.
    WordPack _dPacks[TRIVIA_MAX_TOPICS] = {};       uint8_t _dPackCount = 0;       // Draw & Guess
    WyrPack  _wyrPacks[TRIVIA_MAX_TOPICS] = {};     uint8_t _wyrPackCount = 0;     // Would You Rather
    WordPack _scrPacks[TRIVIA_MAX_TOPICS] = {};     uint8_t _scrPackCount = 0;     // Word Scramble
    WyrPack  _specPacks[TRIVIA_MAX_TOPICS] = {};    uint8_t _specPackCount = 0;    // Spectrum
    WordPack _kmkPacks[TRIVIA_MAX_TOPICS] = {};     uint8_t _kmkPackCount = 0;     // Kiss Marry Kill
    WordPack _secretsPacks[TRIVIA_MAX_TOPICS] = {}; uint8_t _secretsPackCount = 0; // Secrets
    FillBlankPack _fbPacks[FB_MAX_PACKS] = {};      uint8_t _fbPackCount = 0;      // Fill the Blank
    SpyPack  _sfPacks[SPYFALL_MAX_PACKS] = {};      uint8_t _sfPackCount = 0;      // Spyfall

    uint32_t _lastPong = 0;
    // Challenge/accept list, shared by every 1v1 game (duels, Pong, Battleship, Chess). It stays
    // live across all of them regardless of which is active, so it lives OUTSIDE the game-state
    // union (which only ever holds one game's match array). Cleared on selectGame()/reset().
    DuelChallenge _c[DUEL_MAX_CHALLENGES] = {};
    // Frankendraw's per-sheet stroke store (~28 KB), lifted out of FrankenState so it never
    // occupies static DRAM. Allocated on demand (PSRAM on the S2/C5, plain heap on the WROOM)
    // only while Frankendraw is the active game; freed on any other selectGame() and on reset().
    // Declared as its own member -- never inside the game-state union -- so a union-wide memset
    // can never zero this live pointer out from under an allocation.
    FdSheet* _fdSheets = nullptr;

    // Cross-cutting game-change vote (above the active game). When _gvActive, the active
    // game is frozen and every client is shown a vote overlay instead of game state.
    bool _gvActive = false;
    uint8_t _gvProposer = 0; // pid who proposed (an implicit YES)
    uint8_t _gvTarget = 0; // proposed game id
    uint32_t _gvStart = 0; // millis the proposal opened (for the timeout)
    int8_t _gvVote[HA_MAX_PLAYERS + 1] = {}; // -1 none, 0 no, 1 yes

    // ---- per-game runtime state: one active game at a time, so they share memory ----
    // Only _active's state is ever live, so every game's runtime state overlaps in one union
    // instead of each reserving its own DRAM. Safe as a plain union because every member is POD
    // (the Strings were lifted into the pack members above). std::variant is unavailable --
    // ha_games.h compiles as gnu++11 on the ESP32 core. The union is anonymous so each state
    // keeps its own unqualified name (_t, _wyr, _fd, ...) at its ~1,500 access sites.
    // selectGame()/reset() zero the whole union (gsZero) then run only the active game's clear,
    // so a game switch never leaves another game's bytes behind.
    union {
        Trivia _t;
        DuelMatch _m[DUEL_MAX_MATCHES]; // Connect-4 / Tic-Tac-Toe / Dots / Reversi matches
        DrawState _d;
        PongMatch _pm[PONG_MAX];
        WyrState _wyr;
        ScrambleState _scr;
        ReactState _react;
        GuessColorState _gc;
        BattleMatch _bm[BATTLE_MAX];
        SpectrumState _spec;
        KmkState _kmk;
        ChessMatch _cm[CHESS_MAX];
        SecretsState _secrets;
        FrankenState _fd;
        FillBlankState _fb;
        WerewolfState _ww;
        SpyfallState _sf;
    };
    // MUST stay immediately after the union: gsZero() zeroes the byte span [_t, _gsSentinel),
    // which covers the whole anonymous union (plus any trailing padding, which is harmless).
    char _gsSentinel = 0;

    // Zero every byte of the game-state union. The union is anonymous (each game keeps its own
    // member name), so it has no name to sizeof; instead span from its first member to the
    // sentinel right after it. uintptr_t math avoids UB from subtracting unrelated pointers.
    void gsZero() {
        memset((void*)&_t, 0, (size_t)((uintptr_t)&_gsSentinel - (uintptr_t)&_t));
    }

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

    // The lobby's testing switch (wire name "minoverride", kept for the client's sake).
    // It used to bypass the per-game minimum check -- the wrong half of the feature:
    // Werewolf then dealt two roles instead of five, Spyfall had no third chair to hide
    // the spy behind, and Draw a Monster left a body part with no owner. Every one of
    // those games sizes its round on the players actually PRESENT, so the seats must
    // exist. Flipping this now fills the missing seats with engine-run bots (botSync)
    // and the minimum check itself stays honest.
    bool _minOverride = false;
    bool enoughPlayers(int need) { return connectedCount() >= need; }

    int humanCount() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && !_p[i].bot) n++;
        return n;
    }

    // Only the games whose round genuinely cannot be seated below their minimum get
    // bot seats. Everything else plays fine with one or two phones and gets none --
    // a bot in a duel or a trivia round would just be a chair that never answers.
    int botTargetMin() {
        if(_active == HA_GAME_WEREWOLF) return WW_MIN_PLAYERS;
        if(_active == HA_GAME_SPYFALL) return SPYFALL_MIN_PLAYERS;
        if(_active == HA_GAME_FRANKENDRAW) return FD_MIN_PLAYERS;
        return 0;
    }

    uint8_t botAdd() {
        uint8_t pid = freePid();
        if(!pid) return 0;
        // Stable names, assigned by the first free one: removing BOT-BEN and refilling
        // brings BOT-BEN back, so the roster does not churn through the alphabet.
        static const char* names[HA_BOT_MAX] = {"BOT-ADA", "BOT-BEN", "BOT-CLU", "BOT-DOT"};
        int k = 0;
        for(; k < HA_BOT_MAX; k++) {
            bool taken = false;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _p[i].bot && strcmp(_p[i].nick, names[k]) == 0) taken = true;
            if(!taken) break;
        }
        if(k >= HA_BOT_MAX) return 0;
        _p[pid] = Player{};
        _p[pid].used = true;
        _p[pid].bot = true;
        strlcpy(_p[pid].nick, names[k], HA_NICK_LEN);
        strlcpy(_p[pid].avatar, "\xF0\x9F\xA4\x96", sizeof(_p[pid].avatar)); // robot face
        haUartJoin(pid, _p[pid].nick);
        return pid;
    }

    void botRemove(uint8_t pid) {
        anyOnLeave(pid); // forfeit anything it was part of, like a leaver would
        _p[pid] = Player{};
        _gvVote[pid] = -1;
        haUartLeave(pid);
    }

    // Keep the bot seats matched to the switch, the room and the active game, then
    // let the seated bots act. Called from tick() -- but never while a game-change
    // vote is pending: the roster feeds the tally, and seats appearing or vanishing
    // mid-vote is exactly the ghost-voter bug this feature exists to avoid.
    void botSync(uint32_t now) {
        if(_gvActive) return;
        int humans = humanCount();
        int want = 0;
        if(_minOverride && humans > 0) {
            want = botTargetMin() - humans;
            if(want < 0) want = 0;
            if(want > HA_BOT_MAX) want = HA_BOT_MAX;
        }
        int have = connectedCount() - humans;
        bool changed = false;
        while(have < want) {
            if(!botAdd()) break;
            have++;
            changed = true;
        }
        for(uint8_t i = HA_MAX_PLAYERS; i >= 1 && have > want; i--) {
            if(_p[i].used && _p[i].bot) {
                botRemove(i);
                have--;
                changed = true;
            }
        }
        if(changed) {
            triviaOnRosterChange();
            partyRosterChanged();
            pushAll();
        }
        if(have > 0) botAct(now);
    }

    // What the bots actually do: ready-up in a lobby, and the few in-round moves a
    // round cannot comfortably wait out. Everything else rides the games' own safety
    // deadlines ("whoever did not act is skipped"). Actions go through the same
    // public handlers a phone would use, so every rule check still applies; a
    // rejected call is simply retried on a later tick. The 1.2-3s delay keeps a
    // phase from resolving before the real players have even seen it.
    uint32_t _botStamp = 0;
    uint32_t _botActAt = 0;
    void botAct(uint32_t now) {
        uint32_t stamp = ((uint32_t)_active << 24);
        if(_active == HA_GAME_WEREWOLF)
            stamp ^= ((uint32_t)_ww.pt.phase << 16) ^ ((uint32_t)_ww.stage << 8) ^ (uint32_t)_ww.pt.round;
        else if(_active == HA_GAME_SPYFALL)
            stamp ^= ((uint32_t)_sf.pt.phase << 16) ^ ((uint32_t)_sf.stage << 8) ^
                     ((uint32_t)_sf.nomStage << 4) ^ (uint32_t)_sf.nominator;
        else if(_active == HA_GAME_FRANKENDRAW)
            stamp ^= ((uint32_t)_fd.pt.phase << 16) ^ (uint32_t)_fd.pt.round;
        if(stamp != _botStamp) {
            _botStamp = stamp;
            _botActAt = now + 1200 + (esp_random() % 1800);
            return;
        }
        if((int32_t)(now - _botActAt) < 0) return;
        _botActAt = now + 2500; // pace repeats; validated no-ops stay cheap
        for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++) {
            if(!_p[pid].used || !_p[pid].bot) continue;
            if(_active == HA_GAME_WEREWOLF) {
                if(_ww.pt.phase == 0 && !_ww.pt.ready[pid]) wwReady(pid, true);
                if(_ww.pt.phase == 2 && _ww.stage == WW_S_NIGHT) {
                    if(_ww.role[pid] == WW_WOLF && _ww.kill[pid] <= 0)
                        wwKill(pid, botPickTarget(pid, true));
                    if(_ww.role[pid] == WW_SEER && !_ww.seerTarget)
                        wwSee(pid, botPickTarget(pid, false));
                    if(_ww.role[pid] == WW_DOCTOR && !_ww.docTarget)
                        wwGuard(pid, botPickTarget(pid, false));
                }
                if(_ww.pt.phase == 2 && _ww.stage == WW_S_DAY && _ww.accuse[pid] <= 0 &&
                   _ww.alive[pid] && _ww.role[pid] != 0)
                    wwAccuse(pid, botPickTarget(pid, false));
            } else if(_active == HA_GAME_SPYFALL) {
                if(_sf.pt.phase == 0 && !_sf.pt.ready[pid]) spyfallReady(pid, true);
                if(_sf.pt.phase == 2 && _sf.stage == 0 && _sf.inRound[pid] && !_sf.seen[pid])
                    spyfallSeen(pid);
                if(_sf.pt.phase == 2 && _sf.stage == 2 && _sf.nomStage == 1 &&
                   _sf.nominator == pid) {
                    // A bot never nominates; it passes its round-robin turn at once so
                    // the table is not left staring at a robot's think time.
                    _sf.nominated[pid] = true;
                    spyfallNextNominator(now);
                }
                if(_sf.pt.phase == 2 && _sf.stage == 2 && _sf.nomStage == 2 &&
                   _sf.inRound[pid] && _sf.agree[pid] < 0)
                    spyfallAgree(pid, _sf.nominee != pid && (esp_random() & 1) != 0);
            } else if(_active == HA_GAME_FRANKENDRAW) {
                if(_fd.pt.phase == 0 && !_fd.pt.ready[pid]) fdReady(pid, true);
                if(_fd.pt.phase == 2 && !_fd.done[pid])
                    fdDone(pid); // hands in an empty panel; the guard validates the seat
            }
        }
    }

    // A random live, dealt-in werewolf target for `pid`. `hunting` = wolves only pick
    // outside the pack; the handlers re-validate everything anyway, so a miss here
    // just means trying again on a later tick.
    int botPickTarget(uint8_t pid, bool hunting) {
        uint8_t cand[HA_MAX_PLAYERS];
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || i == pid) continue;
            if(!_ww.alive[i] || _ww.role[i] == 0) continue;
            if(hunting && _ww.role[i] == WW_WOLF) continue;
            cand[n++] = i;
        }
        if(!n) return 0;
        return cand[esp_random() % (uint32_t)n];
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
            else if(_active == HA_GAME_FILLBLANK)
                haWsSendWs(_p[pid].wsId, fillblankJson(pid));
            else if(_active == HA_GAME_WEREWOLF)
                haWsSendWs(_p[pid].wsId, wwJson(pid));
            else if(_active == HA_GAME_SPYFALL)
                haWsSendWs(_p[pid].wsId, spyfallJson(pid));
            else if(_active == HA_GAME_FRANKENDRAW)
                haWsSendWs(_p[pid].wsId, fdJson(pid));
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
        case HA_GAME_FILLBLANK:
            return "fillblank";
        case HA_GAME_WEREWOLF:
            return "werewolf";
        case HA_GAME_SPYFALL:
            return "spyfall";
        case HA_GAME_FRANKENDRAW:
            return "frankendraw";
        default:
            return "none";
        }
    }

    String lobbyJson() {
        return String("{\"t\":\"lobby\",\"game\":\"") + gameName(_active) +
               "\",\"players\":" + playersJson() + ",\"minoverride\":" + (_minOverride ? "true" : "false") + "}";
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
    // The shared 1v1 challenge list (_c lives outside the game-state union). Cleared on a game
    // switch/teardown so a stale challenge from one match game never leaks into the next.
    void challengesClear() {
        for(int i = 0; i < DUEL_MAX_CHALLENGES; i++) _c[i] = DuelChallenge{};
    }

    void duelClear() {
        for(int i = 0; i < DUEL_MAX_MATCHES; i++) _m[i] = DuelMatch{};
        challengesClear();
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
        if(!isDuel(_active)) return nullptr; // _m is union memory; only read it while duel is live
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
        fdOnLeave(pid);
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
        if(_dPackCount == 0) return; // no pack streamed: refuse to start a round
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
        WordPack& dp = _dPacks[_d.pack];
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
        if(_active != HA_GAME_PONG) return nullptr; // _pm is union memory; read only while Pong is live
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
        else if(_active == HA_GAME_FILLBLANK)
            fillblankRosterChanged();
        else if(_active == HA_GAME_WEREWOLF)
            wwRosterChanged();
        else if(_active == HA_GAME_SPYFALL)
            spyfallRosterChanged();
        else if(_active == HA_GAME_FRANKENDRAW)
            fdCheckStart();
    }

    // ---------- would you rather (live A/B poll) ----------
    // Which pack wins the pre-round vote, mirroring triviaWinningTopic(): most
    // votes wins, ties broken at random, and an untallied vote (total == 0)
    // picks uniformly at random among all packs. Guard packCount == 0 so an
    // empty game (no packs streamed yet) never indexes out of range.
    int wyrWinningPack() {
        if(_wyrPackCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _wyr.vote[i] >= 0 && _wyr.vote[i] < _wyrPackCount) {
                votes[_wyr.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_wyrPackCount);
        int best = 0;
        for(int i = 1; i < _wyrPackCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _wyrPackCount; i++)
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
        _wyr.splitCount = 0;
        for(int i = 0; i < WYR_ROUNDS; i++) {
            _wyr.splitA[i] = 0;
            _wyr.splitB[i] = 0;
        }
    }

    // Tally the current prompt's A/B votes over the connected players.
    void wyrCounts(int& cA, int& cB) {
        cA = 0;
        cB = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(_wyr.choice[i] == 0) cA++;
            else if(_wyr.choice[i] == 1) cB++;
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
        if(pack < 0 || pack >= _wyrPackCount) return;
        _wyr.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void wyrCheckStart() {
        Party& pt = _wyr.pt;
        if(pt.phase == 0 && _wyrPackCount > 0 && partyAllReady(pt)) {
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
        WyrPack& pk = _wyrPacks[_wyr.pack];
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
        // Latch this prompt's split before flipping to reveal: rounds are indexed
        // 1..WYR_ROUNDS, and reveal happens exactly once per round (phase 2 -> 3),
        // so splitCount tracks the round number. A round nobody voted in is stored
        // as 0/0 and skipped by the chart rather than counted as total agreement.
        if(_wyr.pt.phase == 2 && _wyr.splitCount < WYR_ROUNDS) {
            int cA, cB;
            wyrCounts(cA, cB);
            _wyr.splitA[_wyr.splitCount] = (uint8_t)cA;
            _wyr.splitB[_wyr.splitCount] = (uint8_t)cB;
            _wyr.splitCount++;
        }
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
                if(_p[i].used && _wyr.vote[i] >= 0 && _wyr.vote[i] < _wyrPackCount) votes[_wyr.vote[i]]++;
            for(int i = 0; i < _wyrPackCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_wyrPacks[i].name.c_str()) + "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_wyr.vote[pid]);
            s += "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"wyr\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4) {
            // Final: hand the client the whole game's A/B history plus the current
            // player count, so it can draw the agreement chart. `voters` is the axis
            // the client buckets into (for n voters the reachable agreement values are
            // ceil(n/2)/n .. n/n); the per-round splits carry the real numbers.
            String s = String("{\"t\":\"wyr\",\"phase\":\"final\",\"you\":") + pid;
            int voters = 0;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used) voters++;
            s += ",\"voters\":" + String(voters) + ",\"rounds\":[";
            for(uint8_t i = 0; i < _wyr.splitCount; i++) {
                if(i) s += ",";
                s += "{\"a\":" + String((int)_wyr.splitA[i]) + ",\"b\":" + String((int)_wyr.splitB[i]) + "}";
            }
            s += "]}";
            return s;
        }
        WyrPack& pk = _wyrPacks[_wyr.pack];
        const char* a = pk.items[_wyr.prompt].a.c_str();
        const char* b = pk.items[_wyr.prompt].b.c_str();
        int cA, cB;
        wyrCounts(cA, cB);
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
        if(_scrPackCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _scr.vote[i] >= 0 && _scr.vote[i] < _scrPackCount) {
                votes[_scr.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_scrPackCount);
        int best = 0;
        for(int i = 1; i < _scrPackCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _scrPackCount; i++)
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
        if(pack < 0 || pack >= _scrPackCount) return;
        _scr.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void scrambleCheckStart() {
        if(_scrPackCount == 0) return;
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
        WordPack& p = _scrPacks[_scr.pack];
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
                if(_p[i].used && _scr.vote[i] >= 0 && _scr.vote[i] < _scrPackCount) votes[_scr.vote[i]]++;
            for(int i = 0; i < _scrPackCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_scrPacks[i].name.c_str()) + "\",\"votes\":" + votes[i] + "}";
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
        if(_active != HA_GAME_BATTLESHIP) return nullptr; // _bm is union memory; read only while live
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
        if(_active != HA_GAME_CHESS) return nullptr; // _cm is union memory; read only while Chess is live
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
        if(_specPackCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _spec.vote[i] >= 0 && _spec.vote[i] < _specPackCount) {
                votes[_spec.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_specPackCount);
        int best = 0;
        for(int i = 1; i < _specPackCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _specPackCount; i++)
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
        if(pack < 0 || pack >= _specPackCount) return;
        _spec.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void spectrumCheckStart() {
        if(_specPackCount == 0) return;
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
        WyrPack& pk = _specPacks[_spec.pack];
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
                if(_p[i].used && _spec.vote[i] >= 0 && _spec.vote[i] < _specPackCount)
                    votes[_spec.vote[i]]++;
            for(int i = 0; i < _specPackCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_specPacks[i].name.c_str()) +
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

        WyrPack& pk = _specPacks[_spec.pack];
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
        if(_kmkPackCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _kmk.vote[i] >= 0 && _kmk.vote[i] < _kmkPackCount) {
                votes[_kmk.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_kmkPackCount);
        int best = 0;
        for(int i = 1; i < _kmkPackCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _kmkPackCount; i++)
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
        if(pack < 0 || pack >= _kmkPackCount) return;
        _kmk.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void kmkCheckStart() {
        if(_kmkPackCount == 0) return;
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
        WordPack& pk = _kmkPacks[_kmk.pack];
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
                if(_p[i].used && _kmk.vote[i] >= 0 && _kmk.vote[i] < _kmkPackCount)
                    votes[_kmk.vote[i]]++;
            for(int i = 0; i < _kmkPackCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_kmkPacks[i].name.c_str()) +
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

        WordPack& pk = _kmkPacks[_kmk.pack];
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
        if(_secretsPackCount == 0) return 0;
        int votes[TRIVIA_MAX_TOPICS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _secrets.vote[i] >= 0 && _secrets.vote[i] < _secretsPackCount) {
                votes[_secrets.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_secretsPackCount);
        int best = 0;
        for(int i = 1; i < _secretsPackCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[TRIVIA_MAX_TOPICS], tn = 0;
        for(int i = 0; i < _secretsPackCount; i++)
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
        if(pack < 0 || pack >= _secretsPackCount) return;
        _secrets.vote[pid] = (int8_t)pack;
        pushAll();
    }

    void secretsCheckStart() {
        if(_secretsPackCount == 0) return;
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
        WordPack& pk = _secretsPacks[_secrets.pack];
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
                if(_p[i].used && _secrets.vote[i] >= 0 && _secrets.vote[i] < _secretsPackCount)
                    votes[_secrets.vote[i]]++;
            for(int i = 0; i < _secretsPackCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_secretsPacks[i].name.c_str()) +
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

        WordPack& pk = _secretsPacks[_secrets.pack];
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
        for(uint8_t id = HA_GAME_NONE; id <= HA_GAME_FILLBLANK; id++)
            if(strcmp(gameName(id), name) == 0) return (int)id;
        // The bound is deliberately NOT "the highest id I know about": that hid a game
        // numbered above it from the phone-side vote entirely, which is how a whole game
        // became unproposable. gameName() answers "none" for every unassigned id and
        // HA_GAME_NONE matches first, so sweeping the byte range is safe -- and it never
        // needs touching again when a game is added, whatever id it takes.
        for(int id = 0; id <= 255; id++)
            if(strcmp(gameName((uint8_t)id), name) == 0) return id;
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
        if(id < 0) return; // unknown name
        if((uint8_t)id == _active) {
            // The switcher normally hides the active game, but a stale page can still
            // propose it. Answer instead of going silent -- a tap that does nothing
            // reads as "the voting is broken" (it cost a play-test an evening).
            if(_p[pid].wsId)
                haWsSendWs(
                    _p[pid].wsId,
                    strncmp(_lang, "de", 2) == 0
                        ? String("{\"t\":\"toast\",\"msg\":\"Das Spiel läuft schon\"}")
                        : String("{\"t\":\"toast\",\"msg\":\"Already the current game\"}"));
            return;
        }
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
            // Bots hold no franchise: a seat that cannot see the overlay must not be
            // waited on, or every vote in a bot-filled room dies in the timeout.
            if(!_p[i].used || _p[i].bot || i == _gvProposer) continue;
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
        // Carry the numeric id too: the Flipper has no name->id map and uses it to update
        // its displayed active game (and to not revert the vote on an ESP reboot).
        haUartEvent(String("{\"gamevote\":\"approved\",\"game\":\"") + gameName(target) +
                    "\",\"id\":" + String((int)target) + "}");
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
            if(!_p[i].used || _p[i].bot) continue; // bots hold no franchise
            if(_gvVote[i] == 1) yes++; // includes the proposer's implicit YES
            else if(_gvVote[i] == 0) no++;
        }
        int others = humanCount() - 1;
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

    // ---------- Fill the Blank (a judge picks the funniest answer) ----------
    // Genre homage to Cards Against Humanity; every card shipped here is our own.
    // Round shape: deal hands -> everyone but the Czar plays one card face down ->
    // the pile is shuffled and shown anonymously -> the Czar picks -> +1 to its author.
    int fillblankWinningPack() {
        if(_fbPackCount == 0) return 0;
        int votes[FB_MAX_PACKS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _fb.vote[i] >= 0 && _fb.vote[i] < _fbPackCount) {
                votes[_fb.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_fbPackCount);
        int best = 0;
        for(int i = 1; i < _fbPackCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[FB_MAX_PACKS], tn = 0;
        for(int i = 0; i < _fbPackCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void fillblankClear() {
        partyClear(_fb.pt);
        _fb.pack = 0;
        _fb.promptSeq = 0;
        _fb.prompt = 0;
        _fb.czar = 0;
        _fb.czarSeq = 0;
        _fb.stage = 0;
        _fb.deckLen = 0;
        _fb.drawNext = 0;
        _fb.subCount = 0;
        _fb.picked = -1;
        _fb.winner = 0;
        _fb.deckWon = false;
        _fb.czarGain = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _fb.vote[i] = -1;
            _fb.played[i] = -1;
            _fb.inRound[i] = false;
            for(int j = 0; j < FB_HAND; j++) _fb.hand[i][j] = -1;
        }
    }

    void fillblankReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_FILLBLANK) return;
        if(_fb.pt.phase != 0 && _fb.pt.phase != 4) return;
        if(_fb.pt.phase == 4 && val) fillblankClear(); // ready from the final screen -> new game
        _fb.pt.ready[pid] = val;
        fillblankCheckStart();
        pushAll();
    }

    void fillblankVote(uint8_t pid, int pack) {
        if(_active != HA_GAME_FILLBLANK || _fb.pt.phase != 0) return;
        if(pack < 0 || pack >= _fbPackCount) return;
        _fb.vote[pid] = (int8_t)pack;
        pushAll();
    }

    // Unlike the other party games this one needs a quorum: a Czar plus at least two
    // submissions to judge between. Below that the lobby simply keeps waiting (and the
    // countdown backs out again if someone leaves) - it never starts an unplayable round.
    void fillblankCheckStart() {
        if(_fbPackCount == 0) return;
        Party& pt = _fb.pt;
        bool quorum = enoughPlayers(FB_MIN_PLAYERS);
        if(pt.phase == 0 && quorum && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && (!quorum || !partyAllReady(pt))) {
            pt.phase = 0;
        }
    }

    // The Czar rotates: the (czarSeq mod N)-th connected player, in pid order.
    uint8_t fillblankPickCzar() {
        int n = connectedCount();
        if(n <= 0) return 0;
        int want = _fb.czarSeq % n, seen = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(seen == want) return i;
            seen++;
        }
        return 0;
    }

    // Rebuild the draw pile from every answer card that is not currently in someone's
    // hand, then shuffle. This is what keeps a long game from dead-ending: cards that
    // were played (or held by players who have since left) come back into circulation
    // instead of the deck running out.
    void fillblankRefillDeck() {
        FillBlankPack& pk = _fbPacks[_fb.pack];
        bool held[FB_MAX_ANSWERS] = {false};
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            for(int j = 0; j < FB_HAND; j++) {
                int8_t c = _fb.hand[i][j];
                if(c >= 0 && c < (int8_t)pk.acount) held[c] = true;
            }
        }
        _fb.deckLen = 0;
        for(uint8_t c = 0; c < pk.acount; c++)
            if(!held[c]) _fb.deck[_fb.deckLen++] = c;
        // Everything is somehow in hand (tiny pack, many players): fall back to the
        // whole deck so a draw still returns a card rather than nothing.
        if(_fb.deckLen == 0)
            for(uint8_t c = 0; c < pk.acount; c++) _fb.deck[_fb.deckLen++] = c;
        for(int i = (int)_fb.deckLen - 1; i > 0; i--) {
            int j = (int)(esp_random() % (uint32_t)(i + 1));
            uint8_t t = _fb.deck[i];
            _fb.deck[i] = _fb.deck[j];
            _fb.deck[j] = t;
        }
        _fb.drawNext = 0;
    }

    int fillblankDraw() {
        if(_fb.drawNext >= _fb.deckLen) fillblankRefillDeck();
        if(_fb.deckLen == 0) return -1;
        return (int)_fb.deck[_fb.drawNext++];
    }

    // Top every connected player back up to a full hand (a mid-game joiner gets one too,
    // so they can play from the next round on).
    void fillblankDealHands() {
        if(_fbPacks[_fb.pack].acount == 0) return;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            for(int j = 0; j < FB_HAND; j++) {
                if(_fb.hand[i][j] >= 0) continue;
                int c = fillblankDraw();
                if(c < 0) return;
                _fb.hand[i][j] = (int8_t)c;
            }
        }
    }

    void fillblankNextRound(uint32_t now) {
        Party& pt = _fb.pt;
        FillBlankPack& pk = _fbPacks[_fb.pack];
        if(pt.round >= FB_ROUNDS || pk.pcount == 0 || pk.acount == 0) {
            pt.phase = 4; // final (an empty pack can't be played)
            pushAll();
            return;
        }
        pt.round++;
        _fb.czar = fillblankPickCzar();
        _fb.czarSeq++;
        _fb.prompt = (uint8_t)(_fb.promptSeq % pk.pcount);
        _fb.promptSeq++;
        _fb.stage = 0;
        _fb.subCount = 0;
        _fb.picked = -1;
        _fb.winner = 0;
        _fb.deckWon = false;
        _fb.czarGain = 0;
        // A played card stayed in its owner's hand all through judging and the reveal, so
        // they could see what they had committed to. Discard it now, then deal back up.
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            if(_fb.played[i] >= 0 && _fb.played[i] < FB_HAND) _fb.hand[i][_fb.played[i]] = -1;
            _fb.played[i] = -1;
            _fb.inRound[i] = _p[i].used;
        }
        fillblankDealHands();
        pt.deadline = now + (uint32_t)FB_PLAY_SECS * 1000;
        pt.phase = 2;
        pushAll();
    }

    bool fillblankAllPlayed() {
        int players = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || i == _fb.czar || !_fb.inRound[i]) continue;
            players++;
            if(_fb.played[i] < 0) return false;
        }
        return players >= 1;
    }

    // Real submissions in the pile (the deck's card is not one of them), for the
    // "3/5 in" tally -- which must keep counting players, not the deck.
    int fillblankRealSubs() {
        int n = 0;
        for(int i = 0; i < (int)_fb.subCount; i++)
            if(_fb.subPid[i] != FB_DECK_PID) n++;
        return n;
    }

    // How many submissions this round is waiting on (for the "3/5 played" line).
    int fillblankExpected() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && i != _fb.czar && _fb.inRound[i]) n++;
        return n;
    }

    void fillblankPlay(uint8_t pid, int slot) {
        if(_active != HA_GAME_FILLBLANK || _fb.pt.phase != 2 || _fb.stage != 0) return;
        if(pid == _fb.czar || !_fb.inRound[pid]) return; // the Czar judges; joiners wait a round
        if(slot < 0 || slot >= FB_HAND) return;
        if(_fb.played[pid] >= 0) return; // one card each, no take-backs
        int card = _fb.hand[pid][slot];
        if(card < 0) return;
        // Leave room for the deck's card, which joins the pile at judging time.
        if(_fb.subCount >= FB_MAX_SUBS - 1) return;
        // The card is NOT removed from the hand: the client keeps showing it, marked as
        // chosen with the rest greyed out, and fillblankNextRound discards it later.
        _fb.played[pid] = (int8_t)slot;
        _fb.subPid[_fb.subCount] = pid;
        _fb.subCard[_fb.subCount] = (uint8_t)card;
        _fb.subCount++;
        if(fillblankAllPlayed()) fillblankToJudge(millis());
        else pushAll();
    }

    // Draw one answer card at random and enter it in the pile as the deck's own, authored
    // by FB_DECK_PID. Retries a few times to avoid duplicating a card already in the pile,
    // which would give the joke away; a duplicate is harmless if the deck is tiny enough
    // that every attempt collides.
    void fillblankAddDeckCard() {
        if(_fb.subCount >= FB_MAX_SUBS) return;
        FillBlankPack& pk = _fbPacks[_fb.pack];
        if(pk.acount == 0) return;
        int card = -1;
        for(int attempt = 0; attempt < 8; attempt++) {
            int c = fillblankDraw();
            if(c < 0) return;
            card = c;
            bool clash = false;
            for(int i = 0; i < (int)_fb.subCount; i++)
                if((int)_fb.subCard[i] == c) { clash = true; break; }
            if(!clash) break;
        }
        if(card < 0) return;
        _fb.subPid[_fb.subCount] = FB_DECK_PID;
        _fb.subCard[_fb.subCount] = (uint8_t)card;
        _fb.subCount++;
    }

    // Close submissions, add the deck's own card, and shuffle the pile. The shuffle is
    // the whole anonymity mechanism: pile order is otherwise pid order, so the Czar could
    // read authorship straight off it. It also hides WHERE the deck's card landed, which
    // is what makes it indistinguishable from a real submission. fillblankJson emits no
    // author for any card until the pick.
    void fillblankToJudge(uint32_t now) {
        if(_fb.subCount == 0) { // nobody played: no card to judge, roll on
            fillblankReveal(now);
            return;
        }
        // One deck card always, then as many more as it takes to reach FB_MIN_PILE: with
        // two players that turns one real answer into a three-card blind choice.
        fillblankAddDeckCard();
        while(_fb.subCount < FB_MIN_PILE && _fb.subCount < FB_MAX_SUBS) {
            uint8_t before = _fb.subCount;
            fillblankAddDeckCard();
            if(_fb.subCount == before) break; // deck exhausted; don't spin
        }
        for(int i = (int)_fb.subCount - 1; i > 0; i--) {
            int j = (int)(esp_random() % (uint32_t)(i + 1));
            uint8_t p = _fb.subPid[i], c = _fb.subCard[i];
            _fb.subPid[i] = _fb.subPid[j];
            _fb.subCard[i] = _fb.subCard[j];
            _fb.subPid[j] = p;
            _fb.subCard[j] = c;
        }
        _fb.stage = 1;
        _fb.pt.deadline = now + (uint32_t)FB_PICK_SECS * 1000;
        pushAll();
    }

    // Award the pick. Split out so the safety timer can pick for a stalled Czar.
    //
    // Two ways to score: the winning card's author gets 1, and the Czar also gets 1 for
    // having picked a real player's card. Picking the deck's card scores nobody at all --
    // that is the joke, and it is also what gives the Czar a stake in judging properly
    // rather than tapping whatever is nearest.
    void fillblankAward(int i, uint32_t now) {
        if(i >= 0 && i < (int)_fb.subCount) {
            _fb.picked = (int8_t)i;
            uint8_t w = _fb.subPid[i];
            if(w == FB_DECK_PID) {
                _fb.winner = 0;
                _fb.deckWon = true;
            } else {
                _fb.winner = w;
                if(_p[w].used) { // the author may have left between playing and the pick
                    _p[w].score += 1;
                    haUartScore(w, 1, "fillblank");
                }
                if(_fb.czar && _p[_fb.czar].used) {
                    _fb.czarGain = 1;
                    _p[_fb.czar].score += 1;
                    haUartScore(_fb.czar, 1, "fillblank-czar");
                }
            }
        }
        fillblankReveal(now);
    }

    void fillblankPick(uint8_t pid, int i) {
        if(_active != HA_GAME_FILLBLANK || _fb.pt.phase != 2 || _fb.stage != 1) return;
        if(pid != _fb.czar) return; // only the Czar judges
        if(i < 0 || i >= (int)_fb.subCount) return;
        fillblankAward(i, millis());
    }

    void fillblankReveal(uint32_t now) {
        haUartRoundResult(String("{\"fillblank\":\"round ") + _fb.pt.round + "\"}");
        _fb.pt.phase = 3;
        _fb.pt.revealUntil = now + FB_REVEAL_MS;
        pushAll();
    }

    void fillblankAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_FILLBLANK || _fb.pt.phase != 4) return;
        fillblankClear();
        pushAll();
    }

    void fillblankTick(uint32_t now) {
        Party& pt = _fb.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                _fb.pack = (uint8_t)fillblankWinningPack();
                _fb.czarSeq = 0;
                _fb.promptSeq = 0;
                for(int i = 0; i <= HA_MAX_PLAYERS; i++)
                    for(int j = 0; j < FB_HAND; j++) _fb.hand[i][j] = -1;
                fillblankRefillDeck();
                fillblankNextRound(now);
            }
        } else if(pt.phase == 2) {
            if(_fb.stage == 0) {
                if((int32_t)(now - pt.deadline) >= 0 || fillblankAllPlayed())
                    fillblankToJudge(now);
            } else {
                // Czar stalled: pick at random rather than hang the game. Random, not
                // a fixed index, because the pile is shuffled but index 0 would still
                // systematically reward whoever happened to land there.
                if((int32_t)(now - pt.deadline) >= 0)
                    fillblankAward((int)random(_fb.subCount), now);
            }
        } else if(pt.phase == 3) {
            if((int32_t)(now - pt.revealUntil) >= 0) fillblankNextRound(now);
        }
    }

    // A join/leave mid-game. Never leaves the round waiting on someone who is gone.
    void fillblankRosterChanged() {
        // A freed pid keeps its stale hand otherwise, and the next player to take that
        // slot would inherit it (and those cards would never return to the deck).
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(_p[i].used) continue;
            _fb.played[i] = -1;
            _fb.inRound[i] = false;
            for(int j = 0; j < FB_HAND; j++) _fb.hand[i][j] = -1;
        }
        if(_fb.pt.phase == 0 || _fb.pt.phase == 1) {
            fillblankCheckStart();
            return;
        }
        if(_fb.pt.phase != 2) return;
        // The Czar left: nobody can judge this round, so end it with no winner and let
        // the rotation carry on rather than sitting on the deadline.
        if(!_fb.czar || !_p[_fb.czar].used) {
            fillblankReveal(millis());
            return;
        }
        fillblankDealHands(); // a joiner gets a hand now, and plays from the next round
        if(_fb.stage == 0 && fillblankAllPlayed()) fillblankToJudge(millis());
    }

    String fillblankJson(uint8_t pid) {
        Party& pt = _fb.pt;
        if(pt.phase == 0) {
            String s = String("{\"t\":\"fillblank\",\"phase\":\"lobby\",\"you\":") + pid +
                       ",\"players\":" + partyPlayersJson(pt) + ",\"min\":" + FB_MIN_PLAYERS;
            s += ",\"packs\":[";
            int votes[FB_MAX_PACKS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _fb.vote[i] >= 0 && _fb.vote[i] < _fbPackCount)
                    votes[_fb.vote[i]]++;
            for(int i = 0; i < _fbPackCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_fbPacks[i].name.c_str()) +
                     "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_fb.vote[pid]) + "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"fillblank\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"fillblank\",\"phase\":\"final\",\"board\":") + triviaBoard() +
                   "}";

        FillBlankPack& pk = _fbPacks[_fb.pack];
        bool me = (pid == _fb.czar);
        bool reveal = (pt.phase == 3);
        const char* stage = reveal ? "reveal" : (_fb.stage == 0 ? "play" : "judge");

        String s = String("{\"t\":\"fillblank\",\"phase\":\"play\",\"stage\":\"") + stage +
                   "\",\"round\":" + pt.round + ",\"rounds\":" + FB_ROUNDS + ",\"czar\":\"" +
                   ha_json_escape(_p[_fb.czar].nick) + "\",\"iam\":" + (me ? "true" : "false") +
                   ",\"prompt\":\"" +
                   ha_json_escape(_fb.prompt < pk.pcount ? pk.prompts[_fb.prompt].c_str() : "") +
                   "\"";
        // Your own hand, slot-indexed (an empty slot stays "" so the index you tap is the
        // index the engine reads back). Only ever sent to its owner.
        if(!me) {
            s += ",\"hand\":[";
            for(int j = 0; j < FB_HAND; j++) {
                if(j) s += ",";
                int8_t c = _fb.hand[pid][j];
                s += "\"";
                if(c >= 0 && c < (int8_t)pk.acount) s += ha_json_escape(pk.answers[c].c_str());
                s += "\"";
            }
            s += "]";
            s += ",\"mine\":" + String((int)_fb.played[pid]);
            s += ",\"waiting\":" + String(_fb.inRound[pid] ? "false" : "true");
        }
        // `played` counts PLAYERS who are in, so it must not include the deck's card.
        s += ",\"played\":" + String(fillblankRealSubs()) + ",\"total\":" +
             String(fillblankExpected());
        // The shuffled pile. Card text only - no pid, no nick, and no hint of which entry
        // is the deck's - so nothing here maps a card to its author while the Czar judges.
        if(_fb.stage == 1 || reveal) {
            s += ",\"subs\":[";
            for(int i = 0; i < (int)_fb.subCount; i++) {
                if(i) s += ",";
                uint8_t c = _fb.subCard[i];
                s += "\"";
                if(c < pk.acount) s += ha_json_escape(pk.answers[c].c_str());
                s += "\"";
            }
            s += "]";
        }
        if(reveal) {
            // Authorship is released only now, and all of it at once: `authors` runs
            // parallel to `subs` so every card can be shown with the player who played it.
            // A card whose author has since left serializes as an empty nick, which is why
            // the deck's card is identified by its INDEX in `deckcard` rather than by an
            // empty author -- the two must not be confusable.
            s += ",\"authors\":[";
            int deckIdx = -1;
            for(int i = 0; i < (int)_fb.subCount; i++) {
                if(i) s += ",";
                uint8_t a = _fb.subPid[i];
                if(a == FB_DECK_PID) deckIdx = i;
                s += "\"";
                if(a != FB_DECK_PID && _p[a].used) s += ha_json_escape(_p[a].nick);
                s += "\"";
            }
            s += "]";
            s += ",\"deckcard\":" + String(deckIdx);
            s += ",\"pick\":" + String((int)_fb.picked);
            s += ",\"winner\":\"" +
                 ha_json_escape(_fb.winner && _p[_fb.winner].used ? _p[_fb.winner].nick : "") +
                 "\"";
            s += ",\"mywin\":" + String(_fb.winner && _fb.winner == pid ? "true" : "false");
            s += ",\"deckwon\":" + String(_fb.deckWon ? "true" : "false");
            s += ",\"czarpts\":" + String((int)_fb.czarGain);
            // What I personally earned this round: 1 as the winning author, 1 as the Czar
            // who picked a player's card, 0 otherwise.
            int mine = 0;
            if(_fb.winner && _fb.winner == pid) mine = 1;
            if(pid == _fb.czar) mine = (int)_fb.czarGain;
            s += ",\"mygain\":" + String(mine);
            s += ",\"deadline\":" + String(pt.revealUntil) + ",\"dur\":" +
                 String(FB_REVEAL_MS / 1000);
        } else {
            s += ",\"deadline\":" + String(pt.deadline) + ",\"dur\":" +
                 String(_fb.stage == 0 ? FB_PLAY_SECS : FB_PICK_SECS);
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }

    // ---------- Werewolf (hidden roles, night/day social deduction) ----------
    // The phones are the referee, not a chat client: players argue out loud in the
    // room and the engine only deals the roles, runs the clock, and resolves the
    // votes. Because everyone is in the same room and cannot whisper, the phone is
    // also the werewolves' only coordination channel -- hence the live pack tally.
    //
    // Secrecy is the game. Roles never leave this engine except through wwJson(),
    // which asks wwMaySeeRole() about every single player it emits -- so a role a
    // viewer is not entitled to simply is not in the bytes their phone receives.
    // The seer's reading, the doctor's shield and the wolves' night votes are gated
    // the same way. Nothing ever announces that a player failed to act: an idle
    // seer just gets no vision, because "the seer did nothing" is information.

    void wwClear() {
        partyClear(_ww.pt);
        _ww.stage = WW_S_ROLES;
        _ww.seer = 0;
        _ww.seerTarget = 0;
        _ww.seerResult = false;
        _ww.doctor = 0;
        _ww.docTarget = 0;
        _ww.docLast = 0;
        _ww.dealt = 0;
        _ww.victim = 0;
        _ww.dawnKind = WW_D_KILLED;
        _ww.lynched = 0;
        _ww.winner = 0;
        _ww.logN = 0;
        for(int i = 0; i < WW_MAX_LOG; i++) _ww.log[i] = WwDay{};
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _ww.role[i] = 0;
            _ww.alive[i] = false;
            _ww.revealed[i] = false;
            _ww.kill[i] = -1;
            _ww.accuse[i] = -1;
        }
    }

    // Living players still holding a role (a mid-game joiner has role 0 and only
    // watches, so they count for nothing here).
    int wwAliveWolves() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _ww.alive[i] && _ww.role[i] == WW_WOLF) n++;
        return n;
    }
    int wwAliveVillage() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _ww.alive[i] && _ww.role[i] != 0 && _ww.role[i] != WW_WOLF) n++;
        return n;
    }
    int wwAliveInGame() { return wwAliveWolves() + wwAliveVillage(); }

    // A strict majority of the living: the hammer threshold, and the only way a
    // day ends before its clock does.
    int wwHammerAt() { return wwAliveInGame() / 2 + 1; }

    // The day is as long as the room needs to actually talk (see WW_DAY_BASE).
    int wwDaySecs() {
        int s = WW_DAY_BASE + WW_DAY_PER * wwAliveInGame();
        if(s < WW_DAY_MIN) s = WW_DAY_MIN;
        if(s > WW_DAY_MAX) s = WW_DAY_MAX;
        return s;
    }

    // At a small table a night-one kill drops the game to four players with no
    // information at all, so the first night is a meeting, not a hunt. The seer
    // and doctor still act. Player count is public, so this leaks nothing.
    bool wwQuietNight() {
        return _ww.pt.round == 1 && _ww.dealt <= WW_QUIET_NIGHT_MAX;
    }

    void wwReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_WEREWOLF) return;
        if(_ww.pt.phase != 0 && _ww.pt.phase != 4) return;
        if(_ww.pt.phase == 4 && val) wwClear(); // ready from the final screen -> new game
        _ww.pt.ready[pid] = val;
        wwCheckStart();
        pushAll();
    }

    void wwAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_WEREWOLF || _ww.pt.phase != 4) return;
        wwClear();
        pushAll();
    }

    // Needs a real village: with fewer than WW_MIN_PLAYERS the role split is
    // degenerate, so the countdown simply does not arm.
    void wwCheckStart() {
        Party& pt = _ww.pt;
        bool enough = enoughPlayers(WW_MIN_PLAYERS);
        if(pt.phase == 0 && enough && partyAllReady(pt)) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && (!enough || !partyAllReady(pt))) {
            pt.phase = 0;
        }
    }

    // Deal roles over a shuffled roster: about one werewolf per four players (at
    // least one), one seer, one doctor from WW_DOCTOR_MIN players up, everyone
    // else a villager. The wolf count is capped so the village always starts
    // ahead AND always keeps at least one plain villager beside its specials.
    //
    //   5 -> 1 wolf, seer,          3 villagers      9..11 -> 2 wolves, seer, doctor
    //   6 -> 1 wolf, seer, doctor,  3 villagers         12 -> 3 wolves, seer, doctor
    //   7 -> 1 wolf, seer, doctor,  4 villagers
    //   8 -> 2 wolves, seer, doctor, 4 villagers
    void wwDeal() {
        uint8_t ord[HA_MAX_PLAYERS];
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used) ord[n++] = i;
        for(int i = n - 1; i > 0; i--) {
            int j = (int)(esp_random() % (uint32_t)(i + 1));
            uint8_t t = ord[i];
            ord[i] = ord[j];
            ord[j] = t;
        }
        bool doc = (n >= WW_DOCTOR_MIN);
        int specials = doc ? 2 : 1; // the seer, and the doctor once the table is big
        int cap = (n - 1) / 2;
        if(cap > n - specials - 1) cap = n - specials - 1;
        int wolves = n / 4;
        if(wolves < 1) wolves = 1;
        if(wolves > cap) wolves = cap;
        _ww.seer = 0;
        _ww.doctor = 0;
        _ww.dealt = (uint8_t)n;
        for(int i = 0; i < n; i++) {
            uint8_t pid = ord[i];
            _ww.alive[pid] = true;
            _ww.revealed[pid] = false;
            if(i < wolves) {
                _ww.role[pid] = WW_WOLF;
            } else if(i == wolves) {
                _ww.role[pid] = WW_SEER;
                _ww.seer = pid;
            } else if(doc && i == wolves + 1) {
                _ww.role[pid] = WW_DOCTOR;
                _ww.doctor = pid;
            } else {
                _ww.role[pid] = WW_VILLAGER;
            }
        }
    }

    // Does this player still owe a night action? Strictly per-viewer: the COUNT of
    // outstanding night actors is deliberately never published, because it would
    // tell the room how many special roles are still alive.
    bool wwOwesNight(uint8_t pid) {
        if(!_ww.alive[pid]) return false;
        if(_ww.role[pid] == WW_WOLF) return !wwQuietNight() && _ww.kill[pid] < 0;
        if(_ww.role[pid] == WW_SEER) return _ww.seerTarget == 0;
        if(_ww.role[pid] == WW_DOCTOR) return _ww.docTarget == 0;
        return false;
    }

    // Fill a pid-indexed ballot from the wolves' night picks. Stale targets (the
    // player left, or died) are dropped here rather than at tap time.
    void wwNightTally(int* votes) {
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_ww.alive[i] || _ww.role[i] != WW_WOLF) continue;
            int8_t t = _ww.kill[i];
            if(t < 1 || t > HA_MAX_PLAYERS || !_p[t].used) continue;
            if(!_ww.alive[t] || _ww.role[t] == WW_WOLF) continue;
            votes[t]++;
        }
    }

    // The pack's victim: most picks wins, ties broken uniformly at random (they
    // are a pack, not a committee). No wolf picked at all -> nobody dies; the
    // engine never invents a kill on their behalf.
    uint8_t wwNightVictim() {
        int votes[HA_MAX_PLAYERS + 1] = {0};
        wwNightTally(votes);
        int best = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(votes[i] > best) best = votes[i];
        if(best == 0) return 0;
        uint8_t tie[HA_MAX_PLAYERS];
        int tn = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(votes[i] == best) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void wwDayTally(int* votes) {
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_ww.alive[i] || _ww.role[i] == 0) continue;
            int8_t t = _ww.accuse[i];
            if(t < 1 || t > HA_MAX_PLAYERS || !_p[t].used) continue;
            if(!_ww.alive[t] || _ww.role[t] == 0) continue;
            votes[t]++;
        }
    }

    // The day ballot resolves differently from the night one on purpose: a tied
    // village is a village that could not agree, and executing a coin-flip winner
    // quietly favours the wolves. A tie -- like an empty ballot -- hangs nobody.
    uint8_t wwDayOutcast() {
        int votes[HA_MAX_PLAYERS + 1] = {0};
        wwDayTally(votes);
        int best = 0;
        uint8_t lead = 0;
        int tied = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(votes[i] > best) {
                best = votes[i];
                lead = i;
                tied = 1;
            } else if(votes[i] == best && best > 0) {
                tied++;
            }
        }
        if(best == 0 || tied > 1) return 0;
        return lead;
    }

    // Has anyone hit a strict majority of the living? That ends the day on the
    // spot (a "hammer"), which is fine to act on because the tally is public.
    uint8_t wwHammered() {
        int votes[HA_MAX_PLAYERS + 1] = {0};
        wwDayTally(votes);
        int need = wwHammerAt();
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(votes[i] >= need) return i;
        return 0;
    }

    void wwLog(uint8_t victim, uint8_t kind, uint8_t lynched, bool day) {
        if(!day) { // a night opens the entry
            if(_ww.logN >= WW_MAX_LOG) return;
            _ww.log[_ww.logN].victim = victim;
            _ww.log[_ww.logN].kind = kind;
            _ww.log[_ww.logN].lynched = 0;
            _ww.logN++;
        } else if(_ww.logN) { // the day that follows closes it
            _ww.log[_ww.logN - 1].lynched = lynched;
        }
    }

    // Villagers win when the last werewolf is out; werewolves win as soon as they
    // are no longer outnumbered (from there they can force any lynch they like).
    // Every player still alive on the winning side scores 1 -- surviving is the
    // whole job -- so the shared leaderboard keeps its meaning across games.
    bool wwCheckEnd(uint32_t now) {
        int w = wwAliveWolves(), v = wwAliveVillage();
        if(w > 0 && w < v) return false;
        _ww.winner = (w == 0) ? WW_VILLAGER : WW_WOLF;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || _ww.role[i] == 0) continue;
            _ww.revealed[i] = true; // the reckoning: every role is public now
            if(!_ww.alive[i]) continue;
            if((_ww.role[i] == WW_WOLF) != (_ww.winner == WW_WOLF)) continue;
            _p[i].score += 1;
            haUartScore(i, 1, "werewolf");
        }
        haUartRoundResult(
            String("{\"werewolf\":\"") + (_ww.winner == WW_WOLF ? "wolves" : "villagers") +
            " win\"}");
        _ww.pt.phase = 4;
        _ww.pt.deadline = now;
        pushAll();
        return true;
    }

    // Nightfall: the wolves converge on a victim, the seer checks somebody, the
    // doctor shields somebody, everyone else waits it out. Also the point where a
    // finished game is caught.
    void wwNight(uint32_t now) {
        if(wwCheckEnd(now)) return;
        Party& pt = _ww.pt;
        pt.round++;
        _ww.stage = WW_S_NIGHT;
        _ww.victim = 0;
        _ww.lynched = 0;
        _ww.seerTarget = 0; // last night's reading expires with the night
        _ww.seerResult = false;
        _ww.docTarget = 0; // ...and so does the shield (docLast remembers it)
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _ww.kill[i] = -1;
            _ww.accuse[i] = -1;
        }
        pt.deadline = now + (uint32_t)WW_NIGHT_SECS * 1000;
        pushAll();
    }

    void wwResolveNight(uint32_t now) {
        uint8_t v = wwQuietNight() ? 0 : wwNightVictim();
        if(wwQuietNight()) {
            _ww.dawnKind = WW_D_NOKILL;
        } else if(!v) {
            _ww.dawnKind = WW_D_QUIET;
        } else if(_ww.docTarget && _ww.docTarget == v) {
            _ww.dawnKind = WW_D_SAVED; // the doctor was standing in the doorway
            v = 0;
        } else {
            _ww.dawnKind = WW_D_KILLED;
        }
        _ww.victim = v;
        if(v) {
            _ww.alive[v] = false;
            _ww.revealed[v] = true; // a body's role is public
        }
        _ww.docLast = _ww.docTarget; // no shielding the same player twice running
        wwLog(v, _ww.dawnKind, 0, false);
        _ww.stage = WW_S_DAWN;
        _ww.pt.deadline = now + WW_ANNOUNCE_MS;
        // "draw" is the engine's generic host-facing status-line key (Spectrum
        // reuses it the same way); the Flipper console prints whatever it holds.
        haUartEvent(
            String("{\"draw\":\"night ") + _ww.pt.round + ": " +
            (v ? ha_json_escape(_p[v].nick) + " died" : String("nobody died")) + "\"}");
        pushAll();
    }

    void wwDay(uint32_t now) {
        if(wwCheckEnd(now)) return;
        _ww.stage = WW_S_DAY;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _ww.accuse[i] = -1;
        _ww.pt.deadline = now + (uint32_t)wwDaySecs() * 1000;
        pushAll();
    }

    void wwResolveDay(uint32_t now) {
        _ww.lynched = wwDayOutcast();
        if(_ww.lynched) {
            _ww.alive[_ww.lynched] = false;
            _ww.revealed[_ww.lynched] = true;
        }
        wwLog(0, 0, _ww.lynched, true);
        _ww.stage = WW_S_DUSK;
        _ww.pt.deadline = now + WW_ANNOUNCE_MS;
        haUartEvent(
            String("{\"draw\":\"day ") + _ww.pt.round + ": " +
            (_ww.lynched ? ha_json_escape(_p[_ww.lynched].nick) + " voted out" :
                           String("no majority")) +
            "\"}");
        pushAll();
    }

    // A wolf naming a victim. Pushes immediately so the rest of the pack watches
    // the tally move in real time -- they are sitting in the same room and cannot
    // say a word to each other, so this is their only channel.
    void wwKill(uint8_t pid, int target) {
        if(_active != HA_GAME_WEREWOLF || _ww.pt.phase != 2 || _ww.stage != WW_S_NIGHT) return;
        if(_ww.role[pid] != WW_WOLF || !_ww.alive[pid]) return;
        if(wwQuietNight()) return; // no hunt on a small table's first night
        if(target < 1 || target > HA_MAX_PLAYERS || !_p[target].used) return;
        // A wolf hunts outside the pack: living non-wolves only.
        if(!_ww.alive[target] || _ww.role[target] == 0 || _ww.role[target] == WW_WOLF) return;
        _ww.kill[pid] = (int8_t)target;
        pushAll(); // the night still runs its full length; only the tally moves
    }

    void wwSee(uint8_t pid, int target) {
        if(_active != HA_GAME_WEREWOLF || _ww.pt.phase != 2 || _ww.stage != WW_S_NIGHT) return;
        if(_ww.role[pid] != WW_SEER || !_ww.alive[pid]) return;
        if(_ww.seerTarget) return; // one reading per night
        if(target < 1 || target > HA_MAX_PLAYERS || !_p[target].used || target == pid) return;
        if(!_ww.alive[target] || _ww.role[target] == 0) return;
        _ww.seerTarget = (uint8_t)target;
        _ww.seerResult = (_ww.role[target] == WW_WOLF);
        pushAll();
    }

    // The doctor's shield. Self-protection is allowed (the usual default), but the
    // same player may not be shielded two nights running -- including themselves.
    void wwGuard(uint8_t pid, int target) {
        if(_active != HA_GAME_WEREWOLF || _ww.pt.phase != 2 || _ww.stage != WW_S_NIGHT) return;
        if(_ww.role[pid] != WW_DOCTOR || !_ww.alive[pid]) return;
        if(_ww.docTarget) return; // one shield per night
        if(target < 1 || target > HA_MAX_PLAYERS || !_p[target].used) return;
        if(!_ww.alive[target] || _ww.role[target] == 0) return;
        if((uint8_t)target == _ww.docLast) return; // not the same player twice running
        _ww.docTarget = (uint8_t)target;
        pushAll();
    }

    void wwAccuse(uint8_t pid, int target) {
        if(_active != HA_GAME_WEREWOLF || _ww.pt.phase != 2 || _ww.stage != WW_S_DAY) return;
        if(_ww.role[pid] == 0 || !_ww.alive[pid]) return; // the dead do not vote
        if(target < 1 || target > HA_MAX_PLAYERS || !_p[target].used || target == pid) return;
        if(!_ww.alive[target] || _ww.role[target] == 0) return;
        _ww.accuse[pid] = (int8_t)target;
        // A strict majority ends the day on the spot; anything short of that runs
        // the clock out, so a split room keeps arguing instead of being rushed.
        if(wwHammered()) wwResolveDay(millis());
        else pushAll();
    }

    // A join or a leave mid-game. A leaver is wiped completely (their pid can be
    // handed to the next player to join, who must arrive as a spectator), and the
    // win condition is re-checked because walking out can decide the game: the
    // last werewolf leaving is a village win.
    void wwRosterChanged() {
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(_p[i].used) continue;
            _ww.role[i] = 0;
            _ww.alive[i] = false;
            _ww.revealed[i] = false;
            _ww.kill[i] = -1;
            _ww.accuse[i] = -1;
            _ww.pt.ready[i] = false;
            if(_ww.seer == i) _ww.seer = 0;
            if(_ww.seerTarget == i) _ww.seerTarget = 0;
            if(_ww.doctor == i) _ww.doctor = 0;
            if(_ww.docTarget == i) _ww.docTarget = 0;
        }
        if(_ww.pt.phase != 2) {
            wwCheckStart();
            return;
        }
        if(wwAliveWolves() == 0 && wwAliveVillage() == 0) {
            wwClear(); // everyone holding a role walked out; back to the lobby
            return;
        }
        if(wwCheckEnd(millis())) return;
        // A departure shrinks the room, which can retroactively make a standing
        // tally a majority.
        if(_ww.stage == WW_S_DAY && wwHammered()) wwResolveDay(millis());
    }

    void wwTick(uint32_t now) {
        Party& pt = _ww.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                wwDeal();
                pt.phase = 2;
                _ww.stage = WW_S_ROLES;
                pt.deadline = now + (uint32_t)WW_ROLES_SECS * 1000;
                pushAll();
            }
        } else if(pt.phase == 2) {
            if((int32_t)(now - pt.deadline) < 0) return;
            if(_ww.stage == WW_S_ROLES || _ww.stage == WW_S_DUSK)
                wwNight(now);
            else if(_ww.stage == WW_S_NIGHT)
                wwResolveNight(now); // fixed-length: whoever did not act is skipped
            else if(_ww.stage == WW_S_DAWN)
                wwDay(now);
            else if(_ww.stage == WW_S_DAY)
                wwResolveDay(now);
        }
    }

    // THE secrecy rule, in one place. Everything wwJson() emits about somebody
    // else's role goes through here first.
    bool wwMaySeeRole(uint8_t viewer, uint8_t target) {
        if(_ww.role[target] == 0) return false; // spectators have no role to show
        if(viewer == target) return true; // your own role is always yours
        if(_ww.revealed[target]) return true; // dead, or the game is over
        return _ww.role[viewer] == WW_WOLF && _ww.role[target] == WW_WOLF; // the pack
    }

    static const char* wwStageName(uint8_t s) {
        switch(s) {
        case WW_S_ROLES:
            return "roles";
        case WW_S_NIGHT:
            return "night";
        case WW_S_DAWN:
            return "dawn";
        case WW_S_DAY:
            return "day";
        default:
            return "dusk";
        }
    }

    static const char* wwDawnName(uint8_t k) {
        switch(k) {
        case WW_D_SAVED:
            return "saved";
        case WW_D_QUIET:
            return "quiet";
        case WW_D_NOKILL:
            return "nokill";
        default:
            return "killed";
        }
    }

    // The roster as seen by `pid`: identity and life/death are public, a role is not.
    String wwRosterJson(uint8_t pid) {
        String s = "[";
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
            s += "\",\"in\":";
            s += _ww.role[i] ? "true" : "false";
            s += ",\"alive\":";
            s += _ww.alive[i] ? "true" : "false";
            if(wwMaySeeRole(pid, i)) {
                s += ",\"role\":";
                s += _ww.role[i];
            }
            s += "}";
        }
        s += "]";
        return s;
    }

    // Night-by-night summary for the final screen. Only ever emitted once the game
    // is over, when every role is public anyway.
    String wwLogJson() {
        String s = "[";
        for(int i = 0; i < _ww.logN; i++) {
            if(i) s += ",";
            s += "{\"day\":";
            s += (i + 1);
            s += ",\"victim\":";
            s += _ww.log[i].victim;
            s += ",\"kind\":\"";
            s += wwDawnName(_ww.log[i].kind);
            s += "\",\"lynched\":";
            s += _ww.log[i].lynched;
            s += "}";
        }
        s += "]";
        return s;
    }

    String wwJson(uint8_t pid) {
        Party& pt = _ww.pt;
        if(pt.phase == 0)
            return String("{\"t\":\"werewolf\",\"phase\":\"lobby\",\"you\":") + pid +
                   ",\"players\":" + partyPlayersJson(pt) + ",\"min\":" + WW_MIN_PLAYERS +
                   ",\"enough\":" + (enoughPlayers(WW_MIN_PLAYERS) ? "true" : "false") + "}";
        if(pt.phase == 1)
            return String("{\"t\":\"werewolf\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"werewolf\",\"phase\":\"final\",\"you\":") + pid +
                   ",\"winner\":\"" + (_ww.winner == WW_WOLF ? "wolves" : "villagers") +
                   "\",\"myrole\":" + _ww.role[pid] + ",\"players\":" + wwRosterJson(pid) +
                   ",\"log\":" + wwLogJson() + ",\"board\":" + triviaBoard() + "}";

        String s = String("{\"t\":\"werewolf\",\"phase\":\"play\",\"stage\":\"") +
                   wwStageName(_ww.stage) + "\",\"you\":" + pid + ",\"day\":" + pt.round +
                   ",\"myrole\":" + _ww.role[pid] +
                   ",\"alive\":" + (_ww.alive[pid] ? "true" : "false") +
                   ",\"wolvesleft\":" + wwAliveWolves() + ",\"villagersleft\":" + wwAliveVillage() +
                   ",\"players\":" + wwRosterJson(pid);

        if(_ww.stage == WW_S_NIGHT) {
            // Whether YOU still owe an action. Never how many others do -- that
            // count is a headcount of the surviving special roles.
            s += ",\"owe\":";
            s += wwOwesNight(pid) ? "true" : "false";
            if(wwQuietNight()) s += ",\"nokill\":true"; // public: derived from the player count
            // The pack's own tally, pushed on every tap so the wolves can converge
            // without speaking. Wolves only: a villager's payload carries no trace
            // that a night vote is even happening.
            if(_ww.role[pid] == WW_WOLF && _ww.alive[pid]) {
                s += ",\"mykill\":";
                s += (int)_ww.kill[pid];
                s += ",\"packsize\":";
                s += wwAliveWolves();
                s += ",\"packvotes\":[";
                bool first = true;
                for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                    if(!_p[i].used || !_ww.alive[i] || _ww.role[i] != WW_WOLF) continue;
                    if(_ww.kill[i] < 0) continue;
                    if(!first) s += ",";
                    first = false;
                    s += "{\"by\":";
                    s += i;
                    s += ",\"pid\":";
                    s += (int)_ww.kill[i];
                    s += "}";
                }
                s += "]";
            }
            // The doctor's shield, and the target they are barred from repeating.
            // Only ever in the doctor's own payload.
            if(_ww.role[pid] == WW_DOCTOR && _ww.alive[pid]) {
                s += ",\"myguard\":";
                s += _ww.docTarget;
                s += ",\"lastguard\":";
                s += _ww.docLast;
            }
        }
        // The seer's reading, from the moment they look until the next night falls
        // -- and only ever in the seer's own payload.
        if(_ww.role[pid] == WW_SEER && _ww.seerTarget && _p[_ww.seerTarget].used) {
            s += ",\"check\":{\"pid\":";
            s += _ww.seerTarget;
            s += ",\"nick\":\"";
            s += ha_json_escape(_p[_ww.seerTarget].nick);
            s += "\",\"wolf\":";
            s += _ww.seerResult ? "true" : "false";
            s += "}";
        }
        if(_ww.stage == WW_S_DAWN) {
            s += ",\"victim\":";
            s += _ww.victim;
            s += ",\"dawnkind\":\"";
            s += wwDawnName(_ww.dawnKind);
            s += "\"";
        }
        if(_ww.stage == WW_S_DAY) {
            // The day vote is out loud by design: everyone watches the tally build,
            // so the outstanding-vote count is public information too.
            int pending = 0;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _ww.alive[i] && _ww.role[i] != 0 && _ww.accuse[i] < 0) pending++;
            s += ",\"myvote\":";
            s += (int)_ww.accuse[pid];
            s += ",\"owe\":";
            s += (_ww.alive[pid] && _ww.role[pid] && _ww.accuse[pid] < 0) ? "true" : "false";
            s += ",\"waiting\":";
            s += pending;
            s += ",\"voters\":";
            s += wwAliveInGame();
            s += ",\"needed\":";
            s += wwHammerAt();
            s += ",\"votes\":[";
            bool first = true;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                if(!_p[i].used || _ww.accuse[i] < 0) continue;
                if(!first) s += ",";
                first = false;
                s += "{\"by\":";
                s += i;
                s += ",\"pid\":";
                s += (int)_ww.accuse[i];
                s += "}";
            }
            s += "]";
        }
        if(_ww.stage == WW_S_DUSK) {
            s += ",\"lynched\":";
            s += _ww.lynched;
        }
        s += ",\"deadline\":";
        s += pt.deadline;
        s += ",\"dur\":";
        s += (_ww.stage == WW_S_ROLES ? WW_ROLES_SECS :
              _ww.stage == WW_S_NIGHT ? WW_NIGHT_SECS :
              _ww.stage == WW_S_DAY   ? wwDaySecs() :
                                        (int)(WW_ANNOUNCE_MS / 1000));
        s += "}";
        return s;
    }

    // ---------- Spyfall (one player doesn't know where they are) ----------
    // Which pack wins the pre-game vote; identical policy to wyrWinningPack(), just
    // over SPYFALL_MAX_PACKS instead of the shared topic cap.
    int spyfallWinningPack() {
        if(_sfPackCount == 0) return 0;
        int votes[SPYFALL_MAX_PACKS] = {0};
        int total = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _sf.vote[i] >= 0 && _sf.vote[i] < _sfPackCount) {
                votes[_sf.vote[i]]++;
                total++;
            }
        if(total == 0) return (int)random(_sfPackCount);
        int best = 0;
        for(int i = 1; i < _sfPackCount; i++)
            if(votes[i] > votes[best]) best = i;
        int tie[SPYFALL_MAX_PACKS], tn = 0;
        for(int i = 0; i < _sfPackCount; i++)
            if(votes[i] == votes[best]) tie[tn++] = i;
        return tie[(int)random(tn)];
    }

    void spyfallClear() {
        partyClear(_sf.pt);
        _sf.pack = 0;
        _sf.locSeq = 0;
        _sf.loc = 0;
        _sf.spy = 0;
        _sf.spySeq = 0;
        _sf.stage = 0;
        _sf.nomStage = 0;
        _sf.nominator = 0;
        _sf.nominee = 0;
        _sf.missCount = 0;
        _sf.outcome = 0;
        _sf.called = -1;
        _sf.blamed = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _sf.vote[i] = -1;
            _sf.inRound[i] = false;
            _sf.role[i] = -1;
            _sf.seen[i] = false;
            _sf.spent[i] = false;
            _sf.nominated[i] = false;
            _sf.agree[i] = -1;
            _sf.gained[i] = 0;
        }
    }

    void spyfallReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_SPYFALL) return;
        if(_sf.pt.phase != 0 && _sf.pt.phase != 4) return;
        if(_sf.pt.phase == 4 && val) spyfallClear(); // ready from final -> new game
        _sf.pt.ready[pid] = val;
        spyfallCheckStart();
        pushAll();
    }

    void spyfallVote(uint8_t pid, int pack) {
        if(_active != HA_GAME_SPYFALL || _sf.pt.phase != 0) return;
        if(pack < 0 || pack >= _sfPackCount) return;
        _sf.vote[pid] = (int8_t)pack;
        pushAll();
    }

    // Unlike the other party games this one needs a quorum: with two players the spy
    // is whoever isn't you, so the lobby holds until SPYFALL_MIN_PLAYERS are in.
    void spyfallCheckStart() {
        if(_sfPackCount == 0) return;
        Party& pt = _sf.pt;
        bool go = partyAllReady(pt) && enoughPlayers(SPYFALL_MIN_PLAYERS);
        if(pt.phase == 0 && go) {
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && !go) {
            pt.phase = 0;
        }
    }

    // The spy rotates across rounds: the (spySeq mod N)-th connected player, the same
    // walk spectrum uses for its psychic, so over a game everyone takes a turn.
    uint8_t spyfallPickSpy() {
        int n = connectedCount();
        if(n <= 0) return 0;
        int want = _sf.spySeq % n, seen = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(seen == want) return i;
            seen++;
        }
        return 0;
    }

    // How many seats are still in this round, and how many of them are not the spy --
    // the latter is the agreement threshold a nomination has to clear.
    int spyfallPlaying() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _sf.inRound[i]) n++;
        return n;
    }
    int spyfallNeed() {
        int n = spyfallPlaying();
        return n > 1 ? n - 1 : 1;
    }

    void spyfallNextRound(uint32_t now) {
        Party& pt = _sf.pt;
        SpyPack& pk = _sfPacks[_sf.pack];
        if(pt.round >= SPYFALL_ROUNDS || pk.count == 0 ||
           connectedCount() < SPYFALL_MIN_PLAYERS) {
            pt.phase = 4; // final
            pushAll();
            return;
        }
        pt.round++;
        _sf.spy = spyfallPickSpy();
        _sf.spySeq++;
        _sf.loc = (uint8_t)(_sf.locSeq % pk.count);
        _sf.locSeq++;
        _sf.stage = 0; // read your card first; the clock waits for that
        _sf.nomStage = 0;
        _sf.nominator = 0;
        _sf.nominee = 0;
        _sf.missCount = 0;
        _sf.outcome = 0;
        _sf.called = -1;
        _sf.blamed = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _sf.inRound[i] = false;
            _sf.role[i] = -1;
            _sf.seen[i] = false;
            _sf.spent[i] = false; // the one-accusation lockout is per ROUND
            _sf.nominated[i] = false;
            _sf.agree[i] = -1;
            _sf.gained[i] = 0;
        }
        // Deal the roles from a shuffled order so the same seat doesn't keep drawing
        // the pack's first role, and so a table smaller than the role list still gets
        // a varied spread. With more players than roles they simply wrap and repeat.
        uint8_t order[SPYFALL_MAX_ROLES];
        uint8_t rc = pk.locs[_sf.loc].roleCount;
        for(uint8_t i = 0; i < rc; i++) order[i] = i;
        for(int i = (int)rc - 1; i > 0; i--) {
            int j = (int)random(i + 1);
            uint8_t t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        int next = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            _sf.inRound[i] = true;
            if(i == _sf.spy) continue; // the spy gets no role, and never will
            if(rc) _sf.role[i] = (int8_t)order[next++ % rc];
        }
        pt.deadline = now + (uint32_t)SPYFALL_CARD_SECS * 1000;
        pt.phase = 2;
        pushAll();
    }

    bool spyfallAllSeen() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_sf.inRound[i]) continue;
            n++;
            if(!_sf.seen[i]) return false;
        }
        return n >= 1;
    }

    // The card goes away and the six minutes begin. Deliberately NOT started at round
    // start: hardware play showed the round felt "far too fast" when the clock was
    // already running while people were still reading their own card.
    void spyfallBeginTalk(uint32_t now) {
        _sf.stage = 1;
        _sf.pt.deadline = now + (uint32_t)SPYFALL_TALK_SECS * 1000;
        pushAll();
    }

    void spyfallSeen(uint8_t pid) {
        if(_active != HA_GAME_SPYFALL || _sf.pt.phase != 2 || _sf.stage != 0) return;
        if(!_sf.inRound[pid] || _sf.seen[pid]) return;
        _sf.seen[pid] = true;
        if(spyfallAllSeen()) spyfallBeginTalk(millis());
        else pushAll();
    }

    // "I know the spy", open to EVERY player including the spy -- that is the point:
    // pressing it is a bluff the spy can use for cover, at the cost of their own one
    // press. Right ends the round; wrong burns that player's press for the round and
    // play carries straight on.
    void spyfallAccuse(uint8_t pid, int target) {
        if(_active != HA_GAME_SPYFALL || _sf.pt.phase != 2 || _sf.stage != 1) return;
        if(!_sf.inRound[pid] || _sf.spent[pid]) return;
        if(target < 1 || target > HA_MAX_PLAYERS || (uint8_t)target == pid) return;
        if(!_p[target].used || !_sf.inRound[target]) return;
        _sf.spent[pid] = true;
        if((uint8_t)target == _sf.spy) {
            _sf.blamed = (uint8_t)target;
            spyfallReveal(millis(), SPYFALL_OUT_CAUGHT);
            return;
        }
        if(_sf.missCount < HA_MAX_PLAYERS) {
            _sf.missBy[_sf.missCount] = pid;
            _sf.missOf[_sf.missCount] = (uint8_t)target;
            _sf.missCount++;
        }
        haUartEvent(
            String("{\"spyfall\":\"") + ha_json_escape(_p[pid].nick) + " missed " +
            ha_json_escape(_p[target].nick) + "\"}");
        pushAll();
    }

    // "I know the location", spy only, any time during the questioning. Right or wrong
    // it settles the round -- that is the spy's gamble.
    void spyfallSolve(uint8_t pid, int loc) {
        if(_active != HA_GAME_SPYFALL || _sf.pt.phase != 2 || _sf.stage != 1) return;
        if(pid != _sf.spy || !_sf.inRound[pid]) return;
        SpyPack& pk = _sfPacks[_sf.pack];
        if(loc < 0 || loc >= pk.count) return;
        _sf.called = (int8_t)loc;
        spyfallReveal(
            millis(), loc == (int)_sf.loc ? SPYFALL_OUT_SOLVED : SPYFALL_OUT_FAILED);
    }

    // Six minutes gone with nobody daring to press anything. Stop the discussion, then
    // work round the table: each seat nominates once, and a nomination only sticks if
    // as many players agree as there are non-spies.
    void spyfallBeginNominate(uint32_t now) {
        _sf.stage = 2;
        _sf.nomStage = 0; // "Time's up. Stop discussing!"
        _sf.nominator = 0;
        _sf.nominee = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) {
            _sf.nominated[i] = false;
            _sf.agree[i] = -1;
        }
        _sf.pt.deadline = now + SPYFALL_HUSH_MS;
        pushAll();
    }

    // Hand the nomination to the next seat that hasn't had a turn. Once every seat has
    // tried and none of them stuck, the spy has survived the table and wins.
    void spyfallNextNominator(uint32_t now) {
        uint8_t start = _sf.nominator, pick = 0;
        for(int step = 1; step <= HA_MAX_PLAYERS; step++) {
            uint8_t i = (uint8_t)(((start + step - 1) % HA_MAX_PLAYERS) + 1);
            if(_p[i].used && _sf.inRound[i] && !_sf.nominated[i]) {
                pick = i;
                break;
            }
        }
        if(!pick) {
            spyfallReveal(now, SPYFALL_OUT_ESCAPED);
            return;
        }
        _sf.nominator = pick;
        _sf.nominee = 0;
        _sf.nomStage = 1;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _sf.agree[i] = -1;
        _sf.pt.deadline = now + (uint32_t)SPYFALL_NOM_SECS * 1000;
        pushAll();
    }

    bool spyfallPollDone() {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_sf.inRound[i]) continue;
            n++;
            if(_sf.agree[i] < 0) return false;
        }
        return n >= 1;
    }

    void spyfallNominate(uint8_t pid, int target) {
        if(_active != HA_GAME_SPYFALL || _sf.pt.phase != 2 || _sf.stage != 2) return;
        if(_sf.nomStage != 1 || pid != _sf.nominator) return;
        if(target < 1 || target > HA_MAX_PLAYERS || (uint8_t)target == pid) return;
        if(!_p[target].used || !_sf.inRound[target]) return;
        _sf.nominee = (uint8_t)target;
        _sf.nomStage = 2;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _sf.agree[i] = -1;
        _sf.agree[pid] = 1; // you're in on your own nomination by definition
        _sf.pt.deadline = millis() + (uint32_t)SPYFALL_POLL_SECS * 1000;
        if(spyfallPollDone()) spyfallResolvePoll(millis());
        else pushAll();
    }

    void spyfallAgree(uint8_t pid, bool yes) {
        if(_active != HA_GAME_SPYFALL || _sf.pt.phase != 2 || _sf.stage != 2) return;
        if(_sf.nomStage != 2 || !_sf.inRound[pid]) return;
        if(_sf.agree[pid] >= 0) return; // one answer each, no changing your mind
        _sf.agree[pid] = yes ? 1 : 0;
        if(spyfallPollDone()) spyfallResolvePoll(millis());
        else pushAll();
    }

    // A nomination that clears the threshold settles the round either way: name the spy
    // and the table scores, condemn an innocent and the spy walks. Short of the
    // threshold the turn simply passes on.
    void spyfallResolvePoll(uint32_t now) {
        int yes = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _sf.inRound[i] && _sf.agree[i] == 1) yes++;
        if(_sf.nominee && _p[_sf.nominee].used && yes >= spyfallNeed()) {
            _sf.blamed = _sf.nominee;
            spyfallReveal(
                now, _sf.nominee == _sf.spy ? SPYFALL_OUT_CAUGHT : SPYFALL_OUT_ESCAPED);
            return;
        }
        _sf.nominated[_sf.nominator] = true;
        _sf.nominee = 0;
        spyfallNextNominator(now);
    }

    // Scoring: every outcome is worth exactly 1 point, so the shared leaderboard stays
    // comparable with the other games. The non-spies take 1 each for naming the spy or
    // for a blown location call; the spy takes 1 for calling the location right, for
    // being condemned-by-proxy (an innocent nominated through), or for surviving the
    // whole table's nominations. A round aborted by the spy leaving scores nobody.
    void spyfallReveal(uint32_t now, uint8_t outcome) {
        _sf.outcome = outcome;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _sf.gained[i] = 0;
        int spyPts = 0, teamPts = 0;
        if(outcome == SPYFALL_OUT_CAUGHT)
            teamPts = 1;
        else if(outcome == SPYFALL_OUT_ESCAPED)
            spyPts = 1;
        else if(outcome == SPYFALL_OUT_SOLVED)
            spyPts = 1;
        else if(outcome == SPYFALL_OUT_FAILED)
            teamPts = 1;
        if(spyPts && _sf.spy && _p[_sf.spy].used) {
            _sf.gained[_sf.spy] = spyPts;
            _p[_sf.spy].score += spyPts;
            haUartScore(_sf.spy, spyPts, "spy");
        }
        if(teamPts) {
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                if(!_p[i].used || !_sf.inRound[i] || i == _sf.spy) continue;
                _sf.gained[i] = teamPts;
                _p[i].score += teamPts;
                haUartScore(i, teamPts, "spyfall");
            }
        }
        haUartRoundResult(String("{\"spyfall\":\"round ") + _sf.pt.round + "\"}");
        _sf.pt.phase = 3;
        _sf.pt.revealUntil = now + SPYFALL_REVEAL_MS;
        pushAll();
    }

    void spyfallAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_SPYFALL || _sf.pt.phase != 4) return;
        spyfallClear();
        pushAll();
    }

    // A join or a leave mid-game. Joiners are simply not in inRound[] and wait for the
    // next round. If the spy walks out there is no round left to referee, so it ends
    // scoring nobody and the rotation carries on; the same applies if the table falls
    // under the quorum. Otherwise a leaver can unblock whatever the room was waiting on.
    void spyfallRosterChanged() {
        // Scrub a departed player's per-round state before anything reads it, so a phone that
        // later reuses their freed pid starts as an out-of-round spectator. Without this, the
        // reused pid inherits a stale inRound + role and spyfallJson() would hand the newcomer
        // the secret location -- mirrors wwRosterChanged / fillblankRosterChanged.
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(_p[i].used) continue;
            _sf.inRound[i] = false;
            _sf.role[i] = -1;
            _sf.seen[i] = false;
            _sf.spent[i] = false;
            _sf.nominated[i] = false;
            _sf.agree[i] = -1;
            _sf.vote[i] = -1;
            _sf.gained[i] = 0;
        }
        spyfallCheckStart();
        Party& pt = _sf.pt;
        if(pt.phase != 2) return;
        uint32_t now = millis();
        if(!_sf.spy || !_p[_sf.spy].used || spyfallPlaying() < SPYFALL_MIN_PLAYERS) {
            spyfallReveal(now, SPYFALL_OUT_ABORT);
            return;
        }
        if(_sf.stage == 0) {
            if(spyfallAllSeen()) spyfallBeginTalk(now);
            return;
        }
        if(_sf.stage != 2) return;
        if(_sf.nomStage == 1 && (!_sf.nominator || !_p[_sf.nominator].used)) {
            spyfallNextNominator(now); // the nominator walked off mid-turn
        } else if(_sf.nomStage == 2) {
            if(!_sf.nominee || !_p[_sf.nominee].used) {
                // Whoever was accused has left; that nomination cannot stand.
                _sf.nominated[_sf.nominator] = true;
                _sf.nominee = 0;
                spyfallNextNominator(now);
            } else if(spyfallPollDone()) {
                spyfallResolvePoll(now);
            }
        }
    }

    // Every wait has a deadline, so no single silent phone can hold up the room: the
    // card acknowledgement, the questioning clock, the hush, each nomination turn, and
    // each agreement poll.
    void spyfallTick(uint32_t now) {
        Party& pt = _sf.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) {
                pt.round = 0;
                _sf.pack = (uint8_t)spyfallWinningPack();
                _sf.spySeq = 0;
                _sf.locSeq = 0;
                spyfallNextRound(now);
            }
        } else if(pt.phase == 2) {
            if((int32_t)(now - pt.deadline) < 0) return;
            if(_sf.stage == 0) {
                spyfallBeginTalk(now);
            } else if(_sf.stage == 1) {
                // The clock running out does NOT end the round -- it moves the table to
                // nominations, which somebody still has to win.
                spyfallBeginNominate(now);
            } else if(_sf.nomStage == 0) {
                spyfallNextNominator(now);
            } else if(_sf.nomStage == 1) {
                _sf.nominated[_sf.nominator] = true; // sat on their hands; turn passes
                spyfallNextNominator(now);
            } else {
                spyfallResolvePoll(now);
            }
        } else if(pt.phase == 3) {
            if((int32_t)(now - pt.revealUntil) >= 0) spyfallNextRound(now);
        }
    }

    static const char* spyfallOutcomeName(uint8_t o) {
        switch(o) {
        case SPYFALL_OUT_CAUGHT:
            return "caught";
        case SPYFALL_OUT_ESCAPED:
            return "escaped";
        case SPYFALL_OUT_SOLVED:
            return "solved";
        case SPYFALL_OUT_FAILED:
            return "failed";
        default:
            return "aborted";
        }
    }

    // Everyone still in the round, as {pid,nick,avatar}. Used for the "I know the spy"
    // picker and the nomination picker; it is the plain roster, nothing secret.
    String spyfallCandsJson() {
        String s = "[";
        bool first = true;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used || !_sf.inRound[i]) continue;
            if(!first) s += ",";
            first = false;
            s += "{\"pid\":" + String(i) + ",\"nick\":\"" + ha_json_escape(_p[i].nick) +
                 "\",\"avatar\":\"" + ha_json_escape(_p[i].avatar) + "\"}";
        }
        s += "]";
        return s;
    }

    // Failed accusations so far this round, so every phone can show them.
    String spyfallMissesJson() {
        String s = "[";
        for(uint8_t i = 0; i < _sf.missCount; i++) {
            if(i) s += ",";
            s += "{\"by\":\"" + ha_json_escape(_p[_sf.missBy[i]].nick) + "\",\"of\":\"" +
                 ha_json_escape(_p[_sf.missOf[i]].nick) + "\"}";
        }
        s += "]";
        return s;
    }

    // THE hidden-information gate for this game. Everything secret is filtered here
    // and nowhere else, exactly as spectrumJson() gates its target:
    //   * the location name is written into a non-spy's payload from the start of the
    //     round, and into the SPY's payload only once phase == 3 (reveal). There is no
    //     other branch that can emit it, and the location INDEX is never serialized at
    //     all, so nothing derivable leaks either;
    //   * a role is only ever written into its own holder's payload -- the full role
    //     list appears only on reveal;
    //   * a mid-round joiner (inRound false) gets neither, whichever they'd have been.
    // Note the card is hidden on the PHONE by default and only shown while the player
    // holds the button down; that is presentation. The rule above is what makes it safe.
    String spyfallJson(uint8_t pid) {
        Party& pt = _sf.pt;
        if(pt.phase == 0) {
            String s = String("{\"t\":\"spyfall\",\"phase\":\"lobby\",\"you\":") + pid +
                       ",\"need\":" + SPYFALL_MIN_PLAYERS +
                       ",\"players\":" + partyPlayersJson(pt);
            s += ",\"packs\":[";
            int votes[SPYFALL_MAX_PACKS] = {0};
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _sf.vote[i] >= 0 && _sf.vote[i] < _sfPackCount)
                    votes[_sf.vote[i]]++;
            for(int i = 0; i < _sfPackCount; i++) {
                if(i) s += ",";
                s += "{\"name\":\"" + ha_json_escape(_sfPacks[i].name.c_str()) +
                     "\",\"votes\":" + votes[i] + "}";
            }
            s += "],\"myvote\":" + String((int)_sf.vote[pid]) + "}";
            return s;
        }
        if(pt.phase == 1)
            return String("{\"t\":\"spyfall\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"spyfall\",\"phase\":\"final\",\"board\":") +
                   triviaBoard() + "}";

        SpyPack& pk = _sfPacks[_sf.pack];
        bool reveal = (pt.phase == 3);
        bool mine = _sf.inRound[pid];
        bool meSpy = (mine && pid == _sf.spy);
        const char* stage = reveal      ? "reveal" :
                            _sf.stage == 0 ? "card" :
                            _sf.stage == 1 ? "talk" :
                                             "nominate";

        String s = String("{\"t\":\"spyfall\",\"phase\":\"play\",\"stage\":\"") + stage +
                   "\",\"round\":" + pt.round + ",\"rounds\":" + SPYFALL_ROUNDS +
                   ",\"me\":" + (mine ? "true" : "false") +
                   ",\"spy\":" + (meSpy ? "true" : "false");
        if(mine && !meSpy && _sf.role[pid] >= 0)
            s += ",\"role\":\"" +
                 ha_json_escape(pk.locs[_sf.loc].roles[_sf.role[pid]].c_str()) + "\"";
        if(reveal || (mine && !meSpy))
            s += ",\"loc\":\"" + ha_json_escape(pk.locs[_sf.loc].name.c_str()) + "\"";
        if(meSpy) {
            // The candidate list -- never which one is right -- so the spy can bluff
            // along and has something to call when they think they've worked it out.
            s += ",\"locs\":[";
            for(int i = 0; i < pk.count; i++) {
                if(i) s += ",";
                s += "\"" + ha_json_escape(pk.locs[i].name.c_str()) + "\"";
            }
            s += "]";
        }
        if(!reveal && _sf.stage == 0) {
            int seen = 0;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                if(_p[i].used && _sf.inRound[i] && _sf.seen[i]) seen++;
            s += ",\"seen\":" + String(seen) + ",\"total\":" + spyfallPlaying() +
                 ",\"myseen\":" + (_sf.seen[pid] ? "true" : "false");
        }
        if(!reveal && _sf.stage == 1) {
            s += ",\"cands\":" + spyfallCandsJson() +
                 ",\"spent\":" + String(_sf.spent[pid] ? "true" : "false") +
                 ",\"misses\":" + spyfallMissesJson();
        }
        if(!reveal && _sf.stage == 2) {
            const char* ns = _sf.nomStage == 0 ? "hush" : (_sf.nomStage == 1 ? "pick" : "poll");
            s += ",\"nomStage\":\"" + String(ns) + "\",\"cands\":" + spyfallCandsJson() +
                 ",\"need\":" + spyfallNeed();
            if(_sf.nominator)
                s += ",\"nominator\":" + String(_sf.nominator) + ",\"nominatorNick\":\"" +
                     ha_json_escape(_p[_sf.nominator].nick) + "\"";
            s += ",\"nomMe\":" + String(_sf.nominator == pid ? "true" : "false");
            if(_sf.nomStage == 2 && _sf.nominee) {
                int yes = 0;
                for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
                    if(_p[i].used && _sf.inRound[i] && _sf.agree[i] == 1) yes++;
                s += ",\"nominee\":" + String(_sf.nominee) + ",\"nomineeNick\":\"" +
                     ha_json_escape(_p[_sf.nominee].nick) + "\",\"agreed\":" + yes +
                     ",\"myagree\":" + String((int)_sf.agree[pid]);
            }
        }
        if(reveal) {
            s += ",\"outcome\":\"" + String(spyfallOutcomeName(_sf.outcome)) +
                 "\",\"spyPid\":" + _sf.spy + ",\"spyNick\":\"" +
                 ha_json_escape(_p[_sf.spy].nick) + "\"";
            if(_sf.called >= 0 && _sf.called < (int8_t)pk.count)
                s += ",\"called\":\"" +
                     ha_json_escape(pk.locs[_sf.called].name.c_str()) + "\"";
            if(_sf.blamed && _p[_sf.blamed].used)
                s += ",\"blamedNick\":\"" + ha_json_escape(_p[_sf.blamed].nick) + "\"";
            s += ",\"misses\":" + spyfallMissesJson();
            s += ",\"roles\":[";
            bool first = true;
            for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
                if(!_p[i].used || !_sf.inRound[i]) continue;
                if(!first) s += ",";
                first = false;
                s += "{\"pid\":" + String(i) + ",\"nick\":\"" +
                     ha_json_escape(_p[i].nick) + "\",\"role\":\"";
                if(i != _sf.spy && _sf.role[i] >= 0)
                    s += ha_json_escape(pk.locs[_sf.loc].roles[_sf.role[i]].c_str());
                s += "\",\"spy\":" + String(i == _sf.spy ? "true" : "false") + "}";
            }
            s += "],\"mygain\":" + String(_sf.gained[pid]);
            s += ",\"deadline\":" + String(pt.revealUntil) + ",\"dur\":" +
                 String(SPYFALL_REVEAL_MS / 1000);
        } else {
            int dur = SPYFALL_CARD_SECS;
            if(_sf.stage == 1)
                dur = SPYFALL_TALK_SECS;
            else if(_sf.stage == 2)
                dur = _sf.nomStage == 0 ? (SPYFALL_HUSH_MS / 1000) :
                      _sf.nomStage == 1 ? SPYFALL_NOM_SECS :
                                          SPYFALL_POLL_SECS;
            s += ",\"deadline\":" + String(pt.deadline) + ",\"dur\":" + String(dur);
        }
        s += ",\"scores\":" + playersJson() + "}";
        return s;
    }
    // ---------- Frankendraw (exquisite corpse: head / torso / legs) ----------
    // Shown to players as "Draw a Monster"; the code, the wire name and the client
    // module keep the fd/frankendraw names, so the protocol is unaffected by the label.
    // Rotation rule. `seat` freezes the table when the game starts (everyone connected
    // at that moment, in pid order) and is never re-filled. In round r seat k holds
    // sheet (k + seats - (r-1)) % seats, so every sheet moves one seat per round and,
    // with seats >= FD_MIN_PLAYERS, is drawn by three different players.
    //  - Joining mid-game: no seat, so nothing to draw on; the new player watches and
    //    plays from the next game (fdJson sends them "wait").
    //  - Leaving mid-game: the sheet in their hands is NOT reassigned. Everyone still
    //    at the table is already holding a sheet of their own, so handing it on would
    //    mean giving somebody two panels to draw at once. The sheet just rotates on to
    //    its next scheduled holder as if the round had happened, which keeps every
    //    other sheet's schedule -- and the "three different hands" guarantee for them
    //    -- intact. A panel is credited to whoever was holding it when the round
    //    started (so a drawer who quits halfway still gets their ink and their name),
    //    and stays blank and uncredited if that seat was already empty.
    // Frankendraw's stroke store is large (~28 KB) and only needed while the game runs, so it
    // is allocated on demand rather than living in static DRAM. ps_malloc prefers PSRAM (S2/C5)
    // and falls back to internal heap; malloc is the second fallback for a board with no PSRAM.
    // Returns false only when neither has the room, in which case Frankendraw refuses to start
    // (fdCheckStart / fdBegin) and every other game keeps working.
    bool fdSheetsEnsure() {
        if(_fdSheets) return true;
        size_t bytes = sizeof(FdSheet) * HA_MAX_PLAYERS;
        _fdSheets = (FdSheet*)ps_malloc(bytes);
        if(!_fdSheets) _fdSheets = (FdSheet*)malloc(bytes);
        if(_fdSheets) memset(_fdSheets, 0, bytes);
        return _fdSheets != nullptr;
    }
    void fdSheetsFree() {
        if(!_fdSheets) return;
        free(_fdSheets);
        _fdSheets = nullptr;
    }

    void fdClear() {
        partyClear(_fd.pt);
        _fd.seats = 0;
        _fd.stage = 0;
        _fd.show = 0;
        _fd.best = 0;
        _fd.bestNet = 0;
        for(int i = 0; i < HA_MAX_PLAYERS; i++) {
            _fd.seat[i] = 0;
            // The store is only allocated while Frankendraw is active; fdClear() also runs from
            // reset()/selectGame() when it is not, so skip the sheet wipe when it is absent.
            if(_fdSheets) _fdSheets[i] = FdSheet{};
        }
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) fdForgetPlayer((uint8_t)i);
    }

    // Per-player state that must not survive a pid being recycled onto a new arrival.
    void fdForgetPlayer(uint8_t pid) {
        _fd.done[pid] = false;
        _fd.artSent[pid] = -1; // "has seen no picture yet"
        for(int s = 0; s < HA_MAX_PLAYERS; s++) _fd.thumb[pid][s] = 0;
    }

    void fdReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_FRANKENDRAW) return;
        if(_fd.pt.phase != 0 && _fd.pt.phase != 4) return;
        if(_fd.pt.phase == 4 && val) fdClear();
        _fd.pt.ready[pid] = val;
        fdCheckStart();
        pushAll();
    }

    // Needs three players, not two: with only two seats a sheet would come straight
    // back to the player who drew its head.
    void fdCheckStart() {
        Party& pt = _fd.pt;
        bool enough = enoughPlayers(FD_MIN_PLAYERS);
        if(pt.phase == 0 && enough && partyAllReady(pt)) {
            // No stroke store (a board with no spare PSRAM or heap) -> can't run Frankendraw;
            // stay in the ready room rather than start a game that would touch a null buffer.
            if(!fdSheetsEnsure()) return;
            pt.phase = 1;
            pt.countdownEnd = millis() + (uint32_t)PARTY_COUNTDOWN * 1000;
            pt.lastSec = -1;
        } else if(pt.phase == 1 && (!enough || !partyAllReady(pt))) {
            pt.phase = 0;
        }
    }

    int fdSeatOf(uint8_t pid) {
        for(int k = 0; k < _fd.seats; k++)
            if(_fd.seat[k] == pid) return k;
        return -1;
    }

    // Which sheet seat k holds this round (the rotation rule above).
    int fdSheetAt(int seatIdx) {
        if(_fd.seats <= 0) return -1;
        int r = _fd.pt.round < 1 ? 1 : _fd.pt.round;
        return (seatIdx + _fd.seats - ((r - 1) % _fd.seats)) % _fd.seats;
    }

    // The sheet `pid` is drawing on this round, or -1 if they have no seat.
    int fdSheetOf(uint8_t pid) {
        int k = fdSeatOf(pid);
        return k < 0 ? -1 : fdSheetAt(k);
    }

    static int fdTop(int panel) { return panel * FD_BAND; }
    static int fdBot(int panel) { return panel * FD_BAND + FD_BAND; }

    void fdBegin(uint32_t now) {
        // The countdown that reaches here only starts once fdCheckStart() has confirmed the
        // store is allocated, but guard anyway: without it, nothing below can run.
        if(!fdSheetsEnsure()) return;
        _fd.seats = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _fd.seats < HA_MAX_PLAYERS) _fd.seat[_fd.seats++] = i;
        for(int i = 0; i < HA_MAX_PLAYERS; i++) _fdSheets[i] = FdSheet{};
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) fdForgetPlayer((uint8_t)i);
        resetScoresAll();
        _fd.pt.round = 0;
        _fd.pt.roundsTotal = FD_PANELS;
        fdNextRound(now);
    }

    void fdNextRound(uint32_t now) {
        Party& pt = _fd.pt;
        if(pt.round >= FD_PANELS || _fd.seats < FD_MIN_PLAYERS) {
            fdGalleryStart(now);
            return;
        }
        pt.round++;
        int panel = pt.round - 1;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _fd.done[i] = false;
        // Credit the panel when it is handed out, not when ink arrives: a drawer who
        // disconnects halfway through still contributed what they drew, and their nick
        // is copied because their Player slot is gone by the time the gallery runs.
        for(int k = 0; k < _fd.seats; k++) {
            uint8_t pid = _fd.seat[k];
            int s = fdSheetAt(k);
            if(s < 0 || !pid || !_p[pid].used) continue;
            _fdSheets[s].by[panel] = pid;
            strlcpy(_fdSheets[s].who[panel], _p[pid].nick, HA_NICK_LEN);
        }
        pt.deadline = now + (uint32_t)FD_DRAW_SECS * 1000;
        pt.phase = 2;
        pushAll();
        haUartEvent(String("{\"draw\":\"frankendraw panel ") + pt.round + "/" + FD_PANELS + "\"}");
    }

    // One normalised 0..1 wire coordinate, quantised onto the 0..FD_UNIT sheet grid.
    static bool fdCoord(const char* json, const char* key, int& out) {
        char num[16];
        if(!jsonNum(json, key, num, sizeof(num))) return false;
        double v = atof(num);
        if(v < 0) v = 0;
        if(v > 1) v = 1;
        out = (int)(v * (double)FD_UNIT + 0.5);
        if(out < 0) out = 0;
        if(out > FD_UNIT) out = FD_UNIT;
        return true;
    }

    // A drawer's segment. Nothing is relayed to anybody: every player is drawing their
    // own sheet at the same time, and ink only becomes visible (as a sliver, then in the
    // gallery) when the server decides it may. Endpoints are clamped into the drawer's
    // own band, so a stroke can never spill into a panel they do not own.
    //
    // Past FD_PANEL_STROKES the segment is refused. The client is told how much of the
    // budget is left (`used`/`cap`) and shows it as a filling ink bar, then stops the pen
    // at the cap -- so running out is visible, rather than ink vanishing unannounced.
    void fdStroke(uint8_t pid, const char* json) {
        if(_active != HA_GAME_FRANKENDRAW || _fd.pt.phase != 2) return;
        int s = fdSheetOf(pid);
        if(s < 0 || _fd.done[pid]) return;
        int panel = _fd.pt.round - 1;
        FdSheet& sh = _fdSheets[s];
        if(sh.n[panel] >= FD_PANEL_STROKES) return; // out of ink
        int c[4];
        static const char* keys[4] = {"x0", "y0", "x1", "y1"};
        for(int i = 0; i < 4; i++)
            if(!fdCoord(json, keys[i], c[i])) return;
        int top = fdTop(panel), bot = fdBot(panel);
        for(int i = 1; i < 4; i += 2) { // y0, y1
            if(c[i] < top) c[i] = top;
            if(c[i] > bot) c[i] = bot;
        }
        FdStroke& st = sh.s[panel][sh.n[panel]++];
        st.x0 = (uint8_t)c[0];
        st.y0 = (uint8_t)c[1];
        st.x1 = (uint8_t)c[2];
        st.y1 = (uint8_t)c[3];
    }

    // Undo drops the last segment of your own panel. Pushed (unlike a stroke, which is
    // silent) so the ink bar's authoritative `used` follows it back down.
    void fdUndo(uint8_t pid) {
        if(_active != HA_GAME_FRANKENDRAW || _fd.pt.phase != 2) return;
        int s = fdSheetOf(pid);
        if(s < 0 || _fd.done[pid]) return;
        uint8_t& n = _fdSheets[s].n[_fd.pt.round - 1];
        if(n) n--;
        pushAll();
    }

    // Everyone still at the table has tapped Next. A seat whose player left is not
    // waited for -- otherwise one disconnect would stall the panel until its timer.
    bool fdAllDone() {
        int drawing = 0;
        for(int k = 0; k < _fd.seats; k++) {
            uint8_t pid = _fd.seat[k];
            if(!pid || !_p[pid].used) continue;
            drawing++;
            if(!_fd.done[pid]) return false;
        }
        return drawing >= 1;
    }

    int fdWaiting() {
        int n = 0;
        for(int k = 0; k < _fd.seats; k++) {
            uint8_t pid = _fd.seat[k];
            if(pid && _p[pid].used && !_fd.done[pid]) n++;
        }
        return n;
    }

    void fdDone(uint8_t pid) {
        if(_active != HA_GAME_FRANKENDRAW || _fd.pt.phase != 2) return;
        if(fdSheetOf(pid) < 0) return;
        _fd.done[pid] = true;
        if(fdAllDone())
            fdNextRound(millis());
        else
            pushAll();
    }

    // A player left. Vacate their seat rather than just noticing that their Player slot
    // is gone: pids are recycled, so the next person to join would otherwise inherit the
    // seat, the sheet in its hands, the credit line already written for the panel, and
    // their thumbs. The seat stays empty for the rest of the game (see the rotation rule
    // above) and the newcomer waits for the next one.
    void fdOnLeave(uint8_t pid) {
        if(!pid) return;
        if(_active != HA_GAME_FRANKENDRAW) return; // _fd is union memory; touch only while FD is live
        for(int k = 0; k < _fd.seats; k++)
            if(_fd.seat[k] == pid) _fd.seat[k] = 0;
        fdForgetPlayer(pid);
    }

    // ---- the gallery walk ----
    // The picture is public and by far the biggest message this game sends, so it goes
    // out once as a broadcast when the gallery advances (one buffer fanned out to every
    // socket) instead of riding along in the per-player state push -- which now fires on
    // every thumb tap, so the counts move live. `artSent` records who already has the
    // current picture, so a phone that joins or reconnects mid-creature gets one unicast
    // copy and nobody gets it twice.
    void fdShowSheet(uint8_t s) {
        _fd.show = s;
        haWsBroadcast(fdArtJson(s));
        for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++)
            if(_p[pid].used && _p[pid].wsId) _fd.artSent[pid] = (int8_t)s;
    }

    void fdPush() {
        if(_fd.pt.phase == 3)
            for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++)
                if(_p[pid].used && _p[pid].wsId && _fd.artSent[pid] != (int8_t)_fd.show) {
                    haWsSendWs(_p[pid].wsId, fdArtJson(_fd.show));
                    _fd.artSent[pid] = (int8_t)_fd.show;
                }
        pushAll();
    }

    void fdGalleryStart(uint32_t now) {
        _fd.stage = 0;
        if(_fd.seats == 0) { // never got going: nothing to show
            _fd.pt.phase = 4;
            pushAll();
            return;
        }
        _fd.pt.phase = 3;
        _fd.pt.revealUntil = now + FD_SHOW_MS;
        fdSaveSheet(0);
        fdShowSheet(0);
        pushAll();
    }

    // One step of the walk: next creature, or (after the last) score the room's thumbs
    // and put the winner back up as the finale, or (after that) the podium.
    void fdGalleryStep(uint32_t now) {
        if(_fd.stage == 0 && _fd.show + 1 < _fd.seats) {
            uint8_t s = (uint8_t)(_fd.show + 1);
            _fd.pt.revealUntil = now + FD_SHOW_MS;
            fdSaveSheet(s);
            fdShowSheet(s);
            pushAll();
            return;
        }
        if(_fd.stage == 0) {
            fdTally();
            _fd.stage = 1;
            _fd.pt.revealUntil = now + FD_FINALE_MS;
            fdShowSheet(_fd.best); // the winner, once more
            pushAll();
            return;
        }
        _fd.pt.phase = 4;
        pushAll();
    }

    // Hand one finished sheet to the host as it comes up in the gallery: begin, a call
    // per segment, end. Each segment is formatted, sent and forgotten, so saving a whole
    // gallery costs one String at a time however many sheets there are.
    void fdSaveSheet(uint8_t s) {
        if(s >= _fd.seats) return;
        FdSheet& sh = _fdSheets[s];
        // Flat "w0".."w2" rather than a who[] array: the Flipper's JSON helper only
        // reads flat objects, and this keeps the host side a plain key lookup.
        String head = String("{\"game\":\"frankendraw\",\"id\":") + (int)s;
        for(int p = 0; p < FD_PANELS; p++)
            head += String(",\"w") + p + "\":\"" + ha_json_escape(sh.who[p]) + "\"";
        haUartArt(HA_ART_BEGIN, head + "}");
        for(int p = 0; p < FD_PANELS; p++)
            for(int i = 0; i < sh.n[p]; i++) {
                FdStroke& st = sh.s[p][i];
                haUartArt(
                    HA_ART_STROKE,
                    String("{\"p\":") + p + ",\"x0\":" + (int)st.x0 + ",\"y0\":" + (int)st.y0 +
                        ",\"x1\":" + (int)st.x1 + ",\"y1\":" + (int)st.y1 + "}");
            }
        haUartArt(HA_ART_END, String("{\"id\":") + (int)s + "}");
    }

    int fdUps(int s) {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _fd.thumb[i][s] > 0) n++;
        return n;
    }
    int fdDowns(int s) {
        int n = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
            if(_p[i].used && _fd.thumb[i][s] < 0) n++;
        return n;
    }
    int fdNet(int s) { return fdUps(s) - fdDowns(s); }

    // A thumb applies to the creature currently on screen, and only during the walk --
    // the finale is the result, not another round of voting. Tapping the same thumb
    // again takes it back. Every tap pushes, so the whole room watches the counts move.
    void fdThumb(uint8_t pid, int sheet, int v) {
        if(_active != HA_GAME_FRANKENDRAW || _fd.pt.phase != 3 || _fd.stage != 0) return;
        if(sheet < 0 || sheet >= _fd.seats || sheet != (int)_fd.show) return;
        int8_t w = v > 0 ? 1 : v < 0 ? -1 : 0;
        _fd.thumb[pid][sheet] = (_fd.thumb[pid][sheet] == w) ? 0 : w;
        fdPush();
    }

    // Scoring. The game has no winner of its own, so the ending is the room's verdict:
    // the creature with the best net score (thumbs up minus thumbs down) wins and is
    // shown again as the finale, and each sheet pays its net score x FD_VOTE_POINTS to
    // each of its three contributors -- floored at zero, so a creature the room disliked
    // simply earns nothing rather than punishing the people who drew it.
    //
    // Ties break on the most thumbs-up, then on the creature shown first (the lowest
    // sheet index). Deterministic on purpose: a coin flip here would make the finale
    // impossible to explain to the room.
    //
    // With exactly three players every sheet has the same three contributors, so the
    // podium is flat by construction and the crowned creature is the whole result --
    // the honest outcome for a game where everybody drew on everything.
    void fdTally() {
        _fd.best = 0;
        _fd.bestNet = _fd.seats ? fdNet(0) : 0;
        for(int s = 1; s < _fd.seats; s++) {
            int net = fdNet(s);
            if(net > _fd.bestNet || (net == _fd.bestNet && fdUps(s) > fdUps(_fd.best))) {
                _fd.best = (uint8_t)s;
                _fd.bestNet = net;
            }
        }
        for(int s = 0; s < _fd.seats; s++) {
            int net = fdNet(s);
            if(net <= 0) continue;
            for(int p = 0; p < FD_PANELS; p++) {
                uint8_t pid = _fdSheets[s].by[p];
                if(!pid || !_p[pid].used) continue;
                _p[pid].score += net * FD_VOTE_POINTS;
                haUartScore(pid, net * FD_VOTE_POINTS, "frankendraw");
            }
        }
        haUartRoundResult(
            String("{\"frankendraw\":\"sheet ") + (int)(_fd.best + 1) + " wins, net " +
            _fd.bestNet + "\"}");
    }

    void fdAgain(uint8_t pid) {
        (void)pid;
        if(_active != HA_GAME_FRANKENDRAW || _fd.pt.phase != 4) return;
        fdClear();
        pushAll();
    }

    void fdTick(uint32_t now) {
        Party& pt = _fd.pt;
        if(pt.phase == 1) {
            if(partyCountdownDone(pt, now)) fdBegin(now);
        } else if(pt.phase == 2) {
            if(connectedCount() == 0) { // room emptied: drop the game and its ink
                fdClear();
                return;
            }
            if((int32_t)(now - pt.deadline) >= 0 || fdAllDone()) fdNextRound(now);
        } else if(pt.phase == 3) {
            if((int32_t)(now - pt.revealUntil) >= 0) fdGalleryStep(now);
        }
    }

    String fdWhoJson(int s) {
        String o = "[";
        for(int p = 0; p < FD_PANELS; p++) {
            if(p) o += ",";
            o += String("\"") + ha_json_escape(_fdSheets[s].who[p]) + "\"";
        }
        o += "]";
        return o;
    }

    // One panel as a flat [x0,y0,x1,y1, ...] array in sheet grid units. With fromY > 0
    // only segments lying ENTIRELY at or below that line are emitted: a segment that
    // merely dips into the sliver would drag its other end -- above the line, in the
    // part the next drawer must not see -- along with it.
    String fdInkJson(FdSheet& sh, int panel, int fromY) {
        String o = "[";
        bool first = true;
        for(int i = 0; i < sh.n[panel]; i++) {
            FdStroke& st = sh.s[panel][i];
            if(st.y0 < fromY || st.y1 < fromY) continue;
            if(!first) o += ",";
            first = false;
            o += String((int)st.x0) + "," + (int)st.y0 + "," + (int)st.x1 + "," + (int)st.y1;
        }
        o += "]";
        return o;
    }

    // The public picture of one finished sheet: all three panels plus the three names,
    // which the client puts in a little label on each band. Its own message type because
    // it is large and only changes when the gallery advances (see fdShowSheet).
    String fdArtJson(uint8_t s) {
        FdSheet& sh = _fdSheets[s];
        String o = String("{\"t\":\"fdart\",\"n\":") + (int)s + ",\"total\":" + (int)_fd.seats +
                   ",\"unit\":" + FD_UNIT + ",\"band\":" + FD_BAND + ",\"who\":" + fdWhoJson(s) +
                   ",\"ink\":[";
        for(int p = 0; p < FD_PANELS; p++) {
            if(p) o += ",";
            o += fdInkJson(sh, p, 0);
        }
        o += "]}";
        return o;
    }

    String fdJson(uint8_t pid) {
        Party& pt = _fd.pt;
        if(pt.phase == 0)
            return String("{\"t\":\"frankendraw\",\"phase\":\"lobby\",\"you\":") + pid +
                   ",\"need\":" + FD_MIN_PLAYERS + ",\"players\":" + partyPlayersJson(pt) + "}";
        if(pt.phase == 1)
            return String("{\"t\":\"frankendraw\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"frankendraw\",\"phase\":\"final\",\"best\":") + (int)_fd.best +
                   ",\"net\":" + _fd.bestNet + ",\"who\":" + fdWhoJson(_fd.best) +
                   ",\"board\":" + triviaBoard() + "}";

        if(pt.phase == 2) {
            int panel = pt.round - 1;
            int s = fdSheetOf(pid);
            String o = String("{\"t\":\"frankendraw\",\"phase\":\"draw\",\"round\":") + pt.round +
                       ",\"rounds\":" + FD_PANELS + ",\"unit\":" + FD_UNIT + ",\"band\":" + FD_BAND +
                       ",\"over\":" + FD_OVERLAP + ",\"cap\":" + FD_PANEL_STROKES;
            if(s < 0) {
                o += ",\"panel\":-1,\"wait\":true"; // joined mid-game: no seat this time
            } else {
                o += String(",\"panel\":") + panel + ",\"top\":" + fdTop(panel) + ",\"bot\":" +
                     fdBot(panel) + ",\"sheet\":" + s + ",\"used\":" +
                     (int)_fdSheets[s].n[panel] + ",\"done\":" +
                     (_fd.done[pid] ? "true" : "false");
                o += String(",\"waiting\":") + fdWaiting();
                // The only ink a drawer is entitled to: the bottom FD_OVERLAP units of
                // the panel directly above theirs, on the sheet now in their hands. Their
                // own strokes are not echoed back (the client already drew them), and no
                // other panel -- and no other sheet -- is ever serialised here.
                o += ",\"ink\":";
                o += panel > 0 ? fdInkJson(_fdSheets[s], panel - 1, fdTop(panel) - FD_OVERLAP) :
                                 String("[]");
            }
            o += ",\"deadline\":" + String(pt.deadline);
            o += String(",\"dur\":") + FD_DRAW_SECS + ",\"scores\":" + playersJson() + "}";
            return o;
        }

        // phase 3: the gallery walk, then the winner once more. The picture itself is a
        // separate `fdart` message; this one is the small, frequently-pushed part.
        String o = String("{\"t\":\"frankendraw\",\"phase\":\"show\",\"n\":") + (int)_fd.show +
                   ",\"total\":" + (int)_fd.seats + ",\"final\":" +
                   (_fd.stage ? "true" : "false") + ",\"up\":" + fdUps(_fd.show) + ",\"down\":" +
                   fdDowns(_fd.show) + ",\"mine\":" + (int)_fd.thumb[pid][_fd.show];
        if(_fd.stage) o += String(",\"net\":") + _fd.bestNet;
        o += ",\"deadline\":" + String(pt.revealUntil);
        o += String(",\"dur\":") + (int)((_fd.stage ? FD_FINALE_MS : FD_SHOW_MS) / 1000) + "}";
        return o;
    }
};
