/**
 * @file id3v2_reader.h
 * @brief ID3v2 embedded cover art extractor interface
 * @brief Интерфейс экстрактора встроенных обложек ID3v2
 * * This module extracts embedded album art from MP3 files (and others supporting ID3v2).
 * Supports ID3v2.2, ID3v2.3, and ID3v2.4 versions.
 * * Этот модуль извлекает встроенные обложки из MP3 файлов (и других с поддержкой ID3v2).
 * Поддерживает версии ID3v2.2, ID3v2.3 и ID3v2.4.
 * * Supported Frames / Поддерживаемые фреймы:
 * - PIC (ID3v2.2)
 * - APIC (ID3v2.3, ID3v2.4)
 * * Logic handles / Логика обрабатывает:
 * - Unsynchronization (SyncSafe integers)
 * - Extended headers
 * - Text encoding byte skipping (ISO-8859-1, UTF-16, UTF-16BE, UTF-8)
 * * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extract cover art from ID3v2 tagged file
 * @brief Извлечь обложку из файла с ID3v2 тегами
 * * Parses the ID3v2 header and iterates through frames to find the attached picture.
 * * Парсит заголовок ID3v2 и итерируется по фреймам для поиска прикрепленного изображения.
 * * @param audioPath Path to the audio file / Путь к аудиофайлу
 * @param phbm [out] Receives handle to loaded bitmap / Получает дескриптор загруженного bitmap'а
 * @param psz [out] Receives bitmap dimensions / Получает размеры bitmap'а
 * * @return TRUE if cover found and loaded / TRUE если обложка найдена и загружена
 */
BOOL ID3v2_LoadCoverToBitmapA(const char* audioPath, HBITMAP* phbm, SIZE* psz);

#ifdef __cplusplus
}
#endif