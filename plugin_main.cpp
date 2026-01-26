/**
 * @file plugin_main.cpp
 * @brief Winamp General Purpose Plugin entry point
 * @brief Точка входа для General Purpose плагина Winamp
 * 
 * This file contains the mandatory functions that Winamp looks for when loading
 * a General Purpose (gen_*.dll) plugin. It acts as the bridge between Winamp
 * and the plugin's internal functionality.
 * 
 * Этот файл содержит обязательные функции, которые Winamp ищет при загрузке
 * General Purpose (gen_*.dll) плагина. Он действует как мост между Winamp
 * и внутренней функциональностью плагина.
 * 
 * Plugin Lifecycle / Жизненный цикл плагина:
 * 1. Winamp loads DLL → DllMain called
 * 2. Winamp calls winampGetGeneralPurposePlugin() → returns plugin structure
 * 3. Winamp calls g_plugin.init() → plugin initializes
 * 4. User clicks "Configure" → Winamp calls g_plugin.config()
 * 5. Winamp closes → Winamp calls g_plugin.quit()
 * 6. Winamp unloads DLL → DllMain called with DLL_PROCESS_DETACH
 * 
 * 1. Winamp загружает DLL → вызывается DllMain
 * 2. Winamp вызывает winampGetGeneralPurposePlugin() → возвращает структуру плагина
 * 3. Winamp вызывает g_plugin.init() → плагин инициализируется
 * 4. Пользователь нажимает "Configure" → Winamp вызывает g_plugin.config()
 * 5. Winamp закрывается → Winamp вызывает g_plugin.quit()
 * 6. Winamp выгружает DLL → вызывается DllMain с DLL_PROCESS_DETACH
 * 
 * Important Notes / Важные заметки:
 * - The plugin structure (g_plugin) must be static and persistent
 * - Winamp fills in hwndParent and hDllInstance after calling winampGetGeneralPurposePlugin()
 * - The description string must remain valid for the plugin's lifetime
 * 
 * - Структура плагина (g_plugin) должна быть статической и постоянной
 * - Winamp заполняет hwndParent и hDllInstance после вызова winampGetGeneralPurposePlugin()
 * - Строка описания должна оставаться валидной на протяжении жизни плагина
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ============================================================================
// SDK and Project Headers / Заголовки SDK и проекта
// ============================================================================
#include "SDK\gen.h"        // Winamp General Purpose Plugin SDK / SDK для General Purpose плагинов Winamp
#include "SwitchLangUI.h"   // Plugin name and localization / Имя плагина и локализация
#include "ui_host.h"        // Our plugin manager implementation / Наша реализация менеджера плагина

// ============================================================================
// Global Plugin Structure / Глобальная структура плагина
// ============================================================================

/**
 * @brief Plugin structure exposed to Winamp
 * @brief Структура плагина, предоставляемая Winamp
 * 
 * This structure must exist for the entire lifetime of the plugin.
 * Winamp expects it to be at a stable memory address and will read
 * from it throughout the plugin's lifecycle.
 * 
 * Эта структура должна существовать на протяжении всей жизни плагина.
 * Winamp ожидает, что она будет по стабильному адресу в памяти и будет
 * читать из неё на протяжении всего жизненного цикла плагина.
 * 
 * Fields filled by us / Поля, заполняемые нами:
 * - version: API version (defined in gen.h as GPPHDR_VER)
 * - description: Plugin name shown in Preferences
 * - init: Initialization function
 * - config: Configuration dialog function
 * - quit: Cleanup function
 * 
 * Fields filled by Winamp / Поля, заполняемые Winamp:
 * - hwndParent: Winamp's main window handle
 * - hDllInstance: Our DLL's instance handle
 */
static winampGeneralPurposePlugin g_plugin = {0};

// ============================================================================
// Plugin Entry Point / Точка входа плагина
// ============================================================================

/**
 * @brief Main plugin entry point called by Winamp
 * @brief Главная точка входа плагина, вызываемая Winamp
 * 
 * Winamp finds this function by name (GetProcAddress) after loading the DLL.
 * This function must be exported with C linkage and return a pointer to
 * the plugin structure.
 * 
 * Winamp находит эту функцию по имени (GetProcAddress) после загрузки DLL.
 * Эта функция должна быть экспортирована с C linkage и возвращать указатель
 * на структуру плагина.
 * 
 * @return Pointer to the plugin structure / Указатель на структуру плагина
 * 
 * @note This is the ONLY function that must be exported by name
 * @note Это ЕДИНСТВЕННАЯ функция, которая должна быть экспортирована по имени
 * 
 * @note extern "C" ensures C linkage (no name mangling)
 * @note extern "C" обеспечивает C linkage (без искажения имён)
 * 
 * @note __declspec(dllexport) makes it visible to Winamp
 * @note __declspec(dllexport) делает её видимой для Winamp
 */
extern "C" __declspec(dllexport)
winampGeneralPurposePlugin* winampGetGeneralPurposePlugin()
{
    // Static buffer for plugin description / Статический буфер для описания плагина
    // Must be static because Winamp keeps a pointer to it
    // Должен быть статическим, потому что Winamp хранит указатель на него
    static char desc[128];
    
    // Format plugin description from localized string / Форматирование описания плагина из локализованной строки
    // APP_NAME is defined in SwitchLangUI.h / APP_NAME определён в SwitchLangUI.h
    wsprintfA(desc, APP_NAME); 

    // Fill in plugin structure fields / Заполнение полей структуры плагина
    
    // API version (defined in SDK\gen.h) / Версия API (определена в SDK\gen.h)
    g_plugin.version      = GPPHDR_VER;
    
    // Plugin description shown in Preferences → Plug-ins → General Purpose
    // Описание плагина, показываемое в Preferences → Plug-ins → General Purpose
    g_plugin.description  = desc;
    
    // These will be filled by Winamp after this function returns
    // Эти поля будут заполнены Winamp после возврата из этой функции
    g_plugin.hwndParent   = 0;  // Winamp's main window handle / Дескриптор главного окна Winamp
    g_plugin.hDllInstance = 0;  // Our DLL's instance handle / Дескриптор нашей DLL
    
    // Bind lifecycle functions to our implementations / Привязка функций жизненного цикла к нашим реализациям
    // These functions are implemented in ui_host.cpp / Эти функции реализованы в ui_host.cpp
    
    /**
     * init() is called when:
     * - Winamp starts (if plugin is enabled)
     * - User enables plugin in Preferences
     * 
     * init() вызывается когда:
     * - Winamp запускается (если плагин включён)
     * - Пользователь включает плагин в Preferences
     */
    g_plugin.init   = UIHost_Init;
    
    /**
     * config() is called when:
     * - User clicks "Configure" button in Preferences
     * 
     * config() вызывается когда:
     * - Пользователь нажимает кнопку "Configure" в Preferences
     */
    g_plugin.config = UIHost_Config;
    
    /**
     * quit() is called when:
     * - Winamp is closing
     * - User disables plugin in Preferences
     * 
     * quit() вызывается когда:
     * - Winamp закрывается
     * - Пользователь отключает плагин в Preferences
     */
    g_plugin.quit   = UIHost_Quit;
    
    return &g_plugin;
}

// ============================================================================
// DLL Entry Point / Точка входа DLL
// ============================================================================

/**
 * @brief Standard Windows DLL entry point
 * @brief Стандартная точка входа Windows DLL
 * 
 * This function is called by Windows when the DLL is loaded or unloaded.
 * For Winamp plugins, we don't need to do anything here because Winamp
 * handles initialization through the plugin structure's init() function.
 * 
 * Эта функция вызывается Windows когда DLL загружается или выгружается.
 * Для плагинов Winamp нам не нужно ничего делать здесь, потому что Winamp
 * обрабатывает инициализацию через функцию init() структуры плагина.
 * 
 * @param hInstance Handle to the DLL module / Дескриптор модуля DLL
 * @param dwReason  Reason for calling (DLL_PROCESS_ATTACH, DLL_PROCESS_DETACH, etc.)
 * @param dwReason  Причина вызова (DLL_PROCESS_ATTACH, DLL_PROCESS_DETACH и т.д.)
 * @param lpReserved Reserved parameter / Зарезервированный параметр
 * @return TRUE to continue loading, FALSE to fail loading
 * @return TRUE для продолжения загрузки, FALSE для прерывания загрузки
 * 
 * @note We return TRUE immediately because we don't need process/thread attach/detach handling
 * @note Мы возвращаем TRUE немедленно, потому что нам не нужна обработка attach/detach процессов/потоков
 */
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) 
{ 
    // Unused parameters / Неиспользуемые параметры
    (void)hInstance;
    (void)dwReason;
    (void)lpReserved;
    
    return TRUE; 
}

// ============================================================================
// Global Accessors / Глобальные аксессоры
// ============================================================================

/**
 * These functions provide controlled access to the plugin structure's fields
 * for other modules (ui_host, ini_store, etc.) without requiring them to
 * include gen.h or directly reference g_plugin.
 * 
 * Эти функции предоставляют контролируемый доступ к полям структуры плагина
 * для других модулей (ui_host, ini_store и т.д.) без необходимости включать
 * gen.h или напрямую обращаться к g_plugin.
 * 
 * This design provides:
 * - Encapsulation (internal structure hidden from other modules)
 * - Compile-time dependency reduction (no need to include gen.h everywhere)
 * - Runtime stability (g_plugin fields are filled by Winamp and remain valid)
 * 
 * Этот дизайн обеспечивает:
 * - Инкапсуляцию (внутренняя структура скрыта от других модулей)
 * - Снижение зависимостей компиляции (не нужно включать gen.h везде)
 * - Стабильность во время выполнения (поля g_plugin заполняются Winamp и остаются валидными)
 */

/**
 * @brief Get the plugin DLL's instance handle
 * @brief Получить дескриптор (Instance Handle) DLL плагина
 * 
 * The instance handle is needed for:
 * - Loading resources (icons, dialogs, strings)
 * - Getting the DLL's file path (for INI file location)
 * - Creating windows (must specify owner module)
 * 
 * Дескриптор необходим для:
 * - Загрузки ресурсов (иконок, диалогов, строк)
 * - Получения пути к файлу DLL (для расположения INI файла)
 * - Создания окон (должен быть указан модуль-владелец)
 * 
 * @return Plugin DLL instance handle / Дескриптор DLL плагина
 * 
 * @note This value is filled by Winamp after winampGetGeneralPurposePlugin() returns
 * @note Это значение заполняется Winamp после возврата из winampGetGeneralPurposePlugin()
 */
HINSTANCE UIHost_GetHInstance() 
{ 
    return g_plugin.hDllInstance; 
}

/**
 * @brief Get Winamp's main window handle
 * @brief Получить дескриптор главного окна Winamp
 * 
 * The Winamp window handle is used for:
 * - Sending IPC messages (WM_WA_IPC) to control playback or query state
 * - Creating child windows or dialogs (Winamp as parent)
 * - Centering dialogs on Winamp's window
 * - Sending commands (WM_COMMAND) for menu actions
 * 
 * Дескриптор окна Winamp используется для:
 * - Отправки IPC сообщений (WM_WA_IPC) для управления воспроизведением или запросов состояния
 * - Создания дочерних окон или диалогов (Winamp как родитель)
 * - Центрирования диалогов относительно окна Winamp
 * - Отправки команд (WM_COMMAND) для действий меню
 * 
 * @return Winamp's main window handle / Дескриптор главного окна Winamp
 * 
 * @note This value is filled by Winamp after winampGetGeneralPurposePlugin() returns
 * @note Это значение заполняется Winamp после возврата из winampGetGeneralPurposePlugin()
 * 
 * Example IPC usage / Пример использования IPC:
 * @code
 * // Get current playlist position / Получить текущую позицию в плейлисте
 * int pos = (int)SendMessage(UIHost_GetWinampWnd(), WM_WA_IPC, 0, IPC_GETLISTPOS);
 * 
 * // Get file path at position / Получить путь к файлу в позиции
 * const char* path = (const char*)SendMessage(UIHost_GetWinampWnd(), WM_WA_IPC, pos, IPC_GETPLAYLISTFILE);
 * @endcode
 */
HWND UIHost_GetWinampWnd()  
{ 
    return g_plugin.hwndParent; 
}
