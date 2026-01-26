/**
 * @file cover_window.h
 * @brief Album cover art viewer interface for Winamp
 * @brief Интерфейс просмотрщика обложек альбомов для Winamp
 * 
 * This header defines the public API for the album cover art viewer component.
 * The viewer creates a custom window that displays embedded or external album
 * artwork for the currently playing track in Winamp.
 * 
 * Этот заголовок определяет публичный API компонента просмотра обложек альбомов.
 * Просмотрщик создаёт кастомное окно, которое отображает встроенные или внешние
 * обложки альбомов для текущего трека в Winamp.
 * 
 * @note The viewer automatically monitors the Winamp playlist and updates
 *       the displayed artwork when the track changes
 * @note Просмотрщик автоматически мониторит плейлист Winamp и обновляет
 *       отображаемую обложку при смене трека
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
 * @brief Attach/create the cover art viewer window to a parent window
 * @brief Прикрепить/создать окно просмотра обложек к родительскому окну
 * 
 * Creates a new cover art viewer window as a child of the specified parent.
 * If the viewer already exists, this function attaches it to the new parent.
 * The viewer fills the entire client area of the parent window.
 * 
 * Создаёт новое окно просмотра обложек как дочернее для указанного родителя.
 * Если просмотрщик уже существует, эта функция прикрепляет его к новому родителю.
 * Просмотрщик заполняет всю клиентскую область родительского окна.
 * 
 * @param parent Handle to the parent window / Дескриптор родительского окна
 * 
 * @note The viewer automatically registers its window class on first use
 * @note Просмотрщик автоматически регистрирует свой класс окна при первом использовании
 * 
 * @note The viewer starts a timer that monitors the current playing track
 * @note Просмотрщик запускает таймер, который мониторит текущий играющий трек
 */
void CoverView_Attach(HWND parent);

/**
 * @brief Force reload cover art for the currently playing track
 * @brief Принудительно перезагрузить обложку для текущего играющего трека
 * 
 * Queries Winamp for the current track and reloads its artwork.
 * This is useful when:
 * - The user has manually changed artwork files
 * - Tag information has been updated
 * - The viewer needs to be refreshed after configuration changes
 * 
 * Запрашивает у Winamp текущий трек и перезагружает его обложку.
 * Это полезно когда:
 * - Пользователь вручную изменил файлы обложек
 * - Информация в тегах была обновлена
 * - Просмотрщик нужно обновить после изменения конфигурации
 * 
 * @note If no track is playing, clears the current artwork
 * @note Если не играет ни один трек, очищает текущую обложку
 */
void CoverView_ReloadFromCurrent();

/**
 * @brief Find an existing cover viewer window attached to a parent
 * @brief Найти существующее окно просмотра обложек, прикреплённое к родителю
 * 
 * Searches for a cover art viewer window that is a child of the specified
 * parent window. Returns NULL if no viewer is found.
 * 
 * Ищет окно просмотра обложек, являющееся дочерним для указанного
 * родительского окна. Возвращает NULL, если просмотрщик не найден.
 * 
 * @param parent Handle to the parent window / Дескриптор родительского окна
 * @return Handle to the viewer window, or NULL if not found
 * @return Дескриптор окна просмотрщика, или NULL если не найден
 * 
 * @note This is useful for checking if a viewer already exists before creating a new one
 * @note Это полезно для проверки существования просмотрщика перед созданием нового
 */
HWND CoverView_FindOn(HWND parent);

#ifdef __cplusplus
}
#endif
