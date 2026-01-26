/**
 * @file flac_reader.h
 * @brief FLAC embedded cover art extractor interface
 * @brief Интерфейс экстрактора встроенных обложек FLAC
 * 
 * This module extracts embedded cover artwork from FLAC audio files.
 * FLAC files can contain multiple pictures in metadata blocks (PICTURE type),
 * including front covers, back covers, artist photos, etc.
 * 
 * Этот модуль извлекает встроенные обложки из FLAC аудиофайлов.
 * FLAC файлы могут содержать несколько изображений в блоках метаданных (тип PICTURE),
 * включая передние обложки, задние обложки, фотографии исполнителей и т.д.
 * 
 * FLAC Picture Block Structure / Структура FLAC блока Picture:
 * - Picture type (4 bytes): 0=Other, 1=Icon, 3=Front cover, 4=Back cover, etc.
 * - MIME type length + string
 * - Description length + string  
 * - Width, Height, Depth, Colors (16 bytes total)
 * - Picture data length + actual image bytes
 * 
 * Selection Priority / Приоритет выбора:
 * 1. Front cover (type 3) - preferred
 * 2. Any other picture type - fallback
 * 
 * 1. Передняя обложка (тип 3) - предпочтительно
 * 2. Любой другой тип изображения - запасной вариант
 * 
 * Supported Image Formats / Поддерживаемые форматы изображений:
 * - JPEG, PNG, GIF, BMP (via image_loader module)
 * 
 * @note Handles FLAC files with or without leading ID3v2 tags
 * @note Обрабатывает FLAC файлы с или без ведущих ID3v2 тегов
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#ifndef FLAC_READER_H
#define FLAC_READER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extract cover art from FLAC file
 * @brief Извлечь обложку из FLAC файла
 * 
 * Parses FLAC metadata blocks to find embedded picture data.
 * Prioritizes front cover images (type 3) over other picture types.
 * Handles files with ID3v2 tags before the FLAC signature.
 * 
 * Парсит блоки метаданных FLAC для поиска встроенных изображений.
 * Приоритизирует изображения передней обложки (тип 3) над другими типами изображений.
 * Обрабатывает файлы с ID3v2 тегами перед сигнатурой FLAC.
 * 
 * Algorithm / Алгоритм:
 * 1. Skip ID3v2 tag if present / Пропустить ID3v2 тег если есть
 * 2. Verify "fLaC" signature / Проверить сигнатуру "fLaC"
 * 3. Iterate through metadata blocks / Итерироваться по блокам метаданных
 * 4. Find PICTURE blocks (type 6) / Найти PICTURE блоки (тип 6)
 * 5. Extract image data and load to bitmap / Извлечь данные изображения и загрузить в bitmap
 * 6. Prefer front cover, fallback to any picture / Предпочесть переднюю обложку, запасной вариант любое изображение
 * 
 * @param audioPath Path to FLAC file / Путь к FLAC файлу
 * @param phbm [out] Receives handle to loaded bitmap / Получает дескриптор загруженного bitmap'а
 * @param psz [out] Receives bitmap dimensions / Получает размеры bitmap'а
 * 
 * @return TRUE if cover art was found and loaded / TRUE если обложка найдена и загружена
 * @return FALSE if no cover art or file invalid / FALSE если нет обложки или файл некорректен
 * 
 * @note Caller must delete bitmap with DeleteObject() when done
 * @note Вызывающая сторона должна удалить bitmap через DeleteObject() после использования
 * 
 * @note Maximum block size is 16 MB for security
 * @note Максимальный размер блока 16 МБ для безопасности
 * 
 * @note Sets phbm to NULL and psz to {0,0} on failure
 * @note Устанавливает phbm в NULL и psz в {0,0} при ошибке
 */
BOOL FLAC_LoadCoverToBitmapA(const char* audioPath, HBITMAP* phbm, SIZE* psz);

#ifdef __cplusplus
}
#endif

#endif // FLAC_READER_H
