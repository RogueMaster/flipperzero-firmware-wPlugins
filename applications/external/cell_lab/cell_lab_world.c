#include "cell_lab_world.h"

#include <furi.h>
#include <furi_hal_random.h>

#include <stdbool.h>
#include <string.h>

// RU: Модуль мира содержит чистую симуляцию: клетки, гены, раздражители и шаг жизни.
// EN: The world module contains the pure simulation: cells, genes, irritants, and life ticks.

typedef enum {
    CellLabTargetStay = 0,
    CellLabTargetFood = 1,
    CellLabTargetKin = 2,
} CellLabTargetKind;

// RU: Соседи Мура: 8 направлений вокруг клетки, включая диагонали.
// EN: Moore neighborhood: 8 directions around a cell, including diagonals.
static const int8_t cell_lab_neighbor_dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int8_t cell_lab_neighbor_dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

uint16_t cell_lab_world_width(const CellLabWorld* world) {
    // RU: Ширина мира не хранится отдельно, а выводится из текущего размера клетки.
    // EN: World width is not stored separately; it is derived from the current cell size.
    return cell_lab_config_world_width(&world->config);
}

uint16_t cell_lab_world_height(const CellLabWorld* world) {
    // RU: Высота мира так же зависит от масштаба клетки на экране Flipper.
    // EN: World height likewise depends on the cell scale on the Flipper screen.
    return cell_lab_config_world_height(&world->config);
}

uint16_t cell_lab_world_capacity(const CellLabWorld* world) {
    // RU: Вместимость нужна для безопасных проходов по активной части статических буферов.
    // EN: Capacity is used to safely iterate over the active part of static buffers.
    return cell_lab_config_world_capacity(&world->config);
}

static uint8_t cell_lab_abs_diff(uint8_t a, uint8_t b) {
    // RU: Маленькая беззнаковая разница часто используется для сравнения гена и среды.
    // EN: A small unsigned difference is used often to compare a gene with the environment.
    return (a > b) ? (a - b) : (b - a);
}

static uint8_t cell_lab_energy_add(const CellLabWorld* world, uint8_t energy, uint8_t bonus) {
    // RU: Энергия насыщается максимумом, чтобы клетка не становилась бесконечной батарейкой.
    // EN: Energy saturates at the maximum so a cell cannot become an infinite battery.
    const uint16_t sum = (uint16_t)energy + bonus;
    return (sum > world->config.max_energy) ? world->config.max_energy : (uint8_t)sum;
}

static uint32_t cell_lab_random(CellLabWorld* world) {
    uint32_t x = world->rng;

    // RU: Для xorshift нулевое состояние мертвое, поэтому заменяем его на ненулевое.
    // EN: For xorshift, zero state is dead, so replace it with a non-zero value.
    if(x == 0) {
        x = 0xA341316CU;
    }

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    world->rng = x;

    return x;
}

static uint16_t cell_lab_random_limit(CellLabWorld* world, uint16_t limit) {
    // RU: Достаточно простое ограничение случайного числа; для симуляции точная равномерность не критична.
    // EN: Simple random limiting is enough here; perfect uniformity is not critical for the simulation.
    return (uint16_t)(cell_lab_random(world) % limit);
}

static uint8_t cell_lab_gene_temp(CellLabGenome genome) {
    // RU: Младшие 4 бита кодируют предпочитаемую температуру.
    // EN: The lowest 4 bits encode preferred temperature.
    return (uint8_t)(genome & 0x000FU);
}

static uint8_t cell_lab_gene_light(CellLabGenome genome) {
    // RU: Следующие 4 бита кодируют предпочитаемый уровень света.
    // EN: The next 4 bits encode preferred light level.
    return (uint8_t)((genome >> 4) & 0x000FU);
}

static uint8_t cell_lab_gene_program(CellLabGenome genome) {
    // RU: Два бита выбирают базовую программу поведения клетки.
    // EN: Two bits select the cell's base behavior program.
    return (uint8_t)((genome >> 8) & 0x0003U);
}

static uint8_t cell_lab_gene_metabolism(CellLabGenome genome) {
    // RU: Метаболизм влияет на цену жизни и переносимость токсинов.
    // EN: Metabolism affects life cost and toxin tolerance.
    return (uint8_t)((genome >> 10) & 0x0003U);
}

static uint8_t cell_lab_gene_mutation(CellLabGenome genome) {
    // RU: Этот ген задает склонность к мутациям при размножении.
    // EN: This gene controls mutation tendency during reproduction.
    return (uint8_t)((genome >> 12) & 0x0003U);
}

static uint8_t cell_lab_gene_stress_resist(CellLabGenome genome) {
    // RU: Один бит устойчивости помогает переживать радиацию и токсины.
    // EN: One resistance bit helps survive radiation and toxins.
    return (uint8_t)((genome >> 14) & 0x0001U);
}

// RU: Старший бит генома разрешает съесть живого соседа и забрать часть его кода.
// EN: The top genome bit allows eating a living neighbor and taking part of its code.
static bool cell_lab_gene_cannibal(CellLabGenome genome) {
    return (genome & 0x8000U) != 0;
}

uint8_t cell_lab_world_light_at(const CellLabWorld* world, uint16_t y) {
    // RU: Свет задан высотой: верх мира яркий, низ темный.
    // EN: Light is height-based: the top of the world is bright, the bottom is dark.
    const uint16_t height = cell_lab_world_height(world);
    return (uint8_t)(15U - ((y * 15U) / (height - 1U)));
}

static uint8_t cell_lab_world_base_temperature(const CellLabWorld* world) {
    // RU: Температура колеблется волной, а heat_swing задает амплитуду.
    // EN: Temperature oscillates as a wave, and heat_swing sets its amplitude.
    const uint8_t phase = (uint8_t)(((world->generation / 32U) + world->climate_seed) & 0x1FU);
    const uint8_t wave = (phase < 16U) ? phase : (uint8_t)(31U - phase);
    return (uint8_t)((wave * world->config.heat_swing) / 15U);
}

uint8_t cell_lab_world_temperature_at(const CellLabWorld* world, uint16_t y) {
    // RU: Локальная температура смешивает общий жар эпохи и прогрев светом.
    // EN: Local temperature mixes epoch heat and light warming.
    const uint8_t base = cell_lab_world_base_temperature(world);
    const uint8_t light = cell_lab_world_light_at(world, y);
    const uint16_t temp = (((uint16_t)base * 2U) + light) / 3U;
    return (temp > 15U) ? 15U : (uint8_t)temp;
}

uint8_t cell_lab_world_radiation_at(const CellLabWorld* world, uint16_t x, uint16_t y) {
    // RU: Радиация сильнее на свету и имеет движущуюся вертикальную "бурю".
    // EN: Radiation is stronger in light and has a moving vertical "storm".
    const uint16_t width = cell_lab_world_width(world);
    const uint16_t storm_x =
        (uint16_t)((world->generation / 6U) + (world->climate_seed * 7U)) % width;
    const uint16_t dx_raw = (x > storm_x) ? (x - storm_x) : (storm_x - x);
    const uint16_t dx = (dx_raw > (width / 2U)) ? (width - dx_raw) : dx_raw;
    const uint8_t light = cell_lab_world_light_at(world, y);
    uint8_t radiation = (uint8_t)((light * world->config.radiation_level) / 15U);

    if(dx < (width / 10U + 1U)) {
        radiation += (uint8_t)((world->config.radiation_level / 2U) + 1U);
    }

    return (radiation > 15U) ? 15U : radiation;
}

uint8_t cell_lab_world_toxin_at(const CellLabWorld* world, uint16_t x, uint16_t y) {
    // RU: Токсины скапливаются в темных местах и пульсируют карманами.
    // EN: Toxins gather in dark places and pulse in pockets.
    const uint8_t darkness = (uint8_t)(15U - cell_lab_world_light_at(world, y));
    const uint8_t pocket =
        (uint8_t)((x * 5U + y * 11U + (world->generation / 12U) + world->climate_seed) & 0x0FU);
    uint8_t toxin = (uint8_t)((darkness * world->config.toxin_level) / 18U);

    if(pocket > 11U) {
        toxin += (uint8_t)(world->config.toxin_level / 3U + 1U);
    }

    return (toxin > 15U) ? 15U : toxin;
}

static void cell_lab_neighbor(
    const CellLabWorld* world,
    uint16_t x,
    uint16_t y,
    uint8_t neighbor,
    uint8_t* out_x,
    uint8_t* out_y) {
    // RU: Мир замкнут в тор: выход за край переносит клетку на противоположную сторону.
    // EN: The world is a torus: leaving one edge wraps to the opposite side.
    int16_t nx = (int16_t)x + cell_lab_neighbor_dx[neighbor];
    int16_t ny = (int16_t)y + cell_lab_neighbor_dy[neighbor];
    const int16_t width = (int16_t)cell_lab_world_width(world);
    const int16_t height = (int16_t)cell_lab_world_height(world);

    if(nx < 0) {
        nx = width - 1;
    } else if(nx >= width) {
        nx = 0;
    }

    if(ny < 0) {
        ny = height - 1;
    } else if(ny >= height) {
        ny = 0;
    }

    *out_x = (uint8_t)nx;
    *out_y = (uint8_t)ny;
}

static CellLabGenome cell_lab_random_genome(CellLabWorld* world) {
    // RU: Стартовый геном — случайная смесь стратегий, включая редкий каннибализм.
    // EN: A starting genome is a random mix of strategies, including rare cannibalism.
    CellLabGenome genome = 0;

    genome |= cell_lab_random_limit(world, 16);
    genome |= (CellLabGenome)cell_lab_random_limit(world, 16) << 4;
    genome |= (CellLabGenome)cell_lab_random_limit(world, 4) << 8;
    genome |= (CellLabGenome)cell_lab_random_limit(world, 4) << 10;
    genome |= (CellLabGenome)cell_lab_random_limit(world, 4) << 12;
    genome |= (CellLabGenome)cell_lab_random_limit(world, 2) << 14;

    if(cell_lab_random_limit(world, 8) == 0) {
        genome |= 0x8000U;
    }

    return genome;
}

static CellLabGenome cell_lab_mutate_genome(CellLabWorld* world, CellLabGenome genome) {
    // RU: Мутации бывают мелкими битовыми и редкими крупными по генам среды.
    // EN: Mutations can be small bit flips or rare larger environmental-gene changes.
    const uint8_t mutation_gene = cell_lab_gene_mutation(genome);
    const uint8_t mutation_chance = (uint8_t)(2U + (mutation_gene * 3U));

    if(cell_lab_random_limit(world, 32) < mutation_chance) {
        genome ^= (CellLabGenome)(1U << cell_lab_random_limit(world, 16));
        world->mutations++;
    }

    if(cell_lab_random_limit(world, 128) <= mutation_gene) {
        if(cell_lab_random(world) & 1U) {
            genome = (CellLabGenome)((genome & 0xFFF0U) | cell_lab_random_limit(world, 16));
        } else {
            genome = (CellLabGenome)((genome & 0xFF0FU) |
                                     ((CellLabGenome)cell_lab_random_limit(world, 16) << 4));
        }
        world->mutations++;
    }

    return genome;
}

// RU: Каннибал получает не весь геном жертвы, а случайную маску битов: это грубая рекомбинация.
// EN: A cannibal does not copy the whole victim genome; a random bit mask recombines it.
static CellLabGenome
    cell_lab_mix_genome(CellLabWorld* world, CellLabGenome hunter, CellLabGenome victim) {
    const CellLabGenome mask = (CellLabGenome)cell_lab_random(world);
    return (CellLabGenome)((hunter & mask) | (victim & ~mask));
}

static uint8_t cell_lab_live_neighbor_count(CellLabWorld* world, uint16_t x, uint16_t y);

static void cell_lab_recount(CellLabWorld* world) {
    // RU: После каждого шага пересчитываем фактическое состояние мира для экранов.
    // EN: After each step, recalculate actual world state for the screens.
    uint32_t energy_sum = 0;
    uint32_t temp_gene_sum = 0;
    uint32_t light_gene_sum = 0;
    uint32_t temp_world_sum = 0;
    uint32_t radiation_sum = 0;
    uint32_t toxin_sum = 0;
    uint32_t crowding_sum = 0;
    const uint16_t capacity = cell_lab_world_capacity(world);
    const uint16_t width = cell_lab_world_width(world);
    const uint16_t height = cell_lab_world_height(world);

    world->life_count = 0;
    world->food_count = 0;
    memset(world->program_count, 0, sizeof(world->program_count));

    for(uint16_t y = 0; y < height; y++) {
        for(uint16_t x = 0; x < width; x++) {
            temp_world_sum += cell_lab_world_temperature_at(world, y);
            radiation_sum += cell_lab_world_radiation_at(world, x, y);
            toxin_sum += cell_lab_world_toxin_at(world, x, y);
        }
    }

    world->avg_world_temp = (uint8_t)(temp_world_sum / capacity);
    world->avg_radiation = (uint8_t)(radiation_sum / capacity);
    world->avg_toxin = (uint8_t)(toxin_sum / capacity);

    for(uint16_t i = 0; i < capacity; i++) {
        if(world->cell[i] == CellLabCellLife) {
            const CellLabGenome genome = world->genome[i];
            const uint8_t program = cell_lab_gene_program(genome);
            const uint16_t x = i % width;
            const uint16_t y = i / width;

            world->life_count++;
            energy_sum += world->energy[i];
            temp_gene_sum += cell_lab_gene_temp(genome);
            light_gene_sum += cell_lab_gene_light(genome);
            crowding_sum += cell_lab_live_neighbor_count(world, x, y);
            world->program_count[program]++;
        } else if(world->cell[i] == CellLabCellFood) {
            world->food_count++;
        }
    }

    if(world->life_count > 0) {
        world->avg_energy = (uint8_t)(energy_sum / world->life_count);
        world->avg_temp_gene = (uint8_t)(temp_gene_sum / world->life_count);
        world->avg_light_gene = (uint8_t)(light_gene_sum / world->life_count);
        world->avg_crowding = (uint8_t)(crowding_sum / world->life_count);
    } else {
        world->avg_energy = 0;
        world->avg_temp_gene = 0;
        world->avg_light_gene = 0;
        world->avg_crowding = 0;
    }

    world->dominant_program = 0;
    for(uint8_t i = 1; i < 4; i++) {
        if(world->program_count[i] > world->program_count[world->dominant_program]) {
            world->dominant_program = i;
        }
    }
}

static void cell_lab_add_food(CellLabWorld* world, uint8_t* field, uint16_t amount) {
    // RU: Еда появляется только на пустых клетках и зависит от раздражителей среды.
    // EN: Food appears only on empty cells and depends on environmental irritants.
    uint16_t placed = 0;
    uint16_t attempts = 0;
    const uint16_t max_attempts = amount * 30U;
    const uint16_t width = cell_lab_world_width(world);
    const uint16_t height = cell_lab_world_height(world);

    while((placed < amount) && (attempts < max_attempts)) {
        const uint16_t x = cell_lab_random_limit(world, width);
        const uint16_t y = cell_lab_random_limit(world, height);
        const size_t index = cell_lab_world_index(world, x, y);

        if(field[index] == CellLabCellEmpty) {
            const uint8_t light = cell_lab_world_light_at(world, y);
            const uint8_t temp = cell_lab_world_temperature_at(world, y);
            const uint8_t radiation = cell_lab_world_radiation_at(world, x, y);
            const uint8_t toxin = cell_lab_world_toxin_at(world, x, y);
            const uint8_t mild = (uint8_t)(15U - cell_lab_abs_diff(temp, 9U));
            uint8_t growth_score = (uint8_t)(2U + light + (mild / 5U));

            // RU: Радиация и токсины подавляют рост еды, создавая пустые зоны.
            // EN: Radiation and toxins suppress food growth, creating empty zones.
            if(radiation > 8U) {
                growth_score = (growth_score > 2U) ? (growth_score - 2U) : 0U;
            }

            if(toxin > 7U) {
                growth_score = (growth_score > 3U) ? (growth_score - 3U) : 0U;
            }

            if(cell_lab_random_limit(world, 28) < growth_score) {
                field[index] = CellLabCellFood;
                placed++;
            }
        }

        attempts++;
    }
}

static void cell_lab_add_life(CellLabWorld* world, uint16_t amount) {
    // RU: Стартовые живые клетки получают энергию и случайный геном.
    // EN: Initial living cells receive energy and a random genome.
    uint16_t placed = 0;
    uint16_t attempts = 0;
    const uint16_t max_attempts = amount * 30U;
    const uint16_t width = cell_lab_world_width(world);
    const uint16_t height = cell_lab_world_height(world);

    while((placed < amount) && (attempts < max_attempts)) {
        const uint16_t x = cell_lab_random_limit(world, width);
        const uint16_t y = cell_lab_random_limit(world, height);
        const size_t index = cell_lab_world_index(world, x, y);

        if(world->cell[index] != CellLabCellLife) {
            world->cell[index] = CellLabCellLife;
            world->energy[index] = world->config.start_energy_min +
                                   cell_lab_random_limit(world, world->config.start_energy_spread);
            world->genome[index] = cell_lab_random_genome(world);
            placed++;
        }

        attempts++;
    }
}

void cell_lab_world_init(CellLabWorld* world, const CellLabWorldConfig* config) {
    // RU: Полное обнуление важно: буферы мира хранятся static и переживают перезапуски app.
    // EN: Full zeroing matters: world buffers are static and survive app restarts.
    memset(world, 0, sizeof(CellLabWorld));
    world->config = *config;
    world->rng = 0xC011ABU ^ furi_hal_random_get() ^ furi_get_tick();
    cell_lab_world_start_new_generation(world);
}

void cell_lab_world_apply_config(CellLabWorld* world, const CellLabWorldConfig* config) {
    // RU: Новая конфигурация может менять размер мира, поэтому старую эпоху полностью сбрасываем.
    // EN: A new config may change world size, so the previous epoch is fully reset.
    world->config = *config;
    cell_lab_world_start_new_generation(world);
}

void cell_lab_world_start_new_generation(CellLabWorld* world) {
    // RU: Новая эпоха очищает все клетки, но сохраняет настройки мира.
    // EN: A new epoch clears all cells while keeping world settings.
    const uint16_t capacity = cell_lab_world_capacity(world);

    memset(world->cell, 0, sizeof(world->cell));
    memset(world->energy, 0, sizeof(world->energy));
    memset(world->genome, 0, sizeof(world->genome));
    memset(world->next_cell, 0, sizeof(world->next_cell));
    memset(world->next_energy, 0, sizeof(world->next_energy));
    memset(world->next_genome, 0, sizeof(world->next_genome));
    memset(world->consumed, 0, sizeof(world->consumed));
    memset(world->processed, 0, sizeof(world->processed));

    world->generation = 0;
    world->births = 0;
    world->deaths = 0;
    world->mutations = 0;
    world->cannibal_events = 0;
    world->food_phase = 0;
    world->epoch++;
    world->rng ^= furi_hal_random_get();
    world->rng ^= furi_get_tick();
    world->climate_seed = (uint8_t)cell_lab_random_limit(world, 32);

    cell_lab_add_food(world, world->cell, world->config.initial_food);
    cell_lab_add_life(world, world->config.initial_life);
    cell_lab_recount(world);

    // RU: Если настройки дали слишком маленький мир, гарантируем хотя бы одну живую клетку.
    // EN: If settings produced a very small world, guarantee at least one living cell.
    if((world->life_count == 0) && (capacity > 0)) {
        world->cell[0] = CellLabCellLife;
        world->energy[0] = world->config.start_energy_min;
        world->genome[0] = cell_lab_random_genome(world);
        cell_lab_recount(world);
    }
}

static int16_t cell_lab_score_place(
    CellLabWorld* world,
    CellLabGenome genome,
    uint16_t x,
    uint16_t y,
    CellLabTargetKind kind,
    uint8_t victim_energy) {
    // RU: Оценка места — сердце поведения: геном переводится в предпочтение направления.
    // EN: Place scoring is the behavior core: the genome becomes movement preference.
    const uint8_t program = cell_lab_gene_program(genome);
    const uint8_t metabolism = cell_lab_gene_metabolism(genome);
    const uint8_t light = cell_lab_world_light_at(world, y);
    const uint8_t temp = cell_lab_world_temperature_at(world, y);
    const uint8_t radiation = cell_lab_world_radiation_at(world, x, y);
    const uint8_t toxin = cell_lab_world_toxin_at(world, x, y);
    const uint8_t light_match =
        (uint8_t)(15U - cell_lab_abs_diff(cell_lab_gene_light(genome), light));
    const uint8_t temp_match =
        (uint8_t)(15U - cell_lab_abs_diff(cell_lab_gene_temp(genome), temp));
    const uint8_t radiation_tolerance = (uint8_t)(5U + (cell_lab_gene_light(genome) / 2U) +
                                                  (cell_lab_gene_stress_resist(genome) * 4U));
    const uint8_t toxin_tolerance =
        (uint8_t)(4U + (metabolism * 2U) + (cell_lab_gene_stress_resist(genome) * 4U));
    const uint8_t radiation_stress =
        (radiation > radiation_tolerance) ? (uint8_t)(radiation - radiation_tolerance) : 0U;
    const uint8_t toxin_stress = (toxin > toxin_tolerance) ? (uint8_t)(toxin - toxin_tolerance) :
                                                             0U;
    int16_t score = (int16_t)cell_lab_random_limit(world, 8);

    // RU: Базово любая клетка любит совпадение со своим светом и температурой.
    // EN: By default, every cell likes matching its preferred light and temperature.
    score += (int16_t)((light_match + temp_match) / 2U);
    score -= (int16_t)(radiation_stress + (toxin_stress * 2U));

    if(kind == CellLabTargetFood) {
        // RU: Еда сильно притягивает, особенно программу Forager.
        // EN: Food is highly attractive, especially for the Forager program.
        score += 24;
        score += (program == CellLabProgramForager) ? 18 : 5;
        score += (metabolism >= 2U) ? 5 : 0;
    } else if(kind == CellLabTargetKin) {
        // RU: Живой сосед привлекателен только для каннибала; другим это почти запрещено.
        // EN: A living neighbor is attractive only for cannibals; others are almost forbidden.
        score += 14 + (victim_energy / 2U);
        score += cell_lab_gene_cannibal(genome) ? 10 : -80;
    }

    if(program == CellLabProgramPhoto) {
        score += (int16_t)(light_match * 2U);
        score += (int16_t)(light / 2U);
    } else if(program == CellLabProgramThermo) {
        score += (int16_t)(temp_match * 2U);
    } else if(program == CellLabProgramNomad) {
        score += (int16_t)cell_lab_random_limit(world, 22);
        score += (int16_t)(cell_lab_gene_stress_resist(genome) ? 4 : 0);
    }

    return score;
}

static bool cell_lab_pick_target(
    CellLabWorld* world,
    CellLabGenome genome,
    uint16_t x,
    uint16_t y,
    uint8_t* out_x,
    uint8_t* out_y,
    CellLabTargetKind* out_kind) {
    // RU: Клетка просматривает стояние на месте и 8 соседей, выбирая лучший вариант.
    // EN: A cell considers staying still plus 8 neighbors and chooses the best option.
    int16_t best_score = -32000;
    uint8_t best_x = (uint8_t)x;
    uint8_t best_y = (uint8_t)y;
    CellLabTargetKind best_kind = CellLabTargetStay;
    const uint8_t start = (uint8_t)cell_lab_random_limit(world, 8);
    const size_t source = cell_lab_world_index(world, x, y);

    for(uint8_t option = 0; option < 9; option++) {
        uint8_t cx = (uint8_t)x;
        uint8_t cy = (uint8_t)y;

        if(option > 0) {
            const uint8_t neighbor = (uint8_t)((start + option - 1U) & 0x07U);
            cell_lab_neighbor(world, x, y, neighbor, &cx, &cy);
        }

        const size_t index = cell_lab_world_index(world, cx, cy);

        if(world->next_cell[index] == CellLabCellLife) {
            continue;
        }

        CellLabTargetKind kind = CellLabTargetStay;
        uint8_t victim_energy = 0;

        if((index != source) && (world->cell[index] == CellLabCellLife)) {
            // RU: Съесть можно только того, кто еще не ходил и не был съеден раньше.
            // EN: A victim can be eaten only if it has not moved and was not eaten already.
            const bool can_eat_kin = cell_lab_gene_cannibal(genome) && !world->processed[index] &&
                                     !world->consumed[index];

            if(!can_eat_kin) {
                continue;
            }

            kind = CellLabTargetKin;
            victim_energy = world->energy[index];
        } else if(
            (world->cell[index] == CellLabCellFood) &&
            (world->next_cell[index] == CellLabCellFood)) {
            kind = CellLabTargetFood;
        }

        const int16_t score = cell_lab_score_place(world, genome, cx, cy, kind, victim_energy);

        if(score > best_score) {
            best_score = score;
            best_x = cx;
            best_y = cy;
            best_kind = kind;
        }
    }

    if(best_score <= -32000) {
        return false;
    }

    *out_x = best_x;
    *out_y = best_y;
    *out_kind = best_kind;
    return true;
}

static bool cell_lab_find_birth_place(
    CellLabWorld* world,
    CellLabGenome child_genome,
    uint16_t x,
    uint16_t y,
    uint8_t* out_x,
    uint8_t* out_y) {
    // RU: Ребенок рождается в соседней нише, которая подходит его уже мутировавшему геному.
    // EN: A child is born into a neighboring niche that suits its already-mutated genome.
    int16_t best_score = -32000;
    uint8_t best_x = (uint8_t)x;
    uint8_t best_y = (uint8_t)y;
    const uint8_t start = (uint8_t)cell_lab_random_limit(world, 8);

    for(uint8_t step = 0; step < 8; step++) {
        uint8_t nx;
        uint8_t ny;
        const uint8_t neighbor = (uint8_t)((start + step) & 0x07U);
        cell_lab_neighbor(world, x, y, neighbor, &nx, &ny);

        const size_t index = cell_lab_world_index(world, nx, ny);
        if(world->next_cell[index] == CellLabCellLife) {
            continue;
        }

        const CellLabTargetKind kind =
            (world->next_cell[index] == CellLabCellFood) ? CellLabTargetFood : CellLabTargetStay;
        const int16_t score = cell_lab_score_place(world, child_genome, nx, ny, kind, 0);

        if(score > best_score) {
            best_score = score;
            best_x = nx;
            best_y = ny;
        }
    }

    if(best_score <= -32000) {
        return false;
    }

    *out_x = best_x;
    *out_y = best_y;
    return true;
}

static uint8_t cell_lab_live_neighbor_count(CellLabWorld* world, uint16_t x, uint16_t y) {
    // RU: Скученность — отдельный раздражитель, который делает плотные колонии опасными.
    // EN: Crowding is a separate irritant that makes dense colonies dangerous.
    uint8_t count = 0;

    for(uint8_t neighbor = 0; neighbor < 8; neighbor++) {
        uint8_t nx;
        uint8_t ny;
        cell_lab_neighbor(world, x, y, neighbor, &nx, &ny);

        if(world->cell[cell_lab_world_index(world, nx, ny)] == CellLabCellLife) {
            count++;
        }
    }

    return count;
}

static void cell_lab_step_life(CellLabWorld* world, uint16_t x, uint16_t y) {
    // RU: Один ход клетки: получить бонусы, заплатить цену среды, выбрать цель и размножиться.
    // EN: One cell turn: gain bonuses, pay environmental cost, choose a target, and reproduce.
    const size_t source = cell_lab_world_index(world, x, y);
    CellLabGenome genome = world->genome[source];
    const uint8_t program = cell_lab_gene_program(genome);
    const uint8_t metabolism = cell_lab_gene_metabolism(genome);
    const uint8_t local_temp = cell_lab_world_temperature_at(world, y);
    const uint8_t local_light = cell_lab_world_light_at(world, y);
    const uint8_t local_radiation = cell_lab_world_radiation_at(world, x, y);
    const uint8_t local_toxin = cell_lab_world_toxin_at(world, x, y);
    const uint8_t temp_miss = cell_lab_abs_diff(cell_lab_gene_temp(genome), local_temp);
    const uint8_t light_miss = cell_lab_abs_diff(cell_lab_gene_light(genome), local_light);
    const uint8_t radiation_tolerance = (uint8_t)(5U + (cell_lab_gene_light(genome) / 2U) +
                                                  (cell_lab_gene_stress_resist(genome) * 4U));
    const uint8_t toxin_tolerance =
        (uint8_t)(4U + (metabolism * 2U) + (cell_lab_gene_stress_resist(genome) * 4U));
    const uint8_t radiation_stress = (local_radiation > radiation_tolerance) ?
                                         (uint8_t)(local_radiation - radiation_tolerance) :
                                         0U;
    const uint8_t toxin_stress =
        (local_toxin > toxin_tolerance) ? (uint8_t)(local_toxin - toxin_tolerance) : 0U;
    const uint8_t crowding = cell_lab_live_neighbor_count(world, x, y);
    uint8_t energy = world->energy[source];
    uint8_t move_cost = world->config.move_cost;

    // RU: Хороший свет и правильная программа дают слабый энергетический бонус.
    // EN: Good light and the right program give a small energy bonus.
    if((light_miss <= 2U) && (((world->generation + source) % 3U) == 0U)) {
        energy = cell_lab_energy_add(world, energy, 1);
    }

    if((program == CellLabProgramPhoto) && (light_miss <= 3U) &&
       (((world->generation + source) & 3U) == 0U)) {
        energy = cell_lab_energy_add(world, energy, 1);
    }

    if((program == CellLabProgramThermo) && (temp_miss <= 1U) &&
       ((world->generation + (source >> 3)) & 7U) == 0U) {
        energy = cell_lab_energy_add(world, energy, 1);
    }

    if(metabolism >= 2U) {
        move_cost++;
    }

    if(temp_miss > 4U) {
        move_cost++;
    }

    if(temp_miss > 8U) {
        move_cost++;
    }

    if((program == CellLabProgramPhoto) && (light_miss > 6U)) {
        move_cost++;
    }

    if(radiation_stress > 2U) {
        move_cost++;
    }

    if(toxin_stress > 1U) {
        move_cost++;
    }

    if((radiation_stress + toxin_stress) > 8U) {
        // RU: Сильный стресс может вызвать мутацию прямо при жизни клетки.
        // EN: Strong stress can mutate a cell while it is alive.
        move_cost++;

        // RU: Иногда стресс не убивает сразу, а меняет наследуемый код выжившей клетки.
        // EN: Sometimes stress does not kill immediately and instead changes inherited code.
        if(cell_lab_random_limit(world, 4) == 0U) {
            genome ^= (CellLabGenome)(1U << cell_lab_random_limit(world, 16));
            world->mutations++;
        }
    }

    if(crowding >= 4U) {
        move_cost++;
    }

    if(crowding >= 6U) {
        move_cost++;

        if((cell_lab_random(world) & 1U) == 0U) {
            world->deaths++;
            return;
        }
    }

    if(energy <= move_cost) {
        // RU: Если энергии не хватило на ход, клетка умирает и иногда становится едой.
        // EN: If energy cannot pay the move cost, the cell dies and may become food.
        world->deaths++;

        if(((cell_lab_random(world) & 0x03U) == 0) &&
           (world->next_cell[source] != CellLabCellLife)) {
            world->next_cell[source] = CellLabCellFood;
        }

        return;
    }

    energy -= move_cost;

    uint8_t target_x = (uint8_t)x;
    uint8_t target_y = (uint8_t)y;
    CellLabTargetKind target_kind = CellLabTargetStay;

    if(!cell_lab_pick_target(world, genome, x, y, &target_x, &target_y, &target_kind)) {
        world->deaths++;
        return;
    }

    const size_t target = cell_lab_world_index(world, target_x, target_y);

    if(target_kind == CellLabTargetFood) {
        // RU: Съеденная еда превращается в энергию, зависящую от генов и среды.
        // EN: Eaten food becomes energy depending on genes and environment.
        uint8_t food_bonus = world->config.food_energy;

        if(program == CellLabProgramForager) {
            food_bonus += 2U;
        }

        if(temp_miss <= 3U) {
            food_bonus += 1U;
        }

        energy = cell_lab_energy_add(world, energy, food_bonus);
    } else if(target_kind == CellLabTargetKin) {
        // RU: Каннибал забирает часть энергии и перемешивает свой геном с геномом жертвы.
        // EN: A cannibal takes part of the energy and mixes its genome with the victim genome.
        const CellLabGenome victim_genome = world->genome[target];
        const uint8_t victim_energy = world->energy[target];

        world->consumed[target] = 1U;
        world->deaths++;
        world->cannibal_events++;

        energy = cell_lab_energy_add(world, energy, (uint8_t)(2U + (victim_energy / 2U)));
        genome = cell_lab_mix_genome(world, genome, victim_genome);
    }

    world->next_cell[target] = CellLabCellLife;
    world->next_energy[target] = energy;
    world->next_genome[target] = genome;

    const uint8_t reproduce_threshold =
        (uint8_t)(world->config.reproduce_energy + (metabolism * 2U) +
                  (cell_lab_gene_stress_resist(genome) ? 3U : 0U) +
                  (cell_lab_gene_cannibal(genome) ? 2U : 0U));

    if(energy >= reproduce_threshold) {
        // RU: Размножение копирует геном с мутациями и делит энергию между родителем и ребенком.
        // EN: Reproduction copies the genome with mutations and splits energy between parent and child.
        uint8_t child_x;
        uint8_t child_y;
        const uint32_t mutations_before = world->mutations;
        const CellLabGenome child_genome = cell_lab_mutate_genome(world, genome);

        if(cell_lab_find_birth_place(world, child_genome, target_x, target_y, &child_x, &child_y)) {
            const size_t child = cell_lab_world_index(world, child_x, child_y);
            const uint8_t child_energy = energy / 2U;
            const uint8_t parent_energy = energy - child_energy;

            world->next_cell[child] = CellLabCellLife;
            world->next_energy[child] = child_energy;
            world->next_genome[child] = child_genome;
            world->next_energy[target] = parent_energy;
            world->births++;
        } else {
            world->mutations = mutations_before;
        }
    }
}

void cell_lab_world_step(CellLabWorld* world) {
    // RU: Один тик строит next-буферы из текущего мира, затем заменяет ими текущий мир.
    // EN: One tick builds next buffers from the current world, then swaps them into current state.
    const uint16_t capacity = cell_lab_world_capacity(world);
    const uint16_t width = cell_lab_world_width(world);
    const uint16_t height = cell_lab_world_height(world);

    memset(world->next_cell, 0, sizeof(world->next_cell));
    memset(world->next_energy, 0, sizeof(world->next_energy));
    memset(world->next_genome, 0, sizeof(world->next_genome));
    memset(world->consumed, 0, sizeof(world->consumed));
    memset(world->processed, 0, sizeof(world->processed));

    for(uint16_t i = 0; i < capacity; i++) {
        // RU: Еду переносим первой, чтобы живые клетки могли съесть ее перезаписью.
        // EN: Food is copied first so living cells can eat it by overwriting it.
        if(world->cell[i] == CellLabCellFood) {
            world->next_cell[i] = CellLabCellFood;
        }
    }

    for(uint16_t y = 0; y < height; y++) {
        // RU: Живые клетки ходят в порядке сканирования, а consumed защищает от двойного хода жертвы.
        // EN: Living cells move in scan order, and consumed prevents an eaten victim from moving.
        for(uint16_t x = 0; x < width; x++) {
            const size_t index = cell_lab_world_index(world, x, y);

            if((world->cell[index] == CellLabCellLife) && !world->consumed[index]) {
                world->processed[index] = 1U;
                cell_lab_step_life(world, x, y);
            }
        }
    }

    cell_lab_add_food(world, world->next_cell, world->config.food_growth);

    uint16_t next_food_count = 0;
    for(uint16_t i = 0; i < capacity; i++) {
        if(world->next_cell[i] == CellLabCellFood) {
            next_food_count++;
        }
    }

    if(next_food_count < world->config.food_low_watermark) {
        // RU: Спасательный рост еды не делает мир легким, но предотвращает вечную пустыню.
        // EN: Rescue food growth does not make the world easy, but prevents eternal desert.
        cell_lab_add_food(world, world->next_cell, world->config.food_rescue_burst);
    }

    memcpy(world->cell, world->next_cell, sizeof(world->cell));
    memcpy(world->energy, world->next_energy, sizeof(world->energy));
    memcpy(world->genome, world->next_genome, sizeof(world->genome));

    world->generation++;
    world->food_phase++;
    cell_lab_recount(world);

    if(world->life_count == 0) {
        // RU: Полное вымирание автоматически запускает новую эпоху.
        // EN: Total extinction automatically starts a new epoch.
        cell_lab_world_start_new_generation(world);
    }
}
