/**
 * @file ape_reader.cpp
 * @brief APEv2 Tag Parser Implementation
 * @brief Реализация парсера тегов APEv2
 * * Optimized to scan file footer without reading the entire file.
 * Оптимизирован для сканирования конца файла без чтения всего файла.
 */

#include "ape_reader.h"
#include "..\image_loader.h"
#include "..\utils_common.h"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

// ============================================================================
// Tag Location Structure / Структура расположения тега
// ============================================================================
typedef struct {
  DWORD absStart;   ///< Absolute offset to tag start / Абсолютное смещение начала тега
  DWORD absFooter;  ///< Absolute offset to tag footer / Абсолютное смещение footer'а
  DWORD totalSize;  ///< Total tag size including header/footer / Полный размер тега
} ApeLoc;

// ============================================================================
// Helper Functions / Вспомогательные функции
// ============================================================================

/**
 * @brief Scan file end for APEv2 footer
 * @brief Сканирование конца файла на наличие APEv2 footer'а
 * * Reads last 4KB of file to find "APETAGEX" signature.
 * Читает последние 4КБ файла для поиска сигнатуры "APETAGEX".
 */
static BOOL Ape_ScanFooter(FileHandle& f, ApeLoc* out) {
    DWORD fSize = f.GetSize();
    if (fSize < 32) return FALSE;

    // Read last 1KB-4KB (usually enough)
    // Читаем последний 1КБ-4КБ (обычно достаточно)
    DWORD scanSize = (fSize > 4096) ? 4096 : fSize;
    BYTE* buf = (BYTE*)GlobalAlloc(GMEM_FIXED, scanSize);
    if (!buf) return FALSE;

    if (!f.ReadAt(fSize - scanSize, buf, scanSize)) {
        GlobalFree(buf); return FALSE;
    }

    // Search for "APETAGEX" backwards
    // Ищем "APETAGEX" с конца
    for (int i = (int)scanSize - 32; i >= 0; --i) {
        BYTE* p = &buf[i];
        if (memcmp(p, "APETAGEX", 8) == 0) {
            DWORD size = LE32(p + 12); // Tag Size (including footer)
            // Validate size
            // Валидация размера
            if (size >= 32 && size <= fSize) {
                out->absFooter = (fSize - scanSize) + i;
                out->totalSize = size;
                out->absStart = out->absFooter + 32 - size;
                GlobalFree(buf);
                return TRUE;
            }
        }
    }
    GlobalFree(buf);
    return FALSE;
}

/**
 * @brief Parse APEv2 items to find cover art
 * @brief Парсинг элементов APEv2 для поиска обложки
 * * Implements priority logic: Front > Cover > Back.
 * Реализует логику приоритетов: Front > Cover > Back.
 */
static BOOL Ape_ParseItems(const BYTE* data, DWORD size, HBITMAP* phbm, SIZE* psz) {
    DWORD pos = 0;
    // Skip Header if present ("APETAGEX" at start)
    // Пропуск заголовка если есть ("APETAGEX" в начале)
    if (size >= 32 && memcmp(data, "APETAGEX", 8) == 0) pos = 32;

    const BYTE* bestData = NULL; 
    DWORD bestSize = 0; 
    int bestRank = 99; // Lower is better / Чем меньше, тем лучше

    while (pos + 8 <= size) {
        DWORD valSize = LE32(data + pos);
        // DWORD flags = LE32(data + pos + 4); // Flags unused / Флаги не используются
        pos += 8;

        DWORD keyStart = pos;
        while (pos < size && data[pos] != 0) ++pos; // Find Key null-terminator
        if (pos >= size) break;
        
        const char* key = (const char*)(data + keyStart);
        pos++; // Skip 0x00

        if (pos + valSize > size) break;
        const BYTE* val = data + pos;

        // Normalize key for comparison
        // Нормализация ключа для сравнения
        char norm[64];
        int o = 0;
        for (int i=0; key[i] && o < 63; ++i) {
            char c = key[i];
            if (c >= 'A' && c <= 'Z') c += 32; // ToLower
            if (c >= 'a' && c <= 'z') norm[o++] = c;
        }
        norm[o] = 0;

        // Determine rank
        // Определение ранга
        int rank = -1;
        if (StrStrIA(norm, "cover") || StrStrIA(norm, "picture")) {
            rank = 1; // Generic
            if (StrStrIA(norm, "front")) rank = 0; // Best
            else if (StrStrIA(norm, "back")) rank = 2; // Fallback
        }

        if (rank >= 0) {
            // Find binary data after filename (APE stores images as Name\0Data)
            // Поиск бинарных данных после имени файла
            DWORD p = 0;
            while (p < valSize && val[p] != 0) ++p;
            
            const BYTE* img = (p < valSize) ? (val + p + 1) : val;
            DWORD imgLen = (p < valSize) ? (valSize - p - 1) : valSize;

            if (imgLen > 0) {
                // Update if better rank or same rank but larger size (heuristic)
                // Обновляем если ранг лучше или ранг тот же, но размер больше
                if (!bestData || rank < bestRank || (rank == bestRank && imgLen > bestSize)) {
                    bestData = img; bestSize = imgLen; bestRank = rank;
                }
            }
        }
        pos += valSize;
    }

    if (bestData) return Img_LoadFromMemoryToBitmap(bestData, bestSize, phbm, psz);
    return FALSE;
}

// ============================================================================
// Public API
// ============================================================================

extern "C" BOOL __cdecl APE_LoadCoverToBitmapA(const char* path, HBITMAP* phbm, SIZE* psz) {
    FileHandle f(path);
    if (!f.IsValid()) return FALSE;

    ApeLoc loc = {0};
    if (Ape_ScanFooter(f, &loc)) {
        DWORD dataSize = loc.totalSize - 32; // Exclude Footer size from data read
        if (loc.totalSize < 32) return FALSE;

        BYTE* buf = (BYTE*)GlobalAlloc(GMEM_FIXED, dataSize);
        if (buf) {
            if (f.ReadAt(loc.absStart, buf, dataSize)) {
                BOOL ok = Ape_ParseItems(buf, dataSize, phbm, psz);
                GlobalFree(buf);
                return ok;
            }
            GlobalFree(buf);
        }
    }
    return FALSE;
}