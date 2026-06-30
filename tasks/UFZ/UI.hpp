#pragma once
#include "Common.hpp"
#include "Filesystem.hpp"

#include <functional>
#include <vector>

#include <gui/modules/menu.h>
#include <gui/modules/button_menu.h>
#include <gui/modules/button_panel.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/empty_screen.h>
#include <gui/modules/loading.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>

#include <gui/view_stack.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <gui/view_dispatcher.h>

// Generates a function from a prefix and postfix, for example given (menu, free) will return the function with the name
// menu_free
#define FROM_PREFIX(x, y) x##_##y

// Generates default implementation for components of a specific type.
// x - The component type
// y - The name of the component, should match flipper zero function prefixes for the given component
#define UFZ_COMPONENT(x, y) public:                                                                                 \
    using UWidget::UWidget;                                                                                         \
    inline operator ::x*() noexcept { return y; };                                                                  \
    x() = default;                                                                                                  \
    inline virtual UFZ::View getWidgetView() noexcept override { return UFZ::View(FROM_PREFIX(y, get_view)(y)); }   \
    inline virtual void reset() noexcept override { FROM_PREFIX(y, reset)(y); };                                    \
private:                                                                                                            \
    ::x* y = nullptr;                                                                                               \
    inline virtual void alloc() noexcept override { (y) = FROM_PREFIX(y, alloc)(); };                               \
    inline virtual void free() noexcept override { FREE_GUARD(FROM_PREFIX(y, free), y); };

#define GET_WIDGET_P(x, y, z) ((UFZ::Application*)(x))->getWidget<y>(z)
#define RENDER_VIEW(x, y) ((UFZ::Application*)(x))->getViewDispatcher().switchToView(y)

namespace UFZ
{
    class Application;

    class View
    {
    public:
        View() = default;
        explicit View(::View* v) noexcept;
        operator ::View*() const noexcept;

        View& allocate() noexcept;

        void setDeferredSetupCallback(const std::function<void(View&)>& f) noexcept;

        [[nodiscard]] const View& setDrawCallback(ViewDrawCallback callback) const noexcept;
        [[nodiscard]] const View& setInputCallback(ViewInputCallback callback) const noexcept;
        [[nodiscard]] const View& setCustomCallback(ViewCustomCallback callback) const noexcept;

        [[nodiscard]] const View& setPreviousCallback(ViewNavigationCallback callback) const noexcept;
        [[nodiscard]] const View& setEnterCallback(ViewCallback callback) const noexcept;
        [[nodiscard]] const View& setExitCallback(ViewCallback callback) const noexcept;

        [[nodiscard]] const View& setUpdateCallback(ViewUpdateCallback callback) const noexcept;

        [[nodiscard]] const View& setUpdateCallbackContext(void* context) const noexcept;
        [[nodiscard]] const View& setContext(void* context) const noexcept;

        [[nodiscard]] const View& setOrientation(ViewOrientation orientation) const noexcept;

        [[nodiscard]] const View& allocateModel(ViewModelType type, size_t size) const noexcept;
        [[nodiscard]] const View& freeModel() const noexcept;
        [[nodiscard]] void* getModel() const noexcept;
        [[nodiscard]] const View& commitModel(bool bUpdate) const noexcept;

        void free() noexcept;
    private:
        friend class UWidget;
        bool bAllocated = false;

        std::function<void(View&)> deferredSetupCallback{};

        ::View* view = nullptr;
    };

    class UWidget
    {
    public:
        UWidget() = default;
        UWidget(const AppSceneOnEnterCallback onEnter, const AppSceneOnEventCallback onEvent, const AppSceneOnExitCallback onExit, const std::vector<View*>& additionalViews = {}) noexcept
            : enter(onEnter), event(onEvent), exit(onExit), views(additionalViews)
        {
        }

        void destroy() noexcept;

        virtual void reset() noexcept = 0;

        AppSceneOnEnterCallback enter{};
        AppSceneOnEventCallback event{};
        AppSceneOnExitCallback exit{};

        Application* application = nullptr;

        virtual ~UWidget() noexcept = default;
    private:
        friend class Application;
        friend class ViewDispatcher;

        size_t id = 0;
        std::vector<View*> views{};

        ::ViewStack* viewStack = nullptr;

        bool bDestroyed = false;

        void allocateViewStack(const View& widgetView) noexcept;

        void addView(const UFZ::View& view) const noexcept;
        void removeView(const UFZ::View& view) const noexcept;

        [[nodiscard]] View getView() const noexcept;
        virtual View getWidgetView() noexcept = 0;

        virtual void alloc() noexcept = 0;
        virtual void free() noexcept = 0;
    };

    class Menu final : public UWidget
    {
        UFZ_COMPONENT(Menu, menu);
    public:
        const Menu& addItem(const char* label, const Icon* icon, uint32_t index, MenuItemCallback callback, void* context) const noexcept;
        void setSelectedItem(uint32_t index) const noexcept;
    };

    class ButtonMenu final : public UWidget
    {
        UFZ_COMPONENT(ButtonMenu, button_menu);
    public:
        ButtonMenuItem* addItem(const char* label, int32_t index, ButtonMenuItemCallback callback, ButtonMenuItemType type, void* context) const noexcept;
        void setHeader(const char* header) const;
        void setSelectedItem(uint32_t index) const noexcept;
    };

    class ButtonPanel final : public UWidget
    {
        UFZ_COMPONENT(ButtonPanel, button_panel);
    public:
        void reserve(size_t x, size_t y) const noexcept;
        const ButtonPanel& addItem(uint32_t index,
                     uint16_t matrix_place_x, uint16_t matrix_place_y,
                     uint16_t x, uint16_t y,
                     const Icon* icon_name, const Icon* icon_name_selected,
                     ButtonItemCallback callback, void* context) const noexcept;

        const ButtonPanel& addLabel(uint16_t x, uint16_t y, Font font, const char* label) const noexcept;
        const ButtonPanel& addIcon(uint16_t x, uint16_t y, const Icon* icon) const noexcept;
    };

    class ByteInput : public UWidget
    {
    public:
        ByteInput() = default;

        void setResultCallback(ByteInputCallback inputCallback, ByteChangedCallback changedCallback, void* context, uint8_t* bytes, uint8_t bytesCount) const noexcept;
        void setHeaderText(const char* text) const noexcept;
    private:
        ::ByteInput* byte_input = nullptr;

        virtual void alloc() noexcept override;
        virtual void free() noexcept override;
        virtual View getWidgetView() noexcept override;
    };

    class NumberInput : public UWidget
    {
    public:
        NumberInput() = default;

        void setResultCallback(NumberInputCallback inputCallback, void* context, int32_t currentNumber, int32_t min, int32_t max) const noexcept;
        void setHeaderText(const char* text) noexcept;
    private:
        ::NumberInput* number_input = nullptr;

        virtual void alloc() noexcept override;
        virtual void free() noexcept override;
        virtual View getWidgetView() noexcept override;
    };

    class DialogEx final : public UWidget
    {
        UFZ_COMPONENT(DialogEx, dialog_ex);
    public:
        const DialogEx& setResultCallback(DialogExResultCallback callback) const noexcept;
        const DialogEx& setContext(void* context) const noexcept;
        const DialogEx& setHeader(const char* text, uint8_t x, uint8_t y, Align horizontal, Align vertical) const noexcept;
        const DialogEx& setText(const char* text, uint8_t x, uint8_t y, Align horizontal, Align vertical) const noexcept;
        const DialogEx& setIcon(uint8_t x, uint8_t y, const Icon* icon) const noexcept;
        const DialogEx& setLeftButtonText(const char* text) const noexcept;
        const DialogEx& setCenterButtonText(const char* text) const noexcept;
        const DialogEx& setRightButtonText(const char* text) const noexcept;
        void enableExtendedEvents() const noexcept;
        void disableExtendedEvents() const noexcept;
    };

    class EmptyScreen : public UWidget
    {
    public:
        EmptyScreen() = default;
    private:
        ::EmptyScreen* empty_screen = nullptr;

        virtual void free() noexcept override;
        virtual void alloc() noexcept override;
        virtual View getWidgetView() noexcept override;
    };

    class Loading : public UWidget
    {
    public:
        Loading() = default;
    private:
        ::Loading* loading = nullptr;

        virtual void free() noexcept override;
        virtual void alloc() noexcept override;
        virtual View getWidgetView() noexcept override;
    };

    class Popup final : public UWidget
    {
        UFZ_COMPONENT(Popup, popup);
    public:
        const Popup& setCallback(PopupCallback callback) const noexcept;
        const Popup& setContext(void* context) const noexcept;
        const Popup& setHeader(const char* text, uint8_t x, uint8_t y, Align horizontal, Align vertical) const noexcept;
        const Popup& setText(const char* text, uint8_t x, uint8_t y, Align horizontal, Align vertical) const noexcept;
        const Popup& setIcon(uint8_t x, uint8_t y, const Icon* icon) const noexcept;
        [[nodiscard]] const Popup& setTimeout(uint32_t milliseconds) const noexcept;

        void enableTimeout() const noexcept;
        void disableTimout() const noexcept;
    };

    class Submenu final : public UWidget
    {
        UFZ_COMPONENT(Submenu, submenu);
    public:
        [[nodiscard]] const Submenu& addItem(const char* label, uint32_t index, SubmenuItemCallback callback, void* context) const noexcept;
        [[nodiscard]] const Submenu& setSelectedItem(uint32_t index) const noexcept;
        [[nodiscard]] const Submenu& setHeader(const char* header) const noexcept;
    };

    class TextBox final : public UWidget
    {
        UFZ_COMPONENT(TextBox, text_box);
    public:
        const TextBox& setText(const char* text) const noexcept;
        [[nodiscard]] const TextBox& setFont(TextBoxFont font) const noexcept;
        [[nodiscard]] const TextBox& setFocus(TextBoxFocus focus) const noexcept;
    };

    class TextInput final : public UWidget
    {
        UFZ_COMPONENT(TextInput, text_input);
    public:
        void setResultCallback(TextInputCallback callback, void* callbackContext, char* textBuffer, size_t textBufferSize, bool clearDefaultText) const noexcept;
        void setValidator(TextInputValidatorCallback callback, void* context) const noexcept;
        void getValidatorCallbackContext() const noexcept;
        void setHeaderText(const char* text) const noexcept;
    };

    class VariableItemList final : public UWidget
    {
        UFZ_COMPONENT(VariableItemList, variable_item_list);
    public:
        VariableItem* add(const char* label, uint8_t values_count, VariableItemChangeCallback callback, void* context) const noexcept;
        void setEnterCallback(VariableItemListEnterCallback callback, void* context) const noexcept;
        void setSelectedItem(uint8_t index) const noexcept;
        [[nodiscard]] uint8_t getSelectedItemIndex() const noexcept;
    };

    class Widget final : public UWidget
    {
        UFZ_COMPONENT(Widget, widget);
    public:
        const Widget& addStringMultilineElement(uint8_t x, uint8_t y, Align horizontal, Align vertical, Font font, const char* text) const noexcept;
        const Widget& addStringElement(uint8_t x, uint8_t y, Align horizontal, Align vertical, Font font, const char* text) const noexcept;
        const Widget& addTextBoxElement(uint8_t x, uint8_t y, uint8_t width, uint8_t height, Align horizontal, Align vertical, const char* text, bool stripToDots) const noexcept;
        const Widget& addTextScrollElement(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const char* text) const noexcept;
        const Widget& addButtonElement(GuiButtonType type, const char* text, ButtonCallback callback, void* context) const noexcept;
        const Widget& addIconElement(uint8_t x, uint8_t y, const Icon* icon) const noexcept;
        [[nodiscard]] const Widget& addFrameElement(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t radius) const noexcept;
    };
}