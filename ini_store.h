/**
 * @file ini_store.h
 * @brief Plugin configuration storage interface
 * @brief Интерфейс хранения конфигурации плагина
 * 
 * This module provides simple INI-based configuration storage for plugin settings.
 * It stores window position, size, and visibility state in a "plugin.ini" file
 * located in the same directory as the plugin DLL.
 * 
 * Этот модуль предоставляет простое INI-based хранилище конфигурации для настроек плагина.
 * Он сохраняет позицию окна, размер и состояние видимости в файле "plugin.ini",
 * расположенном в той же директории, что и DLL плагина.
 * 
 * INI file structure / Структура INI файла:
 * [Album Art]
 * x=100        ; Window X position / Позиция окна X
 * y=100        ; Window Y position / Позиция окна Y
 * w=300        ; Window width / Ширина окна
 * h=300        ; Window height / Высота окна
 * open=1       ; Window open state (0=closed, 1=open) / Состояние окна (0=закрыто, 1=открыто)
 * 
 * @note Uses Windows API GetPrivateProfileInt/WritePrivateProfileString
 * @note Использует Windows API GetPrivateProfileInt/WritePrivateProfileString
 * 
 * @note Unicode/ANSI compatible through TCHAR macros
 * @note Совместим с Unicode/ANSI через макросы TCHAR
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#pragma once
#include <windows.h>

/**
 * @brief Load saved window position and size from configuration
 * @brief Загрузить сохранённую позицию и размер окна из конфигурации
 * 
 * Attempts to read window geometry from the INI file.
 * Returns false if this is the first run (no saved configuration).
 * 
 * Пытается прочитать геометрию окна из INI файла.
 * Возвращает false если это первый запуск (нет сохранённой конфигурации).
 * 
 * @param x [out] Window X position in screen coordinates / Позиция окна X в экранных координатах
 * @param y [out] Window Y position in screen coordinates / Позиция окна Y в экранных координатах
 * @param w [out] Window width in pixels / Ширина окна в пикселях
 * @param h [out] Window height in pixels / Высота окна в пикселях
 * 
 * @return true if valid configuration was loaded, false if using defaults
 * @return true если загружена валидная конфигурация, false если используются значения по умолчанию
 * 
 * @note Returns -1 for all values if no configuration exists
 * @note Возвращает -1 для всех значений, если конфигурация не существует
 * 
 * @note Caller should validate that loaded position is within screen bounds
 * @note Вызывающая сторона должна проверить, что загруженная позиция находится в пределах экрана
 */
bool Ini_LoadWindowPos(int& x, int& y, int& w, int& h);

/**
 * @brief Save current window position and size to configuration
 * @brief Сохранить текущую позицию и размер окна в конфигурацию
 * 
 * Writes window geometry to the INI file for persistence across sessions.
 * Invalid coordinates (-1) are not saved to avoid corrupting the configuration.
 * 
 * Записывает геометрию окна в INI файл для сохранения между сеансами.
 * Некорректные координаты (-1) не сохраняются во избежание повреждения конфигурации.
 * 
 * @param x Window X position in screen coordinates / Позиция окна X в экранных координатах
 * @param y Window Y position in screen coordinates / Позиция окна Y в экранных координатах
 * @param w Window width in pixels / Ширина окна в пикселях
 * @param h Window height in pixels / Высота окна в пикселях
 * 
 * @note Automatically creates the INI file if it doesn't exist
 * @note Автоматически создаёт INI файл, если он не существует
 * 
 * @note Silently returns if coordinates are invalid (-1)
 * @note Тихо возвращается, если координаты некорректны (-1)
 */
void Ini_SaveWindowPos(int x, int y, int w, int h);

/**
 * @brief Load saved window visibility state from configuration
 * @brief Загрузить сохранённое состояние видимости окна из конфигурации
 * 
 * Reads whether the window should be open on plugin startup.
 * This allows the plugin to restore the window state from the last session.
 * 
 * Читает, должно ли окно быть открыто при запуске плагина.
 * Это позволяет плагину восстановить состояние окна из последнего сеанса.
 * 
 * @param isOpen [out] Window state (0=closed, 1=open) / Состояние окна (0=закрыто, 1=открыто)
 * 
 * @return true if valid state was loaded, false if using default
 * @return true если загружено валидное состояние, false если используется значение по умолчанию
 * 
 * @note Values > 1 are normalized to 1 for safety
 * @note Значения > 1 нормализуются до 1 для безопасности
 * 
 * @note Returns -1 in isOpen if no configuration exists
 * @note Возвращает -1 в isOpen, если конфигурация не существует
 */
bool Ini_LoadWindowOpen(int& isOpen);

/**
 * @brief Save current window visibility state to configuration
 * @brief Сохранить текущее состояние видимости окна в конфигурацию
 * 
 * Writes window visibility state to the INI file.
 * The state is normalized to 0 or 1 before saving.
 * 
 * Записывает состояние видимости окна в INI файл.
 * Состояние нормализуется до 0 или 1 перед сохранением.
 * 
 * @param isOpen Window state (0=closed, 1=open, any non-zero = open)
 * @param isOpen Состояние окна (0=закрыто, 1=открыто, любое ненулевое = открыто)
 * 
 * @note Automatically creates the INI file if it doesn't exist
 * @note Автоматически создаёт INI файл, если он не существует
 * 
 * @note Non-zero values are converted to 1 before saving
 * @note Ненулевые значения конвертируются в 1 перед сохранением
 */
void Ini_SaveWindowOpen(int isOpen);
