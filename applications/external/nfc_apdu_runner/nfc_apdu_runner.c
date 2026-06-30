/*
 * @Author: SpenserCai
 * @Date: 2025-02-28 17:52:49
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-16 19:59:42
 * @Description: file content
 */
#include "nfc_apdu_runner.h"
#include "scenes/nfc_apdu_runner_scene.h"

// 菜单项枚举
typedef enum {
    NfcApduRunnerSubmenuIndexLoadFile,
    NfcApduRunnerSubmenuIndexViewLogs,
    NfcApduRunnerSubmenuIndexAbout,
} NfcApduRunnerSubmenuIndex;

// 前向声明
static void nfc_apdu_runner_free(NfcApduRunner* app);
static NfcApduRunner* nfc_apdu_runner_alloc();
static void nfc_apdu_runner_init(NfcApduRunner* app);
static bool nfc_apdu_runner_custom_event_callback(void* context, uint32_t event);
static bool nfc_apdu_runner_back_event_callback(void* context);
static void nfc_apdu_runner_tick_event_callback(void* context);

// 释放APDU脚本资源
void nfc_apdu_script_free(NfcApduScript* script) {
    if(script == NULL) return;

    for(uint32_t i = 0; i < script->command_count; i++) {
        free(script->commands[i]);
    }
    free(script);
}

// 释放APDU响应资源
void nfc_apdu_responses_free(NfcApduResponse* responses, uint32_t count) {
    if(responses == NULL) return;

    for(uint32_t i = 0; i < count; i++) {
        free(responses[i].command);
        free(responses[i].response);
    }
    free(responses);
}

// 解析卡类型字符串
static CardType parse_card_type(const char* type_str) {
    if(strcmp(type_str, "iso14443_3a") == 0) {
        return CardTypeIso14443_3a;
    } else if(strcmp(type_str, "iso14443_3b") == 0) {
        return CardTypeIso14443_3b;
    } else if(strcmp(type_str, "iso14443_4a") == 0) {
        return CardTypeIso14443_4a;
    } else if(strcmp(type_str, "iso14443_4b") == 0) {
        return CardTypeIso14443_4b;
    } else {
        return CardTypeUnknown;
    }
}

// 解析APDU脚本文件
NfcApduScript* nfc_apdu_script_parse(Storage* storage, const char* file_path) {
    FURI_LOG_I("APDU_DEBUG", "开始解析脚本文件: %s", file_path);

    if(!storage || !file_path) {
        FURI_LOG_E("APDU_DEBUG", "无效的存储或文件路径");
        return NULL;
    }

    NfcApduScript* script = malloc(sizeof(NfcApduScript));
    if(!script) {
        FURI_LOG_E("APDU_DEBUG", "分配脚本内存失败");
        return NULL;
    }

    memset(script, 0, sizeof(NfcApduScript));

    FuriString* temp_str = furi_string_alloc();
    if(!temp_str) {
        FURI_LOG_E("APDU_DEBUG", "分配临时字符串内存失败");
        free(script);
        return NULL;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E("APDU_DEBUG", "分配文件失败");
        furi_string_free(temp_str);
        free(script);
        return NULL;
    }

    bool success = false;
    bool in_data_section = false;

    do {
        if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E("APDU_DEBUG", "打开文件失败: %s", file_path);
            break;
        }

        // 读取整个文件内容到缓冲区
        char file_buf[1024];
        size_t bytes_read = storage_file_read(file, file_buf, sizeof(file_buf) - 1);
        if(bytes_read <= 0) {
            FURI_LOG_E("APDU_DEBUG", "读取文件内容失败");
            break;
        }
        file_buf[bytes_read] = '\0';
        FURI_LOG_I("APDU_DEBUG", "读取文件内容成功, 大小: %zu 字节", bytes_read);

        // 解析文件内容
        char* line = file_buf;
        char* next_line;

        // 读取文件类型行
        next_line = strchr(line, '\n');
        if(!next_line) {
            FURI_LOG_E("APDU_DEBUG", "文件格式错误，找不到换行符");
            break;
        }
        *next_line = '\0';

        if(strncmp(line, "Filetype: APDU Script", 21) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的文件类型: %s", line);
            break;
        }

        // 读取版本行
        line = next_line + 1;
        next_line = strchr(line, '\n');
        if(!next_line) {
            FURI_LOG_E("APDU_DEBUG", "文件格式错误，找不到版本行结束");
            break;
        }
        *next_line = '\0';

        if(strncmp(line, "Version: 1", 10) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的版本: %s", line);
            break;
        }

        // 读取卡类型行
        line = next_line + 1;
        next_line = strchr(line, '\n');
        if(!next_line) {
            FURI_LOG_E("APDU_DEBUG", "文件格式错误，找不到卡类型行结束");
            break;
        }
        *next_line = '\0';

        if(strncmp(line, "CardType: ", 10) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的卡类型行: %s", line);
            break;
        }

        // 提取卡类型
        const char* card_type_str = line + 10; // 跳过 "CardType: "
        script->card_type = parse_card_type(card_type_str);

        if(script->card_type == CardTypeUnknown) {
            FURI_LOG_E("APDU_DEBUG", "不支持的卡类型: %s", card_type_str);
            break;
        }
        FURI_LOG_I("APDU_DEBUG", "卡类型解析成功: %s", card_type_str);

        // 读取数据行
        line = next_line + 1;
        next_line = strchr(line, '\n');

        // 检查当前行是否为 Data 行
        if(strncmp(line, "Data: [", 7) != 0) {
            FURI_LOG_E("APDU_DEBUG", "无效的数据行: %s", line);
            break;
        }

        in_data_section = true;

        // 解析命令数组
        char* data_str = line + 7; // 跳过 "Data: ["

        // 处理多行格式的命令
        // 将整个文件内容中的所有换行符和空格替换为空格，以便正确解析命令
        char* p = data_str;
        while(*p) {
            if(*p == '\n' || *p == '\r') {
                *p = ' '; // 将换行符替换为空格
            }
            p++;
        }

        // 清理多余的空格，使解析更加稳健
        p = data_str;
        char* q = data_str;
        bool in_quotes = false;

        while(*p) {
            // 在引号内保留所有字符
            if(*p == '"') {
                in_quotes = !in_quotes;
                *q++ = *p++;
                continue;
            }

            // 在引号外，跳过空格
            if(!in_quotes && (*p == ' ' || *p == '\t')) {
                p++;
                continue;
            }

            // 复制其他字符
            *q++ = *p++;
        }
        *q = '\0'; // 确保字符串正确终止

        // 查找第一个引号
        char* command_start = strchr(data_str, '"');
        if(!command_start) {
            FURI_LOG_E("APDU_DEBUG", "未找到命令开始引号");
            break;
        }

        while(command_start != NULL && script->command_count < MAX_APDU_COMMANDS) {
            command_start++; // 跳过开始的引号

            // 查找结束引号
            char* command_end = strchr(command_start, '"');
            if(!command_end) {
                FURI_LOG_E("APDU_DEBUG", "未找到命令结束引号");
                break;
            }

            size_t command_len = command_end - command_start;

            if(command_len == 0) {
                // 空命令，跳过
                FURI_LOG_W("APDU_DEBUG", "空命令，跳过");
                command_start = strchr(command_end + 1, '"');
                continue;
            }

            // 验证命令是否只包含十六进制字符
            bool valid_hex = true;
            for(size_t i = 0; i < command_len; i++) {
                char c = command_start[i];
                if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                    valid_hex = false;
                    break;
                }
            }

            if(!valid_hex) {
                FURI_LOG_E(
                    "APDU_DEBUG", "无效的十六进制命令: %.*s", (int)command_len, command_start);
                command_start = strchr(command_end + 1, '"');
                continue;
            }

            // 命令长度必须是偶数
            if(command_len % 2 != 0) {
                FURI_LOG_E(
                    "APDU_DEBUG", "命令长度必须是偶数: %.*s", (int)command_len, command_start);
                command_start = strchr(command_end + 1, '"');
                continue;
            }

            // 分配内存并复制命令
            script->commands[script->command_count] = malloc(command_len + 1);
            if(!script->commands[script->command_count]) {
                FURI_LOG_E("APDU_DEBUG", "分配命令内存失败");
                break;
            }

            strncpy(script->commands[script->command_count], command_start, command_len);
            script->commands[script->command_count][command_len] = '\0';
            script->command_count++;

            // 查找下一个命令
            command_start = strchr(command_end + 1, '"');
        }

        success = in_data_section && (script->command_count > 0);
        FURI_LOG_I("APDU_DEBUG", "命令解析完成, 命令数: %lu", script->command_count);
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(temp_str);

    if(!success) {
        FURI_LOG_E("APDU_DEBUG", "解析失败，释放脚本资源");
        nfc_apdu_script_free(script);
        return NULL;
    }

    FURI_LOG_I("APDU_DEBUG", "脚本解析成功");
    return script;
}

// 保存APDU响应结果
bool nfc_apdu_save_responses(
    Storage* storage,
    const char* file_path,
    NfcApduResponse* responses,
    uint32_t response_count,
    const char* custom_save_path) {
    FuriString* temp_str = furi_string_alloc();
    FuriString* file_path_str = furi_string_alloc_set(file_path);
    FuriString* response_path = furi_string_alloc();

    // 构建响应文件路径
    if(custom_save_path != NULL) {
        // 使用自定义保存路径
        furi_string_set(response_path, custom_save_path);
    } else {
        // 使用默认路径
        FuriString* filename = furi_string_alloc();
        path_extract_filename(file_path_str, filename, true);
        furi_string_cat_printf(
            response_path,
            "%s/%s%s",
            APP_DIRECTORY_PATH,
            furi_string_get_cstr(filename),
            RESPONSE_EXTENSION);
        furi_string_free(filename);
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    do {
        if(!storage_file_open(
               file, furi_string_get_cstr(response_path), FSAM_WRITE, FSOM_CREATE_ALWAYS))
            break;

        // 写入文件头
        furi_string_printf(temp_str, "Filetype: APDU Script Response\n");
        if(!storage_file_write(file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
            break;

        furi_string_printf(temp_str, "Response:\n");
        if(!storage_file_write(file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
            break;

        // 写入每个命令和响应
        for(uint32_t i = 0; i < response_count; i++) {
            furi_string_printf(temp_str, "In: %s\n", responses[i].command);
            if(!storage_file_write(
                   file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                break;

            furi_string_printf(temp_str, "Out: ");
            if(!storage_file_write(
                   file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                break;

            // 将响应数据转换为十六进制字符串
            for(uint16_t j = 0; j < responses[i].response_length; j++) {
                furi_string_printf(temp_str, "%02X", responses[i].response[j]);
                if(!storage_file_write(
                       file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                    break;
            }

            furi_string_printf(temp_str, "\n");
            if(!storage_file_write(
                   file, furi_string_get_cstr(temp_str), furi_string_size(temp_str)))
                break;
        }

        success = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(temp_str);
    furi_string_free(file_path_str);
    furi_string_free(response_path);

    return success;
}

// 添加日志
void add_log_entry(NfcApduRunner* app, const char* message, bool is_error) {
    if(app->log_count >= MAX_LOG_ENTRIES) {
        // 移除最旧的日志
        free(app->log_entries[0].message);
        for(uint32_t i = 0; i < app->log_count - 1; i++) {
            app->log_entries[i] = app->log_entries[i + 1];
        }
        app->log_count--;
    }

    app->log_entries[app->log_count].message = strdup(message);
    app->log_entries[app->log_count].is_error = is_error;
    app->log_count++;
}

// 释放日志资源
void free_logs(NfcApduRunner* app) {
    if(!app) {
        return;
    }

    if(!app->log_entries) {
        return;
    }

    for(uint32_t i = 0; i < app->log_count; i++) {
        if(app->log_entries[i].message) {
            free(app->log_entries[i].message);
            app->log_entries[i].message = NULL;
        }
    }
    app->log_count = 0;
}

// 视图分发器回调
static bool nfc_apdu_runner_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcApduRunner* app = context;

    bool handled = scene_manager_handle_custom_event(app->scene_manager, event);

    return handled;
}

static bool nfc_apdu_runner_back_event_callback(void* context) {
    furi_assert(context);
    NfcApduRunner* app = context;

    // 直接交给场景管理器处理返回事件
    bool handled = scene_manager_handle_back_event(app->scene_manager);

    return handled;
}

static void nfc_apdu_runner_tick_event_callback(void* context) {
    furi_assert(context);
    NfcApduRunner* app = context;

    scene_manager_handle_tick_event(app->scene_manager);
}

// 分配应用程序资源
static NfcApduRunner* nfc_apdu_runner_alloc() {
    NfcApduRunner* app = malloc(sizeof(NfcApduRunner));
    if(!app) {
        return NULL;
    }

    memset(app, 0, sizeof(NfcApduRunner));

    // 分配GUI资源
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->scene_manager = scene_manager_alloc(&nfc_apdu_runner_scene_handlers, app);
    if(!app->scene_manager) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->submenu = submenu_alloc();
    if(!app->submenu) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->dialogs = furi_record_open(RECORD_DIALOGS);
    if(!app->dialogs) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->widget = widget_alloc();
    if(!app->widget) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->text_box = text_box_alloc();
    if(!app->text_box) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->text_box_store = furi_string_alloc();
    if(!app->text_box_store) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->popup = popup_alloc();
    if(!app->popup) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->button_menu = button_menu_alloc();
    if(!app->button_menu) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->text_input = text_input_alloc();
    if(!app->text_input) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    // 分配NFC资源
    app->nfc = nfc_alloc();
    if(!app->nfc) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    // 分配NFC Worker
    app->worker = nfc_worker_alloc(app->nfc);
    if(!app->worker) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    // 分配存储资源
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->storage) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->file_path = furi_string_alloc();
    if(!app->file_path) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    // 设置默认目录为APP_DIRECTORY_PATH
    furi_string_set(app->file_path, APP_DIRECTORY_PATH);

    // 分配日志资源
    app->log_entries = malloc(sizeof(LogEntry) * MAX_LOG_ENTRIES);
    if(!app->log_entries) {
        nfc_apdu_runner_free(app);
        return NULL;
    }

    app->log_count = 0;

    return app;
}

// 释放应用程序资源
static void nfc_apdu_runner_free(NfcApduRunner* app) {
    if(!app) {
        return;
    }

    // 释放脚本和响应资源
    if(app->script) {
        nfc_apdu_script_free(app->script);
        app->script = NULL;
    }

    if(app->responses) {
        nfc_apdu_responses_free(app->responses, app->response_count);
        app->responses = NULL;
        app->response_count = 0;
    }

    // 先释放所有视图
    if(app->submenu) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewSubmenu);
        submenu_free(app->submenu);
        app->submenu = NULL;
    }

    if(app->widget) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewWidget);
        widget_free(app->widget);
        app->widget = NULL;
    }

    if(app->text_box) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewTextBox);
        text_box_free(app->text_box);
        app->text_box = NULL;
    }

    if(app->text_box_store) {
        furi_string_free(app->text_box_store);
        app->text_box_store = NULL;
    }

    if(app->popup) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewPopup);
        popup_free(app->popup);
        app->popup = NULL;
    }

    if(app->button_menu) {
        button_menu_free(app->button_menu);
        app->button_menu = NULL;
    }

    if(app->text_input) {
        view_dispatcher_remove_view(app->view_dispatcher, NfcApduRunnerViewTextInput);
        text_input_free(app->text_input);
        app->text_input = NULL;
    }

    // 关闭记录
    if(app->dialogs) {
        furi_record_close(RECORD_DIALOGS);
        app->dialogs = NULL;
    }

    if(app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }

    // 释放NFC资源
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    // 释放NFC Worker
    if(app->worker) {
        nfc_worker_free(app->worker);
        app->worker = NULL;
    }

    // 释放存储资源
    if(app->file_path) {
        furi_string_free(app->file_path);
        app->file_path = NULL;
    }

    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
    }

    // 释放日志资源
    free_logs(app);

    if(app->log_entries) {
        free(app->log_entries);
        app->log_entries = NULL;
    }

    // 先释放场景管理器
    if(app->scene_manager) {
        scene_manager_free(app->scene_manager);
        app->scene_manager = NULL;
    }

    // 最后处理视图分发器
    if(app->view_dispatcher) {
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
        FURI_LOG_I("APDU_DEBUG", "View dispatcher freed");
    }

    // 释放应用程序资源
    free(app);
}

// 初始化应用程序
static void nfc_apdu_runner_init(NfcApduRunner* app) {
    if(!app) {
        return;
    }

    // 检查视图分发器是否已分配
    if(!app->view_dispatcher) {
        return;
    }

    // 配置视图分发器
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_apdu_runner_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_apdu_runner_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, nfc_apdu_runner_tick_event_callback, 100);

    // 检查GUI是否已分配
    if(!app->gui) {
        return;
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // 检查视图组件是否已分配
    if(!app->submenu || !app->widget || !app->text_box || !app->popup || !app->text_input) {
        return;
    }

    // 添加视图
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, NfcApduRunnerViewTextInput, text_input_get_view(app->text_input));

    // 配置文本框
    text_box_set_font(app->text_box, TextBoxFontText);

    // 检查存储是否已分配
    if(!app->storage) {
        return;
    }

    // 确保应用目录存在
    if(!storage_dir_exists(app->storage, APP_DIRECTORY_PATH)) {
        if(!storage_simply_mkdir(app->storage, APP_DIRECTORY_PATH)) {
            FURI_LOG_E("APDU_DEBUG", "无法创建应用目录");
            return;
        }
    }

    // 启动场景管理器
    scene_manager_next_scene(app->scene_manager, NfcApduRunnerSceneStart);
}

// 应用程序入口点
int32_t nfc_apdu_runner_app(void* p) {
    UNUSED(p);
    furi_hal_power_suppress_charge_enter();
    // 分配应用程序资源
    NfcApduRunner* app = nfc_apdu_runner_alloc();
    if(!app) {
        return -1;
    }

    // 初始化应用程序
    nfc_apdu_runner_init(app);

    // 运行事件循环
    view_dispatcher_run(app->view_dispatcher);

    // 释放应用程序资源
    nfc_apdu_runner_free(app);

    furi_hal_power_suppress_charge_exit();

    return 0;
}
