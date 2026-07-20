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

#define TRIVIA_MAX_TOPICS 6
#define TRIVIA_MAX_QS 20
#define TRIVIA_QDUR 20 // seconds per question (safety timer)
#define TRIVIA_COUNTDOWN 5 // seconds after all-ready before the first question
#define TRIVIA_REVEAL_MS 4000 // pause on the reveal before the next question

#define DRAW_SECS 70 // per drawing round
#define DRAW_REVEAL_MS 4000 // reveal pause before the next round
#define PONG_MAX 4 // concurrent pong matches
#define PONG_WIN 5 // points to win
#define PONG_TICK_MS 33 // ~30 Hz

// Whole-group "party" games (would-you-rather / scramble / reaction) share a
// lobby -> countdown -> round -> reveal -> ... -> final skeleton (see Party).
#define PARTY_COUNTDOWN 5 // seconds after all-ready before round 1
#define WYR_ROUNDS 6
#define WYR_VOTE_SECS 20 // safety timer per prompt
#define WYR_REVEAL_MS 5000
#define SCR_ROUNDS 6
#define SCR_SECS 30 // safety timer per word
#define SCR_REVEAL_MS 5000
#define REACT_ROUNDS 5
#define REACT_REVEAL_MS 4000

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

// Would You Rather: a live A/B poll, one prompt per round, no scoring.
struct WyrState {
    Party pt;
    uint8_t promptSeq; // rotates prompts across games
    uint8_t prompt; // current prompt index
    int8_t vote[HA_MAX_PLAYERS + 1]; // 0/1, -1 = not voted
};

// Word scramble race: everyone unscrambles the same word; fastest correct win most.
struct ScrambleState {
    Party pt;
    uint16_t wordSeq;
    char word[24]; // the answer
    char scram[24]; // shown (letters shuffled)
    bool solved[HA_MAX_PLAYERS + 1];
    uint8_t solvedCount;
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
            strlcpy(_p[pid].nick, (nick && nick[0]) ? nick : "Player", HA_NICK_LEN);
            strlcpy(_p[pid].avatar, (avatar && avatar[0]) ? avatar : "\xF0\x9F\x99\x82", sizeof(_p[pid].avatar));
            haUartJoin(pid, _p[pid].nick);
        } else {
            if(nick && nick[0]) strlcpy(_p[pid].nick, nick, HA_NICK_LEN);
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
        } else if(strcmp(type, "vote") == 0 && ha_json_int(json, "topic", &v)) {
            triviaVote(pid, v);
        } else if(strcmp(type, "tap") == 0) {
            reactTap(pid);
        } else if(strcmp(type, "again") == 0) {
            triviaAgain(pid);
            drawAgain(pid);
            wyrAgain(pid);
            scrambleAgain(pid);
            reactAgain(pid);
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
        } else if(strcmp(type, "paddle") == 0 && ha_json_int(json, "dir", &v)) {
            pongPaddle(pid, v);
        } else if(strcmp(type, "guess") == 0) {
            char g[64];
            if(ha_json_str(json, "text", g, sizeof(g))) {
                drawGuess(pid, g);
                scrambleGuess(pid, g);
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
    DuelMatch _m[DUEL_MAX_MATCHES] = {};
    DuelChallenge _c[DUEL_MAX_CHALLENGES] = {};
    DrawState _d = {};
    PongMatch _pm[PONG_MAX] = {};
    uint32_t _lastPong = 0;
    WyrState _wyr = {};
    ScrambleState _scr = {};
    ReactState _react = {};

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

    // Challenge/accept are shared by all 1v1 games (duels + pong).
    bool isMatchGame() { return isDuel(_active) || _active == HA_GAME_PONG; }
    bool inAnyMatch(uint8_t pid) { return matchOf(pid) || pongMatchOf(pid); }

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
        } else {
            for(int i = 0; i < DUEL_MAX_MATCHES; i++)
                if(!_m[i].used) {
                    duelStart(&_m[i], from, pid, from); // challenger moves first
                    break;
                }
        }
        duelRemoveChallengesInvolving(pid);
        duelRemoveChallengesInvolving(from);
        const char* key = (_active == HA_GAME_PONG) ? "pong" : "duel";
        haUartEvent(
            String("{\"") + key + "\":\"" + ha_json_escape(_p[from].nick) + " vs " +
            ha_json_escape(_p[pid].nick) + "\"}");
        pushAll();
    }

    void anyOnLeave(uint8_t pid) {
        duelOnLeave(pid);
        pongOnLeave(pid);
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
        if(!m->aIn || !m->bIn) return; // opponent left
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
    static const char* drawWord(int i) {
        static const char* words[] = {
            "apple",  "house",  "car",   "tree",  "cat",     "dog",    "sun",   "star",
            "boat",   "fish",   "clock", "flower", "book",   "phone",  "chair", "heart",
            "cloud",  "key",    "shoe",  "smile", "pizza",   "robot",  "ghost", "snake",
            "crown",  "guitar", "rocket", "cake",  "moon",   "ladder"};
        const int n = sizeof(words) / sizeof(words[0]);
        return words[((i % n) + n) % n];
    }

    void drawClear() { _d = DrawState{}; }

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
    }

    // Emoji reaction: broadcast so it floats up on every client, in any game.
    // Uses type "emoji" so it never collides with the reaction-duel game's
    // {t:"react",phase} state messages.
    void onReact(uint8_t pid, const char* emoji) {
        if(!emoji[0]) return;
        haWsBroadcast(
            String("{\"t\":\"emoji\",\"pid\":") + pid + ",\"nick\":\"" + ha_json_escape(_p[pid].nick) +
            "\",\"avatar\":\"" + ha_json_escape(_p[pid].avatar) + "\",\"emoji\":\"" +
            ha_json_escape(emoji) + "\"}");
    }

    void drawStart(uint32_t now) {
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
        _d.drawer = drawer;
        strlcpy(_d.word, drawWord(_d.wordSeq++), sizeof(_d.word));
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
            if(m->bx <= 0.05f) {
                if(fabsf(m->by - m->p1) <= PADHALF) {
                    m->bx = 0.05f;
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
            if(m->phase == 1 && m->bx >= 0.95f) {
                if(fabsf(m->by - m->p2) <= PADHALF) {
                    m->bx = 0.95f;
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
    }

    // ---------- would you rather (live A/B poll) ----------
    static int wyrPromptCount() { return 12; }
    static void wyrPromptText(int i, const char*& a, const char*& b) {
        static const char* p[][2] = {
            {"Be able to fly", "Be invisible"},
            {"Read minds", "See the future"},
            {"Always be 10 min early", "Always be 5 min late"},
            {"Never use the internet again", "Never watch films again"},
            {"Fight one horse-sized duck", "100 duck-sized horses"},
            {"Live without music", "Live without TV"},
            {"Have unlimited pizza", "Have unlimited tacos"},
            {"Teleport anywhere", "Time travel"},
            {"Talk to animals", "Speak every language"},
            {"Always be too hot", "Always be too cold"},
            {"No phone for a week", "No coffee for a month"},
            {"Explore space", "Explore the deep ocean"},
        };
        int n = wyrPromptCount();
        i = ((i % n) + n) % n;
        a = p[i][0];
        b = p[i][1];
    }

    void wyrClear() {
        partyClear(_wyr.pt);
        _wyr.prompt = 0;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _wyr.vote[i] = -1;
    }

    void wyrReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_WYR) return;
        if(_wyr.pt.phase != 0 && _wyr.pt.phase != 4) return;
        if(_wyr.pt.phase == 4 && val) wyrClear(); // ready from the final screen -> new game
        _wyr.pt.ready[pid] = val;
        wyrCheckStart();
        pushAll();
    }

    void wyrCheckStart() {
        Party& pt = _wyr.pt;
        if(pt.phase == 0 && partyAllReady(pt)) {
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
            if(_wyr.vote[i] < 0) return false;
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
        pt.round++;
        _wyr.prompt = _wyr.promptSeq % wyrPromptCount();
        _wyr.promptSeq++;
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _wyr.vote[i] = -1;
        pt.phase = 2;
        pt.deadline = now + (uint32_t)WYR_VOTE_SECS * 1000;
        pushAll();
    }

    void wyrAnswer(uint8_t pid, int c) {
        if(_active != HA_GAME_WYR || _wyr.pt.phase != 2) return;
        if(c != 0 && c != 1) return;
        _wyr.vote[pid] = (int8_t)c;
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
        if(pt.phase == 0)
            return String("{\"t\":\"wyr\",\"phase\":\"lobby\",\"you\":") + pid +
                   ",\"players\":" + partyPlayersJson(pt) + "}";
        if(pt.phase == 1)
            return String("{\"t\":\"wyr\",\"phase\":\"countdown\",\"sec\":") +
                   partyCountdownSec(pt) + "}";
        if(pt.phase == 4)
            return String("{\"t\":\"wyr\",\"phase\":\"final\",\"you\":") + pid + "}";
        const char *a, *b;
        wyrPromptText(_wyr.prompt, a, b);
        int cA = 0, cB = 0;
        for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++) {
            if(!_p[i].used) continue;
            if(_wyr.vote[i] == 0) cA++;
            else if(_wyr.vote[i] == 1) cB++;
        }
        String s = String("{\"t\":\"wyr\",\"phase\":\"") + (pt.phase == 3 ? "reveal" : "vote") +
                   "\",\"round\":" + pt.round + ",\"rounds\":" + WYR_ROUNDS + ",\"a\":\"" +
                   ha_json_escape(a) + "\",\"b\":\"" + ha_json_escape(b) + "\",\"myvote\":" +
                   _wyr.vote[pid] + ",\"counts\":[" + cA + "," + cB + "]";
        if(pt.phase == 2) {
            s += ",\"deadline\":";
            s += pt.deadline;
            s += ",\"dur\":";
            s += WYR_VOTE_SECS;
        }
        s += "}";
        return s;
    }

    // ---------- word scramble race ----------
    static const char* scrambleWord(int i) {
        static const char* w[] = {
            "planet", "guitar", "picture", "monster", "rainbow", "diamond", "kitchen",
            "bicycle", "dolphin", "penguin", "volcano", "pyramid", "compass", "blanket",
            "cactus", "harbor", "jungle", "magnet", "puzzle", "rocket", "sunset", "tiger",
            "violin", "wizard", "anchor", "bridge", "castle", "forest", "island", "orbit"};
        const int n = sizeof(w) / sizeof(w[0]);
        return w[((i % n) + n) % n];
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
        for(int i = 0; i <= HA_MAX_PLAYERS; i++) _scr.solved[i] = false;
    }

    void scrambleReady(uint8_t pid, bool val) {
        if(_active != HA_GAME_SCRAMBLE) return;
        if(_scr.pt.phase != 0 && _scr.pt.phase != 4) return;
        if(_scr.pt.phase == 4 && val) scrambleClear();
        _scr.pt.ready[pid] = val;
        scrambleCheckStart();
        pushAll();
    }

    void scrambleCheckStart() {
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
        pt.round++;
        strlcpy(_scr.word, scrambleWord(_scr.wordSeq++), sizeof(_scr.word));
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
        if(pt.phase == 0)
            return String("{\"t\":\"scramble\",\"phase\":\"lobby\",\"you\":") + pid +
                   ",\"players\":" + partyPlayersJson(pt) + "}";
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
};
