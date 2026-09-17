#include "irb_app.h"
#include <furi_hal_infrared.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool irb_progress(IrbLoadPhase phase, uint32_t done, uint32_t total, void* context) {
    IrbJob* job = context;
    if(atomic_load(&job->cancel)) return false;
    if(phase == IrbLoadCommit) atomic_store(&job->committing, true);
    unsigned percent = total ? (uint64_t)done * 100 / total : 0;
    if(phase != job->last_phase || percent / 5 != job->last_percent / 5 || phase == IrbLoadCommit) {
        const char* status = phase == IrbLoadHash     ? "Checking source..."
                             : phase == IrbLoadVerify ? "Verifying files..."
                             : phase == IrbLoadCache  ? "Using saved index"
                             : phase == IrbLoadCommit ? "Saving files..."
                                                      : "Reading / writing...";
        with_view_model(
            job->app->view, IrbViewModel * m,
            {
                m->progress = percent;
                m->committing = atomic_load(&job->committing);
                m->cancelable = !m->committing;
                snprintf(m->status, sizeof(m->status), "%s", status);
            },
            true);
        job->last_percent = percent;
        job->last_phase = phase;
    }
    return !atomic_load(&job->cancel);
}
static bool transmit_signal(IrbJob* job, const InfraredSignal* signal) {
    bool ok = infrared_signal_is_valid(signal);
    if(ok && infrared_signal_is_raw(signal)) {
        const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
        uint64_t duration = 0;
        for(size_t i = 0; i < raw->timings_size; ++i)
            duration += raw->timings[i];
        ok = duration <= 10000000U;
    }
    if(atomic_load(&job->cancel)) ok = false;
    if(ok && job->app->simulate)
        furi_delay_ms(75);
    else if(ok) {
        ok = !furi_hal_infrared_is_busy();
        if(ok) {
            furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
            infrared_signal_transmit(signal);
        }
    }
    if(!ok)
        snprintf(job->error, sizeof(job->error),
                 "Cannot send: invalid signal, busy IR, or RAW longer than 10 seconds.");
    return ok;
}
static void checking(IrbJob* job, bool active) {
    with_view_model(
        job->app->view, IrbViewModel * m,
        {
            m->checking = active;
            if(active) {
                m->progress = 0;
                snprintf(m->status, sizeof(m->status), "Checking library...");
            }
        },
        true);
}
static uint32_t elapsed_ms(uint32_t started) {
    return (uint64_t)(furi_get_tick() - started) * 1000 / furi_kernel_get_tick_frequency();
}
static bool run_send(IrbJob* job) {
    IrbSignalKey key;
    if(!irb_signal_key(&job->app->library, &job->project, job->slot, &key)) {
        strcpy(job->error, "No valid signal assigned.");
        return false;
    }
    const InfraredSignal* signal = irb_cache_find(&job->app->signals, &key);
    if(!signal) {
        checking(job, true);
        signal = irb_cache_read(&job->app->signals, job->app->storage, &key, job->error,
                                irb_progress, job);
    }
    checking(job, false);
    if(!signal) return false;
    uint32_t count = 0;
    do {
        if(atomic_load(&job->cancel)) break;
        uint32_t started = furi_get_tick();
        if(!transmit_signal(job, signal)) return false;
        ++count;
        // Keep the feedback useful without spending display time on every 100 ms repeat.
        // The first send is immediate; subsequent redraws are limited to about 3 Hz.
        with_view_model(
            job->app->view, IrbViewModel * m, { m->send_count = count; },
            count == 1 || count % 3 == 0);
        if(!job->repeatable) break;
        uint32_t period = count == 1 ? 350 : IRB_SCAN_PERIOD_MS;
        while(atomic_load(&job->held) && !atomic_load(&job->cancel) && elapsed_ms(started) < period)
            furi_delay_ms(5);
    } while(atomic_load(&job->held) && !atomic_load(&job->cancel));
    return true;
}
static void scan_view(IrbJob* job, bool sending) {
    bool changed = false;
    with_view_model(
        job->app->view, IrbViewModel * m,
        {
            changed = memcmp(&m->scan, &job->scan, sizeof(job->scan)) != 0 || m->sending != sending;
            m->scan = job->scan;
            m->sending = sending;
            if(job->scan.paused) m->pausing = false;
        },
        changed);
}
static bool scan_command(IrbJob* job, IrbLibraryReader* reader) {
    IrbScanCommand command = atomic_exchange(&job->command, IrbScanNone);
    if(command != IrbScanNone) irb_library_reader_close(reader);
    if(command == IrbScanResume) {
        IrbSignalKey key;
        checking(job, true);
        irb_project_set_position(&job->project, job->slot, job->scan.current);
        bool ok = irb_signal_key(&job->app->library, &job->project, job->slot, &key) &&
                  irb_signal_source_check(job->app->storage, &key, job->error, irb_progress, job);
        checking(job, false);
        if(!ok) return false;
    }
    irb_scan_command(&job->scan, command);
    return true;
}
static bool run_scan(IrbJob* job) {
    int group = irb_slot_group_index(job->slot);
    if(group < 0) {
        strcpy(job->error, "This button has no candidate list.");
        return false;
    }
    irb_scan_init(&job->scan, job->app->library.counts[group], job->position);
    scan_view(job, false);
    IrbSignalKey key;
    irb_project_set_position(&job->project, job->slot, job->scan.current);
    checking(job, true);
    bool verified = irb_signal_key(&job->app->library, &job->project, job->slot, &key) &&
                    irb_signal_source_check(job->app->storage, &key, job->error, irb_progress, job);
    checking(job, false);
    if(!verified) return false;
    IrbLibraryReader reader = {0};
    InfraredSignal* signal = infrared_signal_alloc();
    bool ok = true;
    while(ok && !job->scan.stopped && !atomic_load(&job->cancel)) {
        if(!(ok = scan_command(job, &reader))) break;
        if(job->scan.stopped) break;
        scan_view(job, job->scan.needs_send);
        if(job->scan.needs_send) {
            uint32_t started = furi_get_tick();
            irb_project_set_position(&job->project, job->slot, job->scan.current);
            ok = irb_signal_key(&job->app->library, &job->project, job->slot, &key);
            const InfraredSignal* emitted = NULL;
            if(ok && job->scan.paused) {
                emitted = irb_cache_find(&job->app->signals, &key);
                if(!emitted) {
                    checking(job, true);
                    emitted = irb_cache_read(&job->app->signals, job->app->storage, &key,
                                             job->error, irb_progress, job);
                    checking(job, false);
                }
                ok = emitted != NULL;
            } else if(ok) {
                ok = irb_library_reader_open(&reader, &job->app->library) &&
                     irb_library_reader_read(&reader, group, job->scan.current, signal);
                if(ok) emitted = irb_cache_put(&job->app->signals, &key, signal);
            }
            if(!ok) {
                strcpy(job->error, "Cannot read scan signal. Reopen the TV library.");
                break;
            }
            if(!(ok = transmit_signal(job, emitted))) break;
            irb_scan_sent(&job->scan);
            scan_view(job, false);
            // Pace starts like Universal TV, instead of adding 350 ms after every send.
            // Keep the emitted index until Pause is processed, including during a send.
            while(!atomic_load(&job->cancel)) {
                if(atomic_load(&job->command) != IrbScanNone) break;
                uint32_t wait = irb_scan_wait_ms(elapsed_ms(started));
                if(!wait) break;
                furi_delay_ms(wait < 5 ? wait : 5);
            }
        } else
            furi_delay_ms(20);
        if(!(ok = scan_command(job, &reader))) break;
        irb_scan_advance(&job->scan);
        if(job->scan.paused) irb_library_reader_close(&reader);
    }
    irb_library_reader_close(&reader);
    infrared_signal_free(signal);
    return ok;
}
static bool commit_draft(IrbJob* job) {
    return irb_progress(IrbLoadCommit, 0, 1, job) &&
           irb_draft_save(job->app->storage, &job->project, job->error);
}
static void name_from_path(char output[IRB_NAME_SIZE], const char* path) {
    const char* name = strrchr(path, '/');
    name = name ? name + 1 : path;
    unsigned length = 0;
    while(name[length] && length < IRB_NAME_SIZE - 1) {
        char c = name[length];
        if(c == '.' && (name[length + 1] == 'i' || name[length + 1] == 'I') &&
           (name[length + 2] == 'r' || name[length + 2] == 'R') && !name[length + 3])
            break;
        output[length] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' ')
                             ? c
                             : '_';
        ++length;
    }
    while(length && output[length - 1] == ' ')
        --length;
    output[length] = 0;
    if(!irb_name_valid(output, true)) snprintf(output, IRB_NAME_SIZE, "IR_Remote");
}
static void rollback_imports(Storage* storage, IrbProject* project, uint32_t keep) {
    while(project->import_count > keep) {
        --project->import_count;
        storage_common_remove(storage, project->imports[project->import_count].path);
        memset(&project->imports[project->import_count], 0, sizeof(IrbImportSource));
    }
}
int32_t irb_work(void* context) {
    IrbJob* job = context;
    IrbApp* app = job->app;
    Storage* storage = app->storage;
    switch(job->type) {
    case JobLoad:
        if(job->restore) {
            if(!irb_project_load(storage, job->path, &job->project, job->error)) break;
            if(job->saved) {
                char expected[IRB_PATH_SIZE];
                irb_remote_path(expected, sizeof(expected), job->project.name, true);
                if(!irb_path_equal(expected, job->path)) {
                    strcpy(job->error, "Project filename and stored name differ.");
                    break;
                }
            }
        } else {
            irb_project_init(&job->project);
            snprintf(job->project.library, sizeof(job->project.library), "%s", job->path);
        }
        if(!irb_library_open_cached(storage, &job->library, &app->library, job->project.library,
                                    job->error, irb_progress, job))
            break;
        if(!job->restore) {
            job->project.source_hash = job->library.hash;
            job->project.source_size = job->library.size;
        }
        if(!irb_project_matches(&job->project, &job->library)) {
            strcpy(job->error,
                   "TV library changed. Restore the original file or start a new remote.");
            break;
        }
        if(job->restore && !irb_project_check_sources(storage, &job->project, IrbLoadVerify,
                                                      job->error, irb_progress, job))
            break;
        job->ok = job->restore ? !atomic_load(&job->cancel) : commit_draft(job);
        break;
    case JobOpen:
        if(!irb_library_open_cached(storage, &job->library, &app->library, job->project.library,
                                    job->error, irb_progress, job))
            break;
        job->project.source_hash = job->library.hash;
        job->project.source_size = job->library.size;
        name_from_path(job->project.name, job->path);
        job->catalog = calloc(1, sizeof(*job->catalog));
        if(!job->catalog ||
           !irb_catalog_load(storage, job->catalog, job->path, job->error, irb_progress, job) ||
           !irb_catalog_add(storage, job->catalog, &job->project, -1, &job->added, &job->mapped,
                            job->error, irb_progress, job))
            break;
        job->ok = irb_project_matches(&job->project, &job->library) && commit_draft(job);
        if(!job->ok) rollback_imports(storage, &job->project, 0);
        break;
    case JobDraft:
        job->ok = commit_draft(job);
        break;
    case JobSave:
        if(!job->replace && irb_remote_exists(storage, job->project.name)) {
            job->conflict = true;
            break;
        }
        job->ok = irb_remote_save_progress(storage, &app->library, &job->project, job->replace,
                                           job->error, irb_progress, job);
        if(job->ok && job->rename && !irb_path_equal(job->old_name, job->project.name)) {
            char new_name[IRB_NAME_SIZE];
            snprintf(new_name, sizeof(new_name), "%s", job->project.name);
            snprintf(job->project.name, sizeof(job->project.name), "%s", job->old_name);
            bool removed = irb_project_delete(storage, &job->project, true, job->error);
            snprintf(job->project.name, sizeof(job->project.name), "%s", new_name);
            if(!removed)
                strcpy(job->error, "New remote saved. Old files could not all be removed.");
        }
        if(job->ok && app->draft_current && !irb_draft_save(storage, &job->project, job->error))
            strcpy(job->error, "Remote saved. Draft could not be updated.");
        break;
    case JobSend:
        job->ok = run_send(job);
        break;
    case JobScan:
        job->ok = run_scan(job);
        break;
    case JobBrowse:
        job->ok = irb_files_load(storage, &job->files, job->page.path, job->page.projects,
                                 job->error, irb_progress, job);
        if(job->ok) irb_files_get_page(&job->files, job->page.start, &job->page);
        break;
    case JobCatalog:
        job->catalog = calloc(1, sizeof(*job->catalog));
        job->ok = job->catalog &&
                  irb_catalog_load(storage, job->catalog, job->path, job->error, irb_progress, job);
        break;
    case JobImport: {
        uint32_t previous_imports = job->project.import_count;
        job->ok = irb_catalog_add(storage, app->catalog, &job->project, job->entry, &job->added,
                                  &job->mapped, job->error, irb_progress, job);
        if(job->ok && !commit_draft(job)) {
            job->ok = false;
            job->added = 0;
            rollback_imports(storage, &job->project, previous_imports);
        }
    } break;
    case JobDelete:
        if(irb_progress(IrbLoadCommit, 0, 1, job))
            job->ok = irb_project_delete(storage, &job->project, job->both, job->error);
        break;
    case JobSettings:
        if(irb_library_open_cached(storage, &job->library, &app->library, job->path, job->error,
                                   irb_progress, job) &&
           irb_progress(IrbLoadCommit, 0, 1, job))
            job->ok = irb_settings_save(storage, job->path, job->error);
        break;
    }
    view_dispatcher_send_custom_event(app->dispatcher, EventDone);
    return 0;
}
