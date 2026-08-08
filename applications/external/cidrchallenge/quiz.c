#include "quiz.h"
#include "subnet.h"

#include <furi_hal_random.h>
#include <stdio.h>
#include <string.h>

#define MAX_CANDIDATES 8

typedef struct {
    uint32_t values[MAX_CANDIDATES];
    uint8_t count;
} CandidateList;

static uint32_t random_below(uint32_t bound) {
    if(bound < 2) return 0;
    return furi_hal_random_get() % bound;
}

static uint8_t clamp_prefix(int32_t prefix) {
    if(prefix < 1) return 1;
    if(prefix > 32) return 32;
    return (uint8_t)prefix;
}

static void candidates_add(CandidateList* list, uint32_t value) {
    if(list->count < MAX_CANDIDATES) list->values[list->count++] = value;
}

static void candidates_shuffle(CandidateList* list) {
    for(uint8_t i = list->count; i > 1; i--) {
        uint8_t j = (uint8_t)random_below(i);
        uint32_t tmp = list->values[i - 1];
        list->values[i - 1] = list->values[j];
        list->values[j] = tmp;
    }
}

static bool is_prefix_question(QuestionType type) {
    return type == QuestionMaskToCidr || type == QuestionCidrForHosts;
}

static void format_value(QuestionType type, uint32_t value, char* out, size_t out_size) {
    if(is_prefix_question(type)) {
        snprintf(out, out_size, "/%lu", (unsigned long)value);
    } else if(type == QuestionHostCount) {
        snprintf(out, out_size, "%lu", (unsigned long)value);
    } else {
        subnet_format_ip(value, out, out_size);
    }
}

static bool answer_add(Question* question, uint8_t* count, uint32_t value) {
    if(*count >= QUIZ_ANSWER_COUNT) return false;

    char text[QUIZ_TEXT_SIZE];
    format_value(question->type, value, text, sizeof(text));

    for(uint8_t i = 0; i < *count; i++) {
        if(strcmp(question->answers[i], text) == 0) return false;
    }

    memcpy(question->answers[*count], text, QUIZ_TEXT_SIZE);
    (*count)++;
    return true;
}

/** Last resort filler so the four slots are always populated. */
static uint32_t make_filler(QuestionType type, uint32_t correct, uint8_t prefix, uint8_t attempt) {
    if(is_prefix_question(type)) {
        return clamp_prefix((int32_t)(8 + random_below(23)));
    }
    if(type == QuestionHostCount) {
        return correct + 1 + attempt + random_below(64);
    }
    return correct + subnet_block_size(prefix) * (1 + attempt);
}

static void build_answers(
    Question* question,
    uint32_t correct,
    uint8_t prefix,
    CandidateList* candidates) {
    uint8_t count = 0;
    answer_add(question, &count, correct);

    candidates_shuffle(candidates);
    for(uint8_t i = 0; i < candidates->count && count < QUIZ_ANSWER_COUNT; i++) {
        answer_add(question, &count, candidates->values[i]);
    }

    for(uint8_t attempt = 0; count < QUIZ_ANSWER_COUNT && attempt < 32; attempt++) {
        answer_add(question, &count, make_filler(question->type, correct, prefix, attempt));
    }

    // The correct answer sits at index 0, follow it while shuffling in place
    question->correct = 0;
    for(uint8_t i = QUIZ_ANSWER_COUNT; i > 1; i--) {
        uint8_t j = (uint8_t)random_below(i);
        char tmp[QUIZ_TEXT_SIZE];
        memcpy(tmp, question->answers[i - 1], QUIZ_TEXT_SIZE);
        memcpy(question->answers[i - 1], question->answers[j], QUIZ_TEXT_SIZE);
        memcpy(question->answers[j], tmp, QUIZ_TEXT_SIZE);

        if(question->correct == i - 1) {
            question->correct = j;
        } else if(question->correct == j) {
            question->correct = i - 1;
        }
    }
}

static uint8_t random_prefix(Difficulty difficulty) {
    switch(difficulty) {
    case DifficultyBeginner:
        return 24 + (uint8_t)random_below(3);
    case DifficultyIntermediate:
        return 20 + (uint8_t)random_below(11);
    default:
        // Advanced regularly serves the /31 and /32 edge cases
        if(random_below(100) < 30) return 31 + (uint8_t)random_below(2);
        return 8 + (uint8_t)random_below(23);
    }
}

static uint32_t random_ip(void) {
    switch(random_below(4)) {
    case 0:
        return 0x0A000000u | random_below(0x01000000u); // 10.0.0.0/8
    case 1:
        return 0xAC100000u | random_below(0x00100000u); // 172.16.0.0/12
    case 2:
        return 0xC0A80000u | random_below(0x00010000u); // 192.168.0.0/16
    default: {
        uint32_t first = 1 + random_below(222);
        if(first == 127) first = 128;
        return (first << 24) | random_below(0x01000000u);
    }
    }
}

static void generate_address_question(Question* question, Difficulty difficulty) {
    uint8_t prefix = random_prefix(difficulty);
    uint32_t ip = random_ip();
    uint32_t block = subnet_block_size(prefix);
    uint32_t network = subnet_network(ip, prefix);
    uint32_t broadcast = subnet_broadcast(ip, prefix);
    uint32_t correct = 0;

    char ip_text[SUBNET_IP_STR_SIZE];
    subnet_format_ip(ip, ip_text, sizeof(ip_text));
    snprintf(question->subject, QUIZ_SUBJECT_SIZE, "%s/%u", ip_text, (unsigned)prefix);

    CandidateList candidates = {0};

    switch(question->type) {
    case QuestionNetwork:
        question->prompt = "Network?";
        correct = network;
        candidates_add(&candidates, network + block);
        if(network >= block) candidates_add(&candidates, network - block);
        candidates_add(&candidates, subnet_network(ip, clamp_prefix(prefix - 1)));
        candidates_add(&candidates, subnet_network(ip, clamp_prefix(prefix + 1)));
        candidates_add(&candidates, ip & 0xFFFFFF00u);
        candidates_add(&candidates, ip);
        break;

    case QuestionBroadcast:
        question->prompt = "Broadcast?";
        correct = broadcast;
        candidates_add(&candidates, network);
        candidates_add(&candidates, broadcast - 1);
        candidates_add(&candidates, broadcast + 1);
        candidates_add(&candidates, subnet_broadcast(ip, clamp_prefix(prefix - 1)));
        candidates_add(&candidates, subnet_broadcast(ip, clamp_prefix(prefix + 1)));
        candidates_add(&candidates, ip | 0x000000FFu);
        break;

    case QuestionFirstHost:
        question->prompt = "First host?";
        correct = subnet_first_host(ip, prefix);
        candidates_add(&candidates, network);
        candidates_add(&candidates, network + 2);
        candidates_add(&candidates, subnet_last_host(ip, prefix));
        candidates_add(&candidates, subnet_first_host(ip, clamp_prefix(prefix - 1)));
        candidates_add(&candidates, subnet_first_host(ip, clamp_prefix(prefix + 1)));
        candidates_add(&candidates, ip);
        break;

    case QuestionLastHost:
        question->prompt = "Last host?";
        correct = subnet_last_host(ip, prefix);
        candidates_add(&candidates, broadcast);
        candidates_add(&candidates, broadcast - 2);
        candidates_add(&candidates, subnet_first_host(ip, prefix));
        candidates_add(&candidates, subnet_last_host(ip, clamp_prefix(prefix - 1)));
        candidates_add(&candidates, subnet_last_host(ip, clamp_prefix(prefix + 1)));
        candidates_add(&candidates, ip);
        break;

    default:
        question->prompt = "Available hosts?";
        correct = subnet_usable_hosts(prefix);
        candidates_add(&candidates, block);
        candidates_add(&candidates, block - 1);
        candidates_add(&candidates, subnet_usable_hosts(clamp_prefix(prefix - 1)));
        candidates_add(&candidates, subnet_usable_hosts(clamp_prefix(prefix + 1)));
        candidates_add(&candidates, block + 2);
        candidates_add(&candidates, 32 - prefix);
        break;
    }

    build_answers(question, correct, prefix, &candidates);
}

static void generate_mask_question(Question* question, Difficulty difficulty) {
    uint8_t prefix = random_prefix(difficulty);
    question->prompt = "CIDR?";

    subnet_format_ip(subnet_mask(prefix), question->subject, QUIZ_SUBJECT_SIZE);

    CandidateList candidates = {0};
    candidates_add(&candidates, clamp_prefix(prefix - 1));
    candidates_add(&candidates, clamp_prefix(prefix + 1));
    candidates_add(&candidates, clamp_prefix(prefix - 2));
    candidates_add(&candidates, clamp_prefix(prefix + 2));
    candidates_add(&candidates, clamp_prefix(32 - prefix));
    candidates_add(&candidates, clamp_prefix(prefix + 4));

    build_answers(question, prefix, prefix, &candidates);
}

static void generate_hosts_question(Question* question, Difficulty difficulty) {
    uint8_t prefix;
    switch(difficulty) {
    case DifficultyBeginner:
        prefix = 24 + (uint8_t)random_below(3);
        break;
    case DifficultyIntermediate:
        prefix = 20 + (uint8_t)random_below(11);
        break;
    default:
        prefix = 16 + (uint8_t)random_below(15);
        break;
    }

    // Pick a host count that only the chosen prefix can satisfy
    uint32_t upper = subnet_usable_hosts(prefix);
    uint32_t lower = (prefix < 30) ? subnet_usable_hosts(prefix + 1) + 1 : 1;
    uint32_t hosts = lower + random_below(upper - lower + 1);

    question->prompt = "CIDR?";
    snprintf(question->subject, QUIZ_SUBJECT_SIZE, "%lu hosts needed", (unsigned long)hosts);

    CandidateList candidates = {0};
    candidates_add(&candidates, clamp_prefix(prefix - 1));
    candidates_add(&candidates, clamp_prefix(prefix + 1));
    candidates_add(&candidates, clamp_prefix(prefix - 2));
    candidates_add(&candidates, clamp_prefix(prefix + 2));
    candidates_add(&candidates, clamp_prefix(32 - prefix));
    candidates_add(&candidates, clamp_prefix(prefix + 3));

    build_answers(question, subnet_prefix_for_hosts(hosts), prefix, &candidates);
}

void quiz_generate(Question* question, Difficulty difficulty) {
    memset(question, 0, sizeof(Question));
    question->type = (QuestionType)random_below(QuestionTypeCount);

    switch(question->type) {
    case QuestionMaskToCidr:
        generate_mask_question(question, difficulty);
        break;
    case QuestionCidrForHosts:
        generate_hosts_question(question, difficulty);
        break;
    default:
        generate_address_question(question, difficulty);
        break;
    }
}

const char* quiz_difficulty_name(Difficulty difficulty) {
    switch(difficulty) {
    case DifficultyBeginner:
        return "Beginner";
    case DifficultyIntermediate:
        return "Intermediate";
    default:
        return "Advanced";
    }
}

const char* quiz_difficulty_range(Difficulty difficulty) {
    switch(difficulty) {
    case DifficultyBeginner:
        return "/24-/26";
    case DifficultyIntermediate:
        return "/20-/30";
    default:
        return "/8-/32";
    }
}
