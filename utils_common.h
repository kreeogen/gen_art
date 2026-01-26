/**
 * @file utils_common.h
 * @brief Common utilities for file I/O and binary operations
 * @brief Общие утилиты для файлового ввода-вывода и бинарных операций
 * 
 * This header provides reusable utility classes and functions for working with
 * binary file formats (MP4, APE, FLAC, ID3v2). It eliminates code duplication
 * across different tag readers by centralizing common operations.
 * 
 * Этот заголовок предоставляет переиспользуемые утилитарные классы и функции для
 * работы с бинарными форматами файлов (MP4, APE, FLAC, ID3v2). Он устраняет
 * дублирование кода в различных ридерах тегов путём централизации общих операций.
 * 
 * Key Features / Ключевые возможности:
 * - RAII file handle wrapper for automatic cleanup
 * - Endianness conversion (big-endian/little-endian)
 * - 64-bit safe file positioning
 * - SyncSafe integer decoding (ID3v2/FLAC)
 * - FourCC code generation
 * 
 * - RAII обёртка дескриптора файла для автоматической очистки
 * - Конверсия порядка байтов (big-endian/little-endian)
 * - 64-битная безопасная позиция в файле
 * - Декодирование SyncSafe целых чисел (ID3v2/FLAC)
 * - Генерация FourCC кодов
 * 
 * @note All functions are inline for zero-cost abstraction
 * @note Все функции inline для нулевой стоимости абстракции
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ============================================================================
// RAII File Wrapper / RAII обёртка файла
// ============================================================================

/**
 * @class FileHandle
 * @brief RAII wrapper for Windows file handles
 * @brief RAII обёртка для Windows дескрипторов файлов
 * 
 * This class provides automatic resource management for file handles.
 * The file is automatically closed when the object goes out of scope,
 * preventing resource leaks even in error paths.
 * 
 * Этот класс предоставляет автоматическое управление ресурсами для файловых дескрипторов.
 * Файл автоматически закрывается когда объект выходит из области видимости,
 * предотвращая утечки ресурсов даже в путях ошибок.
 * 
 * Benefits / Преимущества:
 * - Exception-safe (file always closed)
 * - Reduces boilerplate cleanup code
 * - Eliminates forgotten CloseHandle() calls
 * 
 * - Безопасно при исключениях (файл всегда закрывается)
 * - Уменьшает шаблонный код очистки
 * - Устраняет забытые вызовы CloseHandle()
 * 
 * Example usage / Пример использования:
 * @code
 * FileHandle f("music.mp3");
 * if (f.IsValid()) {
 *     BYTE header[10];
 *     f.Read(header, 10);
 *     // File automatically closed when 'f' goes out of scope
 * }
 * @endcode
 */
class FileHandle {
    HANDLE h;  ///< Windows file handle / Windows дескриптор файла
    
public:
    /**
     * @brief Constructor - opens file for reading
     * @brief Конструктор - открывает файл для чтения
     * 
     * Opens the file with:
     * - GENERIC_READ access
     * - FILE_SHARE_READ | FILE_SHARE_WRITE sharing
     * - OPEN_EXISTING disposition
     * 
     * Открывает файл с:
     * - GENERIC_READ доступом
     * - FILE_SHARE_READ | FILE_SHARE_WRITE расшариванием
     * - OPEN_EXISTING диспозицией
     * 
     * @param path Path to file / Путь к файлу
     * 
     * @note Check IsValid() after construction to verify success
     * @note Проверьте IsValid() после конструкции для проверки успеха
     */
    FileHandle(const char* path) {
        h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    
    /**
     * @brief Destructor - automatically closes file
     * @brief Деструктор - автоматически закрывает файл
     * 
     * The file handle is closed if valid. This happens automatically
     * when the object goes out of scope.
     * 
     * Дескриптор файла закрывается если валиден. Это происходит автоматически
     * когда объект выходит из области видимости.
     */
    ~FileHandle() { 
        if (IsValid()) CloseHandle(h); 
    }
    
    /**
     * @brief Check if file was opened successfully
     * @brief Проверить, был ли файл успешно открыт
     * 
     * @return TRUE if file is open and valid / TRUE если файл открыт и валиден
     */
    BOOL IsValid() const { 
        return h != INVALID_HANDLE_VALUE; 
    }
    
    /**
     * @brief Conversion operator to HANDLE
     * @brief Оператор конверсии в HANDLE
     * 
     * Allows FileHandle to be used directly in Win32 API calls.
     * Позволяет FileHandle использоваться напрямую в Win32 API вызовах.
     * 
     * @return Underlying file handle / Базовый дескриптор файла
     */
    operator HANDLE() const { 
        return h; 
    }

    /**
     * @brief Get file size
     * @brief Получить размер файла
     * 
     * @return File size in bytes, or 0 if invalid / Размер файла в байтах, или 0 если невалиден
     * 
     * @note Returns 0 for files >4GB (uses 32-bit GetFileSize)
     * @note Возвращает 0 для файлов >4GB (использует 32-битный GetFileSize)
     */
    DWORD GetSize() const { 
        return IsValid() ? GetFileSize(h, NULL) : 0; 
    }
    
    /**
     * @brief Read exact number of bytes from current position
     * @brief Прочитать точное количество байтов с текущей позиции
     * 
     * @param buf Buffer to receive data / Буфер для получения данных
     * @param size Number of bytes to read / Количество байтов для чтения
     * @return TRUE if all bytes were read / TRUE если все байты были прочитаны
     * 
     * @note Returns FALSE if less than 'size' bytes are available
     * @note Возвращает FALSE если доступно меньше 'size' байтов
     */
    BOOL Read(void* buf, DWORD size) {
        DWORD rd = 0;
        return IsValid() && ReadFile(h, buf, size, &rd, NULL) && rd == size;
    }

    /**
     * @brief Read bytes from specific file offset (64-bit safe)
     * @brief Прочитать байты с определённого смещения в файле (64-битная безопасность)
     * 
     * This function supports reading from offsets >4GB by splitting
     * the 64-bit offset into high/low 32-bit parts.
     * 
     * Эта функция поддерживает чтение со смещений >4GB путём разделения
     * 64-битного смещения на высокие/низкие 32-битные части.
     * 
     * @param offset 64-bit file offset / 64-битное смещение в файле
     * @param buf Buffer to receive data / Буфер для получения данных
     * @param size Number of bytes to read / Количество байтов для чтения
     * @return TRUE if successful / TRUE при успехе
     * 
     * @note Automatically seeks to offset before reading
     * @note Автоматически переходит к смещению перед чтением
     */
    BOOL ReadAt(unsigned __int64 offset, void* buf, DWORD size) {
        if (!IsValid()) return FALSE;
        
        // Split 64-bit offset into high and low 32-bit parts
        // Разделить 64-битное смещение на высокую и низкую 32-битные части
        LONG hi = (LONG)(offset >> 32);
        DWORD lo = (DWORD)(offset & 0xFFFFFFFF);
        
        // Seek to position / Переход к позиции
        if (SetFilePointer(h, lo, &hi, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
            if (GetLastError() != NO_ERROR) return FALSE;
        }
        
        return Read(buf, size);
    }
    
    /**
     * @brief Seek to relative position
     * @brief Переход к относительной позиции
     * 
     * @param dist Distance to seek (can be negative) / Расстояние для перехода (может быть отрицательным)
     * @param method Seek method (FILE_BEGIN, FILE_CURRENT, FILE_END) / Метод перехода
     * @return TRUE if successful / TRUE при успехе
     */
    BOOL Seek(LONG dist, DWORD method = FILE_CURRENT) {
        return IsValid() && SetFilePointer(h, dist, NULL, method) != INVALID_SET_FILE_POINTER;
    }
};

// ============================================================================
// Endianness Conversion Helpers / Помощники конверсии порядка байтов
// ============================================================================

/**
 * @brief Convert 4 bytes from big-endian to native (little-endian on x86)
 * @brief Конвертировать 4 байта из big-endian в нативный (little-endian на x86)
 * 
 * Used for: MP4 atoms, FLAC blocks, network protocols
 * Используется для: MP4 атомов, FLAC блоков, сетевых протоколов
 * 
 * @param p Pointer to 4 bytes / Указатель на 4 байта
 * @return 32-bit value in native endianness / 32-битное значение в нативном порядке байтов
 */
inline DWORD BE32(const BYTE* p) {
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | 
           ((DWORD)p[2] << 8)  | (DWORD)p[3];
}

/**
 * @brief Convert 3 bytes from big-endian to 32-bit value
 * @brief Конвертировать 3 байта из big-endian в 32-битное значение
 * 
 * Used for: FLAC block sizes
 * Используется для: Размеров FLAC блоков
 * 
 * @param p Pointer to 3 bytes / Указатель на 3 байта
 * @return 32-bit value (high byte is 0) / 32-битное значение (старший байт 0)
 */
inline DWORD BE24(const BYTE* p) {
    return ((DWORD)p[0] << 16) | ((DWORD)p[1] << 8) | (DWORD)p[2];
}

/**
 * @brief Convert 8 bytes from big-endian to 64-bit value
 * @brief Конвертировать 8 байтов из big-endian в 64-битное значение
 * 
 * Used for: MP4 extended sizes (files >4GB)
 * Используется для: Расширенных размеров MP4 (файлы >4GB)
 * 
 * @param p Pointer to 8 bytes / Указатель на 8 байтов
 * @return 64-bit value in native endianness / 64-битное значение в нативном порядке байтов
 */
inline unsigned __int64 BE64(const BYTE* p) {
    unsigned __int64 v = 0;
    for (int i = 0; i < 8; i++) {
        v = (v << 8) | p[i];
    }
    return v;
}

/**
 * @brief Convert 4 bytes from little-endian to native
 * @brief Конвертировать 4 байта из little-endian в нативный
 * 
 * Used for: APE tags, Windows structures
 * Используется для: APE тегов, структур Windows
 * 
 * @param p Pointer to 4 bytes / Указатель на 4 байта
 * @return 32-bit value in native endianness / 32-битное значение в нативном порядке байтов
 * 
 * @note On x86, this is essentially a no-op (just reads DWORD)
 * @note На x86 это по сути no-op (просто читает DWORD)
 */
inline DWORD LE32(const BYTE* p) {
    return ((DWORD)p[0]) | ((DWORD)p[1] << 8) | 
           ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
}

// ============================================================================
// Specialized Decoders / Специализированные декодеры
// ============================================================================

/**
 * @brief Decode SyncSafe integer (7 bits per byte)
 * @brief Декодировать SyncSafe целое число (7 бит на байт)
 * 
 * SyncSafe integers use only the lower 7 bits of each byte, with the
 * high bit always 0. This ensures the value never contains 0xFF patterns
 * that could be confused with sync markers.
 * 
 * SyncSafe целые числа используют только младшие 7 бит каждого байта,
 * со старшим битом всегда 0. Это гарантирует, что значение никогда не содержит
 * паттерны 0xFF, которые можно спутать с маркерами синхронизации.
 * 
 * Used in: ID3v2 tag sizes, FLAC metadata
 * Используется в: Размерах ID3v2 тегов, метаданных FLAC
 * 
 * Format / Формат:
 * - Input:  0bAaaaaaaa 0bBbbbbbb 0bCcccccc 0bDdddddd
 * - Output: 0bAaaaaaaBbbbbbbCccccccDdddddd (28 bits used)
 * 
 * @param b Pointer to 4 bytes / Указатель на 4 байта
 * @return Decoded 32-bit value (max 268,435,455) / Декодированное 32-битное значение (макс 268,435,455)
 * 
 * @note Maximum representable value is 2^28 - 1 (256 MB)
 * @note Максимальное представимое значение 2^28 - 1 (256 МБ)
 */
inline DWORD SyncSafeToInt(const BYTE* b) {
    return ((DWORD)(b[0] & 0x7F) << 21) | 
           ((DWORD)(b[1] & 0x7F) << 14) |
           ((DWORD)(b[2] & 0x7F) << 7)  | 
           (DWORD)(b[3] & 0x7F);
}

/**
 * @brief Create FourCC (Four Character Code) from 4 characters
 * @brief Создать FourCC (Four Character Code) из 4 символов
 * 
 * FourCC codes are used extensively in multimedia containers (MP4, AVI, etc.)
 * to identify box/chunk types. They're stored as big-endian 32-bit integers.
 * 
 * FourCC коды широко используются в мультимедийных контейнерах (MP4, AVI и т.д.)
 * для идентификации типов box'ов/чанков. Они хранятся как big-endian 32-битные целые.
 * 
 * @param a First character / Первый символ
 * @param b Second character / Второй символ
 * @param c Third character / Третий символ
 * @param d Fourth character / Четвёртый символ
 * @return 32-bit FourCC code / 32-битный FourCC код
 * 
 * Example / Пример:
 * @code
 * DWORD moov = FCC('m', 'o', 'o', 'v');  // 0x6D6F6F76
 * if (boxType == FCC('f', 't', 'y', 'p')) { ... }
 * @endcode
 */
inline DWORD FCC(char a, char b, char c, char d) {
    return ((DWORD)(BYTE)(a) << 24) | ((DWORD)(BYTE)(b) << 16) | 
           ((DWORD)(BYTE)(c) << 8)  | (DWORD)(BYTE)(d);
}
