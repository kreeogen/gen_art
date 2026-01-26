/**
 * @file hotkeys.h
 * @brief Global hotkey handler for Winamp plugins
 * @brief Обработчик глобальных горячих клавиш для плагинов Winamp
 * 
 * This module provides thread-safe keyboard hook functionality for Winamp plugins.
 * It uses Windows keyboard hooks (WH_KEYBOARD) to intercept hotkey combinations
 * and send commands to Winamp without requiring the Winamp window to have focus.
 * 
 * Этот модуль предоставляет потокобезопасную функциональность перехвата клавиатуры
 * для плагинов Winamp. Он использует хуки клавиатуры Windows (WH_KEYBOARD) для
 * перехвата комбинаций горячих клавиш и отправки команд в Winamp без необходимости
 * фокуса на окне Winamp.
 * 
 * @note Uses thread-local hooks (not system-wide) for safety
 * @note Использует локальные хуки потока (не системные) для безопасности
 * 
 * @note Currently supports Alt+A hotkey for toggling the cover art window
 * @note В настоящее время поддерживает горячую клавишу Alt+A для переключения окна обложки
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize keyboard hook for the current Winamp thread
 * @brief Инициализировать перехватчик клавиатуры для текущего потока Winamp
 * 
 * Sets up a keyboard hook that monitors for specific hotkey combinations.
 * The hook is installed on the current thread (Winamp's main thread) only,
 * ensuring it doesn't interfere with other applications.
 * 
 * Устанавливает перехватчик клавиатуры, который отслеживает определённые
 * комбинации горячих клавиш. Хук устанавливается только на текущий поток
 * (главный поток Winamp), гарантируя отсутствие влияния на другие приложения.
 * 
 * @param hWinamp Handle to the Winamp main window / Дескриптор главного окна Winamp
 * @param cmdId   Command ID to send when hotkey is pressed / ID команды для отправки при нажатии горячей клавиши
 * 
 * @note This function is safe to call multiple times - it will only install the hook once
 * @note Эту функцию безопасно вызывать несколько раз - хук будет установлен только один раз
 * 
 * @note The hook monitors Alt+A and sends WM_COMMAND with the specified cmdId
 * @note Хук отслеживает Alt+A и отправляет WM_COMMAND с указанным cmdId
 */
void Hotkeys_Init(HWND hWinamp, int cmdId);

/**
 * @brief Remove the keyboard hook and clean up resources
 * @brief Удалить перехватчик клавиатуры и освободить ресурсы
 * 
 * Uninstalls the keyboard hook previously set by Hotkeys_Init().
 * This should be called when the plugin is unloaded or when hotkey
 * functionality is no longer needed.
 * 
 * Удаляет перехватчик клавиатуры, ранее установленный через Hotkeys_Init().
 * Это должно вызываться при выгрузке плагина или когда функциональность
 * горячих клавиш больше не нужна.
 * 
 * @note Safe to call even if the hook was never initialized
 * @note Безопасно вызывать, даже если хук никогда не инициализировался
 * 
 * @note Automatically resets internal state to allow re-initialization
 * @note Автоматически сбрасывает внутреннее состояние для возможности повторной инициализации
 */
void Hotkeys_Uninit(void);

#ifdef __cplusplus
}
#endif
