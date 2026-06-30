#include "Scenes.hpp"

void FTasks::Data::writeContainerString(const FTasks::NoteContainer& container, const UFZ::File& file) noexcept
{
    for (size_t i = 0; i < container.size(); i++)
    {
        const auto& a = container[i];
        file.write((void*)a.first.data(), a.first.size());
        file.write((void*)newLine, 1);
        file.write((void*)a.second.data(), a.second.size());

        if (i < (container.size() - 1))
            file.write((void*)newLine, 1);
    }
}

void FTasks::Data::readContainerString(FTasks::NoteContainer& container, const UFZ::File& file) noexcept
{
    std::string content;
    content.resize(file.size() + 1);
    file.read(content.data(), content.size());

    container.clear();

    size_t pos;
    size_t previous = 0;
    size_t i = 1;
    for (; (pos = content.find(newLine, previous)) != std::string::npos; i++)
    {
        auto str = content.substr(previous, pos - previous);
        previous = pos + 1;
        if ((i % 2) == 0)
            container.back().second = str;
        else
            container.emplace_back(str, "");
    }
    if ((i % 2) == 0)
        container.back().second = content.substr(previous, pos);
    else
        container.emplace_back(content.substr(previous, pos), "");
}

void FTasks::Data::save(const UFZ::Application& application) noexcept
{
    UFZ::File file{};

    const auto* ctx = CTX(application.getUserPointer());
    if (file.open(application.getFilesystem(), TASKS_TODO_DATA_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        writeContainerString(ctx->containers.todo, file);
    file.close();
    if (file.open(application.getFilesystem(), TASKS_DONE_DATA_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        writeContainerString(ctx->containers.done, file);
}

void FTasks::Data::load(const UFZ::Application& application) noexcept
{
    UFZ::File file{};

    auto* ctx = CTX(application.getUserPointer());
    if (file.open(application.getFilesystem(), TASKS_TODO_DATA_FILE, FSAM_READ_WRITE, FSOM_OPEN_EXISTING))
    {
        if (file.size() == 0)
            return;
        readContainerString(ctx->containers.todo, file);
    }
    file.close();
    if (file.open(application.getFilesystem(), TASKS_DONE_DATA_FILE, FSAM_READ_WRITE, FSOM_OPEN_EXISTING))
    {
        if (file.size() == 0)
            return;
        readContainerString(ctx->containers.done, file);
    }
}