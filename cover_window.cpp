/**
 * @file cover_window.cpp
 * @brief Album cover art viewer implementation (Optimized)
 * @brief Реализация просмотрщика обложек альбомов (Оптимизированная)
 * 
 * This module implements a custom window that displays album artwork for the
 * currently playing track in Winamp. It supports both embedded artwork (from
 * ID3v2, FLAC, APE, MP4 tags) and external image files (cover.jpg, folder.png, etc.)
 * 
 * Этот модуль реализует кастомное окно, отображающее обложку альбома для
 * текущего играющего трека в Winamp. Поддерживает как встроенные обложки (из
 * тегов ID3v2, FLAC, APE, MP4), так и внешние файлы изображений (cover.jpg, folder.png и т.д.)
 * 
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h> // PathRemoveFileSpec, PathFileExists, PathIsURL
#include <stdio.h>

#pragma comment(lib, "shlwapi.lib")

// ============================================================================
// Project Headers / Заголовки проекта
// ============================================================================
#include "SDK\wa_dlg.h"
#include "SwitchLangUI.h"
#include "cover_window.h"
#include "image_loader.h"
#include "skin_util.h"
#include "ini_store.h"

// ============================================================================
// Tag Readers / Ридеры тегов
// ============================================================================
#include "Extensions\id3v2_reader.h"
#include "Extensions\flac_reader.h"
#include "Extensions\ape_reader.h"
#include "Extensions\mp4_reader.h"

// ============================================================================
// Constants and Macros / Константы и макросы
// ============================================================================

#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)
#endif

#ifndef IPC_GETLISTPOS
#define IPC_GETLISTPOS 125  ///< Get current playlist position
#endif

#ifndef IPC_GETPLAYLISTFILE
#define IPC_GETPLAYLISTFILE 211  ///< Get file path at playlist position
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

// ============================================================================
// Global State / Глобальное состояние
// ============================================================================

static HWND    s_view        = NULL;           ///< Handle to cover viewer window / Дескриптор окна просмотрщика
static HBITMAP s_hbm         = NULL;           ///< Current cover bitmap / Текущий bitmap обложки
static SIZE    s_bm          = {0,0};          ///< Bitmap dimensions / Размеры bitmap'а
static UINT    s_timer       = 0;              ///< Main monitoring timer ID / ID основного таймера мониторинга
static char    s_lastPath[MAX_PATH] = {0};    ///< Last loaded track path / Путь последнего загруженного трека
static int     s_retryTries  = 0;              ///< Remaining retry attempts / Оставшиеся попытки повтора

#define TAG_RETRY_TIMER_ID 2  ///< Timer ID for tag retry mechanism / ID таймера для механизма повтора тегов

// ============================================================================
// Helper Functions / Вспомогательные функции
// ============================================================================

/**
 * @brief Find Winamp main window
 * @brief Найти главное окно Winamp
 * 
 * @return Handle to Winamp window, or NULL if not found
 * @return Дескриптор окна Winamp, или NULL если не найден
 */
static HWND FindWinamp() { 
    return FindWindowA("Winamp v1.x", NULL); 
}

/**
 * @brief Get the file path of the currently playing song
 * @brief Получить путь к файлу текущей играющей песни
 * 
 * Queries Winamp via IPC to get the current playlist position
 * and then retrieves the file path for that position.
 * 
 * Запрашивает Winamp через IPC для получения текущей позиции в плейлисте
 * и затем извлекает путь к файлу для этой позиции.
 * 
 * @param out Buffer to receive the file path / Буфер для получения пути к файлу
 * @param cch Buffer size in characters / Размер буфера в символах
 * @return TRUE if path was retrieved successfully / TRUE если путь успешно получен
 */
static BOOL GetCurrentSongPathA(char* out, int cch)
{
    HWND wa = FindWinamp(); 
    if (!wa) return FALSE;

    // Get current playlist position / Получить текущую позицию в плейлисте
    int pos = (int)SendMessageA(wa, WM_WA_IPC, 0, IPC_GETLISTPOS); 
    if (pos < 0) return FALSE;

    // Get file path at that position / Получить путь к файлу в этой позиции
    const char* p = (const char*)SendMessageA(wa, WM_WA_IPC, pos, IPC_GETPLAYLISTFILE);
    if (!p || !*p) return FALSE; 
    
    lstrcpynA(out, p, cch); 
    return TRUE;
}

/**
 * @brief Safely release the current bitmap and reset dimensions
 * @brief Безопасно освободить текущий bitmap и сбросить размеры
 */
static void SafeResetBitmap() { 
    if (s_hbm) { 
        DeleteObject(s_hbm); 
        s_hbm = NULL; 
    } 
    s_bm.cx = s_bm.cy = 0; 
}

/**
 * @brief Case-insensitive string comparison
 * @brief Сравнение строк без учёта регистра
 * 
 * @param a First string / Первая строка
 * @param b Second string / Вторая строка
 * @return 0 if equal, non-zero otherwise / 0 если равны, ненулевое значение иначе
 */
static int ascii_icmp(const char* a, const char* b) { 
    return lstrcmpiA(a, b);
}

/**
 * @brief Check if path is an HTTP/HTTPS URL
 * @brief Проверить, является ли путь HTTP/HTTPS URL
 * 
 * @param path Path to check / Путь для проверки
 * @return TRUE if path is a URL / TRUE если путь является URL
 */
static BOOL IsHttpUrl(const char* path) {
    if (!path) return FALSE;
    return PathIsURLA(path);
}

// ============================================================================
// Cover Art Search Logic / Логика поиска обложек
// ============================================================================

/**
 * @brief Search for external cover art files in the audio file's directory
 * @brief Поиск внешних файлов обложек в директории аудиофайла
 * 
 * Searches for common cover art filenames (cover.jpg, folder.png, etc.)
 * in the same directory as the audio file. This is used as a fallback
 * when embedded artwork is not found in the audio tags.
 * 
 * Ищет общие имена файлов обложек (cover.jpg, folder.png и т.д.)
 * в той же директории, что и аудиофайл. Используется как запасной вариант
 * когда встроенная обложка не найдена в тегах аудио.
 * 
 * Search priority / Приоритет поиска:
 * 1. cover.{jpg,jpeg,png,bmp}
 * 2. folder.{jpg,jpeg,png,bmp}
 * 3. front.{jpg,jpeg,png,bmp}
 * 4. main.{jpg,jpeg,png,bmp}
 * 5. AlbumArtSmall.{jpg,jpeg,png,bmp}
 * 6. AlbumArt.{jpg,jpeg,png,bmp}
 * 
 * @param audioPath Path to the audio file / Путь к аудиофайлу
 * @return TRUE if cover art was found and loaded / TRUE если обложка найдена и загружена
 */
static BOOL TryLoadBesideA(const char* audioPath)
{
    // Skip HTTP URLs / Пропустить HTTP URLs
    if (IsHttpUrl(audioPath)) return FALSE;

    // Extract directory path / Извлечь путь к директории
    char dir[MAX_PATH];
    lstrcpynA(dir, audioPath, MAX_PATH);
    PathRemoveFileSpecA(dir); // Remove filename, keep directory / Убрать имя файла, оставить директорию

    // Common cover art filenames / Общие имена файлов обложек
    static const char* kNames[] = { "cover", "folder", "front", "main", "AlbumArtSmall", "AlbumArt" };
    static const char* kExts[]  = { ".jpg", ".jpeg", ".png", ".bmp" };

    char testPath[MAX_PATH];

    // Try all combinations / Попробовать все комбинации
    for (int i = 0; i < ARRAYSIZE(kNames); ++i) {
        for (int j = 0; j < ARRAYSIZE(kExts); ++j) {
            // Build path: dir\name.ext / Построить путь: dir\name.ext
            wsprintfA(testPath, "%s\\%s%s", dir, kNames[i], kExts[j]);

            // Check if file exists / Проверить существование файла
            if (PathFileExistsA(testPath)) {
                HBITMAP hb; 
                SIZE s;
                // Try to load the image / Попытаться загрузить изображение
                if (Img_LoadFromFileA(testPath, &hb, &s)) {
                    SafeResetBitmap();
                    s_hbm = hb; 
                    s_bm = s;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/**
 * @brief Stop the tag retry timer
 * @brief Остановить таймер повтора тегов
 */
static void StopRetry() { 
    if (s_view && IsWindow(s_view)) {
        KillTimer(s_view, TAG_RETRY_TIMER_ID); 
    }
    s_retryTries = 0; 
}

/**
 * @brief Start the tag retry timer
 * @brief Запустить таймер повтора тегов
 * 
 * Some audio taggers write tags asynchronously, so the artwork
 * might not be immediately available when the file starts playing.
 * This timer retries loading the artwork several times over ~2.4 seconds.
 * 
 * Некоторые редакторы тегов записывают теги асинхронно, поэтому обложка
 * может быть недоступна сразу когда файл начинает проигрываться.
 * Этот таймер повторяет попытки загрузки обложки несколько раз за ~2.4 секунды.
 */
static void StartRetry() { 
    if (!s_view || !IsWindow(s_view)) return; 
    s_retryTries = 8; // 8 attempts * 300ms = 2.4 seconds
    SetTimer(s_view, TAG_RETRY_TIMER_ID, 300, NULL); 
}

/**
 * @brief Load cover art for the specified audio file
 * @brief Загрузить обложку для указанного аудиофайла
 * 
 * This is the main loading logic that:
 * 1. Skips if already loaded the same file
 * 2. Tries embedded artwork (ID3v2, FLAC, MP4, APE)
 * 3. Falls back to external files (cover.jpg, folder.png, etc.)
 * 4. Starts retry timer if nothing found
 * 
 * Это основная логика загрузки, которая:
 * 1. Пропускает, если уже загружен тот же файл
 * 2. Пробует встроенную обложку (ID3v2, FLAC, MP4, APE)
 * 3. Откатывается к внешним файлам (cover.jpg, folder.png и т.д.)
 * 4. Запускает таймер повтора, если ничего не найдено
 * 
 * @param path Path to the audio file / Путь к аудиофайлу
 */
static void LoadForPathA(const char* path)
{
    if (!path || !*path) return;
    
    // Handle HTTP streams / Обработка HTTP потоков
    if (IsHttpUrl(path)) {
        lstrcpynA(s_lastPath, path, MAX_PATH);
        SafeResetBitmap();
        InvalidateRect(s_view, NULL, TRUE);
        return;
    }

    // Skip if already loaded and has artwork / Пропустить если уже загружен и есть обложка
    // (but retry if no artwork, in case tags were just written)
    // (но повторить если нет обложки, на случай если теги только что записаны)
    if (s_hbm && ascii_icmp(path, s_lastPath) == 0) return;

    HBITMAP hb = 0;
    SIZE    sz = {0,0};
    BOOL    loaded = FALSE;

    // Priority: Try embedded artwork first / Приоритет: Сначала попробовать встроенную обложку
    // ID3v2 (MP3) > FLAC > MP4 (M4A) > APE
    if      (ID3v2_LoadCoverToBitmapA(path, &hb, &sz)) loaded = TRUE;
    else if (FLAC_LoadCoverToBitmapA (path, &hb, &sz)) loaded = TRUE;
    else if (MP4_LoadCoverToBitmapA  (path, &hb, &sz)) loaded = TRUE;
    else if (APE_LoadCoverToBitmapA  (path, &hb, &sz)) loaded = TRUE;

    if (loaded && hb) {
        // Embedded artwork found / Встроенная обложка найдена
        SafeResetBitmap();
        s_hbm = hb; 
        s_bm = sz;
        StopRetry();
    } 
    else if (TryLoadBesideA(path)) {
        // External file found / Внешний файл найден
        StopRetry();
    } 
    else {
        // Nothing found - start retry timer / Ничего не найдено - запустить таймер повтора
        SafeResetBitmap();
        StartRetry();
    }

    // Remember this path / Запомнить этот путь
    lstrcpynA(s_lastPath, path, MAX_PATH);

    // Update UI / Обновить интерфейс
    WADlg_init(FindWinamp());
    Skin_RefreshDialogBrush();
    if (s_view && IsWindow(s_view)) {
        InvalidateRect(s_view, NULL, TRUE);
        UpdateWindow(s_view);
    }
}

// ============================================================================
// Window Procedure / Оконная процедура
// ============================================================================

/**
 * @brief Main window procedure for the cover viewer
 * @brief Главная оконная процедура для просмотрщика обложек
 * 
 * Handles all window messages including:
 * - WM_CREATE: Initialize timers and skin
 * - WM_TIMER: Monitor track changes and retry tag loading
 * - WM_PAINT: Render the cover art with double buffering
 * - WM_SIZE: Trigger redraw on resize
 * - WM_SYSCOLORCHANGE/WM_DISPLAYCHANGE: Update skin on system changes
 * - WM_DESTROY: Clean up resources
 * 
 * Обрабатывает все оконные сообщения включая:
 * - WM_CREATE: Инициализация таймеров и скина
 * - WM_TIMER: Мониторинг смены треков и повтор загрузки тегов
 * - WM_PAINT: Отрисовка обложки с двойной буферизацией
 * - WM_SIZE: Перерисовка при изменении размера
 * - WM_SYSCOLORCHANGE/WM_DISPLAYCHANGE: Обновление скина при системных изменениях
 * - WM_DESTROY: Очистка ресурсов
 */
static LRESULT CALLBACK ViewProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_CREATE:
        // Initialize main monitoring timer (checks every 700ms) / Инициализация основного таймера мониторинга (проверка каждые 700мс)
        s_timer = SetTimer(h, 1, 700, NULL);
        Skin_RefreshDialogBrush();
        return 0;

    case WM_TIMER:
        // Main track monitoring timer / Основной таймер мониторинга треков
        if (w == 1) {
            char cur[MAX_PATH];
            if (GetCurrentSongPathA(cur, MAX_PATH)) {
                // Check if track changed / Проверить, изменился ли трек
                if (ascii_icmp(cur, s_lastPath) != 0) {
                    LoadForPathA(cur);
                }
            }
            return 0;
        }
        
        // Tag retry timer (for delayed tag writes) / Таймер повтора тегов (для отложенной записи тегов)
        if (w == TAG_RETRY_TIMER_ID) {
            if (s_retryTries > 0 && s_lastPath[0] && !IsHttpUrl(s_lastPath)) {
                HBITMAP hb = NULL; 
                SIZE sz;
                
                // Only try embedded tags (external files unlikely to appear later)
                // Пробуем только встроенные теги (внешние файлы вряд ли появятся позже)
                if (ID3v2_LoadCoverToBitmapA(s_lastPath, &hb, &sz) ||
                    FLAC_LoadCoverToBitmapA (s_lastPath, &hb, &sz) ||
                    MP4_LoadCoverToBitmapA  (s_lastPath, &hb, &sz) ||
                    APE_LoadCoverToBitmapA  (s_lastPath, &hb, &sz))
                {
                    SafeResetBitmap();
                    s_hbm = hb; 
                    s_bm = sz;
                    InvalidateRect(h, NULL, TRUE);
                    StopRetry();
                    return 0;
                }
                
                --s_retryTries;
                if (s_retryTries <= 0) StopRetry();
            } else {
                StopRetry();
            }
            return 0;
        }
        break;

    case WM_ERASEBKGND: 
        // Prevent flicker by handling background in WM_PAINT / Предотвращение мерцания путём обработки фона в WM_PAINT
        return 1;

    case WM_SIZE: 
        InvalidateRect(h, NULL, TRUE); 
        return 0;

    case WM_SYSCOLORCHANGE:
    case WM_DISPLAYCHANGE:
        // Update skin when system colors or display settings change
        // Обновить скин при изменении системных цветов или настроек дисплея
        WADlg_init(FindWinamp());
        Skin_RefreshDialogBrush();
        InvalidateRect(h, NULL, TRUE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps; 
        HDC dc = BeginPaint(h, &ps);
        RECT rc; 
        GetClientRect(h, &rc);
        int W = rc.right - rc.left; 
        int H = rc.bottom - rc.top;

        if (W > 0 && H > 0) {
            // Double buffering for flicker-free rendering / Двойная буферизация для отрисовки без мерцания
            HDC mem = CreateCompatibleDC(dc); 
            HBITMAP bmp = CreateCompatibleBitmap(dc, W, H); 
            HGDIOBJ old = SelectObject(mem, bmp);
            
            // Draw background using Winamp skin brush / Отрисовка фона используя кисть скина Winamp
            if (Skin_GetDialogBrush()) {
                FillRect(mem, &rc, Skin_GetDialogBrush());
            }

            if (s_hbm) {
                // Calculate scaling to fit image within window while preserving aspect ratio
                // Вычисление масштабирования для вписывания изображения в окно с сохранением пропорций
                double sx = (double)W / (double)s_bm.cx;
                double sy = (double)H / (double)s_bm.cy;
                double s = (sx < sy) ? sx : sy; // Fit inside / Вписать внутрь

                int wdst = (int)(s_bm.cx * s);
                int hdst = (int)(s_bm.cy * s);
                int x = (W - wdst) / 2;  // Center horizontally / Центрировать по горизонтали
                int y = (H - hdst) / 2;  // Center vertically / Центрировать по вертикали

                HDC src = CreateCompatibleDC(dc); 
                HGDIOBJ oS = SelectObject(src, s_hbm);
                
                // Use HALFTONE mode for downscaling (better quality) / Использовать режим HALFTONE для уменьшения (лучшее качество)
                // Use COLORONCOLOR for upscaling (faster) / Использовать COLORONCOLOR для увеличения (быстрее)
                int mode = (s < 1.0) ? HALFTONE : COLORONCOLOR; 
                SetStretchBltMode(mem, mode); 
                if (mode == HALFTONE) {
                    SetBrushOrgEx(mem, 0, 0, NULL);
                }

                StretchBlt(mem, x, y, wdst, hdst, src, 0, 0, s_bm.cx, s_bm.cy, SRCCOPY);
                
                SelectObject(src, oS); 
                DeleteDC(src);
            } else {
                // No cover available - display placeholder text / Нет доступной обложки - показать текст-заполнитель
                SetBkMode(mem, TRANSPARENT); 
                SetTextColor(mem, RGB(160, 160, 160));
                RECT rtxt = rc; 
                DrawTextA(mem, STR_NO_COVER, -1, &rtxt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Copy from backbuffer to screen / Копирование из заднего буфера на экран
            BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
            
            SelectObject(mem, old); 
            DeleteObject(bmp); 
            DeleteDC(mem);
        }
        EndPaint(h, &ps);
        return 0;
    }

    case WM_DESTROY:
        // Clean up resources / Очистка ресурсов
        if (s_timer) KillTimer(h, s_timer);
        StopRetry(); 
        if (h == s_view) s_view = NULL;
        SafeResetBitmap();
        return 0;
    }
    
    return DefWindowProcA(h, m, w, l);
}

// ============================================================================
// Public API Implementation / Реализация публичного API
// ============================================================================

/**
 * @brief Attach/create the cover art viewer window
 * @brief Прикрепить/создать окно просмотра обложек
 * 
 * Creates the viewer window if it doesn't exist, or reattaches it to
 * the specified parent. The window fills the entire parent client area.
 * 
 * Создаёт окно просмотрщика если оно не существует, или прикрепляет его к
 * указанному родителю. Окно заполняет всю клиентскую область родителя.
 */
void CoverView_Attach(HWND parent)
{
    if (!parent || !IsWindow(parent)) return;
    
    if (!s_view || !IsWindow(s_view)) {
        // Register window class on first use / Регистрация класса окна при первом использовании
        static ATOM s_cls = 0;
        if (!s_cls) {
            WNDCLASSA wc = {0};
            wc.lpfnWndProc   = ViewProc;
            wc.hInstance     = GetModuleHandleA(NULL);
            wc.lpszClassName = "APT_CoverArtView";
            wc.hbrBackground = NULL;  // We handle background in WM_PAINT / Обрабатываем фон в WM_PAINT
            wc.style         = CS_HREDRAW | CS_VREDRAW; // Redraw on resize / Перерисовка при изменении размера
            s_cls = RegisterClassA(&wc);
        }
        
        // Create window filling parent's client area / Создание окна, заполняющего клиентскую область родителя
        RECT rc; 
        GetClientRect(parent, &rc);
        s_view = CreateWindowExA(0, "APT_CoverArtView", "", 
                               WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                               0, 0, rc.right, rc.bottom, 
                               parent, NULL, GetModuleHandleA(NULL), NULL);
                               
        CoverView_ReloadFromCurrent();
    }
}

/**
 * @brief Force reload cover art for current track
 * @brief Принудительно перезагрузить обложку для текущего трека
 */
void CoverView_ReloadFromCurrent()
{
    char cur[MAX_PATH];
    if (GetCurrentSongPathA(cur, MAX_PATH)) {
        LoadForPathA(cur);
    } else { 
        // No track playing - clear display / Не играет ни один трек - очистить дисплей
        s_lastPath[0] = 0; 
        SafeResetBitmap(); 
        if (s_view && IsWindow(s_view)) {
            InvalidateRect(s_view, NULL, TRUE); 
            UpdateWindow(s_view);
        }
    }
}

/**
 * @brief Find existing cover viewer window on parent
 * @brief Найти существующее окно просмотрщика обложек на родителе
 */
HWND CoverView_FindOn(HWND parent)
{
    return FindWindowExA(parent, NULL, "APT_CoverArtView", NULL);
}
