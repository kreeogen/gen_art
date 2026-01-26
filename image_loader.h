/**
 * @file image_loader.h
 * @brief Universal image loader API for Win32 applications
 * @brief Универсальный API загрузки изображений для Win32 приложений
 * 
 * This module provides a high-level interface for loading various image formats
 * (JPEG, PNG, GIF, BMP, ICO) into GDI bitmaps. It automatically detects the
 * image format and uses the most appropriate decoder (OLE IPicture or GDI+).
 * 
 * Этот модуль предоставляет высокоуровневый интерфейс для загрузки различных
 * форматов изображений (JPEG, PNG, GIF, BMP, ICO) в GDI bitmap'ы. Он автоматически
 * определяет формат изображения и использует наиболее подходящий декодер
 * (OLE IPicture или GDI+).
 * 
 * Supported formats / Поддерживаемые форматы:
 * - JPEG (.jpg, .jpeg)
 * - PNG (.png) - preferred via GDI+
 * - GIF (.gif) - including animated (first frame only)
 * - BMP (.bmp)
 * - ICO (.ico)
 * 
 * @note Dynamically loads GDI+ for better compatibility with older systems
 * @note Динамически загружает GDI+ для лучшей совместимости со старыми системами
 * 
 * @note Compatible with Windows 98/ME/2000/XP and later
 * @note Совместим с Windows 98/ME/2000/XP и более поздними версиями
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
 * @brief Load an image from memory buffer into a GDI bitmap
 * @brief Загрузить изображение из буфера памяти в GDI bitmap
 * 
 * Loads image data from a memory buffer and creates a GDI bitmap.
 * The function automatically detects the image format by examining
 * the file signature (magic bytes) and selects the appropriate decoder.
 * 
 * Загружает данные изображения из буфера памяти и создаёт GDI bitmap.
 * Функция автоматически определяет формат изображения путём анализа
 * сигнатуры файла (магических байтов) и выбирает подходящий декодер.
 * 
 * Decoder selection strategy / Стратегия выбора декодера:
 * - PNG images: Try GDI+ first, fallback to OLE IPicture
 * - Other formats: Try OLE IPicture first, fallback to GDI+
 * 
 * @param buf Pointer to image data in memory / Указатель на данные изображения в памяти
 * @param sz  Size of image data in bytes / Размер данных изображения в байтах
 * @param phbm [out] Receives handle to created bitmap / Получает дескриптор созданного bitmap'а
 * @param psz  [out] Receives bitmap dimensions in pixels / Получает размеры bitmap'а в пикселях
 * 
 * @return Non-zero on success, zero on failure / Ненулевое значение при успехе, ноль при ошибке
 * 
 * @note Maximum supported image size is 32 MB for security
 * @note Максимальный поддерживаемый размер изображения 32 МБ для безопасности
 * 
 * @note Caller is responsible for deleting the bitmap with DeleteObject()
 * @note Вызывающая сторона должна удалить bitmap через DeleteObject()
 * 
 * @note The function performs format validation before attempting to load
 * @note Функция выполняет валидацию формата перед попыткой загрузки
 */
int __cdecl Img_LoadFromMemoryToBitmap(const BYTE* buf, DWORD sz, HBITMAP* phbm, SIZE* psz);

/**
 * @brief Load an image from a file into a GDI bitmap
 * @brief Загрузить изображение из файла в GDI bitmap
 * 
 * Opens an image file, reads its contents, and creates a GDI bitmap.
 * The file is opened with FILE_SHARE_READ | FILE_SHARE_WRITE to allow
 * other applications to access it simultaneously.
 * 
 * Открывает файл изображения, читает его содержимое и создаёт GDI bitmap.
 * Файл открывается с FILE_SHARE_READ | FILE_SHARE_WRITE для разрешения
 * доступа другим приложениям одновременно.
 * 
 * @param path Path to the image file (ANSI) / Путь к файлу изображения (ANSI)
 * @param phbm [out] Receives handle to created bitmap / Получает дескриптор созданного bitmap'а
 * @param psz  [out] Receives bitmap dimensions in pixels / Получает размеры bitmap'а в пикселях
 * 
 * @return Non-zero on success, zero on failure / Ненулевое значение при успехе, ноль при ошибке
 * 
 * @note The entire file is loaded into memory before processing
 * @note Весь файл загружается в память перед обработкой
 * 
 * @note Files larger than 32 MB are rejected
 * @note Файлы размером больше 32 МБ отклоняются
 * 
 * @note Format validation is performed after reading the file
 * @note Валидация формата выполняется после чтения файла
 */
int __cdecl Img_LoadFromFileA(const char* path, HBITMAP* phbm, SIZE* psz);

/**
 * @brief Clean up image loader resources (GDI+ shutdown)
 * @brief Очистить ресурсы загрузчика изображений (завершение работы GDI+)
 * 
 * Releases all resources used by the image loader, including:
 * - GDI+ library (if loaded)
 * - GDI+ token (shutdown)
 * - Function pointers
 * 
 * Освобождает все ресурсы, используемые загрузчиком изображений, включая:
 * - Библиотеку GDI+ (если загружена)
 * - Токен GDI+ (завершение работы)
 * - Указатели на функции
 * 
 * @note This should be called when the plugin is unloaded
 * @note Это должно вызываться при выгрузке плагина
 * 
 * @note Prevents process hang on exit by properly shutting down GDI+
 * @note Предотвращает зависание процесса при выходе через корректное завершение GDI+
 * 
 * @note After calling this, the loader can be re-initialized on next use
 * @note После вызова загрузчик может быть повторно инициализирован при следующем использовании
 */
void Img_Cleanup(void);

#ifdef __cplusplus
}
#endif
