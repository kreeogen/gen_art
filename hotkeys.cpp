/**
 * @file hotkeys.cpp
 * @brief Thread-local keyboard hook implementation for global hotkeys
 * @brief Реализация локального хука клавиатуры для глобальных горячих клавиш
 * 
 * This module implements a safe, thread-local keyboard hook for Winamp plugins.
 * Unlike system-wide hooks, thread-local hooks only affect the Winamp process,
 * making them much safer and more reliable.
 * 
 * Этот модуль реализует безопасный, локальный хук клавиатуры для плагинов Winamp.
 * В отличие от системных хуков, локальные хуки влияют только на процесс Winamp,
 * делая их гораздо безопаснее и надёжнее.
 * 
 * Technical Details / Технические детали:
 * - Uses WH_KEYBOARD hook type (thread-local)
 * - Monitors Alt+A hotkey combination
 * - Sends WM_COMMAND to Winamp when hotkey is pressed
 * - Returns 1 to "consume" the keypress and prevent further processing
 * 
 * Why Thread-Local Hooks? / Почему локальные хуки?
 * 1. More reliable - no interference from other processes
 * 2. Safer - can't crash other applications
 * 3. Easier to debug - isolated to Winamp process
 * 4. No admin rights required
 * 
 * 1. Более надёжны - нет интерференции от других процессов
 * 2. Безопаснее - не могут упасть другие приложения
 * 3. Легче отлаживать - изолированы в процессе Winamp
 * 4. Не требуют прав администратора
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Hotkeys.h"

// ============================================================================
// Global State / Глобальное состояние
// ============================================================================

static HHOOK g_hHook     = NULL;  ///< Keyboard hook handle / Дескриптор хука клавиатуры
static HWND  g_hWinamp   = NULL;  ///< Winamp window handle / Дескриптор окна Winamp
static int   g_cmdId     = 0;     ///< Command ID to send / ID команды для отправки

// ============================================================================
// Keyboard Hook Procedure / Процедура-фильтр клавиатуры
// ============================================================================

/**
 * @brief Keyboard hook callback function
 * @brief Функция обратного вызова хука клавиатуры
 * 
 * This function is called by Windows whenever a keyboard event occurs in
 * the Winamp thread. It checks for the Alt+A hotkey combination and sends
 * a command to Winamp when detected.
 * 
 * Эта функция вызывается Windows'ом при каждом событии клавиатуры в
 * потоке Winamp. Она проверяет комбинацию Alt+A и отправляет
 * команду в Winamp при обнаружении.
 * 
 * @param code Hook code (HC_ACTION means we should process the message)
 * @param code Код хука (HC_ACTION означает, что мы должны обработать сообщение)
 * @param wParam Virtual-key code (VK_*)
 * @param wParam Код виртуальной клавиши (VK_*)
 * @param lParam Key data (bits 29, 30, 31 contain modifier state)
 * @param lParam Данные клавиши (биты 29, 30, 31 содержат состояние модификаторов)
 * 
 * @return 0 to allow further processing, 1 to consume the event
 * @return 0 для продолжения обработки, 1 для потребления события
 * 
 * lParam bit flags / Флаги битов lParam:
 * - Bit 29: Alt key state (1 = pressed)
 * - Bit 30: Previous key state (1 = was down)
 * - Bit 31: Transition state (0 = pressed, 1 = released)
 */
static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    // Only process when code is HC_ACTION / Обрабатываем только когда code == HC_ACTION
    // Negative codes mean we should NOT process the message
    // Отрицательные коды означают, что мы НЕ должны обрабатывать сообщение
    if (code == HC_ACTION)
    {
        // Check if key is being pressed (not released) / Проверяем нажатие клавиши (не отпускание)
        // Bit 31 == 0 means key is being pressed
        // Бит 31 == 0 означает, что клавиша нажимается
        if (!(lParam & 0x80000000)) 
        {
            // Check for 'A' key (virtual-key code 0x41) / Проверяем клавишу 'A' (код 0x41)
            if (wParam == 0x41) 
            {
                // Extract modifier key states / Извлечь состояния клавиш-модификаторов
                // Bit 29 == 1 means Alt is down / Бит 29 == 1 означает, что Alt нажата
                BOOL altDown  = (lParam & (1 << 29));
                BOOL ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000);

                // We want Alt+A, but NOT Ctrl+Alt+A / Мы хотим Alt+A, но НЕ Ctrl+Alt+A
                if (altDown && !ctrlDown)
                {
                    // BINGO! Alt+A detected / БИНГО! Alt+A обнаружена
                    
                    // Send command to Winamp (PostMessage is safe inside hook)
                    // Отправить команду в Winamp (PostMessage безопасен внутри хука)
                    if (g_hWinamp && IsWindow(g_hWinamp)) {
                        PostMessage(g_hWinamp, WM_COMMAND, (WPARAM)g_cmdId, 0);
                    }

                    // Return 1 to "consume" the keypress / Возвращаем 1 чтобы "съесть" нажатие
                    // This prevents Winamp and other windows from seeing it
                    // Это предотвращает видимость нажатия для Winamp и других окон
                    return 1; 
                }
            }
        }
    }
    
    // Pass control to next hook in chain (REQUIRED!) / Передать управление следующему хуку в цепочке (ОБЯЗАТЕЛЬНО!)
    // Failing to call this can break other hooks and cause unpredictable behavior
    // Непередача управления может сломать другие хуки и вызвать непредсказуемое поведение
    return CallNextHookEx(g_hHook, code, wParam, lParam);
}

// ============================================================================
// Public API Implementation / Реализация публичного API
// ============================================================================

/**
 * @brief Initialize the keyboard hook
 * @brief Инициализировать хук клавиатуры
 * 
 * Installs a thread-local keyboard hook on the current thread (Winamp's main thread).
 * The hook will monitor for Alt+A and send the specified command ID to Winamp.
 * 
 * Устанавливает локальный хук клавиатуры на текущий поток (главный поток Winamp).
 * Хук будет отслеживать Alt+A и отправлять указанный ID команды в Winamp.
 * 
 * @param hWinamp Handle to Winamp's main window / Дескриптор главного окна Winamp
 * @param cmdId   Command ID to send via WM_COMMAND / ID команды для отправки через WM_COMMAND
 */
void Hotkeys_Init(HWND hWinamp, int cmdId)
{
    // Prevent double initialization / Предотвращение двойной инициализации
    if (g_hHook) return;

    // Store parameters / Сохранить параметры
    g_hWinamp = hWinamp;
    g_cmdId   = cmdId;

    // Install thread-local hook (GetCurrentThreadId) / Установить локальный хук (GetCurrentThreadId)
    // NULL hMod means this DLL's module
    // This affects ONLY the Winamp thread, not the entire system
    // NULL hMod означает модуль этой DLL
    // Это влияет ТОЛЬКО на поток Winamp, не на всю систему
    DWORD tid = 0;
    if (g_hWinamp && IsWindow(g_hWinamp)) {
        tid = GetWindowThreadProcessId(g_hWinamp, NULL);
    }
    if (!tid) tid = GetCurrentThreadId();

    g_hHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, tid);
}

/**
 * @brief Remove the keyboard hook
 * @brief Удалить хук клавиатуры
 * 
 * Uninstalls the keyboard hook and resets all state.
 * Safe to call even if hook was never installed.
 * 
 * Удаляет хук клавиатуры и сбрасывает всё состояние.
 * Безопасно вызывать, даже если хук никогда не устанавливался.
 */
void Hotkeys_Uninit(void)
{
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    }
    g_hWinamp = NULL;
}
