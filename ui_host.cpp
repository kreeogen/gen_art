/**
 * @file ui_host.cpp (formerly PluginManager.cpp)
 * @brief Central plugin manager for Winamp integration
 * @brief Центральный менеджер плагина для интеграции с Winamp
 *
 * 
 * This is the heart of the plugin. It manages:
 * - Plugin lifecycle (initialization, shutdown)
 * - Winamp menu integration
 * - Window hooks for message interception
 * - Keyboard shortcuts (Alt+A)
 * - Embedded window creation and management
 * - State persistence (window position, visibility)
 * - Skin integration and color changes
 * 
 * Это сердце плагина. Он управляет:
 * - Жизненным циклом плагина (инициализация, завершение)
 * - Интеграцией меню Winamp
 * - Хуками окон для перехвата сообщений
 * - Горячими клавишами (Alt+A)
 * - Созданием и управлением встраиваемыми окнами
 * - Сохранением состояния (позиция окна, видимость)
 * - Интеграцией скинов и изменением цветов
 * 
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <tchar.h>

// ============================================================================
// Dependencies / Зависимости
// ============================================================================

#include "SwitchLangUI.h"      // Localization strings / Строки локализации
#include "SDK\gen.h"            // Winamp plugin SDK structures / Структуры SDK плагинов Winamp
#define WA_DLG_IMPLEMENT        // Enable wa_dlg implementation / Включить реализацию wa_dlg
#include "SDK\wa_dlg.h"         // Winamp dialog utilities / Утилиты диалогов Winamp

#include "resource.h"           // Resource IDs (IDD_DIALOG1) / ID ресурсов
#include "ui_host.h"            // Public API / Публичный API
#include "ini_store.h"          // Configuration persistence / Сохранение конфигурации
#include "skin_util.h"          // Skin color integration / Интеграция цветов скинов
#include "image_loader.h"       // Image decoding / Декодирование изображений
#include "cover_window.h"       // Album art viewer / Просмотрщик обложек
#include "Hotkeys.h"            // Keyboard hook (Alt+A) / Хук клавиатуры (Alt+A)

// ============================================================================
// External Functions / Внешние функции
// ============================================================================

extern HWND      UIHost_GetWinampWnd();    // Implemented in plugin_main.cpp
extern HINSTANCE UIHost_GetHInstance();    // Implemented in plugin_main.cpp
int UIHost_Init();                          // Forward declaration / Прямое объявление

// ============================================================================
// Winamp IPC Constants / Константы IPC Winamp
// ============================================================================

/**
 * WM_WA_IPC - Main IPC message for Winamp communication
 * WM_WA_IPC - Главное IPC сообщение для коммуникации с Winamp
 * 
 * Usage: SendMessage(winampWnd, WM_WA_IPC, param, IPC_COMMAND)
 * Использование: SendMessage(winampWnd, WM_WA_IPC, param, IPC_COMMAND)
 */
#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)
#endif

#ifndef IPC_GET_HMENU
#define IPC_GET_HMENU 0x33              // Get Winamp's menu handle / Получить дескриптор меню Winamp
#endif

#ifndef IPC_GET_EMBEDIF
#define IPC_GET_EMBEDIF 0x39            // Get embedded window interface / Получить интерфейс встраиваемого окна
#endif

#ifndef IPC_PLAYLIST_MODIFIED
#define IPC_PLAYLIST_MODIFIED 3002      // Playlist changed / Плейлист изменён
#endif

#ifndef IPC_PLAYING_FILE
#define IPC_PLAYING_FILE 3003           // Now playing file changed / Изменился воспроизводимый файл
#endif

#ifndef IPC_GETVERSION
#define IPC_GETVERSION 0                // Get Winamp version / Получить версию Winamp
#endif

#ifndef IPC_ADJUST_OPTIONSMENUPOS
#define IPC_ADJUST_OPTIONSMENUPOS 0x3F  // Adjust Options menu item positions / Скорректировать позиции пунктов меню Options
#endif

// ============================================================================
// Plugin Constants / Константы плагина
// ============================================================================

#define DEFAULT_W    275                        // Default window width / Ширина окна по умолчанию
#define DEFAULT_H    116                        // Default window height / Высота окна по умолчанию
#define WM_APT_REFRESH (WM_USER + 0x6E01)      // Custom refresh message / Пользовательское сообщение обновления
#define WA_CMD_EQUALIZER 40258                  // Winamp equalizer menu command / Команда меню эквалайзера Winamp

/**
 * MENUID_APT - Unique menu item ID for this plugin
 * MENUID_APT - Уникальный ID пункта меню для этого плагина
 * 
 * This ID must be:
 * - Unique across all Winamp plugins
 * - Same everywhere it's used (menu insertion, command handling, checkmark)
 * 
 * Этот ID должен быть:
 * - Уникальным среди всех плагинов Winamp
 * - Одинаковым везде где используется (вставка меню, обработка команд, галочка)
 */
#define MENUID_APT 0x7001

/**
 * EMBED_FLAGS_NOWINDOWMENU - Suppress window menu in embedded window
 * EMBED_FLAGS_NOWINDOWMENU - Подавить меню окна во встраиваемом окне
 * 
 * Prevents Winamp from adding "Window" submenu to embedded windows.
 * Only available in Winamp 5.0+
 * 
 * Предотвращает добавление Winamp подменю "Window" к встраиваемым окнам.
 * Доступно только в Winamp 5.0+
 */
#define EMBED_FLAGS_NOWINDOWMENU 0x04

// ============================================================================
// State Management Structure / Структура управления состоянием
// ============================================================================

/**
 * @struct UIHostState
 * @brief Centralized plugin state (following modern C best practices)
 * @brief Централизованное состояние плагина (следуя современным best practices C)
 * 
 * This structure replaces scattered global variables with a single
 * cohesive state object. Benefits:
 * - Easier to reason about state
 * - Prevents variable naming conflicts
 * - Simplifies initialization (single memset)
 * - Better for future multi-instance support
 * 
 * Эта структура заменяет разбросанные глобальные переменные одним
 * связным объектом состояния. Преимущества:
 * - Проще рассуждать о состоянии
 * - Предотвращает конфликты имён переменных
 * - Упрощает инициализацию (один memset)
 * - Лучше для будущей поддержки множественных экземпляров
 */
typedef struct {
    // === Window Handles / Дескрипторы окон ===
    HWND winampWnd;     ///< Winamp's main window / Главное окно Winamp
    HWND dlg;           ///< Our dialog window / Наше диалоговое окно
    HWND embed;         ///< Embedded window host / Хост встраиваемого окна
    
    // === Window Procedures / Оконные процедуры ===
    WNDPROC oldProc;    ///< Original Winamp window procedure / Оригинальная оконная процедура Winamp
    WNDPROC hostOld;    ///< Original embed host procedure / Оригинальная процедура хоста встраивания
    
    // === State Flags / Флаги состояния ===
    int menuReady;      ///< TRUE if menu item inserted / TRUE если пункт меню вставлен
    int waVersion;      ///< Winamp version (e.g. 0x5092 for 5.092) / Версия Winamp
    int isOpen;         ///< TRUE if window is currently open / TRUE если окно открыто
    int isQuitting;     ///< TRUE during shutdown / TRUE при завершении работы
    int isInitialized;  ///< TRUE if fully initialized / TRUE если полностью инициализировано
    int oleInited;      ///< TRUE if OleInitialize was called by us / TRUE если OleInitialize вызван нами
    int refreshPosted; ///< TRUE if WM_APT_REFRESH already posted / TRUE если WM_APT_REFRESH уже запощен
    
    // === Window Position / Позиция окна ===
    struct {
        int x;          ///< Window X position / X позиция окна
        int y;          ///< Window Y position / Y позиция окна
        int w;          ///< Window width / Ширина окна
        int h;          ///< Window height / Высота окна
        int saved;      ///< TRUE if position was saved during session / TRUE если позиция сохранена во время сеанса
    } pos;
    
    // === Embed Window State / Состояние встраиваемого окна ===
    embedWindowState* ews;  ///< Winamp embed window state structure / Структура состояния встраиваемого окна Winamp
} UIHostState;

/**
 * Global state instance / Экземпляр глобального состояния
 * 
 * Initialized to zero, which sets all pointers to NULL and flags to FALSE.
 * Инициализирован нулями, что устанавливает все указатели в NULL и флаги в FALSE.
 */
static UIHostState g_state = {0};


// ============================================================================
// Helper Functions - Window Management
// Вспомогательные функции - Управление окнами
// ============================================================================

/**
 * @brief Remove all borders from a window for seamless embedding
 * @brief Удалить все границы окна для органичного встраивания
 * 
 * Removes:
 * - Extended styles: WS_EX_CLIENTEDGE, WS_EX_STATICEDGE, WS_EX_DLGMODALFRAME
 * - Regular styles: WS_BORDER, WS_THICKFRAME
 * 
 * This makes the window blend seamlessly into Winamp's interface
 * without visible borders or frame.
 * 
 * Удаляет:
 * - Расширенные стили: WS_EX_CLIENTEDGE, WS_EX_STATICEDGE, WS_EX_DLGMODALFRAME
 * - Обычные стили: WS_BORDER, WS_THICKFRAME
 * 
 * Это позволяет окну органично вписаться в интерфейс Winamp
 * без видимых границ или рамки.
 * 
 * @param hwnd Window to modify / Окно для модификации
 */
static void RemoveWindowBorders(HWND hwnd)
{
    LONG exStyle;
    LONG style;
    
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    
    // Remove extended border styles / Удалить расширенные стили границ
    exStyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME);
    SetWindowLongA(hwnd, GWL_EXSTYLE, exStyle);
    
    // Remove regular border styles / Удалить обычные стили границ
    style = GetWindowLongA(hwnd, GWL_STYLE);
    style &= ~(WS_BORDER | WS_THICKFRAME);
    SetWindowLongA(hwnd, GWL_STYLE, style);
    
    // Apply changes immediately / Применить изменения немедленно
    // SWP_FRAMECHANGED forces window to recalculate frame
    // SWP_FRAMECHANGED заставляет окно пересчитать рамку
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

/**
 * @brief Save current window position to memory and INI file
 * @brief Сохранить текущую позицию окна в память и INI файл
 * 
 * Stores window position both in g_state (for current session)
 * and plugin.ini (for persistence across sessions).
 * 
 * Сохраняет позицию окна как в g_state (для текущего сеанса),
 * так и в plugin.ini (для сохранения между сеансами).
 * 
 * @param hwnd Window whose position to save / Окно, позицию которого сохранить
 */
static void SaveWindowPosition(HWND hwnd)
{
    RECT rc;
    
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    
    if (GetWindowRect(hwnd, &rc)) {
        // Save to memory / Сохранить в память
        g_state.pos.x = rc.left;
        g_state.pos.y = rc.top;
        g_state.pos.w = rc.right - rc.left;
        g_state.pos.h = rc.bottom - rc.top;
        g_state.pos.saved = 1;
        
        // Save to INI file / Сохранить в INI файл
        Ini_SaveWindowPos(g_state.pos.x, g_state.pos.y, 
                         g_state.pos.w, g_state.pos.h);
    }
}

/**
 * @brief Restore window position from saved state
 * @brief Восстановить позицию окна из сохранённого состояния
 * 
 * Applies saved position if valid, otherwise uses defaults.
 * Применяет сохранённую позицию если валидна, иначе использует значения по умолчанию.
 * 
 * @param hwnd Window to position / Окно для позиционирования
 */
static void RestoreWindowPosition(HWND hwnd)
{
    int w, h;
    
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    
    // Check if we have valid saved position / Проверить есть ли валидная сохранённая позиция
    if (g_state.pos.x != -1 && g_state.pos.y != -1) {
        // Use saved size or defaults / Использовать сохранённый размер или значения по умолчанию
        w = (g_state.pos.w > 0) ? g_state.pos.w : DEFAULT_W;
        h = (g_state.pos.h > 0) ? g_state.pos.h : DEFAULT_H;
        
        SetWindowPos(hwnd, NULL, 
                    g_state.pos.x, g_state.pos.y, w, h,
                    SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

/**
 * @brief Check if plugin window is currently open
 * @brief Проверить открыто ли окно плагина в данный момент
 * 
 * @return 1 if open, 0 if closed / 1 если открыто, 0 если закрыто
 */
static int IsWindowOpen(void)
{
    return (g_state.dlg && IsWindow(g_state.dlg)) ? 1 : 0;
}

/**
 * @brief Update cached open state from actual window state
 * @brief Обновить кэшированное состояние открытости из фактического состояния окна
 */
static void UpdateOpenState(void)
{
    g_state.isOpen = IsWindowOpen();
}

// ============================================================================
// Menu Functions - EXACT THINGER.C APPROACH
// Функции меню - ТОЧНЫЙ ПОДХОД THINGER.C
// ============================================================================

/**
 * @brief Insert plugin menu item into Winamp's Options menu
 * @brief Вставить пункт меню плагина в меню Options Winamp
 * 
 * 
 * This function:
 * 1. Gets Winamp's main menu
 * 2. Finds the "Main Window" menu item (ID 40258)
 * 3. Searches forward for the separator
 * 4. Checks if our item already exists (prevents duplicates)
 * 5. Inserts our menu item just before the separator
 * 
 * Эта функция:
 * 1. Получает главное меню Winamp
 * 2. Находит пункт меню "Main Window" (ID 40258)
 * 3. Ищет вперёд разделитель
 * 4. Проверяет существует ли уже наш пункт (предотвращает дубликаты)
 * 5. Вставляет наш пункт меню перед разделителем
 * 
 * IMPORTANT: This is called ONCE during initialization, NOT on WM_INITMENU!
 * ВАЖНО: Это вызывается ОДИН РАЗ при инициализации, НЕ при WM_INITMENU!
 */
static void InsertMenuItemInWinamp(void)
{
    int i;
    HMENU WinampMenu;
    UINT id;

    // Get main menu from Winamp / Получить главное меню из Winamp
    WinampMenu = (HMENU)SendMessage(g_state.winampWnd, WM_WA_IPC, 0, IPC_GET_HMENU);

    // Find menu item "main window" (Equalizer, ID 40258)
    // Найти пункт меню "main window" (Эквалайзер, ID 40258)
    for (i = GetMenuItemCount(WinampMenu); i >= 0; i--)
    {
        if (GetMenuItemID(WinampMenu, i) == 40258)
        {
            // Find the separator and check if menu item already exists
            // Найти разделитель и проверить существует ли пункт меню
            do {
                id = GetMenuItemID(WinampMenu, ++i);
                
                // If our item already exists, don't insert again!
                // Если наш пункт уже существует, не вставлять снова!
                if (id == MENUID_APT) return;
                
            } while (id != 0xFFFFFFFF);  // 0xFFFFFFFF = separator / разделитель

            // Insert menu just before the separator
            // Вставить меню перед разделителем
            InsertMenu(WinampMenu, i - 1, MF_BYPOSITION | MF_STRING, MENUID_APT, kMenuText);
            break;
        }
    }
}

/**
 * @brief Remove plugin menu item from Winamp's Options menu
 * @brief Удалить пункт меню плагина из меню Options Winamp
 * 
 * EXACT COPY from thinger.c (lines 1098-1103)
 * ТОЧНАЯ КОПИЯ из thinger.c (строки 1098-1103)
 * 
 * Called during plugin shutdown.
 * Вызывается при завершении работы плагина.
 */
static void RemoveMenuItemFromWinamp(void)
{
    HMENU WinampMenu;
    WinampMenu = (HMENU)SendMessage(g_state.winampWnd, WM_WA_IPC, 0, IPC_GET_HMENU);
    RemoveMenu(WinampMenu, MENUID_APT, MF_BYCOMMAND);
}

/**
 * @brief Update menu item checkmark to reflect window state
 * @brief Обновить галочку пункта меню для отражения состояния окна
 * 
 * Uses SetMenuItemInfo directly without GetMenuItemInfo first.
 * 
 * Использует SetMenuItemInfo напрямую без предварительного GetMenuItemInfo.
 * 
 * @param checked 1 to show checkmark, 0 to hide / 1 показать галочку, 0 скрыть
 */
static void UpdateMenuCheckmark(int checked)
{
    HMENU WinampMenu;
    MENUITEMINFO mii;
    
    WinampMenu = (HMENU)SendMessage(g_state.winampWnd, WM_WA_IPC, 0, IPC_GET_HMENU);
    if (!WinampMenu) {
        return;
    }
    
    // Set state directly without getting first (thinger.c pattern)
    // Установить состояние напрямую без предварительного получения (паттерн thinger.c)
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;
    mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
    SetMenuItemInfo(WinampMenu, MENUID_APT, FALSE, &mii);
}

// ============================================================================
// Message Handlers
// Обработчики сообщений
// ============================================================================

/**
 * @brief Handle skin change messages (WM_DISPLAYCHANGE, WM_SYSCOLORCHANGE)
 * @brief Обработать сообщения смены скина
 * 
 * When Winamp's skin changes:
 * 1. Reinitialize WADlg with new skin
 * 2. Refresh dialog brush to match new colors
 * 3. Invalidate all windows to trigger repaint
 * 
 * Когда скин Winamp меняется:
 * 1. Реинициализировать WADlg с новым скином
 * 2. Обновить кисть диалога для соответствия новым цветам
 * 3. Инвалидировать все окна для перерисовки
 */
static LRESULT HandleSkinChanges(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND coverView;
    
    // Reinitialize dialog system with new skin / Реинициализировать систему диалогов с новым скином
    WADlg_init(hwnd);
    Skin_RefreshDialogBrush();
    
    // Repaint our dialog / Перерисовать наш диалог
    if (g_state.dlg && IsWindow(g_state.dlg)) {
        InvalidateRect(g_state.dlg, NULL, TRUE);
    }
    
    // Repaint cover viewer / Перерисовать просмотрщик обложек
    coverView = CoverView_FindOn(g_state.dlg);
    if (coverView) {
        InvalidateRect(coverView, NULL, TRUE);
    }
    
    // Forward to original window procedure / Переслать оригинальной оконной процедуре
    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

/**
 * @brief Handle Winamp IPC messages
 * @brief Обработать IPC сообщения Winamp
 * 
 * Listens for:
 * - IPC_PLAYING_FILE: Track changed
 * - IPC_PLAYLIST_MODIFIED: Playlist updated
 * 
 * Слушает:
 * - IPC_PLAYING_FILE: Трек изменился
 * - IPC_PLAYLIST_MODIFIED: Плейлист обновлён
 * 
 * When detected, posts WM_APT_REFRESH to reload album art.
 * При обнаружении отправляет WM_APT_REFRESH для перезагрузки обложки.
 */
static void RequestCoverRefreshAsync(void)
{
    // Do NOT refresh from inside Winamp's WndProc stack.
    // Не обновляемся прямо из стека оконной процедуры Winamp - избегаем реэнтранси.
    //
    // Also coalesce refresh storms: we only keep ONE pending WM_APT_REFRESH in the queue.
    // Также "схлопываем" штормы обновлений: держим только ОДНО ожидающее WM_APT_REFRESH в очереди.
    if (g_state.dlg && IsWindow(g_state.dlg)) {
        if (InterlockedExchange((LONG*)&g_state.refreshPosted, 1) == 0) {
            PostMessage(g_state.dlg, WM_APT_REFRESH, 0, 0);
        }
    }
}


static LRESULT HandleIPCMessages(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Check if this is a playing/playlist change event
    // Проверить является ли это событием смены воспроизведения/плейлиста
    if (lp == IPC_PLAYING_FILE || lp == IPC_PLAYLIST_MODIFIED) {
        // Post refresh message to update album art
        // Отправить сообщение обновления для обновления обложки
        RequestCoverRefreshAsync();
    }
    
    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

/**
 * @brief Handle WM_COMMAND messages (menu and hotkey commands)
 * @brief Обработать WM_COMMAND сообщения (команды меню и горячих клавиш)
 * 
 * Responds to:
 * - Menu clicks on our menu item
 * - Alt+A hotkey
 * 
 * Отвечает на:
 * - Клики по нашему пункту меню
 * - Горячую клавишу Alt+A
 * 
 * Toggles window visibility and updates checkmark.
 * Переключает видимость окна и обновляет галочку.
 */
static LRESULT HandleCommand(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND host;
    
    // Check if this is our menu command / Проверить является ли это нашей командой меню
    if (LOWORD(wp) == MENUID_APT) {
        if (IsWindowOpen()) {
            // === CLOSE WINDOW / ЗАКРЫТЬ ОКНО ===
            host = GetParent(g_state.dlg);
            
            // Update state / Обновить состояние
            g_state.isOpen = 0;
            Ini_SaveWindowOpen(0);
            
            // Destroy window / Уничтожить окно
            if (host && IsWindow(host)) {
                DestroyWindow(host);
            } else {
                DestroyWindow(g_state.dlg);
            }
            
            g_state.dlg = NULL;
            g_state.embed = NULL;
            
            // Update menu checkmark / Обновить галочку меню
            UpdateMenuCheckmark(0);
        } else {
            // === OPEN WINDOW / ОТКРЫТЬ ОКНО ===
            g_state.isOpen = 1;
            Ini_SaveWindowOpen(1);
            
            // Initialize and show window / Инициализировать и показать окно
            UIHost_Init();
            UpdateMenuCheckmark(1);
        }
        return 0;
    }
    
    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

// ============================================================================
// Window Procedures / Оконные процедуры
// ============================================================================

/**
 * @brief Window procedure for embedded window host
 * @brief Оконная процедура для хоста встраиваемого окна
 * 
 * Handles:
 * - WM_GETMINMAXINFO: Enforce minimum window size
 * - WM_SIZE: Resize child dialog
 * - WM_EXITSIZEMOVE: Save position after user moves/resizes
 * - WM_DESTROY: Unhook and cleanup
 * 
 * Обрабатывает:
 * - WM_GETMINMAXINFO: Обеспечить минимальный размер окна
 * - WM_SIZE: Изменить размер дочернего диалога
 * - WM_EXITSIZEMOVE: Сохранить позицию после перемещения/изменения размера пользователем
 * - WM_DESTROY: Снять хук и очистить
 */
static LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MINMAXINFO* mmi;
    RECT cr, wr;
    int minW, minH;
    
    switch (msg)
    {
    case WM_GETMINMAXINFO:
        // Enforce minimum window size / Обеспечить минимальный размер окна
        mmi = (MINMAXINFO*)lp;
        if (mmi) {
            GetClientRect(hwnd, &cr);
            GetWindowRect(hwnd, &wr);
            
            // Calculate minimum size including non-client area
            // Вычислить минимальный размер включая не-клиентскую область
            minW = DEFAULT_W + (wr.right - wr.left) - (cr.right - cr.left);
            minH = DEFAULT_H + (wr.bottom - wr.top) - (cr.bottom - cr.top);
            
            if (mmi->ptMinTrackSize.x < minW) {
                mmi->ptMinTrackSize.x = minW;
            }
            if (mmi->ptMinTrackSize.y < minH) {
                mmi->ptMinTrackSize.y = minH;
            }
        }
        return 0;

    case WM_SIZE:
        // Resize child dialog to fill client area / Изменить размер дочернего диалога для заполнения клиентской области
        if (g_state.dlg && IsWindow(g_state.dlg)) {
            GetClientRect(hwnd, &cr);
            MoveWindow(g_state.dlg, 0, 0, 
                      cr.right - cr.left, 
                      cr.bottom - cr.top, TRUE);
        }
        break;

    case WM_EXITSIZEMOVE:
        // User finished moving/resizing - save position
        // Пользователь закончил перемещение/изменение размера - сохранить позицию
        SaveWindowPosition(hwnd);
        return 0;

    case WM_DESTROY:
        // Unhook our window procedure / Снять нашу оконную процедуру
        if ((WNDPROC)GetWindowLongA(hwnd, GWL_WNDPROC) == HostProc) {
            SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)g_state.hostOld);
        }
        g_state.embed = NULL;
        break;
    }
    
    return CallWindowProcA(g_state.hostOld, hwnd, msg, wp, lp);
}

/**
 * @brief Window procedure hook for Winamp's main window
 * @brief Хук оконной процедуры для главного окна Winamp
 * 
 * Intercepts messages before Winamp processes them:
 * - WM_NCDESTROY: Safe unhooking
 * - WM_DISPLAYCHANGE / WM_SYSCOLORCHANGE: Skin changes
 * - WM_WA_IPC: Playback events
 * - WM_APT_REFRESH: Custom refresh trigger
 * - WM_COMMAND: Menu and hotkey commands
 * - WM_ENDSESSION / WM_CLOSE: Shutdown handling
 * 
 * Перехватывает сообщения до их обработки Winamp:
 * - WM_NCDESTROY: Безопасное снятие хука
 * - WM_DISPLAYCHANGE / WM_SYSCOLORCHANGE: Смены скина
 * - WM_WA_IPC: События воспроизведения
 * - WM_APT_REFRESH: Пользовательский триггер обновления
 * - WM_COMMAND: Команды меню и горячих клавиш
 * - WM_ENDSESSION / WM_CLOSE: Обработка завершения работы
 * 
 * CRITICAL: NO WM_INITMENU or WM_INITMENUPOPUP handling!
 * КРИТИЧНО: НЕТ обработки WM_INITMENU или WM_INITMENUPOPUP!
 */
static LRESULT CALLBACK WinampWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Safe unhooking on window destruction / Безопасное снятие хука при уничтожении окна
    if (msg == WM_NCDESTROY) {
        WNDPROC old = g_state.oldProc;
        if (g_state.oldProc) {
            SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)g_state.oldProc);
            g_state.oldProc = NULL;
        }
        // Extremely defensive: should never happen, but avoids CallWindowProc(NULL) crash
        // Супер-защита: по идее не случится, но предотвращает краш CallWindowProc(NULL)
        if (!old) {
            return DefWindowProcA(hwnd, msg, wp, lp);
        }
        return CallWindowProcA(old, hwnd, msg, wp, lp);
    }

    // NO WM_INITMENU or WM_INITMENUPOPUP handling - following thinger.c
    // НЕТ обработки WM_INITMENU или WM_INITMENUPOPUP - следуя thinger.c

    // Skin changes / Изменения скина
    if ((msg == WM_DISPLAYCHANGE && wp == 0 && lp == 0) || 
        msg == WM_SYSCOLORCHANGE) {
        return HandleSkinChanges(hwnd, msg, wp, lp);
    }

    // IPC events (track changes, playlist updates) / IPC события
    if (msg == WM_WA_IPC) {
        return HandleIPCMessages(hwnd, msg, wp, lp);
    }

    // Custom refresh message / Пользовательское сообщение обновления
    if (msg == WM_APT_REFRESH) {
        RequestCoverRefreshAsync();
        return 0;
    }

    // Command message (menu/hotkey) / Командное сообщение (меню/горячая клавиша)
    if (msg == WM_COMMAND) {
        return HandleCommand(hwnd, msg, wp, lp);
    }

    // Shutdown handling / Обработка завершения работы
    if (msg == WM_ENDSESSION || msg == WM_CLOSE) {
        g_state.isQuitting = 1;
    }

    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

/**
 * @brief Dialog procedure for album art viewer window
 * @brief Диалоговая процедура для окна просмотрщика обложек
 * 
 * Handles:
 * - WM_INITDIALOG: Initialize viewer and load first image
 * - WM_CTLCOLORDLG / WM_CTLCOLORSTATIC: Apply skin colors
 * - WM_ERASEBKGND: Paint background with skin color
 * - WM_SIZE: Resize cover viewer to fill window
 * - WM_DISPLAYCHANGE / WM_SYSCOLORCHANGE: Update skin
 * - WM_CLOSE: Hide window and save state
 * - WM_DESTROY: Cleanup and update menu
 * 
 * Обрабатывает:
 * - WM_INITDIALOG: Инициализировать просмотрщик и загрузить первое изображение
 * - WM_CTLCOLORDLG / WM_CTLCOLORSTATIC: Применить цвета скина
 * - WM_ERASEBKGND: Нарисовать фон с цветом скина
 * - WM_SIZE: Изменить размер просмотрщика обложек для заполнения окна
 * - WM_DISPLAYCHANGE / WM_SYSCOLORCHANGE: Обновить скин
 * - WM_CLOSE: Скрыть окно и сохранить состояние
 * - WM_DESTROY: Очистка и обновление меню
 */
static LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int ret;
    RECT rc;
    HWND view;
    HWND host;
    
    // Let WADlg handle its messages first / Позволить WADlg обработать его сообщения первым
    ret = WADlg_handleDialogMsgs(hwnd, msg, wp, lp);
    if (ret) {
        return ret;
    }

    switch (msg)
    {
    case WM_INITDIALOG:
        g_state.pos.saved = 0;  // Clear saved flag / Очистить флаг сохранения
        
        // Initialize skin integration / Инициализировать интеграцию скина
        WADlg_init(UIHost_GetWinampWnd());
        Skin_RefreshDialogBrush();
        
        // Set window title and show / Установить заголовок окна и показать
        host = GetParent(hwnd);
        SetWindowText(host ? host : hwnd, kAptTitle);
        ShowWindow(host ? host : hwnd, SW_SHOWNORMAL);
        
        // Create cover viewer and load first image
        // Создать просмотрщик обложек и загрузить первое изображение
        CoverView_Attach(hwnd);

        // Update state / Обновить состояние
        g_state.isOpen = 1;
        Ini_SaveWindowOpen(1);
        
        // Update menu checkmark / Обновить галочку меню
        UpdateMenuCheckmark(1);
        
        return 0;

    case WM_APT_REFRESH:
        // Refresh cover art asynchronously (posted from Winamp events)
        // Обновить обложку асинхронно (постится из событий Winamp)
        // Allow next refresh to be posted / Разрешить пост следующего обновления
        g_state.refreshPosted = 0;
        CoverView_ReloadFromCurrent();
        return 0;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        // Apply skin colors to dialog controls / Применить цвета скина к элементам диалога
        if (Skin_GetDialogBrush()) {
            SetBkMode((HDC)wp, TRANSPARENT);
            return (INT_PTR)Skin_GetDialogBrush();
        }
        return 0;

    case WM_ERASEBKGND:
        // Paint background with skin color / Нарисовать фон с цветом скина
        if (Skin_GetDialogBrush()) {
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wp, &rc, Skin_GetDialogBrush());
            return 1;
        }
        return 0;

    case WM_SIZE:
        // Resize cover viewer to fill window / Изменить размер просмотрщика обложек для заполнения окна
        view = CoverView_FindOn(hwnd);
        if (view) {
            GetClientRect(hwnd, &rc);
            MoveWindow(view, 0, 0, rc.right, rc.bottom, TRUE);
        }
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_SYSCOLORCHANGE:
        // Update skin integration / Обновить интеграцию скина
        WADlg_init(UIHost_GetWinampWnd());
        Skin_RefreshDialogBrush();
        InvalidateRect(hwnd, NULL, TRUE);
        
        // Update cover viewer / Обновить просмотрщик обложек
        view = CoverView_FindOn(hwnd);
        if (view) {
            SendMessage(view, msg, wp, lp);
            InvalidateRect(view, NULL, TRUE);
            UpdateWindow(view);
        }
        return 0;

    case WM_CLOSE:
        // Save state before closing / Сохранить состояние перед закрытием
        if (!g_state.isQuitting) {
            g_state.isOpen = 0;
            Ini_SaveWindowOpen(0);
        }
        
        // Save position / Сохранить позицию
        host = GetParent(hwnd);
        SaveWindowPosition(host ? host : hwnd);
        
        // Destroy window / Уничтожить окно
        DestroyWindow(host ? host : hwnd);
        g_state.embed = NULL;
        return 0;

    case WM_DESTROY:
        // Save position if not already saved / Сохранить позицию если ещё не сохранена
        if (!g_state.pos.saved) {
            host = GetParent(hwnd);
            SaveWindowPosition(host ? host : hwnd);
        }
        
        // Cleanup dialog system / Очистить систему диалогов
        WADlg_close();
        g_state.dlg = NULL;
        
        // Update checkmark when window closes - following thinger.c
        // Обновить галочку когда окно закрывается - следуя thinger.c
        UpdateMenuCheckmark(0);
        return 0;
    }
    
    return 0;
}

// ============================================================================
// Public API Implementation / Реализация публичного API
// ============================================================================

/**
 * @brief Initialize the plugin
 * @brief Инициализировать плагин
 * 
 * This is the main initialization function called by Winamp.
 * Это главная функция инициализации, вызываемая Winamp.
 * 
 * Initialization sequence / Последовательность инициализации:
 * 1. Prevent double initialization / Предотвратить двойную инициализацию
 * 2. Get Winamp window and version / Получить окно и версию Winamp
 * 3. Install window hooks / Установить хуки окон
 * 4. Initialize keyboard shortcuts / Инициализировать горячие клавиши
 * 5. Insert menu item (ONCE!) / Вставить пункт меню (ОДИН РАЗ!)
 * 6. Load saved window position and state / Загрузить сохранённую позицию и состояние окна
 * 7. Create embedded window / Создать встраиваемое окно
 * 8. Create dialog if should be open / Создать диалог если должен быть открыт
 * 9. Update menu checkmark / Обновить галочку меню
 * 
 * @return 0 on success / 0 при успехе
 */
int UIHost_Init(void)
{
    int shouldOpen;
    RECT rcWinamp;
    HWND host;
    
    // ========================================================================
    // STEP 1: Prevent double initialization
    // ШАГ 1: Предотвратить двойную инициализацию
    // ========================================================================
    if (g_state.isInitialized && g_state.dlg && IsWindow(g_state.dlg)) {
        return 0;
    }
    
    // ========================================================================
    // STEP 2: Get Winamp window and version
    // ШАГ 2: Получить окно и версию Winamp
    // ========================================================================
    g_state.winampWnd = UIHost_GetWinampWnd();

    // Initialize OLE/COM once (required for OleLoadPicture in image_loader)
    // Инициализировать OLE/COM один раз (нужно для OleLoadPicture в image_loader)
    if (!g_state.oleInited) {
        HRESULT hrOle = OleInitialize(NULL);
        // S_OK: initialized now, S_FALSE: already initialized for this thread (still needs OleUninitialize)
        if (hrOle == S_OK || hrOle == S_FALSE) {
            g_state.oleInited = 1;
        }
        // RPC_E_CHANGED_MODE means COM is already initialized in a different mode.
        // In that case we must NOT call OleUninitialize(), and OleLoadPicture should still work.
    }
    g_state.waVersion = (int)SendMessage(g_state.winampWnd, WM_WA_IPC, 
                                         0, IPC_GETVERSION);

    // ========================================================================
    // STEP 3: Install window hook if not already installed
    // ШАГ 3: Установить хук окна если ещё не установлен
    // ========================================================================
    // This allows us to intercept messages sent to Winamp's main window.
    // Это позволяет нам перехватывать сообщения, отправленные главному окну Winamp.
    if (!g_state.oldProc) {
        g_state.oldProc = (WNDPROC)SetWindowLongA(g_state.winampWnd, 
                                                  GWL_WNDPROC, 
                                                  (LONG)WinampWndProc);
    }

    // ========================================================================
    // STEP 4: Initialize keyboard hook interceptor (Alt+A)
    // ШАГ 4: Инициализировать перехватчик клавиатурного хука (Alt+A)
    // ========================================================================
    Hotkeys_Init(g_state.winampWnd, MENUID_APT);

    // ========================================================================
    // STEP 5: Insert menu item ONCE - following thinger.c approach
    // ШАГ 5: Вставить пункт меню ОДИН РАЗ - следуя подходу thinger.c
    // ========================================================================
    // CRITICAL: This is called ONCE, not on every WM_INITMENU!
    // КРИТИЧНО: Это вызывается ОДИН РАЗ, не при каждом WM_INITMENU!
    if (!g_state.menuReady) {
        InsertMenuItemInWinamp();
        SendMessage(g_state.winampWnd, WM_WA_IPC, 1, IPC_ADJUST_OPTIONSMENUPOS);
        g_state.menuReady = 1;
    }

    // ========================================================================
    // STEP 6: Load saved window position
    // ШАГ 6: Загрузить сохранённую позицию окна
    // ========================================================================
    if (!Ini_LoadWindowPos(g_state.pos.x, g_state.pos.y, 
                          g_state.pos.w, g_state.pos.h)) {
        // No saved position - use defaults / Нет сохранённой позиции - использовать значения по умолчанию
        g_state.pos.x = -1;
        g_state.pos.y = -1;
        g_state.pos.w = -1;
        g_state.pos.h = -1;
    }

    // ========================================================================
    // STEP 7: Load saved open state
    // ШАГ 7: Загрузить сохранённое состояние открытости
    // ========================================================================
    shouldOpen = 1;  // Default to open / По умолчанию открыто
    if (!Ini_LoadWindowOpen(shouldOpen)) {
        shouldOpen = 1;
    }
    g_state.isOpen = shouldOpen ? 1 : 0;

    // ========================================================================
    // STEP 8: Allocate embed window state (free old one if exists)
    // ШАГ 8: Выделить состояние встраиваемого окна (освободить старое если существует)
    // ========================================================================
    if (g_state.ews) {
        GlobalFree(g_state.ews);
        g_state.ews = NULL;
    }
    
    g_state.ews = (embedWindowState*)GlobalAlloc(GPTR, sizeof(embedWindowState));
    if (!g_state.ews) {
        return 0;
    }
    
    // Set flags for Winamp 5.0+ / Установить флаги для Winamp 5.0+
    g_state.ews->flags = (g_state.waVersion >= 0x5000) ? 
                         EMBED_FLAGS_NOWINDOWMENU : 0;

    // ========================================================================
    // STEP 9: Set initial window rectangle
    // ШАГ 9: Установить начальный прямоугольник окна
    // ========================================================================
    if (g_state.pos.x != -1) {
        // Use saved position / Использовать сохранённую позицию
        g_state.ews->r.left = g_state.pos.x;
        g_state.ews->r.top = g_state.pos.y;
        g_state.ews->r.right = g_state.pos.x + g_state.pos.w;
        g_state.ews->r.bottom = g_state.pos.y + g_state.pos.h;
    } else {
        // Position below Winamp window / Позиционировать под окном Winamp
        GetWindowRect(g_state.winampWnd, &rcWinamp);
        g_state.ews->r.left = rcWinamp.left;
        g_state.ews->r.top = rcWinamp.bottom;
        g_state.ews->r.right = rcWinamp.left + DEFAULT_W;
        g_state.ews->r.bottom = rcWinamp.bottom + DEFAULT_H;
    }

    // ========================================================================
    // STEP 10: Create embed window if needed
    // ШАГ 10: Создать встраиваемое окно если нужно
    // ========================================================================
    if (!g_state.embed) {
        g_state.embed = (HWND)SendMessage(g_state.winampWnd, WM_WA_IPC, 
                                         (WPARAM)g_state.ews, IPC_GET_EMBEDIF);
        if (!g_state.embed) {
            return 0;
        }
        
        // Add clip styles for proper child window rendering
        // Добавить стили клиппинга для правильной отрисовки дочернего окна
        SetWindowLongA(g_state.embed, GWL_STYLE, 
                      GetWindowLongA(g_state.embed, GWL_STYLE) | 
                      WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        
        // Hook embed window procedure / Перехватить процедуру встраиваемого окна
        g_state.hostOld = (WNDPROC)SetWindowLongA(g_state.embed, 
                                                  GWL_WNDPROC, 
                                                  (LONG)HostProc);
        
        // Remove borders for seamless embedding / Удалить границы для органичного встраивания
        RemoveWindowBorders(g_state.embed);
    }

    // ========================================================================
    // STEP 11: Create dialog if should be open
    // ШАГ 11: Создать диалог если должен быть открыт
    // ========================================================================
    if (g_state.isOpen && (!g_state.dlg || !IsWindow(g_state.dlg))) {
        g_state.dlg = CreateDialog(UIHost_GetHInstance(), 
                                   MAKEINTRESOURCE(IDD_DIALOG1), 
                                   g_state.embed, 
                                   (DLGPROC)DlgProc);
        if (g_state.dlg) {
            Ini_SaveWindowOpen(1);
            
            // Remove borders from parent / Удалить границы у родителя
            host = GetParent(g_state.dlg);
            RemoveWindowBorders(host ? host : g_state.dlg);
            
            g_state.isInitialized = 1;
        }
    }

    // ========================================================================
    // STEP 12: Update checkmark to reflect current state
    // ШАГ 12: Обновить галочку для отражения текущего состояния
    // ========================================================================
    UpdateMenuCheckmark(g_state.isOpen);
    return 0;
}

/**
 * @brief Display plugin configuration dialog
 * @brief Показать диалог конфигурации плагина
 * 
 * Called when user clicks "Configure" button in Winamp Preferences.
 * Currently displays a simple message box with plugin information.
 * 
 * Вызывается когда пользователь нажимает кнопку "Configure" в Preferences Winamp.
 * В настоящее время показывает простой диалог с информацией о плагине.
 */
void UIHost_Config(void)
{
    MessageBoxA(UIHost_GetWinampWnd(), 
               APP_CONFIG, 
               APP_CONFIG_TITLE, 
               MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

/**
 * @brief Shut down the plugin and clean up all resources
 * @brief Завершить работу плагина и очистить все ресурсы
 * 
 * Cleanup sequence / Последовательность очистки:
 * 1. Remove keyboard hook / Удалить хук клавиатуры
 * 2. Save window state / Сохранить состояние окна
 * 3. Destroy windows / Уничтожить окна
 * 4. Free embed window state / Освободить состояние встраиваемого окна
 * 5. Remove window hooks / Удалить хуки окон
 * 6. Remove menu items / Удалить пункты меню
 * 7. Cleanup GDI resources / Очистить GDI ресурсы
 * 8. Reset state / Сбросить состояние
 * 
 * CRITICAL: Must be called to prevent resource leaks!
 * КРИТИЧНО: Должна быть вызвана для предотвращения утечек ресурсов!
 */
void UIHost_Quit(void)
{
    HWND host;
    
    // ========================================================================
    // STEP 1: Remove keyboard hook
    // ШАГ 1: Удалить хук клавиатуры
    // ========================================================================
    Hotkeys_Uninit();

    // ========================================================================
    // STEP 2: Save state to INI file
    // ШАГ 2: Сохранить состояние в INI файл
    // ========================================================================
    if (g_state.pos.x != -1) {
        Ini_SaveWindowPos(g_state.pos.x, g_state.pos.y, 
                         g_state.pos.w, g_state.pos.h);
    }
    Ini_SaveWindowOpen(g_state.isOpen ? 1 : 0);

    // ========================================================================
    // STEP 3: Destroy dialog window
    // ШАГ 3: Уничтожить диалоговое окно
    // ========================================================================
    g_state.isQuitting = 1;  // Prevent WM_CLOSE from saving state again
    if (g_state.dlg && IsWindow(g_state.dlg)) {
        host = GetParent(g_state.dlg);
        if (host && IsWindow(host)) {
            DestroyWindow(host);
        } else {
            DestroyWindow(g_state.dlg);
        }
        g_state.dlg = NULL;
    }

    // ========================================================================
    // STEP 4: Free embed window state
    // ШАГ 4: Освободить состояние встраиваемого окна
    // ========================================================================
    if (g_state.ews) {
        GlobalFree(g_state.ews);
        g_state.ews = NULL;
    }

    // ========================================================================
    // STEP 5: Remove window hook
    // ШАГ 5: Удалить хук окна
    // ========================================================================
    if (g_state.winampWnd && g_state.oldProc) {
        SetWindowLongA(g_state.winampWnd, GWL_WNDPROC, (LONG)g_state.oldProc);
        g_state.oldProc = NULL;
    }

    // ========================================================================
    // STEP 6: Remove menu item
    // ШАГ 6: Удалить пункт меню
    // ========================================================================
    if (g_state.menuReady && g_state.winampWnd && IsWindow(g_state.winampWnd)) {
        RemoveMenuItemFromWinamp();
    }
    g_state.menuReady = 0;

    // ========================================================================
    // STEP 7: Cleanup GDI and image resources
    // ШАГ 7: Очистить GDI и ресурсы изображений
    // ========================================================================
    Skin_DeleteDialogBrush();
    Img_Cleanup();  // CRITICAL: Prevents GDI+ process hang / КРИТИЧНО: Предотвращает зависание процесса GDI+
    
        // Unregister our cover view class (safety against stale classes)
    // Снять регистрацию класса просмотрщика (защита от "мертвых" классов)
    {
        HINSTANCE hi = UIHost_GetHInstance();
        if (hi) {
            UnregisterClassA("APT_CoverArtView", hi);
        }
        // Backward safety: in case an older build registered on Winamp.exe
        UnregisterClassA("APT_CoverArtView", GetModuleHandleA(NULL));
    }

    // Uninitialize OLE/COM if we initialized it
    // Деинициализировать OLE/COM если мы его инициализировали
    if (g_state.oleInited) {
        OleUninitialize();
        g_state.oleInited = 0;
    }

// ========================================================================
    // STEP 8: Reset initialization state
    // ШАГ 8: Сбросить состояние инициализации
    // ========================================================================
    g_state.isInitialized = 0;
}