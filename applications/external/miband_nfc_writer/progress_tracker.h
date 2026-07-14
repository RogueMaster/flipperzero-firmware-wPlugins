/**
 * @file progress_tracker.h
 * @brief Progress tracking utility for long operations
 * 
 * This module provides a reusable progress tracker that can be used
 * across different scenes to show detailed progress information with
 * percentage, progress bar, and estimated time remaining.
 */

#pragma once

#include <furi.h>
#include <gui/modules/popup.h>

/**
 * @brief Progress tracker structure
 */
typedef struct ProgressTracker ProgressTracker;

/**
 * @brief Create a new progress tracker
 * 
 * @param total_items Total number of items to process
 * @param operation_name Name of the operation (e.g., "Writing")
 * @return Allocated ProgressTracker instance
 */
ProgressTracker* progress_tracker_alloc(uint32_t total_items, const char* operation_name);

/**
 * @brief Free progress tracker
 * 
 * @param tracker Progress tracker instance
 */
void progress_tracker_free(ProgressTracker* tracker);

/**
 * @brief Update progress
 * 
 * @param tracker Progress tracker instance
 * @param completed_items Number of completed items
 */
void progress_tracker_update(ProgressTracker* tracker, uint32_t completed_items);

/**
 * @brief Increment progress by one item
 * 
 * @param tracker Progress tracker instance
 */
void progress_tracker_increment(ProgressTracker* tracker);

/**
 * @brief Get current progress percentage
 * 
 * @param tracker Progress tracker instance
 * @return Progress percentage (0-100)
 */
uint8_t progress_tracker_get_percentage(const ProgressTracker* tracker);

/**
 * @brief Get formatted progress string
 * 
 * Generates a string like: "Writing: 45/64 blocks (70%)"
 * 
 * @param tracker Progress tracker instance
 * @return Allocated FuriString with progress text
 */
FuriString* progress_tracker_get_text(const ProgressTracker* tracker);

/**
 * @brief Get progress bar string
 * 
 * Generates ASCII progress bar like: "[=======>   ]"
 * 
 * @param tracker Progress tracker instance
 * @param width Width of progress bar in characters
 * @return Allocated FuriString with progress bar
 */
FuriString* progress_tracker_get_bar(const ProgressTracker* tracker, uint8_t width);

/**
 * @brief Get estimated time remaining
 * 
 * @param tracker Progress tracker instance
 * @return Estimated seconds remaining, or 0 if unknown
 */
uint32_t progress_tracker_get_eta_seconds(const ProgressTracker* tracker);

/**
 * @brief Update popup with progress information
 * 
 * Convenience function to update a popup view with complete progress info.
 * 
 * @param tracker Progress tracker instance
 * @param popup Popup view to update
 * @param header Optional header text (NULL to keep current)
 */
void progress_tracker_update_popup(ProgressTracker* tracker, Popup* popup, const char* header);
