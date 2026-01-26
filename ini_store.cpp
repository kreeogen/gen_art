/**
 * @file ini_store.cpp
 * @brief Plugin configuration storage implementation
 * @brief Реализация хранилища конфигурации плагина
 * 
 * This module manages persistent plugin settings using Windows INI files.
 * The INI file ("plugin.ini") is created in the same directory as the plugin DLL,
 * making it portable and easy to backup.
 * 
 * Этот модуль управляет постоянными настройками плагина используя Windows INI файлы.
 * INI файл ("plugin.ini") создаётся в той же директории, что и DLL плагина,
 * делая его портативным и легким для резервного копирования.
 * 
 * Why INI files? / Почему INI файлы?
 * - Simple and human-readable / Простые и читаемые человеком
 * - Portable (no registry dependency) / Портативные (нет зависимости от реестра)
 * - Easy to backup and transfer / Легко создавать резервные копии и переносить
 * - Native Windows API support / Нативная поддержка Windows API
 * 
 * Technical Details / Технические детали:
 * - Uses GetPrivateProfileInt/WritePrivateProfileString
 * - Supports both Unicode and ANSI builds via TCHAR
 * - Lazy initialization (path computed on first use)
 * - Guards against invalid coordinates
 * 
 * @author [Your Name]
 * @date 2025
 * @version 1.0
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#include "ini_store.h"
#include "ui_host.h" // For getting plugin DLL instance handle / Для получения дескриптора DLL плагина

// ============================================================================
// Global State / Глобальное состояние
// ============================================================================

/**
 * @brief Cached path to the INI file
 * @brief Кэшированный путь к INI файлу
 * 
 * This is computed once on first use and reused for all subsequent operations.
 * Это вычисляется один раз при первом использовании и переиспользуется для всех последующих операций.
 */
static TCHAR s_iniPath[MAX_PATH] = {0};

// ============================================================================
// Helper Functions / Вспомогательные функции
// ============================================================================

/**
 * @brief Ensure INI file path is computed and cached
 * @brief Убедиться, что путь к INI файлу вычислен и закэширован
 * 
 * This function performs lazy initialization of the INI file path.
 * The path is built by:
 * 1. Getting the full path to the plugin DLL
 * 2. Removing the filename (leaving just the directory)
 * 3. Appending "plugin.ini"
 * 
 * Эта функция выполняет ленивую инициализацию пути к INI файлу.
 * Путь строится путём:
 * 1. Получения полного пути к DLL плагина
 * 2. Удаления имени файла (оставляя только директорию)
 * 3. Добавления "plugin.ini"
 * 
 * Example / Пример:
 * "C:\Winamp\Plugins\gen_art.dll" → "C:\Winamp\Plugins\plugin.ini"
 */
static void Ini_EnsurePath()
{
    // Skip if already computed (optimization) / Пропустить если уже вычислен (оптимизация)
    if (s_iniPath[0]) return;

    // 1. Get our plugin's DLL instance handle / Получить дескриптор DLL нашего плагина
    HINSTANCE h = UIHost_GetHInstance();
    
    // 2. Get full path to our DLL / Получить полный путь к нашей DLL
    GetModuleFileName(h, s_iniPath, MAX_PATH);

    // 3. Find last backslash to separate directory from filename
    //    Найти последний обратный слеш для отделения директории от имени файла
    TCHAR* slash = NULL; 
    for (TCHAR* p = s_iniPath; *p; ++p) {
        if (*p == TEXT('\\')) slash = p;
    }

    // 4. Replace DLL filename with INI filename / Заменить имя DLL на имя INI
#if defined(UNICODE) || defined(_UNICODE)
    if (slash) lstrcpyW(slash + 1, L"plugin.ini");
#else
    if (slash) lstrcpyA(slash + 1, "plugin.ini");
#endif
}

// ============================================================================
// Public API Implementation / Реализация публичного API
// ============================================================================

/**
 * @brief Load saved window geometry from configuration
 * @brief Загрузить сохранённую геометрию окна из конфигурации
 * 
 * Reads window position and size from the INI file's [Album Art] section.
 * If any value is missing or invalid, returns false to indicate first run.
 * 
 * Читает позицию и размер окна из секции [Album Art] INI файла.
 * Если любое значение отсутствует или некорректно, возвращает false для индикации первого запуска.
 * 
 * INI structure / Структура INI:
 * [Album Art]
 * x=100
 * y=100
 * w=300
 * h=300
 * 
 * @param x [out] Window X coordinate / Координата окна X
 * @param y [out] Window Y coordinate / Координата окна Y
 * @param w [out] Window width / Ширина окна
 * @param h [out] Window height / Высота окна
 * @return true if valid geometry was loaded, false if defaults should be used
 * @return true если загружена валидная геометрия, false если нужно использовать значения по умолчанию
 */
bool Ini_LoadWindowPos(int& x, int& y, int& w, int& h)
{
    Ini_EnsurePath();

    // Read integer values from INI file / Чтение целочисленных значений из INI файла
    // GetPrivateProfileInt returns the default (-1) if key doesn't exist
    // GetPrivateProfileInt возвращает значение по умолчанию (-1), если ключ не существует
    x = GetPrivateProfileInt(TEXT("Album Art"), TEXT("x"), -1, s_iniPath);
    y = GetPrivateProfileInt(TEXT("Album Art"), TEXT("y"), -1, s_iniPath);
    w = GetPrivateProfileInt(TEXT("Album Art"), TEXT("w"), -1, s_iniPath);
    h = GetPrivateProfileInt(TEXT("Album Art"), TEXT("h"), -1, s_iniPath);

    // Consider load successful only if position is valid / Считать загрузку успешной только если позиция валидна
    // Note: We only check x,y because w,h might legitimately be -1 (minimized?)
    // Примечание: Мы проверяем только x,y потому что w,h могут быть -1 (свёрнуто?)
    return (x != -1 && y != -1);
}

/**
 * @brief Save current window geometry to configuration
 * @brief Сохранить текущую геометрию окна в конфигурацию
 * 
 * Writes window position and size to the INI file.
 * Guards against saving invalid coordinates (-1).
 * 
 * Записывает позицию и размер окна в INI файл.
 * Защищается от сохранения некорректных координат (-1).
 * 
 * @param x Window X coordinate / Координата окна X
 * @param y Window Y coordinate / Координата окна Y
 * @param w Window width / Ширина окна
 * @param h Window height / Высота окна
 * 
 * @note Silently returns if coordinates are invalid / Тихо возвращается если координаты некорректны
 * @note Creates INI file if it doesn't exist / Создаёт INI файл, если он не существует
 */
void Ini_SaveWindowPos(int x, int y, int w, int h)
{
    Ini_EnsurePath();
    
    // Don't save invalid coordinates / Не сохранять некорректные координаты
    // This could happen if window was never properly positioned
    // Это может произойти если окно никогда не было правильно позиционировано
    if (x == -1 || y == -1) return;

    TCHAR buf[32];

    // Windows API only accepts strings, so convert integers to strings
    // Windows API принимает только строки, поэтому конвертируем целые числа в строки
    
#if defined(UNICODE) || defined(_UNICODE)
    wsprintfW(buf, L"%d", x); WritePrivateProfileStringW(L"Album Art", L"x", buf, s_iniPath);
    wsprintfW(buf, L"%d", y); WritePrivateProfileStringW(L"Album Art", L"y", buf, s_iniPath);
    wsprintfW(buf, L"%d", w); WritePrivateProfileStringW(L"Album Art", L"w", buf, s_iniPath);
    wsprintfW(buf, L"%d", h); WritePrivateProfileStringW(L"Album Art", L"h", buf, s_iniPath);
#else
    wsprintfA(buf, "%d", x); WritePrivateProfileStringA("Album Art", "x", buf, s_iniPath);
    wsprintfA(buf, "%d", y); WritePrivateProfileStringA("Album Art", "y", buf, s_iniPath);
    wsprintfA(buf, "%d", w); WritePrivateProfileStringA("Album Art", "w", buf, s_iniPath);
    wsprintfA(buf, "%d", h); WritePrivateProfileStringA("Album Art", "h", buf, s_iniPath);
#endif
}

// ============================================================================
// Window Visibility State Management / Управление состоянием видимости окна
// ============================================================================

/**
 * @brief Load saved window visibility state
 * @brief Загрузить сохранённое состояние видимости окна
 * 
 * Reads whether the window should be open on plugin startup.
 * This allows the plugin to remember if the user had the window
 * open or closed when Winamp was last shut down.
 * 
 * Читает, должно ли окно быть открыто при запуске плагина.
 * Это позволяет плагину запомнить, было ли окно открыто или
 * закрыто когда Winamp был последний раз закрыт.
 * 
 * INI structure / Структура INI:
 * [Album Art]
 * open=1  ; 1=open, 0=closed
 * 
 * @param isOpen [out] Window state (0=closed, 1=open) / Состояние окна (0=закрыто, 1=открыто)
 * @return true if state was loaded, false if default should be used
 * @return true если состояние загружено, false если нужно использовать значение по умолчанию
 */
bool Ini_LoadWindowOpen(int& isOpen)
{
    Ini_EnsurePath();
    isOpen = GetPrivateProfileInt(TEXT("Album Art"), TEXT("open"), -1, s_iniPath);
    
    // Normalize: any non-zero positive value should be 1 / Нормализация: любое ненулевое положительное значение должно быть 1
    // This guards against corruption (e.g., if someone manually edited "open=42")
    // Это защищает от повреждения (например, если кто-то вручную отредактировал "open=42")
    if (isOpen > 0) isOpen = 1; 
    
    return (isOpen != -1);
}

/**
 * @brief Save current window visibility state
 * @brief Сохранить текущее состояние видимости окна
 * 
 * Writes window visibility state to the INI file.
 * The state is normalized to 0 or 1 before saving.
 * 
 * Записывает состояние видимости окна в INI файл.
 * Состояние нормализуется до 0 или 1 перед сохранением.
 * 
 * @param isOpen Window state (any non-zero = open) / Состояние окна (любое ненулевое = открыто)
 */
void Ini_SaveWindowOpen(int isOpen)
{
    Ini_EnsurePath();
    TCHAR buf[8];

    // Convert boolean/int to string "1" or "0" / Конвертировать boolean/int в строку "1" или "0"
    // Normalize non-zero to 1 / Нормализовать ненулевое в 1
#if defined(UNICODE) || defined(_UNICODE)
    wsprintfW(buf, L"%d", isOpen ? 1 : 0);
    WritePrivateProfileStringW(L"Album Art", L"open", buf, s_iniPath);
#else
    wsprintfA(buf, "%d", isOpen ? 1 : 0);
    WritePrivateProfileStringA("Album Art", "open", buf, s_iniPath);
#endif
}
