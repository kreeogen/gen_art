/**
 * @file mp4_reader.cpp
 * @brief MP4/M4A cover art extractor for media players
 * @brief Экстрактор обложек из MP4/M4A файлов для медиаплееров
 * 
 * This module extracts embedded cover artwork from MP4 container files
 * (M4A, M4B, MP4, M4V, MOV) by parsing the ISO Base Media File Format
 * structure and locating the 'covr' atom within the iTunes metadata.
 * 
 * Этот модуль извлекает встроенные обложки из файлов-контейнеров MP4
 * (M4A, M4B, MP4, M4V, MOV) путём парсинга структуры ISO Base Media File Format
 * и поиска атома 'covr' в метаданных iTunes.
 * 
 * @note File structure: ftyp → moov → udta → meta → ilst → covr → data
 * @note Структура файла: ftyp → moov → udta → meta → ilst → covr → data
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#include "mp4_reader.h"
#include "..\image_loader.h"
#include "..\utils_common.h"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

// ============================================================================
// Type Definitions / Определения типов
// ============================================================================

typedef unsigned __int64 U64;  ///< 64-bit unsigned integer for file offsets

// ============================================================================
// Helper Functions / Вспомогательные функции
// ============================================================================

/**
 * @brief Check if file has a valid MP4-related extension
 * @brief Проверка валидности расширения MP4-файла
 * 
 * @param path File path to check / Путь к файлу для проверки
 * @return TRUE if extension is .m4a, .m4b, .mp4, .m4v, or .mov
 * @return TRUE если расширение .m4a, .m4b, .mp4, .m4v или .mov
 */
static BOOL HasMp4Ext(const char* path) {
    const char* ext = PathFindExtensionA(path);
    if (!ext || !*ext) return FALSE;
    
    return lstrcmpiA(ext, ".m4a") == 0 || lstrcmpiA(ext, ".m4b") == 0 ||
           lstrcmpiA(ext, ".mp4") == 0 || lstrcmpiA(ext, ".m4v") == 0 ||
           lstrcmpiA(ext, ".mov") == 0;
}

/**
 * @brief Read and parse an MP4 box header
 * @brief Чтение и парсинг заголовка MP4 box'а
 * 
 * MP4 boxes have the following structure:
 * - 4 bytes: size (big-endian)
 * - 4 bytes: type (FourCC)
 * - If size == 1: additional 8 bytes for extended size
 * - If size == 0: box extends to end of file
 * 
 * Структура MP4 box'ов:
 * - 4 байта: размер (big-endian)
 * - 4 байта: тип (FourCC)
 * - Если size == 1: дополнительные 8 байт для расширенного размера
 * - Если size == 0: box занимает весь файл до конца
 * 
 * @param f File handle / Дескриптор файла
 * @param off Offset to box start / Смещение начала box'а
 * @param limit Maximum file position / Максимальная позиция в файле
 * @param outSize [out] Total box size including header / Полный размер box'а с заголовком
 * @param outType [out] Box type (FourCC) / Тип box'а (FourCC)
 * @param outPayloadOff [out] Offset to payload data / Смещение данных payload
 * @return TRUE on success / TRUE при успехе
 */
static BOOL ReadBoxHeader(FileHandle& f, U64 off, U64 limit, U64* outSize, DWORD* outType, U64* outPayloadOff) {
    BYTE hdr[8];
    
    // Validate we can read minimum header (8 bytes)
    // Проверяем возможность чтения минимального заголовка (8 байт)
    if (off + 8 > limit) return FALSE;
    if (!f.ReadAt(off, hdr, 8)) return FALSE;

    // Parse basic header: size (4 bytes) + type (4 bytes)
    // Парсим базовый заголовок: размер (4 байта) + тип (4 байта)
    U64 size = (U64)BE32(hdr);        // Big-endian 32-bit size
    DWORD type = BE32(hdr + 4);       // FourCC type identifier
    U64 payload = off + 8;            // Default payload offset

    // Handle extended size (size == 1 means 64-bit size follows)
    // Обработка расширенного размера (size == 1 означает 64-битный размер)
    if (size == 1) {
        BYTE ex[8];
        if (off + 16 > limit) return FALSE;
        if (!f.ReadAt(off + 8, ex, 8)) return FALSE;
        size = BE64(ex);              // Read 64-bit extended size
        payload = off + 16;           // Payload starts after extended header
    } 
    // Handle "to end of file" marker (size == 0)
    // Обработка маркера "до конца файла" (size == 0)
    else if (size == 0) {
        size = limit - off;
    }

    // Validate box size integrity
    // Проверка целостности размера box'а
    if (size < (payload - off)) return FALSE;  // Size can't be less than header
    if (off + size > limit) return FALSE;      // Box can't exceed file bounds

    // Return parsed values
    // Возврат распарсенных значений
    if (outSize) *outSize = size;
    if (outType) *outType = type;
    if (outPayloadOff) *outPayloadOff = payload;
    return TRUE;
}

/**
 * @brief Find the first occurrence of a box with specified FourCC
 * @brief Поиск первого вхождения box'а с указанным FourCC
 * 
 * Sequentially scans boxes in the specified range until finding
 * a box matching the requested FourCC type.
 * 
 * Последовательно сканирует box'ы в указанном диапазоне до нахождения
 * box'а с запрошенным типом FourCC.
 * 
 * @param f File handle / Дескриптор файла
 * @param start Start offset for search / Начальное смещение для поиска
 * @param limit End boundary for search / Конечная граница поиска
 * @param fourcc Target box type (FourCC) / Искомый тип box'а (FourCC)
 * @param outOff [out] Offset where box was found / Смещение найденного box'а
 * @param outSize [out] Size of found box / Размер найденного box'а
 * @return TRUE if box found / TRUE если box найден
 */
static BOOL FindFirstBox(FileHandle& f, U64 start, U64 limit, DWORD fourcc, U64* outOff, U64* outSize) {
    U64 pos = start;
    
    // Iterate through all boxes in range
    // Итерация по всем box'ам в диапазоне
    while (pos + 8 <= limit) {
        U64 size = 0, payload = 0;
        DWORD type = 0;
        
        // Read current box header
        // Чтение заголовка текущего box'а
        if (!ReadBoxHeader(f, pos, limit, &size, &type, &payload)) return FALSE;
        
        // Check if this is the box we're looking for
        // Проверка, является ли это искомым box'ом
        if (type == fourcc) {
            if (outOff) *outOff = pos;
            if (outSize) *outSize = size;
            return TRUE;
        }
        
        // Move to next box
        // Переход к следующему box'у
        pos += size;
    }
    
    return FALSE;  // Box not found / Box не найден
}

/**
 * @brief Find a child box within a parent box
 * @brief Поиск дочернего box'а внутри родительского
 * 
 * Searches for a child box within the payload of a parent box.
 * Special handling for 'meta' boxes which have 4 bytes of version/flags
 * before the child boxes begin.
 * 
 * Ищет дочерний box внутри payload родительского box'а.
 * Специальная обработка для 'meta' box'ов, у которых есть 4 байта
 * версии/флагов перед началом дочерних box'ов.
 * 
 * @param f File handle / Дескриптор файла
 * @param parentOff Parent box offset / Смещение родительского box'а
 * @param parentSize Parent box size / Размер родительского box'а
 * @param childFCC Child box FourCC to find / FourCC искомого дочернего box'а
 * @param outOff [out] Offset of found child box / Смещение найденного дочернего box'а
 * @param outSize [out] Size of found child box / Размер найденного дочернего box'а
 * @return TRUE if child box found / TRUE если дочерний box найден
 */
static BOOL FindChildBox(FileHandle& f, U64 parentOff, U64 parentSize, DWORD childFCC, U64* outOff, U64* outSize) {
    U64 pLimit = parentOff + parentSize;
    U64 payload = 0, tmp = 0; 
    DWORD pType = 0;

    // Read parent box header to get payload offset
    // Чтение заголовка родительского box'а для получения смещения payload
    if (!ReadBoxHeader(f, parentOff, pLimit, &tmp, &pType, &payload)) return FALSE;

    // Special case: 'meta' box has 4 bytes (version + flags) before children
    // Специальный случай: 'meta' box имеет 4 байта (версия + флаги) перед потомками
    if (pType == FCC('m', 'e', 't', 'a')) {
        if (payload + 4 > pLimit) return FALSE;
        payload += 4;  // Skip version (1 byte) + flags (3 bytes)
    }
    
    // Search for child box in parent's payload
    // Поиск дочернего box'а в payload родителя
    return FindFirstBox(f, payload, pLimit, childFCC, outOff, outSize);
}

// ============================================================================
// Public API / Публичный API
// ============================================================================

/**
 * @brief Extract cover art from MP4 file and load it as a bitmap
 * @brief Извлечение обложки из MP4 файла и загрузка в виде bitmap'а
 * 
 * This function navigates the MP4 box hierarchy to locate embedded cover art:
 * 1. Validates file type (ftyp box)
 * 2. Finds movie metadata (moov → udta → meta → ilst)
 * 3. Locates cover art (covr box)
 * 4. Extracts image data from 'data' box
 * 5. Loads image into GDI bitmap
 * 
 * Функция проходит по иерархии MP4 box'ов для поиска встроенной обложки:
 * 1. Проверяет тип файла (ftyp box)
 * 2. Находит метаданные фильма (moov → udta → meta → ilst)
 * 3. Обнаруживает обложку (covr box)
 * 4. Извлекает данные изображения из 'data' box'а
 * 5. Загружает изображение в GDI bitmap
 * 
 * @param path Path to MP4/M4A file / Путь к MP4/M4A файлу
 * @param phbm [out] Handle to loaded bitmap / Дескриптор загруженного bitmap'а
 * @param psz [out] Bitmap dimensions / Размеры bitmap'а
 * @return TRUE on success, FALSE on failure / TRUE при успехе, FALSE при ошибке
 * 
 * @note Caller is responsible for deleting the bitmap with DeleteObject()
 * @note Вызывающая сторона должна удалить bitmap через DeleteObject()
 * 
 * @note Maximum image size is 32 MB for security
 * @note Максимальный размер изображения 32 МБ для безопасности
 */
extern "C" BOOL __cdecl MP4_LoadCoverToBitmapA(const char* path, HBITMAP* phbm, SIZE* psz) {
    // Validate input parameters
    // Проверка входных параметров
    if (!path || !*path || !HasMp4Ext(path)) return FALSE;

    // Open file using RAII wrapper (automatic cleanup on scope exit)
    // Открытие файла через RAII-обёртку (автоматическая очистка при выходе из scope)
    FileHandle f(path);
    if (!f.IsValid()) return FALSE;

    U64 fileLimit = (U64)f.GetSize();
    if (fileLimit < 16) return FALSE;  // Minimum valid MP4 size

    // Declare variables for box offsets and sizes
    // Объявление переменных для смещений и размеров box'ов
    U64 ftypOff, ftypSz, moovOff, moovSz, udtaOff, udtaSz;
    U64 metaOff, metaSz, ilstOff, ilstSz, covrOff, covrSz;

    // Navigate through MP4 box hierarchy (chain of responsibility pattern)
    // Навигация по иерархии MP4 box'ов (паттерн цепочки обязанностей)
    // If any box is not found, entire chain fails and function returns FALSE
    // Если любой box не найден, вся цепочка прерывается и функция возвращает FALSE
    
    // 1. Validate file type box (ftyp) - identifies file as MP4
    // 1. Проверка типа файла (ftyp) - идентифицирует файл как MP4
    if (!FindFirstBox(f, 0, fileLimit, FCC('f', 't', 'y', 'p'), &ftypOff, &ftypSz)) return FALSE;
    
    // 2. Find movie metadata container (moov)
    // 2. Поиск контейнера метаданных фильма (moov)
    if (!FindFirstBox(f, 0, fileLimit, FCC('m', 'o', 'o', 'v'), &moovOff, &moovSz)) return FALSE;
    
    // 3. Find user data box (udta) - contains custom metadata
    // 3. Поиск box'а пользовательских данных (udta) - содержит кастомные метаданные
    if (!FindChildBox(f, moovOff, moovSz, FCC('u', 'd', 't', 'a'), &udtaOff, &udtaSz)) return FALSE;
    
    // 4. Find metadata box (meta) - iTunes/QuickTime metadata
    // 4. Поиск box'а метаданных (meta) - метаданные iTunes/QuickTime
    if (!FindChildBox(f, udtaOff, udtaSz, FCC('m', 'e', 't', 'a'), &metaOff, &metaSz)) return FALSE;
    
    // 5. Find item list box (ilst) - contains metadata items
    // 5. Поиск списка элементов (ilst) - содержит элементы метаданных
    if (!FindChildBox(f, metaOff, metaSz, FCC('i', 'l', 's', 't'), &ilstOff, &ilstSz)) return FALSE;
    
    // 6. Find cover art box (covr) - contains embedded artwork
    // 6. Поиск box'а обложки (covr) - содержит встроенную обложку
    if (!FindChildBox(f, ilstOff, ilstSz, FCC('c', 'o', 'v', 'r'), &covrOff, &covrSz)) return FALSE;

    // Parse cover art box to find image data
    // Парсинг box'а обложки для поиска данных изображения
    U64 covrLimit = covrOff + covrSz;
    U64 hdrSz = 0, payload = 0; 
    DWORD type = 0;
    
    // Read covr box header
    // Чтение заголовка covr box'а
    if (!ReadBoxHeader(f, covrOff, covrLimit, &hdrSz, &type, &payload)) return FALSE;
    
    // Search for 'data' box containing actual image bytes
    // Поиск 'data' box'а, содержащего фактические байты изображения
    U64 pos = payload;
    while (pos + 8 <= covrLimit) {
        U64 dSz = 0, dPay = 0; 
        DWORD dType = 0;
        
        // Read potential data box header
        // Чтение заголовка потенциального data box'а
        if (!ReadBoxHeader(f, pos, covrLimit, &dSz, &dType, &dPay)) break;

        // Check if this is a 'data' box
        // Проверка, является ли это 'data' box'ом
        if (dType == FCC('d', 'a', 't', 'a')) {
            // 'data' box structure:
            // - 4 bytes: type indicator (0x00 = reserved, 0x0D = JPEG, 0x0E = PNG)
            // - 4 bytes: locale indicator (usually 0)
            // - N bytes: actual image data
            //
            // Структура 'data' box'а:
            // - 4 байта: индикатор типа (0x00 = зарезервировано, 0x0D = JPEG, 0x0E = PNG)
            // - 4 байта: индикатор локали (обычно 0)
            // - N байт: фактические данные изображения
            
            if (dPay + 8 <= pos + dSz) {
                // Skip 8 bytes (Type + Locale indicators)
                // Пропуск 8 байт (индикаторы Type + Locale)
                U64 imgOff = dPay + 8;
                U64 imgLen = (pos + dSz) - imgOff;

                // Validate image size (prevent memory exhaustion attacks)
                // Проверка размера изображения (защита от атак на память)
                if (imgLen > 0 && imgLen < (32 * 1024 * 1024)) {  // Max 32 MB
                    // Allocate buffer for image data
                    // Выделение буфера для данных изображения
                    BYTE* buf = (BYTE*)GlobalAlloc(GMEM_FIXED, (SIZE_T)imgLen);
                    if (buf) {
                        // Read image data from file
                        // Чтение данных изображения из файла
                        if (f.ReadAt(imgOff, buf, (DWORD)imgLen)) {
                            // Attempt to load image (supports JPEG, PNG, BMP, etc.)
                            // Попытка загрузить изображение (поддержка JPEG, PNG, BMP и др.)
                            int ok = Img_LoadFromMemoryToBitmap(buf, (DWORD)imgLen, phbm, psz);
                            GlobalFree(buf);
                            if (ok) return TRUE;  // Success! / Успех!
                        } else {
                            GlobalFree(buf);
                        }
                    }
                }
            }
        }
        
        // Move to next box
        // Переход к следующему box'у
        pos += dSz;
    }
    
    // No valid cover art found
    // Валидная обложка не найдена
    return FALSE;
}
