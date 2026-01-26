/**
 * @file skin_util.cpp
 * @brief Winamp skin integration implementation
 * @brief Реализация интеграции со скинами Winamp
 * 
 * This module manages GDI resources for rendering plugin windows with colors
 * that match the current Winamp skin. It provides seamless visual integration
 * with both Classic and Modern Winamp skins.
 * 
 * Этот модуль управляет GDI ресурсами для отрисовки окон плагина с цветами,
 * соответствующими текущему скину Winamp. Он обеспечивает органичную визуальную
 * интеграцию как с Classic, так и с Modern скинами Winamp.
 * 
 * Technical Details / Технические детали:
 * - Uses Winamp SDK's WADlg_getColor() to query skin colors
 * - Maintains a single static brush that matches current skin
 * - Automatically handles skin changes via WM_DISPLAYCHANGE
 * - Prevents GDI resource leaks through proper cleanup
 * 
 * - Использует WADlg_getColor() из Winamp SDK для запроса цветов скина
 * - Поддерживает единственную статическую кисть, соответствующую текущему скину
 * - Автоматически обрабатывает смену скинов через WM_DISPLAYCHANGE
 * - Предотвращает утечки GDI ресурсов через правильную очистку
 * 
 * Why This Matters / Почему это важно:
 * Without skin integration, plugin windows would appear as standard gray
 * Windows dialogs, breaking visual consistency with Winamp's themed interface.
 * 
 * Без интеграции скинов окна плагина выглядели бы как стандартные серые
 * Windows диалоги, нарушая визуальную согласованность с тематическим интерфейсом Winamp.
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ============================================================================
// SDK Headers / Заголовки SDK
// ============================================================================

/**
 * wa_dlg.h - Winamp Dialog utilities from SDK
 * wa_dlg.h - Утилиты диалогов Winamp из SDK
 * 
 * Provides functions for working with Winamp skin colors:
 * - WADlg_init() - Initialize dialog with skin
 * - WADlg_getColor() - Get color for specific UI element
 * 
 * Предоставляет функции для работы с цветами скинов Winamp:
 * - WADlg_init() - Инициализировать диалог со скином
 * - WADlg_getColor() - Получить цвет для определённого UI элемента
 */
#include "SDK\wa_dlg.h" 
#include "skin_util.h"

// ============================================================================
// Global State / Глобальное состояние
// ============================================================================

/**
 * @brief Current dialog background brush
 * @brief Текущая кисть фона диалога
 * 
 * This static variable holds the GDI brush used for painting dialog backgrounds.
 * It's created by Skin_RefreshDialogBrush() and must be deleted before creating
 * a new one to prevent GDI resource leaks.
 * 
 * Эта статическая переменная хранит GDI кисть, используемую для рисования фонов диалогов.
 * Она создаётся Skin_RefreshDialogBrush() и должна быть удалена перед созданием
 * новой для предотвращения утечек GDI ресурсов.
 * 
 * @note static = internal linkage (not visible outside this file)
 * @note static = внутренняя связь (не видна вне этого файла)
 */
static HBRUSH s_br = NULL;

// ============================================================================
// Public API Implementation / Реализация публичного API
// ============================================================================

/**
 * @brief Refresh dialog background brush to match current skin
 * @brief Обновить кисть фона диалога для соответствия текущему скину
 * 
 * This function performs three steps:
 * 1. Delete old brush if it exists (prevent GDI leak)
 * 2. Query current skin color from Winamp
 * 3. Create new solid brush with that color
 * 
 * Эта функция выполняет три шага:
 * 1. Удалить старую кисть если она существует (предотвращение GDI утечки)
 * 2. Запросить текущий цвет скина из Winamp
 * 3. Создать новую сплошную кисть с этим цветом
 * 
 * When to call / Когда вызывать:
 * - Plugin initialization / Инициализация плагина
 * - WM_DISPLAYCHANGE (display settings changed) / Изменились настройки дисплея
 * - WM_SYSCOLORCHANGE (color scheme changed) / Изменилась цветовая схема
 * - When user changes Winamp skin / Когда пользователь меняет скин Winamp
 */
void Skin_RefreshDialogBrush()
{
    // Step 1: Delete old brush to prevent resource leak
    // Шаг 1: Удалить старую кисть для предотвращения утечки ресурсов
    // 
    // If we don't do this, repeatedly calling this function would create
    // new brushes without freeing old ones, eventually exhausting GDI resources.
    // This can cause graphics artifacts and system instability in Windows.
    // 
    // Если мы не сделаем этого, повторные вызовы этой функции будут создавать
    // новые кисти без освобождения старых, в конечном итоге исчерпывая GDI ресурсы.
    // Это может вызвать графические артефакты и нестабильность системы в Windows.
    if (s_br) { 
        DeleteObject(s_br); 
        s_br = NULL; 
    }

    // Step 2: Query skin color from Winamp SDK
    // Шаг 2: Запросить цвет скина из Winamp SDK
    // 
    // WADLG_ITEMBG is a constant defined in wa_dlg.h that means
    // "background color for list items and dialog areas"
    // WADlg_getColor() returns an RGB value (COLORREF) from the active skin.
    // 
    // WADLG_ITEMBG - это константа, определённая в wa_dlg.h, означающая
    // "цвет фона для элементов списка и областей диалога"
    // WADlg_getColor() возвращает RGB значение (COLORREF) из активного скина.
    COLORREF skinColor = WADlg_getColor(WADLG_ITEMBG);

    // Step 3: Create new solid brush with skin color
    // Шаг 3: Создать новую сплошную кисть с цветом скина
    // 
    // CreateSolidBrush() creates a GDI brush object that can be used
    // with FillRect(), painting APIs, etc.
    // 
    // CreateSolidBrush() создаёт объект GDI кисти, который может использоваться
    // с FillRect(), API рисования и т.д.
    s_br = CreateSolidBrush(skinColor);
}

/**
 * @brief Get current dialog background brush
 * @brief Получить текущую кисть фона диалога
 * 
 * Returns the brush created by Skin_RefreshDialogBrush().
 * This brush should be used in WM_ERASEBKGND and WM_CTLCOLORDLG
 * handlers to paint backgrounds that match the Winamp skin.
 * 
 * Возвращает кисть, созданную Skin_RefreshDialogBrush().
 * Эта кисть должна использоваться в обработчиках WM_ERASEBKGND и WM_CTLCOLORDLG
 * для рисования фонов, соответствующих скину Winamp.
 * 
 * @return Handle to brush, or NULL if not initialized
 * @return Дескриптор кисти, или NULL если не инициализирована
 * 
 * @note Do NOT delete this brush yourself - it's managed by this module
 * @note НЕ удаляйте эту кисть сами - она управляется этим модулем
 */
HBRUSH Skin_GetDialogBrush() 
{ 
    return s_br; 
}

/**
 * @brief Delete dialog background brush and free resources
 * @brief Удалить кисть фона диалога и освободить ресурсы
 * 
 * This function MUST be called during plugin shutdown to properly
 * release GDI resources. Failing to call this can cause:
 * - GDI resource leaks
 * - Graphics artifacts in Windows
 * - System instability (if GDI resources are exhausted)
 * 
 * Эта функция ДОЛЖНА быть вызвана при завершении работы плагина для правильного
 * освобождения GDI ресурсов. Невызов может привести к:
 * - Утечкам GDI ресурсов
 * - Графическим артефактам в Windows
 * - Нестабильности системы (если GDI ресурсы исчерпаны)
 * 
 * @note Safe to call multiple times (checks if brush exists)
 * @note Безопасно вызывать несколько раз (проверяет существование кисти)
 * 
 * @note Called automatically by UIHost_Quit()
 * @note Вызывается автоматически UIHost_Quit()
 */
void Skin_DeleteDialogBrush() 
{ 
    if (s_br) { 
        DeleteObject(s_br); 
        s_br = NULL; 
    } 
}
