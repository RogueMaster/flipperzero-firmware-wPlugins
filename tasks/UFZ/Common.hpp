#pragma once
#include <vector>
#include <functional>
#include <furi.h>
#include <storage/storage.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <gui/view_dispatcher.h>

// Only frees if memory is allocated
#define FREE_GUARD(x, y) if ((y) != nullptr) x(y); (y) = nullptr;

#define SEND_CUSTOM_EVENT(x, y) (x)->getViewDispatcher().sendCustomEvent(y)

// Navigates to a new scene without wiping the scenes stack
#define NEXT_SCENE(x, y) UNUSED((x)->getSceneManager().nextScene(y))

// Navigates to a new scene and wipes the scenes stack
#define FORCE_NEXT_SCENE(x, y) UNUSED((x)->getSceneManager().searchAndSwitchToAnotherScene(y))

#define EXIT_SCENE(x) (x)->getSceneManager().stop()
#define EXIT_APPLICATION(x) (x)->getViewDispatcher().stop()

namespace UFZ
{
    class UWidget;
    class Application;

    class ViewDispatcher
    {
    public:
        ViewDispatcher() = default;

        void switchToView(uint32_t id) const noexcept;

        void sendCustomEvent(uint32_t event) const noexcept;
        void sendToFront() const noexcept;
        void sendToBack() const noexcept;

        void stop() const noexcept;
    private:
        friend class Application;

        void init() noexcept;
        void free() noexcept;

        Application* application = nullptr;
        ::ViewDispatcher* viewDispatcher = nullptr;
    };

    class SceneManager
    {
    public:
        SceneManager() = default;

        void setSceneState(uint32_t id, uint32_t state) const noexcept;
        [[nodiscard]] uint32_t getSceneState(uint32_t id) const noexcept;

        [[nodiscard]] bool handleCustomEvent(uint32_t event) const noexcept;
        [[nodiscard]] bool handleBackEvent() const noexcept;
        void handleTickEvent() const noexcept;

        void nextScene(uint32_t id) const noexcept;
        [[nodiscard]] bool previousScene() const noexcept;
        [[nodiscard]] bool hasPreviousScene(uint32_t id) const noexcept;

        [[nodiscard]] bool searchAndSwitchToPreviousScene(uint32_t id) const noexcept;
        bool searchAndSwitchToPreviousSceneOneOf(const uint32_t* ids, size_t idsSize) const noexcept;
        [[nodiscard]] bool searchAndSwitchToAnotherScene(uint32_t id) const noexcept;

        void stop() const noexcept;
    private:
        friend class Application;

        void alloc(const SceneManagerHandlers& handlers, Application& app) noexcept;
        void free() noexcept;

        ::SceneManager* sceneManager = nullptr;
        Application* application = nullptr;
    };

    class Filesystem
    {
    public:
        Filesystem() = default;

        FS_Error timestamp(const char* path, uint32_t* timestamp) const noexcept;
        FS_Error stat(const char* path, FileInfo* fileInfo) const noexcept;
        bool exists(const char* path) const noexcept;
        FS_Error remove(const char* path) const noexcept;

        FS_Error rename(const char* oldPath, const char* newPath) const noexcept;

        FS_Error copy(const char* oldPath, const char* newPath) const noexcept;
        FS_Error merge(const char* oldPath, const char* newPath) const noexcept;
        FS_Error migrate(const char* source, const char* destination) const noexcept;

        FS_Error mkdir(const char* path) const noexcept;
        FS_Error filesystemInfo(const char* path, uint64_t* totalSpace, uint64_t* freeSpace) const noexcept;

        void resolvePathAndEnsureAppDirectory(FuriString* path) const noexcept;

        bool areEquivalent(const char* path1, const char* path2) const noexcept;

        [[nodiscard]] static const char* getErrorDescription(FS_Error error) noexcept;

        FS_Error SDCardInfo(SDInfo* info) const noexcept;
        [[nodiscard]] FS_Error SDCardStatus() const noexcept;

        bool removeSimple(const char* path) const noexcept;
        bool removeRecursiveSimple(const char* path) const noexcept;
        bool mkdirSimple(const char* path) const noexcept;
        void getNextFilename(const char* dirname, const char* filename, const char* fileExtension, FuriString* nextFilename, uint8_t maxLength) const noexcept;
    private:
        friend class Application;
        friend class File;
        friend class Directory;

        void init() noexcept;
        void destroy() noexcept;

        ::Storage* storage = nullptr;
    };

    class Application
    {
    public:
        Application() = default;
        explicit Application(const std::vector<UWidget*>& widgetsRef, void* userPointer, const std::function<void(Application&)>& begin = [](Application&) -> void {}, uint32_t tickPeriod = 0) noexcept;
        void run(const std::vector<UWidget*>& widgetsRef, void* userPointer, const std::function<void(Application&)>& begin = [](Application&) -> void {}, uint32_t tickPeriod = 0) noexcept;

        template<typename T>
        T* getWidget(const size_t i) noexcept
        {
            return static_cast<T*>(widgets[i]);
        }

        [[nodiscard]] const ViewDispatcher& getViewDispatcher() const noexcept;
        [[nodiscard]] const SceneManager& getSceneManager() const noexcept;
        [[nodiscard]] const Filesystem& getFilesystem() const noexcept;

        [[nodiscard]] void* getUserPointer() const noexcept;

        void destroy() noexcept;
    private:
        friend class ViewDispatcher;

        SceneManager sceneManager;
        ViewDispatcher viewDispatcher;
        Gui* gui = nullptr;

        Filesystem filesystem{};

        void* ctx = nullptr;
        size_t tickInterval = 0;

        std::vector<AppSceneOnEnterCallback> enterCallbacks{};
        std::vector<AppSceneOnEventCallback> eventCallbacks{};
        std::vector<AppSceneOnExitCallback> exitCallbacks{};

        SceneManagerHandlers handlers{};

        std::vector<UWidget*> widgets;

        bool bDestroyed = false;

        void initSceneManager() noexcept;
        void initViewDispatcher() noexcept;
        void initGUI() noexcept;

        void freeSceneManager() noexcept;
        void freeViewDispatcher() noexcept;
        static void freeGUI() noexcept;
    };
}
