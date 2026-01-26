/**
 * @file id3v2_reader.cpp
 * @brief ID3v2 parser implementation
 * @brief Реализация парсера ID3v2
 */

#include "..\Extensions\id3v2_reader.h"
#include "..\image_loader.h"
#include "..\utils_common.h"

// ============================================================================
// Helper Functions / Вспомогательные функции
// ============================================================================

/**
 * @brief Calculate length of encoded string to skip it
 * @brief Вычислить длину кодированной строки, чтобы пропустить её
 * * ID3v2 APIC frames contain a text description between the header and image data.
 * We need to scan for the null terminator based on the encoding.
 * * ID3v2 APIC фреймы содержат текстовое описание между заголовком и данными изображения.
 * Нам нужно найти нуль-терминатор, основываясь на кодировке.
 * * @param p Pointer to string data / Указатель на данные строки
 * @param cb Maximum bytes available / Максимум доступных байт
 * @param enc Encoding byte (0=Latin1, 1=UTF-16, 2=UTF-16BE, 3=UTF-8)
 * @return Total bytes to skip (including terminator) / Всего байт для пропуска (включая терминатор)
 */
static DWORD SkipEncodedString(const BYTE* p, DWORD cb, BYTE enc) {
    if (!p || !cb) return 0;
    
    // UTF-16 (BOM or BE) -> 2 bytes terminator (0x00 0x00)
    // UTF-16 (BOM или BE) -> 2-байтовый терминатор (0x00 0x00)
    if (enc == 1 || enc == 2) {
        for (DWORD i = 0; i + 1 < cb; i += 2)
            if (p[i] == 0 && p[i + 1] == 0) return i + 2;
    } 
    // Latin1 or UTF-8 -> 1 byte terminator (0x00)
    // Latin1 или UTF-8 -> 1-байтовый терминатор (0x00)
    else { 
        for (DWORD i = 0; i < cb; ++i)
            if (p[i] == 0) return i + 1;
    }
    return cb;
}

// ============================================================================
// Main Logic / Основная логика
// ============================================================================

BOOL __cdecl ID3v2_LoadCoverToBitmapA(const char* audioPath, HBITMAP* phbm, SIZE* psz) {
    FileHandle f(audioPath);
    if (!f.IsValid()) return FALSE;

    // 1. Read and validate ID3v2 Header (10 bytes)
    // 1. Чтение и валидация заголовка ID3v2 (10 байт)
    BYTE hdr[10];
    if (!f.Read(hdr, 10) || memcmp(hdr, "ID3", 3) != 0) return FALSE;

    BYTE ver = hdr[3];      // Version (e.g., 3 for ID3v2.3)
    BYTE flags = hdr[5];    // Flags
    DWORD tagSize = SyncSafeToInt(&hdr[6]); // Size is encoded as SyncSafe (7 bits per byte)

    // Safety check: Tag size reasonable? (Max 32MB)
    // Проверка безопасности: Разумен ли размер тега? (Макс 32МБ)
    if (tagSize < 10 || tagSize > (32 * 1024 * 1024)) return FALSE;

    // 2. Handle Extended Header (if present)
    // 2. Обработка расширенного заголовка (если есть)
    if ((ver == 3 || ver == 4) && (flags & 0x40)) {
        BYTE ex[10];
        if (!f.Read(ex, 10)) return FALSE;
        
        // v2.4 uses SyncSafe for ext header size, v2.3 uses regular integer
        DWORD extSize = (ver == 4) ? SyncSafeToInt(ex) : BE32(ex);
        
        if (extSize > tagSize) return FALSE;
        
        // Skip extended header data
        // For v2.3 size excludes header itself, for v2.4 it's tricky.
        // Assuming standard structure for safety: seek minus read bytes.
        f.Seek(extSize - 10, FILE_CURRENT); 
        tagSize -= extSize;
    }

    DWORD remaining = tagSize;
    BOOL ok = FALSE;

    // 3. Iterate frames
    // 3. Итерация по фреймам
    while (remaining > 0) {
        DWORD frameSize = 0;
        BOOL isCover = FALSE;
        DWORD headerLen = 0;

        // Parse frame header based on version
        // Парсинг заголовка фрейма в зависимости от версии
        if (ver == 2) {
            // ID3v2.2: 3 char ID, 3 byte size
            BYTE fh[6];
            if (!f.Read(fh, 6) || fh[0] == 0) break; // Padding reached / Достигнут padding
            frameSize = BE24(&fh[3]);
            if (memcmp(fh, "PIC", 3) == 0) isCover = TRUE;
            headerLen = 6;
        } else { 
            // ID3v2.3/2.4: 4 char ID, 4 byte size
            BYTE fh[10];
            if (!f.Read(fh, 10) || fh[0] == 0) break;
            // v2.4 uses SyncSafe size, v2.3 uses Integer
            frameSize = (ver == 4) ? SyncSafeToInt(&fh[4]) : BE32(&fh[4]);
            if (memcmp(fh, "APIC", 4) == 0) isCover = TRUE;
            headerLen = 10;
        }

        if (frameSize > remaining) break;
        remaining -= headerLen;

        if (!isCover) {
            // Skip irrelevant frames
            // Пропуск ненужных фреймов
            f.Seek(frameSize, FILE_CURRENT);
        } else {
            // 4. Extract Picture
            // 4. Извлечение изображения
            BYTE* buf = (BYTE*)GlobalAlloc(GMEM_FIXED, frameSize);
            if (buf) {
                if (f.Read(buf, frameSize)) {
                    // Frame structure / Структура фрейма:
                    // PIC (v2):  [Enc(1)] [Fmt(3)] [Type(1)] [Desc...] [Data]
                    // APIC (v3): [Enc(1)] [Mime(str)] [Type(1)] [Desc...] [Data]
                    
                    DWORD p = 1; 
                    BYTE enc = buf[0];
                    
                    if (ver == 2) {
                        p = 5; // Skip Enc(1) + Fmt(3) + Type(1)
                    } else {
                        // Skip MIME type (null-terminated string)
                        while (p < frameSize && buf[p] != 0) ++p; 
                        p++; // Skip zero
                        p++; // Skip Picture Type
                    }
                    
                    // Skip Description (encoded string)
                    // Пропуск описания (кодированная строка)
                    p += SkipEncodedString(&buf[p], frameSize - p, enc);

                    // Load actual image data
                    if (p < frameSize) {
                        ok = Img_LoadFromMemoryToBitmap(&buf[p], frameSize - p, phbm, psz);
                    }
                }
                GlobalFree(buf);
            }
            if (ok) return TRUE; // Stop after first valid cover / Остановка после первой валидной обложки
        }
        
        if (remaining < frameSize) break;
        remaining -= frameSize;
    }
    return FALSE;
}