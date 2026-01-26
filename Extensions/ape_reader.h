/**
 * @file ape_reader.h
 * @brief APEv2 embedded cover art extractor interface
 * @brief Интерфейс экстрактора встроенных обложек APEv2
 * 
 * This module extracts embedded cover artwork from APE (Monkey's Audio) files
 * that contain APEv2 tags. APEv2 is a flexible tagging format also used by
 * MP3, WavPack, OptimFROG, and other audio formats.
 * 
 * Этот модуль извлекает встроенные обложки из APE (Monkey's Audio) файлов,
 * содержащих APEv2 теги. APEv2 - это гибкий формат тегов, также используемый
 * MP3, WavPack, OptimFROG и другими аудиоформатами.
 * 
 * APEv2 Tag Structure / Структура APEv2 тега:
 * - Usually located at end of file (footer-only or header+footer)
 * - Signature: "APETAGEX" (8 bytes)
 * - Contains key-value items
 * - Picture items have format: filename\0<binary image data>
 * 
 * - Обычно расположен в конце файла (только footer или header+footer)
 * - Сигнатура: "APETAGEX" (8 байт)
 * - Содержит элементы ключ-значение
 * - Элементы изображений имеют формат: filename\0<бинарные данные изображения>
 * 
 * Picture Key Recognition / Распознавание ключей изображений:
 * Keys are case-insensitive and may contain:
 * - "cover" or "picture" → recognized as image
 * - "front" → front cover (highest priority)
 * - "back" → back cover (lower priority)
 * 
 * Ключи нечувствительны к регистру и могут содержать:
 * - "cover" или "picture" → распознаются как изображение
 * - "front" → передняя обложка (высший приоритет)
 * - "back" → задняя обложка (низший приоритет)
 * 
 * Selection Priority / Приоритет выбора:
 * 1. Front cover (rank 0) - most preferred
 * 2. Generic cover/picture (rank 1) - preferred
 * 3. Back cover (rank 2) - fallback
 * 
 * 1. Передняя обложка (ранг 0) - наиболее предпочтительна
 * 2. Общая обложка/изображение (ранг 1) - предпочтительна
 * 3. Задняя обложка (ранг 2) - запасной вариант
 * 
 * @note Scans last 4KB of file for footer (performance optimization)
 * @note Сканирует последние 4KB файла для поиска footer (оптимизация производительности)
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
 * @brief Extract cover art from APE/APEv2 tagged file
 * @brief Извлечь обложку из файла с APE/APEv2 тегами
 * 
 * Searches for APEv2 tag footer at end of file, reads the tag data,
 * and extracts embedded cover artwork. Handles various picture key
 * formats and prioritizes front cover images.
 * 
 * Ищет footer APEv2 тега в конце файла, читает данные тега
 * и извлекает встроенную обложку. Обрабатывает различные форматы
 * ключей изображений и приоритизирует изображения передней обложки.
 * 
 * Algorithm / Алгоритм:
 * 1. Scan last 4KB of file for "APETAGEX" signature
 * 2. Read tag size and locate tag start
 * 3. Parse key-value items
 * 4. Identify picture items by key names (cover, picture, front, back)
 * 5. Extract binary image data (skip filename)
 * 6. Select best picture based on priority ranking
 * 7. Load image to bitmap
 * 
 * 1. Сканировать последние 4KB файла для поиска сигнатуры "APETAGEX"
 * 2. Прочитать размер тега и найти начало тега
 * 3. Парсить элементы ключ-значение
 * 4. Идентифицировать элементы изображений по именам ключей (cover, picture, front, back)
 * 5. Извлечь бинарные данные изображения (пропустить имя файла)
 * 6. Выбрать лучшее изображение на основе ранжирования приоритета
 * 7. Загрузить изображение в bitmap
 * 
 * @param audioPath Path to audio file with APEv2 tags / Путь к аудиофайлу с APEv2 тегами
 * @param phbm [out] Receives handle to loaded bitmap / Получает дескриптор загруженного bitmap'а
 * @param psz [out] Receives bitmap dimensions / Получает размеры bitmap'а
 * 
 * @return TRUE if cover art was found and loaded / TRUE если обложка найдена и загружена
 * @return FALSE if no cover art or file invalid / FALSE если нет обложки или файл некорректен
 * 
 * @note Caller must delete bitmap with DeleteObject() when done
 * @note Вызывающая сторона должна удалить bitmap через DeleteObject() после использования
 * 
 * @note If multiple pictures exist, returns the highest priority one
 * @note Если существует несколько изображений, возвращает наивысший приоритет
 * 
 * @note Key comparison is case-insensitive
 * @note Сравнение ключей нечувствительно к регистру
 * 
 * Example tag item format / Пример формата элемента тега:
 * - Key: "Cover Art (front)"
 * - Value: "cover.jpg\0<JPEG binary data>"
 */
BOOL __cdecl APE_LoadCoverToBitmapA(const char* audioPath, HBITMAP* phbm, SIZE* psz);

#ifdef __cplusplus
}
#endif
