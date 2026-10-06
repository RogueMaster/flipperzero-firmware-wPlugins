#include "dnd_inventory_transaction.h"
#include "dnd_profile_handoff.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_TXN_MAGIC       0x444E4954U
#define DND_TXN_VERSION     1U
#define DND_TXN_NAME_SIZE   48U
#define DND_TXN_PATH_SIZE   128U
#define DND_TXN_BUFFER_SIZE 256U

typedef struct {
    char name[DND_TXN_NAME_SIZE];
    uint32_t old_present;
    uint32_t old_size;
    uint32_t old_crc;
    uint32_t new_size;
    uint32_t new_crc;
} DndInventoryTransactionEntry;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t profile;
    DndInventoryTransactionEntry entry[2];
    uint32_t crc;
} DndInventoryTransaction;

static uint32_t dnd_txn_crc(uint32_t crc, const void* data, size_t size) {
    const uint8_t* bytes = data;
    for(size_t i = 0U; i < size; ++i) {
        crc ^= bytes[i];
        for(uint8_t bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
    }
    return crc;
}

static bool dnd_txn_path(char* path, const char* name, const char* suffix) {
    int length =
        snprintf(path, DND_TXN_PATH_SIZE, "%s/%s%s", DND_CHARACTER_DATA_ROOT, name, suffix);
    return length > 0 && (size_t)length < DND_TXN_PATH_SIZE;
}

static bool dnd_txn_journal_path(char* path, uint32_t profile, const char* suffix) {
    int length = snprintf(
        path,
        DND_TXN_PATH_SIZE,
        "%s/inventory_move_%lu.txn%s",
        DND_CHARACTER_DATA_ROOT,
        (unsigned long)profile,
        suffix);
    return length > 0 && (size_t)length < DND_TXN_PATH_SIZE;
}

/* Do not confuse an SD/read error or a directory collision with a missing file. */
static bool dnd_txn_exists(Storage* storage, const char* path, bool* exists) {
    FileInfo info;
    FS_Error error = storage_common_stat(storage, path, &info);
    *exists = error == FSE_OK;
    return error == FSE_NOT_EXIST || (error == FSE_OK && !file_info_is_dir(&info));
}

static bool dnd_txn_remove(Storage* storage, const char* path) {
    bool exists;
    if(!dnd_txn_exists(storage, path, &exists)) return false;
    return !exists || storage_common_remove(storage, path) == FSE_OK;
}

static bool dnd_txn_name_valid(const char* name, uint32_t profile) {
    const char* end = memchr(name, '\0', DND_TXN_NAME_SIZE);
    if(!end || strncmp(name, "inv", 3U)) return false;
    char suffix[24];
    int length = snprintf(suffix, sizeof(suffix), "_%lu.txt", (unsigned long)profile);
    if(length <= 0 || (size_t)length >= sizeof(suffix)) return false;
    size_t name_size = (size_t)(end - name);
    size_t suffix_size = (size_t)length;
    if(name_size <= 3U + suffix_size || strcmp(end - suffix_size, suffix)) return false;
    size_t bag_size = name_size - 3U - suffix_size;
    if(bag_size > 23U) return false;
    for(const char* p = name + 3U; p < end - suffix_size; ++p) {
        if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
             *p == '_' || *p == '-'))
            return false;
    }
    return true;
}

static bool dnd_txn_same_name(const char* a, const char* b) {
    while(*a && *b) {
        char x = *a++, y = *b++;
        if(x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if(y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if(x != y) return false;
    }
    return *a == *b;
}

static bool dnd_txn_set_name(char* output, const char* live, uint32_t profile) {
    static const char root[] = DND_CHARACTER_DATA_ROOT "/";
    if(!live || strncmp(live, root, sizeof(root) - 1U)) return false;
    const char* name = live + sizeof(root) - 1U;
    size_t length = strlen(name);
    if(length >= DND_TXN_NAME_SIZE) return false;
    memset(output, 0, DND_TXN_NAME_SIZE);
    memcpy(output, name, length);
    return dnd_txn_name_valid(output, profile);
}

/* When destination is NULL this is a streaming fingerprint. Copying also
   checks both closes: a successful write/sync alone is not a durable stage. */
static bool dnd_txn_file(
    Storage* storage,
    const char* source,
    const char* destination,
    uint32_t* size,
    uint32_t* crc) {
    File* input = storage_file_alloc(storage);
    File* output = destination ? storage_file_alloc(storage) : NULL;
    bool input_open = false, output_open = false, success = false;
    if(!input || (destination && !output)) goto cleanup;
    input_open = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING);
    if(!input_open) goto cleanup;
    uint64_t expected = storage_file_size(input);
    if(expected > UINT32_MAX) goto cleanup;
    if(destination) {
        output_open = storage_file_open(output, destination, FSAM_WRITE, FSOM_CREATE_ALWAYS);
        if(!output_open) goto cleanup;
    }
    uint8_t buffer[DND_TXN_BUFFER_SIZE];
    uint32_t remaining = (uint32_t)expected;
    uint32_t checksum = UINT32_MAX;
    while(remaining) {
        size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if(storage_file_read(input, buffer, count) != count) goto cleanup;
        checksum = dnd_txn_crc(checksum, buffer, count);
        if(output && storage_file_write(output, buffer, count) != count) goto cleanup;
        remaining -= count;
    }
    if(storage_file_get_error(input) != FSE_OK || storage_file_size(input) != expected)
        goto cleanup;
    if(output && !storage_file_sync(output)) goto cleanup;
    *size = (uint32_t)expected;
    *crc = checksum ^ UINT32_MAX;
    success = true;
cleanup:
    if(input_open && !storage_file_close(input)) success = false;
    if(output_open && !storage_file_close(output)) success = false;
    if(input) storage_file_free(input);
    if(output) storage_file_free(output);
    return success;
}

static bool dnd_txn_matches(Storage* storage, const char* path, uint32_t size, uint32_t crc) {
    uint32_t actual_size, actual_crc;
    return dnd_txn_file(storage, path, NULL, &actual_size, &actual_crc) && actual_size == size &&
           actual_crc == crc;
}

static bool dnd_txn_entry_valid(Storage* storage, const DndInventoryTransactionEntry* entry) {
    char live[DND_TXN_PATH_SIZE], staged[DND_TXN_PATH_SIZE], backup[DND_TXN_PATH_SIZE];
    if(!dnd_txn_path(live, entry->name, "") || !dnd_txn_path(staged, entry->name, ".move.tmp") ||
       !dnd_txn_path(backup, entry->name, ".move.bak"))
        return false;
    bool has_live, has_stage, has_backup;
    if(!dnd_txn_exists(storage, live, &has_live) || !dnd_txn_exists(storage, staged, &has_stage) ||
       !dnd_txn_exists(storage, backup, &has_backup))
        return false;
    if(has_backup &&
       (!entry->old_present || !dnd_txn_matches(storage, backup, entry->old_size, entry->old_crc)))
        return false;
    if(!has_stage)
        return has_live && dnd_txn_matches(storage, live, entry->new_size, entry->new_crc);
    if(!dnd_txn_matches(storage, staged, entry->new_size, entry->new_crc)) return false;
    if(has_backup) return !has_live;
    return entry->old_present ?
               has_live && dnd_txn_matches(storage, live, entry->old_size, entry->old_crc) :
               !has_live;
}

static bool dnd_txn_publish_entry(Storage* storage, const DndInventoryTransactionEntry* entry) {
    char live[DND_TXN_PATH_SIZE], staged[DND_TXN_PATH_SIZE], backup[DND_TXN_PATH_SIZE];
    if(!dnd_txn_path(live, entry->name, "") || !dnd_txn_path(staged, entry->name, ".move.tmp") ||
       !dnd_txn_path(backup, entry->name, ".move.bak"))
        return false;
    bool has_stage, has_live;
    if(!dnd_txn_exists(storage, staged, &has_stage)) return false;
    if(!has_stage) return true; // Already verified as the new live file in preflight.
    if(!dnd_txn_exists(storage, live, &has_live)) return false;
    if(has_live && storage_common_rename_safe(storage, live, backup) != FSE_OK) return false;
    return storage_common_rename_safe(storage, staged, live) == FSE_OK;
}

bool dnd_inventory_transaction_recover(Storage* storage, uint32_t profile, bool* recovered) {
    if(recovered) *recovered = false;
    if(!storage) return false;
    char journal[DND_TXN_PATH_SIZE];
    if(!dnd_txn_journal_path(journal, profile, "")) return false;
    bool exists;
    if(!dnd_txn_exists(storage, journal, &exists)) return false;
    if(!exists) return true;
    if(recovered) *recovered = true;

    DndInventoryTransaction transaction;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool opened = storage_file_open(file, journal, FSAM_READ, FSOM_OPEN_EXISTING);
    bool success = opened && storage_file_size(file) == sizeof(transaction) &&
                   storage_file_read(file, &transaction, sizeof(transaction)) ==
                       sizeof(transaction);
    if(opened && !storage_file_close(file)) success = false;
    storage_file_free(file);
    if(!success || transaction.magic != DND_TXN_MAGIC || transaction.version != DND_TXN_VERSION ||
       transaction.profile != profile ||
       transaction.crc !=
           (dnd_txn_crc(UINT32_MAX, &transaction, offsetof(DndInventoryTransaction, crc)) ^
            UINT32_MAX) ||
       !dnd_txn_name_valid(transaction.entry[0].name, profile) ||
       !dnd_txn_name_valid(transaction.entry[1].name, profile) ||
       dnd_txn_same_name(transaction.entry[0].name, transaction.entry[1].name) ||
       transaction.entry[0].old_present > 1U || transaction.entry[1].old_present > 1U)
        return false;

    // Validate both sides before touching either: corruption is not permission
    // to overwrite a live file or discard its retained original.
    for(uint8_t i = 0U; i < 2U; ++i)
        if(!dnd_txn_entry_valid(storage, &transaction.entry[i])) return false;
    for(uint8_t i = 0U; i < 2U; ++i)
        if(!dnd_txn_publish_entry(storage, &transaction.entry[i])) return false;
    char path[DND_TXN_PATH_SIZE];
    for(uint8_t i = 0U; i < 2U; ++i) {
        const DndInventoryTransactionEntry* entry = &transaction.entry[i];
        if(!dnd_txn_path(path, entry->name, "") ||
           !dnd_txn_matches(storage, path, entry->new_size, entry->new_crc))
            return false;
    }
    // Backups disappear only after BOTH new files verify. Journal deletion is
    // last, so reset during cleanup is replayable and never exposes half a move.
    for(uint8_t i = 0U; i < 2U; ++i) {
        if(!dnd_txn_path(path, transaction.entry[i].name, ".move.bak") ||
           !dnd_txn_remove(storage, path))
            return false;
    }
    return storage_common_remove(storage, journal) == FSE_OK;
}

DndStorageTransferResult dnd_inventory_transaction_publish(
    Storage* storage,
    uint32_t profile,
    const char* source_snapshot,
    const char* source_live,
    const char* destination_snapshot,
    const char* destination_live) {
    if(!storage || !source_snapshot || !destination_snapshot) return DndStorageTransferFailed;
    bool recovered;
    if(!dnd_inventory_transaction_recover(storage, profile, &recovered))
        return DndStorageTransferPending;
    if(recovered) return DndStorageTransferRecovered;

    DndInventoryTransaction transaction = {
        .magic = DND_TXN_MAGIC, .version = DND_TXN_VERSION, .profile = profile};
    if(!dnd_txn_set_name(transaction.entry[0].name, source_live, profile) ||
       !dnd_txn_set_name(transaction.entry[1].name, destination_live, profile) ||
       dnd_txn_same_name(transaction.entry[0].name, transaction.entry[1].name))
        return DndStorageTransferFailed;
    const char* snapshots[2] = {source_snapshot, destination_snapshot};
    char path[DND_TXN_PATH_SIZE], staged[DND_TXN_PATH_SIZE];
    if(dnd_txn_same_name(source_snapshot, destination_snapshot)) return DndStorageTransferFailed;
    // Neither input may alias a live file or any transaction work file. Check
    // both sides before staging can truncate a path needed by the other side.
    const char* suffixes[] = {"", ".move.tmp", ".move.bak"};
    for(uint8_t i = 0U; i < 2U; ++i) {
        for(uint8_t suffix = 0U; suffix < 3U; ++suffix) {
            if(!dnd_txn_path(path, transaction.entry[i].name, suffixes[suffix]) ||
               dnd_txn_same_name(source_snapshot, path) ||
               dnd_txn_same_name(destination_snapshot, path))
                return DndStorageTransferFailed;
        }
    }
    if(!dnd_txn_journal_path(path, profile, "") ||
       !dnd_txn_journal_path(staged, profile, ".new") ||
       dnd_txn_same_name(source_snapshot, path) || dnd_txn_same_name(source_snapshot, staged) ||
       dnd_txn_same_name(destination_snapshot, path) ||
       dnd_txn_same_name(destination_snapshot, staged))
        return DndStorageTransferFailed;
    for(uint8_t i = 0U; i < 2U; ++i) {
        DndInventoryTransactionEntry* entry = &transaction.entry[i];
        bool exists;
        // Unjournaled backups may belong to an older interrupted firmware
        // operation. Never silently delete or replace that recovery evidence.
        if(!dnd_txn_path(path, entry->name, ".move.bak") ||
           !dnd_txn_exists(storage, path, &exists) || exists ||
           !dnd_txn_path(path, entry->name, "") || !dnd_txn_exists(storage, path, &exists))
            return DndStorageTransferFailed;
        entry->old_present = exists;
        if(exists && !dnd_txn_file(storage, path, NULL, &entry->old_size, &entry->old_crc))
            return DndStorageTransferFailed;
        if(!dnd_txn_path(staged, entry->name, ".move.tmp") ||
           !dnd_txn_file(storage, snapshots[i], staged, &entry->new_size, &entry->new_crc))
            return DndStorageTransferFailed;
    }
    transaction.crc =
        dnd_txn_crc(UINT32_MAX, &transaction, offsetof(DndInventoryTransaction, crc)) ^ UINT32_MAX;
    if(!dnd_txn_journal_path(path, profile, "") || !dnd_txn_journal_path(staged, profile, ".new"))
        return DndStorageTransferFailed;
    File* file = storage_file_alloc(storage);
    if(!file) return DndStorageTransferFailed;
    bool opened = storage_file_open(file, staged, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool success = opened &&
                   storage_file_write(file, &transaction, sizeof(transaction)) ==
                       sizeof(transaction) &&
                   storage_file_sync(file);
    if(opened && !storage_file_close(file)) success = false;
    storage_file_free(file);
    if(!success) return DndStorageTransferFailed;

    // This no-overwrite rename is the commit point. Its failure is deliberately
    // Pending too: a device error may leave its outcome uncertain. The caller
    // must discard its old selection and reload before attempting another move.
    if(storage_common_rename_safe(storage, staged, path) != FSE_OK)
        return DndStorageTransferPending;
    return dnd_inventory_transaction_recover(storage, profile, NULL) ? DndStorageTransferComplete :
                                                                       DndStorageTransferPending;
}
