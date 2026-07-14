#include "../app.h"
#include "../datetime/datetime.h"
#include "../tasks/task.h"
#include <furi.h>
#include <stdlib.h>
#include <storage/storage.h>

#define TAG "tracker_app"

bool write_task_to_csv(App* app, const Task* task) {
    if(!storage_file_open(app->file, APP_DATA_PATH("data.csv"), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FURI_LOG_E(TAG, "Failed to open file for writing");
        return false;
    }

    // Allocate a FuriString buffer
    FuriString* buffer = furi_string_alloc();

    // Convert DateTime fields to ISO 8601 strings
    char start_time_str[20], end_time_str[20], last_start_time_str[20];
    datetime_to_string_iso8601(start_time_str, sizeof(start_time_str), &task->start_time);
    datetime_to_string_iso8601(end_time_str, sizeof(end_time_str), &task->end_time);
    datetime_to_string_iso8601(
        last_start_time_str, sizeof(last_start_time_str), &task->last_start_time);

    // Format the task into the buffer
    furi_string_printf(
        buffer,
        "%s,%s,%s,%.2f,%s,%s,%s,%d,%u,%s\n",
        task->id,
        task->name,
        task->description,
        (double)task->price_per_hour,
        start_time_str,
        end_time_str,
        last_start_time_str,
        task->completed,
        task->total_time_minutes,
        task_status_to_string(task->status));

    // Write the buffer to the file
    if(!storage_file_write(app->file, furi_string_get_cstr(buffer), furi_string_size(buffer))) {
        FURI_LOG_E(TAG, "Failed to write task to file");
        furi_string_free(buffer);
        storage_file_close(app->file);
        return false;
    }

    // Free the buffer
    furi_string_free(buffer);
    storage_file_close(app->file);
    return true;
}

static bool read_line_from_file(App* app, FuriString* str_result) {
    furi_string_reset(str_result);
    uint8_t buffer[1]; // Read one character at a time
    bool result = false;

    FURI_LOG_I(TAG, "starting to read the file");

    while(true) {
        size_t read_count = storage_file_read(app->file, buffer, sizeof(buffer));
        if(read_count == 0) {
            break; // End of file
        }

        if(buffer[0] == '\n') {
            result = true;
            break; // End of line
        } else {
            furi_string_push_back(str_result, buffer[0]);
        }
    }

    // Ensure the string is null-terminated
    furi_string_push_back(str_result, '\0');

    FURI_LOG_I(TAG, "result: %s", furi_string_get_cstr(str_result));
    return result;
}

bool read_tasks_from_csv(App* app) {
    // Allocate a FuriString buffer
    FuriString* buffer = furi_string_alloc();
    // Ensure the file is open before reading
    if(!storage_file_open(app->file, APP_DATA_PATH("data.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file");
        furi_string_free(buffer);
        return false;
    }

    furi_assert(app);
    FURI_LOG_I(TAG, "read_tasks_from_csv");

    while(read_line_from_file(app, buffer)) {
        // Allocate a task
        Task task = {0};

        // Ensure the buffer is null-terminated
        const char* buffer_str = furi_string_get_cstr(buffer);
        FURI_LOG_I(TAG, "Buffer string: %s", buffer_str);

        // Temporary strings to hold datetime and other fields
        char price_per_hour_str[20] = {0};
        char start_time_str[20] = {0};
        char end_time_str[20] = {0};
        char last_start_time_str[20] = {0};
        char status_str[20] = {0};

        sscanf(
            buffer_str,
            "%49[^,],%49[^,],%99[^,],%19[^,],%19[^,],%19[^,],%19[^,],%d,%u,%19[^,]",
            task.id,
            task.name,
            task.description,
            price_per_hour_str,
            start_time_str,
            end_time_str,
            last_start_time_str,
            (int*)&task.completed,
            &task.total_time_minutes,
            status_str);

        // Convert strings to appropriate types
        task.price_per_hour = strtof(price_per_hour_str, NULL);
        string_to_datetime_iso8601(start_time_str, &task.start_time);
        string_to_datetime_iso8601(end_time_str, &task.end_time);
        string_to_datetime_iso8601(last_start_time_str, &task.last_start_time);
        task.status = string_to_task_status(status_str);

        // Add the task to the tasks
        if(tasks_add(app, &task)) {
            FURI_LOG_I(TAG, "Task added to tasks");
        } else {
            FURI_LOG_E(TAG, "Failed to add task to tasks");
        };
    }

    // Free the buffer
    furi_string_free(buffer);
    storage_file_close(app->file);
    return true;
}

bool find_and_replace_task_in_csv(App* app, const Task* current_task) {
    FURI_LOG_I(TAG, "Finding and replacing task with ID: %s", current_task->id);
    furi_assert(app);

    // Allocate a FuriString buffer for reading lines
    FuriString* buffer_task = furi_string_alloc();
    // Allocate a FuriString buffer for the new content
    FuriString* buffer_new_content = furi_string_alloc();

    bool task_found = false;

    if(!storage_file_open(app->file, APP_DATA_PATH("data.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file for reading");
        furi_string_free(buffer_task);
        furi_string_free(buffer_new_content);
        storage_file_close(app->file);
        return false;
    }

    // Read each line and check if it matches the task_id
    while(read_line_from_file(app, buffer_task)) {
        Task task = {0};

        // Ensure the buffer is null-terminated
        const char* buffer_str = furi_string_get_cstr(buffer_task);
        FURI_LOG_I(TAG, "Buffer string: %s", buffer_str);

        // Temporary strings to hold datetime and other fields
        char price_per_hour_str[20] = {0};
        char start_time_str[20] = {0};
        char end_time_str[20] = {0};
        char last_start_time_str[20] = {0};
        char status_str[20] = {0};

        sscanf(
            buffer_str,
            "%49[^,],%49[^,],%99[^,],%19[^,],%19[^,],%19[^,],%19[^,],%d,%u,%19[^,]",
            task.id,
            task.name,
            task.description,
            price_per_hour_str,
            start_time_str,
            end_time_str,
            last_start_time_str,
            (int*)&task.completed,
            &task.total_time_minutes,
            status_str);

        // Convert strings to appropriate types
        task.price_per_hour = strtof(price_per_hour_str, NULL);
        string_to_datetime_iso8601(start_time_str, &task.start_time);
        string_to_datetime_iso8601(end_time_str, &task.end_time);
        string_to_datetime_iso8601(last_start_time_str, &task.last_start_time);
        task.status = string_to_task_status(status_str);

        FURI_LOG_I(TAG, "Read line with ID: %s", task.id);

        if(strcmp(task.id, current_task->id) != 0) {
            // If the current line does not match the task_id, append it to new
            // content
            furi_string_cat(buffer_new_content, buffer_str);
            furi_string_cat(buffer_new_content, "\n");
        } else {
            // Replace the task with the current_task
            char start_time_str[20], end_time_str[20], last_start_time_str[20];
            datetime_to_string_iso8601(
                start_time_str, sizeof(start_time_str), &current_task->start_time);
            datetime_to_string_iso8601(
                end_time_str, sizeof(end_time_str), &current_task->end_time);
            datetime_to_string_iso8601(
                last_start_time_str, sizeof(last_start_time_str), &current_task->last_start_time);

            furi_string_printf(
                buffer_task,
                "%s,%s,%s,%.2f,%s,%s,%s,%d,%u,%s\n",
                current_task->id,
                current_task->name,
                current_task->description,
                (double)current_task->price_per_hour,
                start_time_str,
                end_time_str,
                last_start_time_str,
                current_task->completed,
                current_task->total_time_minutes,
                task_status_to_string(current_task->status));
            furi_string_cat(buffer_new_content, furi_string_get_cstr(buffer_task));
            task_found = true;
        }
    }

    // Close the file after reading
    storage_file_close(app->file);

    if(task_found) {
        // Reopen the file for writing
        if(!storage_file_open(
               app->file, APP_DATA_PATH("data.csv"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to open file for writing");
            furi_string_free(buffer_task);
            furi_string_free(buffer_new_content);
            storage_file_close(app->file);
            return false;
        }

        // Write the new content to the file
        if(!storage_file_write(
               app->file,
               furi_string_get_cstr(buffer_new_content),
               furi_string_size(buffer_new_content))) {
            FURI_LOG_E(TAG, "Failed to write new content to file");
            furi_string_free(buffer_task);
            furi_string_free(buffer_new_content);
            storage_file_close(app->file);
            return false;
        }

        // Close the file after writing
        storage_file_close(app->file);
    }

    // Free the buffers
    furi_string_free(buffer_task);
    furi_string_free(buffer_new_content);

    FURI_LOG_I(TAG, "Task find and replace completed. Task found: %d", task_found);

    return task_found;
}

bool delete_task_from_csv(App* app, const char* task_id) {
    FURI_LOG_I(TAG, "Removing task with ID: %s", task_id);
    furi_assert(app);

    // Allocate a FuriString buffer for reading lines
    FuriString* buffer_task = furi_string_alloc();
    // Allocate a FuriString buffer for the new content
    FuriString* buffer_new_content = furi_string_alloc();

    bool task_found = false;

    if(!storage_file_open(app->file, APP_DATA_PATH("data.csv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file for reading");
        furi_string_free(buffer_task);
        furi_string_free(buffer_new_content);
        storage_file_close(app->file);
        return false;
    }

    // Read each line and check if it matches the task_id
    while(read_line_from_file(app, buffer_task)) {
        Task task = {0};

        // Ensure the buffer is null-terminated
        const char* buffer_str = furi_string_get_cstr(buffer_task);
        FURI_LOG_I(TAG, "Buffer string: %s", buffer_str);

        // Temporary strings to hold datetime and other fields
        char price_per_hour_str[20] = {0};
        char start_time_str[20] = {0};
        char end_time_str[20] = {0};
        char last_start_time_str[20] = {0};
        char status_str[20] = {0};

        sscanf(
            buffer_str,
            "%49[^,],%49[^,],%99[^,],%19[^,],%19[^,],%19[^,],%19[^,],%d,%u,%19[^,]",
            task.id,
            task.name,
            task.description,
            price_per_hour_str,
            start_time_str,
            end_time_str,
            last_start_time_str,
            (int*)&task.completed,
            &task.total_time_minutes,
            status_str);

        // Convert strings to appropriate types
        task.price_per_hour = strtof(price_per_hour_str, NULL);
        string_to_datetime_iso8601(start_time_str, &task.start_time);
        string_to_datetime_iso8601(end_time_str, &task.end_time);
        string_to_datetime_iso8601(last_start_time_str, &task.last_start_time);
        task.status = string_to_task_status(status_str);

        FURI_LOG_I(TAG, "Read line with ID: %s", task.id);

        if(strcmp(task.id, task_id) != 0) {
            // If the current line does not match the task_id, append it to new
            // content
            furi_string_cat(buffer_new_content, buffer_str);
            furi_string_cat(buffer_new_content, "\n");
        } else {
            // Mark the task as found
            task_found = true;
        }
    }

    // Close the file after reading
    storage_file_close(app->file);

    if(task_found) {
        // Reopen the file for writing
        if(!storage_file_open(
               app->file, APP_DATA_PATH("data.csv"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "Failed to open file for writing");
            furi_string_free(buffer_task);
            furi_string_free(buffer_new_content);
            storage_file_close(app->file);
            return false;
        }

        // Write the new content to the file
        if(!storage_file_write(
               app->file,
               furi_string_get_cstr(buffer_new_content),
               furi_string_size(buffer_new_content))) {
            FURI_LOG_E(TAG, "Failed to write new content to file");
            furi_string_free(buffer_task);
            furi_string_free(buffer_new_content);
            storage_file_close(app->file);
            return false;
        }

        // Close the file after writing
        storage_file_close(app->file);
    }

    // Free the buffers
    furi_string_free(buffer_task);
    furi_string_free(buffer_new_content);

    FURI_LOG_I(TAG, "Task removal completed. Task found: %d", task_found);

    return task_found;
}
