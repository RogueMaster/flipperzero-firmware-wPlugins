#include "Scenes.hpp"

namespace {
// Saves written before the input buffer was trimmed carry up to 128 bytes of NUL padding per field, and the
// last description additionally collected one extra NUL on every load/save round trip. Neither is visible
// through c_str(), but both inflate the file and the buffer readContainerString allocates, so drop them here.
// Task text can never legitimately end in a NUL: the keyboard cannot produce one
void trimPadding(std::string& str) noexcept {
    str.resize(
        str.find_last_not_of('\0') + 1); // npos + 1 == 0, i.e. an all-NUL field becomes empty
}

// Both helpers scope their own file handle, so the UFZ::File destructor closes it on every path.
// skip drops that many leading entries — todo passes 1 to leave its code-owned sentinel out of the file
void saveContainer(
    const UFZ::Application& application,
    const char* path,
    const FTasks::NoteContainer& container,
    const size_t skip = 0) noexcept {
    UFZ::File file{};
    if(file.open(application.getFilesystem(), path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        FTasks::Data::writeContainerString(container, file, skip);
}

// An absent or empty file leaves the container at its default contents
void loadContainer(
    const UFZ::Application& application,
    const char* path,
    FTasks::NoteContainer& container) noexcept {
    UFZ::File file{};
    if(file.open(application.getFilesystem(), path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       file.size() > 0)
        FTasks::Data::readContainerString(container, file);
}
}

// Writes field-by-field straight out of the container's strings on purpose. Save runs after every
// mutation and once more after run() returns but before destroy() (see tasks.cpp), so the
// widgets/GUI occupy the heap either way — batching
// into a temporary buffer copies the whole dataset (~3x transiently with std::string growth) and
// can hit furi_crash("Out of memory") on the ~256KB-SRAM Flipper. Load can afford a full-file
// buffer only because begin() runs before any GUI allocation. Extra storage_file_write calls are
// cheap at to-do-list data sizes; if batching is ever needed, use a small fixed-size chunk buffer.
void FTasks::Data::writeContainerString(
    const FTasks::NoteContainer& container,
    const UFZ::File& file,
    const size_t start) noexcept {
    // start lets save() drop the leading sentinel from todo without copying the container (this runs under the
    // tight save-time memory budget described above). The separator goes before every entry except the first
    // written one, so the file has no leading or trailing newline regardless of where writing begins
    for(size_t i = start; i < container.size(); i++) {
        const auto& a = container[i];
        if(i > start) file.write(newLine, 1);

        file.write(a.first.data(), a.first.size());
        file.write(newLine, 1);
        file.write(a.second.data(), a.second.size());
    }
}

void FTasks::Data::readContainerString(
    FTasks::NoteContainer& container,
    const UFZ::File& file) noexcept {
    std::string content;
    content.resize(
        file.size()); // Exactly the file, no slack: a spare byte would be parsed as part of the last field
    file.read(content.data(), content.size());

    container.clear();

    size_t pos;
    size_t previous = 0;
    size_t i = 1;
    for(; (pos = content.find(newLine, previous)) != std::string::npos; i++) {
        auto str = content.substr(previous, pos - previous);
        trimPadding(str);
        previous = pos + 1;
        if((i % 2) == 0)
            container.back().second = str;
        else
            container.emplace_back(str, "");
    }

    auto last = content.substr(previous, pos);
    trimPadding(last);
    if((i % 2) == 0)
        container.back().second = last;
    else
        container.emplace_back(last, "");
}

void FTasks::Data::save(const UFZ::Application& application) noexcept {
    const auto* ctx = CTX(application.getUserPointer());
    saveContainer(
        application,
        TASKS_TODO_DATA_FILE,
        ctx->containers.todo,
        1); // Skip the code-owned sentinel at index 0
    saveContainer(application, TASKS_DONE_DATA_FILE, ctx->containers.done);
}

void FTasks::Data::load(const UFZ::Application& application) noexcept {
    auto* ctx = CTX(application.getUserPointer());
    loadContainer(application, TASKS_TODO_DATA_FILE, ctx->containers.todo);
    loadContainer(application, TASKS_DONE_DATA_FILE, ctx->containers.done);

    // Guarantee exactly one sentinel at the front of todo. An absent/empty file leaves the seeded sentinel in
    // place; a file written by this version has no sentinel; a file written by an older version still carries it
    // as its first record. Drop that leading copy if present (matching both fields so a real task merely named
    // "+ New task" survives), then prepend the code-owned one. Runs in begin(), before any GUI allocation
    auto& todo = ctx->containers.todo;
    if(!todo.empty() && todo.front().first == newTaskName &&
       todo.front().second == newTaskDescription)
        todo.erase(todo.begin());
    todo.insert(todo.begin(), {newTaskName, newTaskDescription});
}
