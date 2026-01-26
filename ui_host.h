/**
 * @file ui_host.h
 * @brief Plugin manager interface for Winamp integration
 * @brief Интерфейс менеджера плагина для интеграции с Winamp
 * 
 * This header defines the public API for the plugin's main management layer.
 * It provides the lifecycle functions that Winamp calls (init, config, quit)
 * and accessor functions for getting plugin and Winamp handles.
 * 
 * Этот заголовок определяет публичный API для главного управляющего слоя плагина.
 * Он предоставляет функции жизненного цикла, которые вызывает Winamp (init, config, quit)
 * и функции-аксессоры для получения дескрипторов плагина и Winamp.
 * 
 * Architecture / Архитектура:
 * - Manages plugin lifecycle (initialization, configuration, shutdown)
 * - Handles embedded window creation and management
 * - Integrates with Winamp's menu system
 * - Manages keyboard shortcuts via hooks
 * - Persists window state between sessions
 * 
 * - Управляет жизненным циклом плагина (инициализация, конфигурация, завершение)
 * - Обрабатывает создание и управление встраиваемым окном
 * - Интегрируется с системой меню Winamp
 * - Управляет горячими клавишами через хуки
 * - Сохраняет состояние окна между сеансами
 * 
 * @note These functions are called by Winamp's plugin system
 * @note Эти функции вызываются системой плагинов Winamp
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#pragma once
#include <windows.h>

// ============================================================================
// Plugin Lifecycle Functions / Функции жизненного цикла плагина
// ============================================================================

extern "C" {

/**
 * @brief Initialize the plugin
 * @brief Инициализировать плагин
 * 
 * Called by Winamp when:
 * - Winamp starts up (if plugin is enabled)
 * - User enables the plugin in Preferences
 * 
 * Вызывается Winamp когда:
 * - Winamp запускается (если плагин включён)
 * - Пользователь включает плагин в Preferences
 * 
 * This function:
 * 1. Installs window hooks to intercept Winamp messages
 * 2. Initializes keyboard shortcuts (Alt+A)
 * 3. Inserts menu item into Winamp's menu
 * 4. Loads saved window position and state
 * 5. Creates the album art viewer window if needed
 * 
 * Эта функция:
 * 1. Устанавливает хуки окна для перехвата сообщений Winamp
 * 2. Инициализирует горячие клавиши (Alt+A)
 * 3. Вставляет пункт меню в меню Winamp
 * 4. Загружает сохранённую позицию и состояние окна
 * 5. Создаёт окно просмотра обложек если нужно
 * 
 * @return 0 on success / 0 при успехе
 * 
 * @note Safe to call multiple times (checks if already initialized)
 * @note Безопасно вызывать несколько раз (проверяет инициализацию)
 */
int  UIHost_Init();

/**
 * @brief Display plugin configuration dialog
 * @brief Показать диалог конфигурации плагина
 * 
 * Called when user clicks "Configure" button in Winamp's Preferences.
 * Currently displays a simple information message box.
 * 
 * Вызывается когда пользователь нажимает кнопку "Configure" в Preferences Winamp.
 * В настоящее время показывает простой информационный диалог.
 * 
 * @note This is where you would add plugin settings UI
 * @note Здесь можно добавить интерфейс настроек плагина
 */
void UIHost_Config();

/**
 * @brief Shut down the plugin and clean up resources
 * @brief Завершить работу плагина и освободить ресурсы
 * 
 * Called by Winamp when:
 * - Winamp is closing
 * - User disables the plugin in Preferences
 * 
 * Вызывается Winamp когда:
 * - Winamp закрывается
 * - Пользователь отключает плагин в Preferences
 * 
 * This function:
 * 1. Removes keyboard hooks
 * 2. Saves window position and state
 * 3. Destroys all windows
 * 4. Removes menu items
 * 5. Cleans up GDI resources (brushes, bitmaps)
 * 6. Shuts down image loading libraries
 * 
 * Эта функция:
 * 1. Удаляет хуки клавиатуры
 * 2. Сохраняет позицию и состояние окна
 * 3. Уничтожает все окна
 * 4. Удаляет пункты меню
 * 5. Очищает GDI ресурсы (кисти, bitmap'ы)
 * 6. Завершает работу библиотек загрузки изображений
 * 
 * @note CRITICAL: Must be called to prevent resource leaks and crashes
 * @note КРИТИЧНО: Должна быть вызвана для предотвращения утечек ресурсов и крашей
 */
void UIHost_Quit();

} // extern "C"

// ============================================================================
// Accessor Functions / Функции-аксессоры
// ============================================================================

/**
 * @brief Get the plugin DLL's instance handle
 * @brief Получить дескриптор (Instance Handle) DLL плагина
 * 
 * Returns the HINSTANCE of the plugin DLL. This is needed for:
 * - Loading resources (icons, dialogs, strings)
 * - Getting the DLL file path (for INI configuration)
 * - Creating windows (must specify owner module)
 * 
 * Возвращает HINSTANCE DLL плагина. Это нужно для:
 * - Загрузки ресурсов (иконок, диалогов, строк)
 * - Получения пути к файлу DLL (для INI конфигурации)
 * - Создания окон (должен быть указан модуль-владелец)
 * 
 * @return Plugin DLL instance handle / Дескриптор DLL плагина
 * 
 * @note This value is set by Winamp after plugin initialization
 * @note Это значение устанавливается Winamp после инициализации плагина
 * 
 * @note Implemented in plugin_main.cpp / Реализовано в plugin_main.cpp
 */
HINSTANCE UIHost_GetHInstance();

/**
 * @brief Get Winamp's main window handle
 * @brief Получить дескриптор главного окна Winamp
 * 
 * Returns the HWND of Winamp's main window. This is used for:
 * - Sending IPC messages (WM_WA_IPC) to query/control playback
 * - Creating child windows (Winamp as parent)
 * - Centering dialogs on Winamp's window
 * - Sending menu commands (WM_COMMAND)
 * 
 * Возвращает HWND главного окна Winamp. Используется для:
 * - Отправки IPC сообщений (WM_WA_IPC) для запросов/управления воспроизведением
 * - Создания дочерних окон (Winamp как родитель)
 * - Центрирования диалогов относительно окна Winamp
 * - Отправки команд меню (WM_COMMAND)
 * 
 * @return Winamp's main window handle / Дескриптор главного окна Winamp
 * 
 * @note This value is set by Winamp after plugin initialization
 * @note Это значение устанавливается Winamp после инициализации плагина
 * 
 * @note Implemented in plugin_main.cpp / Реализовано в plugin_main.cpp
 * 
 * Example IPC usage / Пример использования IPC:
 * @code
 * // Get current track position / Получить текущую позицию трека
 * int pos = (int)SendMessage(UIHost_GetWinampWnd(), WM_WA_IPC, 0, IPC_GETLISTPOS);
 * @endcode
 */
HWND UIHost_GetWinampWnd();
