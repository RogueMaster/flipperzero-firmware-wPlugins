#pragma once

#include "../subbrute_custom_event.h"
#include <gui/view.h>
#include <input/input.h>
#include <gui/elements.h>

typedef void (*SubBruteAttackViewCallback)(SubBruteCustomEvent event, void* context);
typedef struct SubBruteAttackView SubBruteAttackView;

/**
 * @brief Sets the callback function and context for the SubBruteAttackView.
 *
 * This function sets the callback function and context that will be used by the SubBruteAttackView
 * instance. The callback function will be called when a particular event occurs in the SubBruteAttackView.
 * The context parameter will be passed to the callback function as an argument.
 *
 * @param[in] instance The SubBruteAttackView instance to set the callback for.
 * @param[in] callback The callback function to be called when an event occurs.
 * @param[in] context The context to be passed to the callback function when it is called.
 *
 * @note The callback function should have the following signature:
 *       `void callback(SubBruteCustomEvent event, void* context)`
 *
 * @note The callback function should not take ownership of the instance, it should only perform
 *       the necessary logic based on the event.
 *
 * @warning The instance parameter must not be NULL.
 * @warning The callback parameter must not be NULL.
 */
void subbrute_attack_view_set_callback(
    SubBruteAttackView* instance,
    SubBruteAttackViewCallback callback,
    void* context);

/**
 * @brief Allocates memory for a new SubBruteAttackView object.
 *
 * This function allocates memory for a new @c SubBruteAttackView structure on the heap, assigns necessary properties
 * and returns a pointer to it. The SubBruteAttackView struct is consisted of attributes attack_type, max_value,
 * current_step, is_attacking and icon. The initial values are assigned in this function. It also sets up
 * the necessary callbacks for view drawing, input handling, and entrance/exit events.
 *
 * @note As there are no input parameters, @c tag is not required for this function. Instead, the structure
 *       SubBruteAttackView and its members are initialized within the function.
 *
 * @return A pointer to a newly allocated @c SubBruteAttackView object.
 *
 */
SubBruteAttackView* subbrute_attack_view_alloc();

/**
 * @brief Frees the memory allocated for a SubBruteAttackView instance.
 *
 * The function verifies the provided instance, frees the icon animation in the
 * associated view model (if any), frees the view, and finally, frees the instance itself.
 * This effectively destroys the SubBruteAttackView instance and cleans up the associated resources.
 * After a call to this function, the provided pointer should not be used.
 *
 * @param[in] instance Pointer to the SubBruteAttackView instance to free.
 *
 */
void subbrute_attack_view_free(SubBruteAttackView* instance);

/**
 * @brief Retrieves the view associated with the SubBruteAttackView instance.
 *
 * @param[in] instance The SubBruteAttackView instance to retrieve the view from.
 *
 * @return A pointer to the associated view.
 *
 */
View* subbrute_attack_view_get_view(SubBruteAttackView* instance);

/**
 * @brief Sets the current step for the SubBruteAttackView instance.
 *
 * This function sets the current step of the SubBruteAttackView instance
 * to the specified value.
 *
 * @param [in] instance A pointer to the SubBruteAttackView instance.
 * @param [in] current_step The value to set as the current step.
 *
 * @note The current step represents the current progress of the attack view.
 *       It should be an unsigned 64-bit integer.
 */
void subbrute_attack_view_set_current_step(SubBruteAttackView* instance, uint64_t current_step);

/**
 * @class SubBruteAttackView
 * @brief Class for initializing the values of a SubBruteAttackView instance.
 *
 * The SubBruteAttackView class is used to store and initialize the values
 * of a SubBruteAttackView instance. These values include the index, maximum value,
 * current step, attack status, and extra repeats.
 *
 * @note We need to call init every time, because not every time we call "enter"
 *       normally, so call "enter" only once
 *
 * @param[in] instance A pointer to the instance of SubBruteAttackView to be initialized.
 * @param[in] index The starting index for attack.
 * @param[in] max_value The maximum value that could be used for the attack.
 * @param[in] current_step The current step of the attack.
 * @param[in] is_attacking A boolean flag to identify if the current instance is attacking.
 * @param[in] extra_repeats The number of extra repeats in the attack.
 *
 */
void subbrute_attack_view_init_values(
    SubBruteAttackView* instance,
    uint8_t index,
    uint64_t max_value,
    uint64_t current_step,
    bool is_attacking,
    uint8_t extra_repeats);
