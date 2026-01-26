/**
 * @file skin_util.h
 * @brief Winamp skin integration utilities
 * @brief Утилиты интеграции со скинами Winamp
 * 
 * This module provides functions for integrating with Winamp's skinning system.
 * It manages GDI brushes that match the current Winamp skin colors, allowing
 * plugin windows to blend seamlessly with the Winamp interface.
 * 
 * Этот модуль предоставляет функции для интеграции с системой скинов Winamp.
 * Он управляет GDI кистями, соответствующими цветам текущего скина Winamp, позволяя
 * окнам плагина органично сливаться с интерфейсом Winamp.
 * 
 * Why Skin Integration? / Почему интеграция скинов?
 * - Professional appearance / Профессиональный внешний вид
 * - Consistency with Winamp UI / Согласованность с интерфейсом Winamp
 * - Support for both Classic and Modern skins / Поддержка Classic и Modern скинов
 * - Automatic adaptation to skin changes / Автоматическая адаптация к сменам скина
 * 
 * Usage Pattern / Паттерн использования:
 * 1. Call Skin_RefreshDialogBrush() on init and WM_DISPLAYCHANGE
 * 2. Use Skin_GetDialogBrush() in WM_ERASEBKGND/WM_CTLCOLORDLG
 * 3. Call Skin_DeleteDialogBrush() on cleanup
 * 
 * 1. Вызвать Skin_RefreshDialogBrush() при init и WM_DISPLAYCHANGE
 * 2. Использовать Skin_GetDialogBrush() в WM_ERASEBKGND/WM_CTLCOLORDLG
 * 3. Вызвать Skin_DeleteDialogBrush() при очистке
 * 
 * @note Uses Winamp SDK's wa_dlg.h for color extraction
 * @note Использует wa_dlg.h из Winamp SDK для извлечения цветов
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#pragma once
#include <windows.h>

/**
 * @brief Refresh the dialog background brush to match current skin
 * @brief Обновить кисть фона диалога для соответствия текущему скину
 * 
 * Creates or recreates the GDI brush used for dialog backgrounds.
 * This function must be called:
 * - During plugin initialization
 * - On WM_DISPLAYCHANGE (display settings changed)
 * - On WM_SYSCOLORCHANGE (color scheme changed)
 * - When user changes Winamp skin
 * 
 * Создаёт или пересоздаёт GDI кисть, используемую для фонов диалогов.
 * Эту функцию нужно вызывать:
 * - При инициализации плагина
 * - При WM_DISPLAYCHANGE (изменились настройки дисплея)
 * - При WM_SYSCOLORCHANGE (изменилась цветовая схема)
 * - Когда пользователь меняет скин Winamp
 * 
 * @note Automatically deletes old brush before creating new one
 * @note Автоматически удаляет старую кисть перед созданием новой
 * 
 * @note Thread-safe (uses static storage)
 * @note Потокобезопасно (использует статическое хранилище)
 */
void Skin_RefreshDialogBrush();

/**
 * @brief Get the current dialog background brush
 * @brief Получить текущую кисть фона диалога
 * 
 * Returns the brush that matches Winamp's current skin color.
 * Use this brush to paint dialog backgrounds in:
 * - WM_ERASEBKGND handler
 * - WM_CTLCOLORDLG handler
 * - Custom painting code
 * 
 * Возвращает кисть, соответствующую цвету текущего скина Winamp.
 * Используйте эту кисть для рисования фонов диалогов в:
 * - Обработчике WM_ERASEBKGND
 * - Обработчике WM_CTLCOLORDLG
 * - Кастомном коде рисования
 * 
 * @return Handle to solid brush, or NULL if not yet created
 * @return Дескриптор сплошной кисти, или NULL если ещё не создана
 * 
 * @note Returns NULL if Skin_RefreshDialogBrush() hasn't been called
 * @note Возвращает NULL если Skin_RefreshDialogBrush() не был вызван
 * 
 * @note Do NOT delete this brush - it's managed internally
 * @note НЕ удаляйте эту кисть - она управляется внутренне
 * 
 * Example usage / Пример использования:
 * @code
 * case WM_ERASEBKGND:
 *     if (Skin_GetDialogBrush()) {
 *         RECT rc;
 *         GetClientRect(hwnd, &rc);
 *         FillRect((HDC)wParam, &rc, Skin_GetDialogBrush());
 *         return 1;
 *     }
 *     break;
 * @endcode
 */
HBRUSH Skin_GetDialogBrush();

/**
 * @brief Delete the dialog background brush and free resources
 * @brief Удалить кисть фона диалога и освободить ресурсы
 * 
 * Frees the GDI brush created by Skin_RefreshDialogBrush().
 * This MUST be called during plugin shutdown to prevent GDI resource leaks.
 * 
 * Освобождает GDI кисть, созданную Skin_RefreshDialogBrush().
 * Это ДОЛЖНО быть вызвано при завершении работы плагина для предотвращения утечек GDI ресурсов.
 * 
 * @note Safe to call even if no brush exists
 * @note Безопасно вызывать даже если кисть не существует
 * 
 * @note Called automatically by UIHost_Quit()
 * @note Вызывается автоматически UIHost_Quit()
 * 
 * @warning Failing to call this can cause GDI resource exhaustion
 * @warning Невызов этой функции может привести к исчерпанию GDI ресурсов
 */
void Skin_DeleteDialogBrush();
