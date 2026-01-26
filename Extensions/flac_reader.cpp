/**
 * @file flac_reader.cpp
 * @brief FLAC Metadata Parser Implementation
 * @brief Реализация парсера метаданных FLAC
 * * NOTE: C++11 lambdas removed for compatibility with legacy compilers (VS2003/VC6).
 * Helper class FLAC_BlockReader used instead.
 * * ПРИМЕЧАНИЕ: Лямбды C++11 удалены для совместимости со старыми компиляторами.
 * Вместо них используется вспомогательный класс FLAC_BlockReader.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "flac_reader.h"
#include "..\image_loader.h"
#include "..\utils_common.h"

// ============================================================================
// Helper Class (Replaces Lambdas) / Вспомогательный класс (Вместо лямбд)
// ============================================================================

class FLAC_BlockReader {
    BYTE* _p;
    BYTE* _end;
public:
    FLAC_BlockReader(BYTE* ptr, DWORD len) {
        _p = ptr;
        _end = ptr + len;
    }

    // Check bounds and safely skip bytes
    // Проверка границ и безопасный сдвиг указателя
    BOOL SafeSkip(DWORD bytes) {
        if (_p + bytes > _end) return FALSE;
        _p += bytes;
        return TRUE;
    }

    // Read 32-bit Big-Endian integer and advance
    // Чтение 32-битного числа Big-Endian и сдвиг
    DWORD ReadU32() {
        if (_p + 4 > _end) return 0;
        DWORD v = BE32(_p);
        _p += 4;
        return v;
    }

    // Get current pointer
    // Получить текущий указатель
    BYTE* Current() const { return _p; }
    
    // Check if N bytes are available
    // Проверить наличие N байт
    BOOL HasBytes(DWORD n) const { return (_p + n <= _end); }
};

// ============================================================================
// Main Function / Главная функция
// ============================================================================

BOOL FLAC_LoadCoverToBitmapA(const char* audioPath, HBITMAP* phbm, SIZE* psz)
{
    if (phbm) *phbm = NULL;
    if (psz) { psz->cx = 0; psz->cy = 0; }

    FileHandle f(audioPath);
    if (!f.IsValid()) return FALSE;

    // 1. Skip ID3v2 if present at start
    // 1. Пропуск ID3v2, если есть в начале
    BYTE probe[10];
    if (f.Read(probe, 10)) {
        if (probe[0] == 'I' && probe[1] == 'D' && probe[2] == '3') {
            DWORD id3size = SyncSafeToInt(&probe[6]);
            f.Seek(id3size, FILE_CURRENT); // Jump over tag / Прыгаем через тег
        } else {
            f.Seek(0, FILE_BEGIN); // Rewind / Возвращаемся в начало
        }
    }

    // 2. Check FLAC signature
    // 2. Проверка сигнатуры FLAC
    BYTE sig[4];
    if (!f.Read(sig, 4) || memcmp(sig, "fLaC", 4) != 0) return FALSE;

    HBITMAP hbmFallback = NULL;
    SIZE szFallback = {0, 0};
    const DWORD kMaxBlock = 16 * 1024 * 1024; // 16MB limit per block / Лимит 16МБ на блок

    // 3. Iterate Metadata Blocks
    // 3. Итерация по блокам метаданных
    for (;;) {
        // Read block header (4 bytes)
        // Читаем заголовок блока (4 байта)
        BYTE hdr[4];
        if (!f.Read(hdr, 4)) break;

        BOOL isLast = (hdr[0] & 0x80) != 0;
        BYTE type = (hdr[0] & 0x7F);
        DWORD length = BE24(&hdr[1]);

        // We only care about type 6 (PICTURE)
        // Нас интересует только тип 6 (PICTURE)
        if (type != 6 || length > kMaxBlock) {
            f.Seek(length, FILE_CURRENT); // Skip block / Пропускаем блок
            if (isLast) break;
            continue;
        }

        // Read Picture block body
        // Читаем тело блока Picture
        BYTE* buf = (BYTE*)GlobalAlloc(GMEM_FIXED, length);
        if (!buf) break;

        if (f.Read(buf, length)) {
            // Use helper for parsing
            // Используем помощник для парсинга
            FLAC_BlockReader reader(buf, length);

            DWORD picType = reader.ReadU32(); // Picture Type (3=Cover)
            DWORD mimeLen = reader.ReadU32(); // MIME Length
            
            if (reader.SafeSkip(mimeLen)) {   // Skip MIME string
                DWORD descLen = reader.ReadU32(); // Description Length
                if (reader.SafeSkip(descLen)) {   // Skip Description
                     // Skip Width(4)+Height(4)+Depth(4)+Colors(4) = 16 bytes
                    if (reader.SafeSkip(16)) { 
                        DWORD dataLen = reader.ReadU32(); // Image Data Length
                        
                        // Check if data is physically in buffer
                        // Проверка наличия данных в буфере
                        if (reader.HasBytes(dataLen)) {
                            BYTE* pData = reader.Current();
                            
                            HBITMAP hb = NULL; SIZE s = {0};
                            if (Img_LoadFromMemoryToBitmap(pData, dataLen, &hb, &s)) {
                                // Priority: Front Cover (Type 3)
                                // Приоритет: Передняя обложка (Тип 3)
                                if (picType == 3) {
                                    if (phbm) *phbm = hb;
                                    if (psz) *psz = s;
                                    if (hbmFallback) DeleteObject(hbmFallback);
                                    GlobalFree(buf);
                                    return TRUE;
                                }
                                // Save as fallback
                                // Сохраняем как запасной вариант
                                if (!hbmFallback) {
                                    hbmFallback = hb;
                                    szFallback = s;
                                } else {
                                    DeleteObject(hb);
                                }
                            }
                        }
                    }
                }
            }
        }
        GlobalFree(buf);
        if (isLast) break;
    }

    // Return fallback if specific Front Cover not found
    // Возвращаем запасной вариант, если Front Cover не найден
    if (hbmFallback) {
        if (phbm) *phbm = hbmFallback;
        if (psz) *psz = szFallback;
        return TRUE;
    }
    return FALSE;
}