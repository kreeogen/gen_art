/**
 * @file mp4_reader.h
 * @brief MP4/M4A Embedded Cover Art Extractor
 * @brief Экстрактор встроенных обложек MP4/M4A
 * * Designed for iTunes-style metadata (covr atom) within ISO Base Media File Format.
 * * Предназначен для метаданных в стиле iTunes (атом covr) внутри формата ISO Base Media File Format.
 * * Supported Containers / Поддерживаемые контейнеры:
 * - .m4a, .m4b (Audiobooks), .mp4, .m4v, .mov
 * * @author [Your Name]
 * @version 1.0
 */

#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load cover art from MP4 file
 * @brief Загрузить обложку из MP4 файла
 * * Navigates atom tree: moov -> udta -> meta -> ilst -> covr -> data
 * * @param audioPath Path to MP4 file / Путь к MP4 файлу
 * @param phbm [out] Result bitmap / Результирующий bitmap
 * @param psz [out] Result dimensions / Результирующие размеры
 * @return TRUE on success / TRUE при успехе
 */
BOOL __cdecl MP4_LoadCoverToBitmapA(const char* audioPath, HBITMAP* phbm, SIZE* psz);

#ifdef __cplusplus
}
#endif