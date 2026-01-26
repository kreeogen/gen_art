/**
 * @file image_loader.cpp
 * @brief Universal image loader implementation for Win32 (Optimized Version)
 * @brief Универсальная реализация загрузчика изображений для Win32 (Оптимизированная версия)
 * 
 * This module provides a robust, cross-compatible image loading solution that works
 * on Windows 98/ME/2000/XP and later. It supports multiple image formats through
 * two different decoders:
 * 
 * Этот модуль предоставляет надёжное, кросс-совместимое решение для загрузки изображений,
 * работающее на Windows 98/ME/2000/XP и более поздних версиях. Поддерживает множество
 * форматов изображений через два различных декодера:
 * 
 * Decoder Strategy / Стратегия декодеров:
 * 1. OLE IPicture - Older, built into Windows, works on Win98+
 * 2. GDI+ - Newer, better PNG support, dynamically loaded
 * 
 * 1. OLE IPicture - Старше, встроен в Windows, работает на Win98+
 * 2. GDI+ - Новее, лучшая поддержка PNG, динамически загружается
 * 
 * Key Features / Ключевые возможности:
 * - Automatic format detection by signature
 * - Dynamic GDI+ loading for compatibility
 * - Deep bitmap copy to avoid handle lifetime issues
 * - HIMETRIC to pixel conversion for proper sizing
 * - Optimized with early returns and reduced duplication
 * - Proper cleanup function to prevent process hang
 * 
 * Optimization Notes / Заметки об оптимизации:
 * - Removed duplicate logic in TryLoadImage
 * - Added Img_Cleanup() to prevent GDI+ process hang
 * - Improved signature checks for performance
 * - Unified stream creation logic
 * 
 * @note Compatible with Visual Studio 2003 and ANSI builds
 * @note Совместим с Visual Studio 2003 и ANSI сборками
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0 (Optimized)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <olectl.h>
#include <objbase.h>
#include <shlwapi.h>
#include <string.h>
#include "image_loader.h" 

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

// ============================================================================
// Constants / Константы
// ============================================================================

static const DWORD kMaxImageBytes = 32u * 1024u * 1024u;  ///< Maximum 32 MB to prevent memory exhaustion

// ============================================================================
// Image Format Signature Detection / Определение формата изображения по сигнатуре
// ============================================================================

/**
 * @brief Check if data looks like a JPEG image
 * @brief Проверить, похожи ли данные на JPEG изображение
 * 
 * JPEG files start with FF D8 FF followed by a marker byte (E0-EF, DB, C0-CF).
 * JPEG файлы начинаются с FF D8 FF за которым следует байт-маркер (E0-EF, DB, C0-CF).
 * 
 * @param p Pointer to data / Указатель на данные
 * @param n Data size / Размер данных
 * @return TRUE if looks like JPEG / TRUE если похоже на JPEG
 */
static BOOL LooksLikeJPEG(const BYTE* p, DWORD n) {
    if (!p || n < 4) return FALSE;
    
    // Check JPEG SOI (Start of Image) marker: FF D8 FF
    // Проверка маркера JPEG SOI (Start of Image): FF D8 FF
    if (!(p[0]==0xFF && p[1]==0xD8 && p[2]==0xFF)) return FALSE;
    
    BYTE m = p[3];
    // Check for valid JPEG markers / Проверка валидных JPEG маркеров
    return ((m>=0xE0 && m<=0xEF) ||  // APP0-APP15 markers
            m==0xDB ||                // DQT (Define Quantization Table)
            (m>=0xC0 && m<=0xCF && m!=0xC8)); // SOF markers (except DHT)
}

/**
 * @brief Check if data looks like a PNG image
 * @brief Проверить, похожи ли данные на PNG изображение
 * 
 * PNG files have a fixed 8-byte signature: 89 50 4E 47 0D 0A 1A 0A
 * PNG файлы имеют фиксированную 8-байтную сигнатуру: 89 50 4E 47 0D 0A 1A 0A
 * 
 * @param p Pointer to data / Указатель на данные
 * @param n Data size / Размер данных
 * @return TRUE if looks like PNG / TRUE если похоже на PNG
 */
static BOOL LooksLikePNG(const BYTE* p, DWORD n) {
    static const BYTE sig[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    return (p && n>=8 && memcmp(p, sig, 8)==0);
}

/**
 * @brief Check if data looks like a GIF image
 * @brief Проверить, похожи ли данные на GIF изображение
 * 
 * GIF files start with either "GIF87a" or "GIF89a".
 * GIF файлы начинаются либо с "GIF87a", либо с "GIF89a".
 * 
 * @param p Pointer to data / Указатель на данные
 * @param n Data size / Размер данных
 * @return TRUE if looks like GIF / TRUE если похоже на GIF
 */
static BOOL LooksLikeGIF(const BYTE* p, DWORD n) {
    return (p && n>=6 && 
            ((memcmp(p, "GIF87a", 6)==0) || (memcmp(p,"GIF89a",6)==0)));
}

/**
 * @brief Check if data looks like a BMP image
 * @brief Проверить, похожи ли данные на BMP изображение
 * 
 * BMP files start with "BM" and have a valid header size.
 * BMP файлы начинаются с "BM" и имеют валидный размер заголовка.
 * 
 * @param p Pointer to data / Указатель на данные
 * @param n Data size / Размер данных
 * @return TRUE if looks like BMP / TRUE если похоже на BMP
 */
static BOOL LooksLikeBMP(const BYTE* p, DWORD n) {
    if (!p || n < 14 || p[0]!='B' || p[1]!='M') return FALSE;
    
    // Validate BITMAPINFOHEADER size (must be >= 12) / Проверка размера BITMAPINFOHEADER (должен быть >= 12)
    if (n >= 18) {
        DWORD hdr = *(const DWORD*)(p+14);
        if (hdr < 12) return FALSE;  // Invalid header size / Неверный размер заголовка
    }
    return TRUE;
}

/**
 * @brief Check if data looks like an ICO (icon) file
 * @brief Проверить, похожи ли данные на ICO (иконка) файл
 * 
 * ICO files have: reserved(0), type(1), count(>=1).
 * ICO файлы имеют: reserved(0), type(1), count(>=1).
 * 
 * @param p Pointer to data / Указатель на данные
 * @param n Data size / Размер данных
 * @return TRUE if looks like ICO / TRUE если похоже на ICO
 */
static BOOL LooksLikeICO(const BYTE* p, DWORD n) {
    if (!p || n < 6) return FALSE;
    
    WORD reserved = *(const WORD*)p;
    WORD type     = *(const WORD*)(p+2);
    WORD count    = *(const WORD*)(p+4);
    
    return (reserved==0 && type==1 && count>=1);
}

/**
 * @brief Check if data matches any known image format
 * @brief Проверить, совпадают ли данные с любым известным форматом изображения
 * 
 * Tries all format detectors with early return for performance.
 * Пробует все детекторы форматов с ранним возвратом для производительности.
 * 
 * @param p Pointer to data / Указатель на данные
 * @param n Data size / Размер данных
 * @return TRUE if format is recognized / TRUE если формат распознан
 */
static BOOL LooksLikeKnownImage(const BYTE* p, DWORD n) {
    if (LooksLikeJPEG(p,n)) return TRUE;
    if (LooksLikePNG(p,n)) return TRUE;
    if (LooksLikeGIF(p,n)) return TRUE;
    if (LooksLikeBMP(p,n)) return TRUE;
    return LooksLikeICO(p,n);
}

// ============================================================================
// DPI and Conversion Helpers / Помощники для DPI и конверсии
// ============================================================================

/**
 * @brief Convert HIMETRIC units to pixels
 * @brief Конвертировать HIMETRIC единицы в пиксели
 * 
 * HIMETRIC is a logical unit of 0.01 millimeter, used by OLE IPicture.
 * This function converts to pixels based on the screen DPI.
 * 
 * HIMETRIC - это логическая единица 0.01 миллиметра, используемая OLE IPicture.
 * Эта функция конвертирует в пиксели на основе DPI экрана.
 * 
 * Formula / Формула: pixels = (HIMETRIC * DPI) / 2540
 * 
 * @param hmWidth  Width in HIMETRIC / Ширина в HIMETRIC
 * @param hmHeight Height in HIMETRIC / Высота в HIMETRIC
 * @param outPx    [out] Output dimensions in pixels / Выходные размеры в пикселях
 */
static void HimetricToPixels(LONG hmWidth, LONG hmHeight, SIZE* outPx) {
    HDC hdc = GetDC(NULL);
    
    // Get screen DPI (default to 96 if unavailable) / Получить DPI экрана (по умолчанию 96, если недоступно)
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    if (dpiX <= 0) dpiX = 96;
    if (dpiY <= 0) dpiY = 96;
    
    // Convert HIMETRIC to pixels / Конвертировать HIMETRIC в пиксели
    outPx->cx = MulDiv(hmWidth,  dpiX, 2540);
    outPx->cy = MulDiv(hmHeight, dpiY, 2540);
    
    ReleaseDC(NULL, hdc);
}

/**
 * @brief Create a deep copy of an HBITMAP
 * @brief Создать глубокую копию HBITMAP
 * 
 * OLE IPicture owns the bitmap handle and will delete it when released.
 * We need to create a copy that we can safely manage ourselves.
 * 
 * OLE IPicture владеет дескриптором bitmap'а и удалит его при освобождении.
 * Нам нужно создать копию, которой мы можем безопасно управлять сами.
 * 
 * @param src Source bitmap to copy / Исходный bitmap для копирования
 * @return New bitmap handle, or NULL on failure / Новый дескриптор bitmap'а, или NULL при ошибке
 */
static HBITMAP DeepCopyHBITMAP(HBITMAP src) {
    if (!src) return NULL;
    
    HDC hdc = GetDC(NULL);
    BITMAP bm = {0};
    
    // Get source bitmap info / Получить информацию об исходном bitmap'е
    if (!GetObject(src, sizeof(bm), &bm) || bm.bmWidth<=0 || bm.bmHeight<=0) { 
        ReleaseDC(NULL,hdc); 
        return NULL; 
    }
    
    // Create destination bitmap / Создать целевой bitmap
    HBITMAP copy = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
    if (!copy) { 
        ReleaseDC(NULL, hdc); 
        return NULL; 
    }
    
    // Copy pixels using BitBlt / Копировать пиксели используя BitBlt
    HDC sdc = CreateCompatibleDC(hdc);
    HDC ddc = CreateCompatibleDC(hdc);
    HBITMAP oldS = (HBITMAP)SelectObject(sdc, src);
    HBITMAP oldD = (HBITMAP)SelectObject(ddc, copy);
    
    BitBlt(ddc, 0, 0, bm.bmWidth, bm.bmHeight, sdc, 0, 0, SRCCOPY);
    
    SelectObject(sdc, oldS);
    SelectObject(ddc, oldD);
    DeleteDC(sdc);
    DeleteDC(ddc);
    ReleaseDC(NULL, hdc);
    
    return copy;
}

// ============================================================================
// Stream Helper / Помощник для потоков
// ============================================================================

/**
 * @brief Create an IStream from memory buffer
 * @brief Создать IStream из буфера памяти
 * 
 * Creates a COM stream object (IStream) that wraps the provided memory buffer.
 * The stream takes ownership of the global memory handle.
 * 
 * Создаёт COM-объект потока (IStream), обёртывающий предоставленный буфер памяти.
 * Поток берёт на себя владение дескриптором глобальной памяти.
 * 
 * @param data Pointer to image data / Указатель на данные изображения
 * @param cb   Size of data in bytes / Размер данных в байтах
 * @return IStream pointer, or NULL on failure / Указатель на IStream, или NULL при ошибке
 */
static IStream* CreateStreamFromMemory(const BYTE* data, DWORD cb) {
    if (!data || !cb) return NULL;
    
    // Allocate global memory / Выделить глобальную память
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, cb);
    if (!h) return NULL;
    
    // Copy data to global memory / Скопировать данные в глобальную память
    void* p = GlobalLock(h);
    if (!p) { 
        GlobalFree(h); 
        return NULL; 
    }
    CopyMemory(p, data, cb);
    GlobalUnlock(h);

    // Create stream from global handle / Создать поток из глобального дескриптора
    IStream* stm = NULL;
    HRESULT hr = CreateStreamOnHGlobal(h, TRUE, &stm);  // TRUE = stream owns memory
    if (FAILED(hr) || !stm) { 
        GlobalFree(h); 
        return NULL; 
    }
    
    return stm;
}

// ============================================================================
// OLE IPicture Decoder / Декодер OLE IPicture
// ============================================================================

/**
 * @brief Load image using OLE IPicture
 * @brief Загрузить изображение используя OLE IPicture
 * 
 * Uses the built-in Windows OLE IPicture interface to load images.
 * Works on Windows 98 and later. Best for JPEG, GIF, and BMP.
 * 
 * Использует встроенный Windows интерфейс OLE IPicture для загрузки изображений.
 * Работает на Windows 98 и более поздних версиях. Лучше всего для JPEG, GIF и BMP.
 * 
 * @param data Pointer to image data / Указатель на данные изображения
 * @param cb   Size of data in bytes / Размер данных в байтах
 * @param phbm [out] Receives bitmap handle / Получает дескриптор bitmap'а
 * @param psz  [out] Receives bitmap dimensions / Получает размеры bitmap'а
 * @return TRUE on success / TRUE при успехе
 */
static BOOL LoadImageViaOle(const BYTE* data, DWORD cb, HBITMAP* phbm, SIZE* psz) {
    // Create stream from memory / Создать поток из памяти
    IStream* stm = CreateStreamFromMemory(data, cb);
    if (!stm) return FALSE;

    // Load picture from stream / Загрузить картинку из потока
    IPicture* pic = NULL;
    HRESULT hr = OleLoadPicture(stm, 0, TRUE, IID_IPicture, (void**)&pic);
    stm->Release();
    if (FAILED(hr) || !pic) return FALSE;

    // Get bitmap handle from picture / Получить дескриптор bitmap'а из картинки
    OLE_HANDLE hPic = 0;
    if (FAILED(pic->get_Handle(&hPic)) || !hPic) { 
        pic->Release(); 
        return FALSE; 
    }

    // Get dimensions in HIMETRIC and convert to pixels / Получить размеры в HIMETRIC и конвертировать в пиксели
    LONG hmW=0, hmH=0;
    pic->get_Width(&hmW);
    pic->get_Height(&hmH);
    SIZE px; 
    HimetricToPixels(hmW, hmH, &px);

    // Create a deep copy (IPicture owns the original) / Создать глубокую копию (IPicture владеет оригиналом)
    HBITMAP copy = DeepCopyHBITMAP((HBITMAP)hPic);
    pic->Release();
    if (!copy) return FALSE;

    if (phbm) *phbm = copy;
    if (psz)  *psz  = px;
    return TRUE;
}

// ============================================================================
// GDI+ Dynamic Loading / Динамическая загрузка GDI+
// ============================================================================

// GDI+ typedefs and function pointers / Определения типов и указатели на функции GDI+
typedef INT   GpStatus;
typedef ULONG ARGB;

typedef struct _GdiplusStartupInput {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;
    BOOL   SuppressBackgroundThread;
    BOOL   SuppressExternalCodecs;
} GdiplusStartupInput;

// Global GDI+ state / Глобальное состояние GDI+
static HMODULE    g_hGdiplus     = NULL;
static ULONG_PTR  g_gdiplusToken = 0;

// GDI+ function pointer types / Типы указателей на функции GDI+
typedef GpStatus (WINAPI *PFN_GdiplusStartup)(ULONG_PTR*, const GdiplusStartupInput*, void*);
typedef void     (WINAPI *PFN_GdiplusShutdown)(ULONG_PTR);
typedef GpStatus (WINAPI *PFN_GdipCreateBitmapFromStream)(IStream*, void**);
typedef GpStatus (WINAPI *PFN_GdipCreateHBITMAPFromBitmap)(void*, HBITMAP*, ARGB);
typedef GpStatus (WINAPI *PFN_GdipDisposeImage)(void*);
typedef GpStatus (WINAPI *PFN_GdipGetImageWidth)(void*, UINT*);
typedef GpStatus (WINAPI *PFN_GdipGetImageHeight)(void*, UINT*);

// GDI+ function pointers / Указатели на функции GDI+
static PFN_GdiplusStartup              pGdiplusStartup = NULL;
static PFN_GdiplusShutdown             pGdiplusShutdown = NULL;
static PFN_GdipCreateBitmapFromStream  pGdipCreateBitmapFromStream = NULL;
static PFN_GdipCreateHBITMAPFromBitmap pGdipCreateHBITMAPFromBitmap = NULL;
static PFN_GdipDisposeImage            pGdipDisposeImage = NULL;
static PFN_GdipGetImageWidth           pGdipGetImageWidth = NULL;
static PFN_GdipGetImageHeight          pGdipGetImageHeight = NULL;

/**
 * @brief Ensure GDI+ is loaded and initialized
 * @brief Убедиться, что GDI+ загружена и инициализирована
 * 
 * Dynamically loads gdiplus.dll and initializes it. This allows the plugin
 * to work on systems that don't have GDI+ (Windows 98/ME without updates).
 * 
 * Динамически загружает gdiplus.dll и инициализирует её. Это позволяет плагину
 * работать на системах без GDI+ (Windows 98/ME без обновлений).
 * 
 * @return TRUE if GDI+ is ready / TRUE если GDI+ готова
 */
static BOOL Gdiplus_Ensure(void) {
    if (g_hGdiplus && g_gdiplusToken) return TRUE;

    if (!g_hGdiplus) {
        // Load GDI+ library / Загрузить библиотеку GDI+
        g_hGdiplus = LoadLibraryA("gdiplus.dll");
        if (!g_hGdiplus) return FALSE;
        
        // Get function pointers / Получить указатели на функции
        pGdiplusStartup              = (PFN_GdiplusStartup)             GetProcAddress(g_hGdiplus, "GdiplusStartup");
        pGdiplusShutdown             = (PFN_GdiplusShutdown)            GetProcAddress(g_hGdiplus, "GdiplusShutdown");
        pGdipCreateBitmapFromStream  = (PFN_GdipCreateBitmapFromStream) GetProcAddress(g_hGdiplus, "GdipCreateBitmapFromStream");
        pGdipCreateHBITMAPFromBitmap = (PFN_GdipCreateHBITMAPFromBitmap)GetProcAddress(g_hGdiplus, "GdipCreateHBITMAPFromBitmap");
        pGdipDisposeImage            = (PFN_GdipDisposeImage)           GetProcAddress(g_hGdiplus, "GdipDisposeImage");
        pGdipGetImageWidth           = (PFN_GdipGetImageWidth)          GetProcAddress(g_hGdiplus, "GdipGetImageWidth");
        pGdipGetImageHeight          = (PFN_GdipGetImageHeight)         GetProcAddress(g_hGdiplus, "GdipGetImageHeight");
        
        // Verify all critical functions are available / Проверить доступность всех критических функций
        if (!pGdiplusStartup || !pGdiplusShutdown || !pGdipCreateBitmapFromStream ||
            !pGdipCreateHBITMAPFromBitmap || !pGdipDisposeImage) {
            FreeLibrary(g_hGdiplus); 
            g_hGdiplus = NULL;
            return FALSE;
        }
    }

    if (!g_gdiplusToken) {
        // Initialize GDI+ / Инициализировать GDI+
        GdiplusStartupInput in = {0};
        in.GdiplusVersion = 1;
        in.SuppressBackgroundThread = FALSE;
        in.SuppressExternalCodecs   = TRUE;
        if (pGdiplusStartup(&g_gdiplusToken, &in, NULL) != 0) return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief Clean up GDI+ resources
 * @brief Очистить ресурсы GDI+
 * 
 * Shuts down GDI+ and unloads the library. This MUST be called before
 * the process exits to prevent hangs.
 * 
 * Завершает работу GDI+ и выгружает библиотеку. Это ДОЛЖНО быть вызвано
 * перед выходом из процесса для предотвращения зависаний.
 */
void Img_Cleanup(void) {
    // Shutdown GDI+ / Завершить работу GDI+
    if (g_gdiplusToken && pGdiplusShutdown) {
        pGdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
    
    // Unload library and clear pointers / Выгрузить библиотеку и очистить указатели
    if (g_hGdiplus) {
        FreeLibrary(g_hGdiplus);
        g_hGdiplus = NULL;
        
        // Clear all function pointers / Очистить все указатели на функции
        pGdiplusStartup = NULL;
        pGdiplusShutdown = NULL;
        pGdipCreateBitmapFromStream = NULL;
        pGdipCreateHBITMAPFromBitmap = NULL;
        pGdipDisposeImage = NULL;
        pGdipGetImageWidth = NULL;
        pGdipGetImageHeight = NULL;
    }
}

/**
 * @brief Load image using GDI+
 * @brief Загрузить изображение используя GDI+
 * 
 * Uses GDI+ for image loading. Better PNG support and handling of
 * modern image formats. Dynamically loaded for compatibility.
 * 
 * Использует GDI+ для загрузки изображений. Лучшая поддержка PNG и обработка
 * современных форматов изображений. Динамически загружается для совместимости.
 * 
 * @param data Pointer to image data / Указатель на данные изображения
 * @param cb   Size of data in bytes / Размер данных в байтах
 * @param phbm [out] Receives bitmap handle / Получает дескриптор bitmap'а
 * @param psz  [out] Receives bitmap dimensions / Получает размеры bitmap'а
 * @return TRUE on success / TRUE при успехе
 */
static BOOL LoadImageViaGdiplus(const BYTE* data, DWORD cb, HBITMAP* phbm, SIZE* psz) {
    if (!Gdiplus_Ensure()) return FALSE;

    // Create stream / Создать поток
    IStream* stm = CreateStreamFromMemory(data, cb);
    if (!stm) return FALSE;

    // Create GDI+ bitmap from stream / Создать GDI+ bitmap из потока
    void* gpimg = NULL;
    if (pGdipCreateBitmapFromStream(stm, &gpimg) != 0 || !gpimg) {
        stm->Release();
        return FALSE;
    }
    stm->Release();

    // Convert to HBITMAP / Конвертировать в HBITMAP
    HBITMAP hb = NULL;
    if (pGdipCreateHBITMAPFromBitmap(gpimg, &hb, 0x00FFFFFF) != 0 || !hb) {
        pGdipDisposeImage(gpimg);
        return FALSE;
    }

    // Get dimensions / Получить размеры
    if (psz) {
        UINT w=0, h=0;
        if (pGdipGetImageWidth && pGdipGetImageHeight &&
            pGdipGetImageWidth(gpimg, &w)==0 && pGdipGetImageHeight(gpimg, &h)==0) {
            psz->cx = (LONG)w; 
            psz->cy = (LONG)h;
        } else {
            // Fallback to GetObject / Запасной вариант через GetObject
            BITMAP bm = {0};
            if (GetObject(hb, sizeof(bm), &bm)) { 
                psz->cx = bm.bmWidth; 
                psz->cy = bm.bmHeight; 
            }
        }
    }

    pGdipDisposeImage(gpimg);
    if (phbm) *phbm = hb;
    return TRUE;
}

// ============================================================================
// Unified Loader / Унифицированный загрузчик
// ============================================================================

/**
 * @brief Try loading image with optimal decoder
 * @brief Попытаться загрузить изображение оптимальным декодером
 * 
 * Selects the best decoder based on format:
 * - PNG: GDI+ first (better support), fallback to OLE
 * - Others: OLE first (more compatible), fallback to GDI+
 * 
 * Выбирает лучший декодер на основе формата:
 * - PNG: Сначала GDI+ (лучшая поддержка), запасной вариант OLE
 * - Остальные: Сначала OLE (более совместимо), запасной вариант GDI+
 * 
 * @param data Pointer to image data / Указатель на данные изображения
 * @param cb   Size of data in bytes / Размер данных в байтах
 * @param phbm [out] Receives bitmap handle / Получает дескриптор bitmap'а
 * @param psz  [out] Receives bitmap dimensions / Получает размеры bitmap'а
 * @return TRUE on success / TRUE при успехе
 */
static BOOL TryLoadImage(const BYTE* data, DWORD cb, HBITMAP* phbm, SIZE* psz) {
    // PNG loads better via GDI+, others via OLE / PNG лучше загружается через GDI+, остальные через OLE
    if (LooksLikePNG(data, cb)) {
        if (LoadImageViaGdiplus(data, cb, phbm, psz)) return TRUE;
        return LoadImageViaOle(data, cb, phbm, psz);
    }
    
    if (LoadImageViaOle(data, cb, phbm, psz)) return TRUE;
    return LoadImageViaGdiplus(data, cb, phbm, psz);
}

// ============================================================================
// Public API Implementation / Реализация публичного API
// ============================================================================

/**
 * @brief Load image from memory buffer
 * @brief Загрузить изображение из буфера памяти
 */
int __cdecl Img_LoadFromMemoryToBitmap(const BYTE* buf, DWORD sz, HBITMAP* phbm, SIZE* psz) {
    // Validate input / Проверка входных данных
    if (!buf || !sz || sz > kMaxImageBytes) return FALSE;
    
    // Check format signature / Проверка сигнатуры формата
    if (!LooksLikeKnownImage(buf, sz)) return FALSE;
    
    return TryLoadImage(buf, sz, phbm, psz);
}

/**
 * @brief Load image from file
 * @brief Загрузить изображение из файла
 */
int __cdecl Img_LoadFromFileA(const char* path, HBITMAP* phbm, SIZE* psz) {
    if (!path || !*path) return FALSE;

    // Open file / Открыть файл
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;

    // Get file size / Получить размер файла
    DWORD total = GetFileSize(f, NULL);
    if (total == INVALID_FILE_SIZE || total == 0 || total > kMaxImageBytes) { 
        CloseHandle(f); 
        return FALSE; 
    }

    // Read entire file / Прочитать весь файл
    BYTE* buf = (BYTE*)GlobalAlloc(GMEM_FIXED, total);
    if (!buf) { 
        CloseHandle(f); 
        return FALSE; 
    }
    
    DWORD got = 0;
    BOOL ok = ReadFile(f, buf, total, &got, NULL);
    CloseHandle(f);
    
    if (!ok || got != total) { 
        GlobalFree(buf); 
        return FALSE; 
    }

    // Validate format / Проверить формат
    if (!LooksLikeKnownImage(buf, total)) { 
        GlobalFree(buf); 
        return FALSE; 
    }

    // Load image / Загрузить изображение
    int ret = TryLoadImage(buf, total, phbm, psz);
    GlobalFree(buf);
    return ret;
}
