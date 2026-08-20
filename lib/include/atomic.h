/**
 * @file atomic.h
 * @brief Атомарные операции с понятными именами
 * 
 * Обёртки над GCC __atomic_* функциями для работы с общими данными
 * между потоками и IRQ контекстом.
 * 
 * ## Почему не FuriMutex?
 * - __atomic_* работают в IRQ контексте (FuriMutex нельзя)
 * - __atomic_* = 1-2 такта CPU (FuriMutex = 100-1000+ тактов)
 * - __atomic_* = 0 байт кода (FuriMutex = ~500 байт на вызов)
 * 
 * ## Режимы памяти:
 * - RELAXED — только атомарность, без барьеров (самый быстрый)
 * - ACQUIRE — барьер после чтения (для acquire semantics)
 * - RELEASE — барьер перед записью (для release semantics)
 * - ACQ_REL — комбинация acquire+release
 * 
 * ## Пример использования:
 * @code
 * volatile uint8_t shared_flag = 0;
 * 
 * // В IRQ контексте
 * void irq_handler() {
 *     ATOMIC_SET(shared_flag, 1);  // Быстрая атомарная запись
 * }
 * 
 * // В основном потоке
 * void main_loop() {
 *     if (ATOMIC_GET(shared_flag)) {  // Быстрое атомарное чтение
 *         // обработка
 *     }
 * }
 * @endcode
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Режимы памяти (memory orders)
 * ============================================================================ */

/**
 * @brief Только атомарность, без барьеров памяти
 * 
 * Самый быстрый режим. Подходит для:
 * - флагов, где порядок не важен
 * - счётчиков в IRQ
 * - однопоточных случаев с IRQ
 */
#define ATOMIC_RELAXED __ATOMIC_RELAXED

/**
 * @brief Барьер после чтения (acquire)
 * 
 * Гарантирует, что все чтения/записи ПОСЛЕ этой операции
 * не будут перемещены ДО неё.
 * 
 * Подходит для:
 * - входа в критическую секцию
 * - чтения флага готовности данных
 */
#define ATOMIC_ACQUIRE __ATOMIC_ACQUIRE

/**
 * @brief Барьер перед записью (release)
 * 
 * Гарантирует, что все чтения/записи ДО этой операции
 * не будут перемещены ПОСЛЕ неё.
 * 
 * Подходит для:
 * - выхода из критической секции
 * - записи флага готовности данных
 */
#define ATOMIC_RELEASE __ATOMIC_RELEASE

/**
 * @brief Комбинация acquire + release
 * 
 * Подходит для:
 * - операций read-modify-write
 * - семафоров и мьютексов
 */
#define ATOMIC_ACQ_REL __ATOMIC_ACQ_REL

/* ============================================================================
 * Базовые операции (чтение/запись)
 * ============================================================================ */

/**
 * @brief Атомарное чтение значения
 * 
 * @param var Атомарная переменная (volatile)
 * @param order Режим памяти (ATOMIC_RELAXED, ATOMIC_ACQUIRE, ...)
 * @return Текущее значение переменной
 * 
 * Пример:
 * @code
 * volatile uint32_t counter;
 * uint32_t val = ATOMIC_LOAD(counter, ATOMIC_RELAXED);
 * @endcode
 */
#define ATOMIC_LOAD(var, order) __atomic_load_n(&(var), (order))

/**
 * @brief Атомарная запись значения
 * 
 * @param var Атомарная переменная (volatile)
 * @param val Новое значение
 * @param order Режим памяти (ATOMIC_RELAXED, ATOMIC_RELEASE, ...)
 * 
 * Пример:
 * @code
 * volatile uint32_t counter;
 * ATOMIC_STORE(counter, 42, ATOMIC_RELEASE);
 * @endcode
 */
#define ATOMIC_STORE(var, val, order) __atomic_store_n(&(var), (val), (order))

/**
 * @brief Атомарный обмен (swap)
 * 
 * @param var Атомарная переменная (volatile)
 * @param val Новое значение
 * @param order Режим памяти
 * @return Предыдущее значение переменной
 * 
 * Пример:
 * @code
 * volatile uint8_t flag;
 * uint8_t old = ATOMIC_EXCHANGE(flag, 1, ATOMIC_ACQ_REL);
 * @endcode
 */
#define ATOMIC_EXCHANGE(var, val, order) __atomic_exchange_n(&(var), (val), (order))

/* ============================================================================
 * Логические операции (битовые маски)
 * ============================================================================ */

/**
 * @brief Атомарное OR с возвратом старого значения
 * 
 * @param var Атомарная переменная (volatile)
 * @param mask Маска для OR
 * @param order Режим памяти
 * @return Значение ДО операции
 * 
 * Пример:
 * @code
 * volatile uint8_t buttons;
 * uint8_t old = ATOMIC_FETCH_OR(buttons, BUTTON_A, ATOMIC_RELAXED);
 * @endcode
 */
#define ATOMIC_FETCH_OR(var, mask, order) __atomic_fetch_or(&(var), (mask), (order))

/**
 * @brief Атомарное AND с возвратом старого значения
 * 
 * @param var Атомарная переменная (volatile)
 * @param mask Маска для AND
 * @param order Режим памяти
 * @return Значение ДО операции
 * 
 * Пример:
 * @code
 * volatile uint8_t buttons;
 * ATOMIC_FETCH_AND(buttons, ~BUTTON_A, ATOMIC_RELAXED);  // Сбросить бит
 * @endcode
 */
#define ATOMIC_FETCH_AND(var, mask, order) __atomic_fetch_and(&(var), (mask), (order))

/**
 * @brief Атомарное XOR с возвратом старого значения
 * 
 * @param var Атомарная переменная (volatile)
 * @param mask Маска для XOR
 * @param order Режим памяти
 * @return Значение ДО операции
 * 
 * Пример:
 * @code
 * volatile uint8_t led_state;
 * ATOMIC_FETCH_XOR(led_state, LED_TOGGLE, ATOMIC_RELAXED);  // Переключить
 * @endcode
 */
#define ATOMIC_FETCH_XOR(var, mask, order) __atomic_fetch_xor(&(var), (mask), (order))

/* ============================================================================
 * Арифметические операции
 * ============================================================================ */

/**
 * @brief Атомарное сложение с возвратом старого значения
 * 
 * @param var Атомарная переменная (volatile)
 * @param val Значение для добавления
 * @param order Режим памяти
 * @return Значение ДО операции
 * 
 * Пример:
 * @code
 * volatile uint32_t counter;
 * uint32_t old = ATOMIC_FETCH_ADD(counter, 1, ATOMIC_RELAXED);
 * @endcode
 */
#define ATOMIC_FETCH_ADD(var, val, order) __atomic_fetch_add(&(var), (val), (order))

/**
 * @brief Атомарное вычитание с возвратом старого значения
 * 
 * @param var Атомарная переменная (volatile)
 * @param val Значение для вычитания
 * @param order Режим памяти
 * @return Значение ДО операции
 * 
 * Пример:
 * @code
 * volatile uint32_t counter;
 * uint32_t old = ATOMIC_FETCH_SUB(counter, 1, ATOMIC_RELAXED);
 * @endcode
 */
#define ATOMIC_FETCH_SUB(var, val, order) __atomic_fetch_sub(&(var), (val), (order))

/* ============================================================================
 * Упрощённые макросы для распространённых случаев
 * ============================================================================ */

/**
 * @brief Быстрое атомарное чтение (RELAXED)
 * 
 * Для случаев, где порядок операций не важен.
 */
#define ATOMIC_GET(var) ATOMIC_LOAD(var, ATOMIC_RELAXED)

/**
 * @brief Быстрая атомарная запись (RELAXED)
 * 
 * Для случаев, где порядок операций не важен.
 */
#define ATOMIC_SET(var, val) ATOMIC_STORE(var, val, ATOMIC_RELAXED)

/**
 * @brief Атомарное чтение с барьером acquire
 * 
 * Для чтения флагов готовности данных.
 */
#define ATOMIC_GET_ACQUIRE(var) ATOMIC_LOAD(var, ATOMIC_ACQUIRE)

/**
 * @brief Атомарная запись с барьером release
 * 
 * Для записи флагов готовности данных.
 */
#define ATOMIC_SET_RELEASE(var, val) ATOMIC_STORE(var, val, ATOMIC_RELEASE)

/**
 * @brief Атомарное ИЛИ (без возврата значения)
 */
#define ATOMIC_OR(var, mask) ATOMIC_FETCH_OR(var, mask, ATOMIC_RELAXED)

/**
 * @brief Атомарное И с инверсией маски (сброс битов)
 */
#define ATOMIC_AND(var, mask) ATOMIC_FETCH_AND(var, (mask), ATOMIC_RELAXED)

/**
 * @brief Атомарное сложение (без возврата значения)
 */
#define ATOMIC_ADD(var, val) ATOMIC_FETCH_ADD(var, val, ATOMIC_RELAXED)

/**
 * @brief Атомарное вычитание (без возврата значения)
 */
#define ATOMIC_SUB(var, val) ATOMIC_FETCH_SUB(var, val, ATOMIC_RELAXED)

/**
 * @brief Атомарный инкремент
 */
#define ATOMIC_INC(var) ATOMIC_FETCH_ADD(var, 1, ATOMIC_RELAXED)

/**
 * @brief Атомарный декремент
 */
#define ATOMIC_DEC(var) ATOMIC_FETCH_SUB(var, 1, ATOMIC_RELAXED)

/* ============================================================================
 * Сравнение с обменом (Compare-And-Swap)
 * ============================================================================ */

/**
 * @brief Атомарное сравнение с обменом (CAS)
 * 
 * Если var == expected, то записать desired в var.
 * Всегда возвращать предыдущее значение var.
 * 
 * @param var Атомарная переменная (volatile)
 * @param expected Ожидаемое значение
 * @param desired Желаемое значение (если expected совпадает)
 * @param order Режим памяти
 * @return Предыдущее значение переменной
 * 
 * Пример:
 * @code
 * volatile uint8_t lock;
 * while (ATOMIC_CAS(lock, 0, 1, ATOMIC_ACQ_REL) != 0) {
 *     // Ждём, пока lock не станет 0
 * }
 * // Критическая секция захвачена
 * @endcode
 */
#define ATOMIC_CAS(var, expected, desired, order) \
    __atomic_compare_exchange_n( \
        &(var), &(expected), (desired), 0, (order), (order) \
    ) ? (desired) : (expected)

/**
 * @brief Упрощённый CAS с возвратом успеха
 * 
 * @return true если обмен произошёл, false если var != expected
 */
#define ATOMIC_TRY_CAS(var, expected, desired, order) \
    __atomic_compare_exchange_n( \
        &(var), &(expected), (desired), 0, (order), (order) \
    )

/* ============================================================================
 * Барьеры памяти (memory barriers)
 * ============================================================================ */

/**
 * @brief Полное упорядочивание (full barrier)
 * 
 * Запрещает перемещение любых операций чтения/записи
 * через этот барьер.
 */
#define ATOMIC_FULL_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)

/**
 * @brief Барьер записи (write barrier)
 * 
 * Запрещает перемещение записей после барьера до барьера.
 */
#define ATOMIC_WRITE_BARRIER() __atomic_thread_fence(__ATOMIC_RELEASE)

/**
 * @brief Барьер чтения (read barrier)
 * 
 * Запрещает перемещение чтений до барьера после барьера.
 */
#define ATOMIC_READ_BARRIER() __atomic_thread_fence(__ATOMIC_ACQUIRE)

/* ============================================================================
 * Макросы для объявлений атомарных переменных
 * ============================================================================ */

/**
 * @brief Объявление атомарной переменной
 * 
 * Пример:
 * @code
 * ATOMIC_VAR(volatile uint8_t, button_state, 0);
 * @endcode
 */
#define ATOMIC_VAR(type, name, init) type name = (init)

/**
 * @brief Объявление атомарного флага
 */
#define ATOMIC_FLAG(name) ATOMIC_VAR(volatile uint8_t, name, 0)

/**
 * @brief Объявление атомарного счётчика
 */
#define ATOMIC_COUNTER(name, init) ATOMIC_VAR(volatile uint32_t, name, (init))

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * C++ обёртки (если используется C++)
 * ============================================================================ */

#ifdef __cplusplus

/**
 * @brief Шаблон атомарной переменной для C++
 * 
 * Пример:
 * @code
 * Atomic<uint8_t> button_state{0};
 * button_state.store(1, ATOMIC_RELEASE);
 * uint8_t val = button_state.load(ATOMIC_ACQUIRE);
 * @endcode
 */
template<typename T>
class Atomic {
private:
    volatile T value_;

public:
    constexpr Atomic() : value_(0) {}
    constexpr Atomic(T init) : value_(init) {}

    T load(int order = ATOMIC_RELAXED) const {
        return ATOMIC_LOAD(value_, order);
    }

    void store(T val, int order = ATOMIC_RELAXED) {
        ATOMIC_STORE(value_, val, order);
    }

    T exchange(T val, int order = ATOMIC_RELAXED) {
        return ATOMIC_EXCHANGE(value_, val, order);
    }

    T fetch_or(T mask, int order = ATOMIC_RELAXED) {
        return ATOMIC_FETCH_OR(value_, mask, order);
    }

    T fetch_and(T mask, int order = ATOMIC_RELAXED) {
        return ATOMIC_FETCH_AND(value_, mask, order);
    }

    T fetch_xor(T mask, int order = ATOMIC_RELAXED) {
        return ATOMIC_FETCH_XOR(value_, mask, order);
    }

    T fetch_add(T val, int order = ATOMIC_RELAXED) {
        return ATOMIC_FETCH_ADD(value_, val, order);
    }

    T fetch_sub(T val, int order = ATOMIC_RELAXED) {
        return ATOMIC_FETCH_SUB(value_, val, order);
    }

    bool compare_exchange_weak(T& expected, T desired, int order = ATOMIC_RELAXED) {
        return ATOMIC_TRY_CAS(value_, expected, desired, order);
    }

    operator T() const {
        return load(ATOMIC_RELAXED);
    }

    T operator=(T val) {
        store(val, ATOMIC_RELAXED);
        return val;
    }

    T operator++() {  // префиксный
        return fetch_add(1, order) + 1;
    }

    T operator++(int) {  // постфиксный
        return fetch_add(1, order);
    }

    T operator--() {  // префиксный
        return fetch_sub(1, order) - 1;
    }

    T operator--(int) {  // постфиксный
        return fetch_sub(1, order);
    }
};

#endif /* __cplusplus */
