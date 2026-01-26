Winamp Classic Album Art Plugin (gen_art)
A lightweight, high-performance General Purpose plugin for Winamp that displays album artwork for the currently playing track.

Designed with a focus on legacy compatibility (Windows 98/ME/2000) while supporting modern features like PNG transparency and high-quality scaling. It seamlessly integrates with Winamp's skinning engine and window management.

✨ Features
🖼️ Artwork Support
Embedded Art: Extracts covers from tags in:

MP3 (ID3v2.2, ID3v2.3, ID3v2.4 frames PIC/APIC).

FLAC (Metadata block type 6, PICTURE).

APE / MPC / WavPack (APEv2 tags, binary items).

M4A / MP4 (iTunes metadata atom covr).

External Files: Automatically looks for cover art in the track's directory:

Filenames: cover, folder, front, main, AlbumArt etc.

Formats: JPG, PNG, BMP, GIF.

🚀 Performance & Compatibility
Legacy OS Support: Runs natively on Windows 98 SE and up.

Smart Rendering Engine:

Uses OLE IPicture for basic formats (native Win98 support).

Dynamically loads GDI+ (if available) for superior PNG support and scaling.

Memory Efficient: Zero-cost abstractions for binary parsing and RAII file handles.

Stable: Uses thread-local hooks and safe menu injection techniques to prevent crashes or conflicts with other plugins.

🎨 Integration
Skin Support: The window background automatically adapts to the current Winamp skin (Classic & Modern) colors.

Winamp Docking: Acts as a native tool window within Winamp.

Hotkeys: Press Alt+A to toggle the artwork window instantly.

Portable: Configuration is saved to plugin.ini next to the DLL, keeping the registry clean.

🛠️ Installation
Download gen_art.dll from the Releases page.

Copy the file into your Winamp Plugins directory:

Example: C:\Program Files\Winamp\Plugins\

Restart Winamp.

Enable/Disable via Options -> Preferences -> Plug-ins -> General Purpose.

🎮 Usage
Toggle Window: Press Alt+A or use the menu item in Options -> Album Art.

Context Menu: Right-click the window to access standard options (future feature).

Resizing: The window is resizable; the image scales while preserving the aspect ratio.

🏗️ Building from Source
This project is configured for compatibility with older compilers (Visual Studio 2003 / VC6) to ensure zero-dependency binaries for Windows 98.

Prerequisites
Visual Studio 2003, 2005, 2008, or newer (with appropriate toolset).

Winamp SDK headers (included in SDK/ folder).

Windows SDK (compatible with target OS).

Build Steps
Open gen_art.sln.

Select Release configuration.

Build solution.

Output file will be in Release/gen_art.dll.

📂 Project Structure
plugin_main.cpp: DLL Entry point and Winamp export structure.

ui_host.cpp: Core plugin manager. Handles lifecycle, menus, and message hooks.

cover_window.cpp: The UI window implementation. Handles painting and file logic.

image_loader.cpp: Hybrid GDI+/OLE image decoder.

*_reader.cpp: Binary parsers for specific tag formats (MP3, FLAC, APE, MP4).

skin_util.cpp: Helpers for Winamp skin color extraction.

hotkeys.cpp: Safe, thread-local keyboard hook implementation.

📝 License
This project is open-source. Feel free to use, modify, and distribute.

Winamp Classic Album Art Plugin (gen_art)
Легковесный и высокопроизводительный плагин общего назначения (General Purpose) для Winamp, который отображает обложку альбома для текущего трека.

Разработан с упором на совместимость с устаревшим железом (Windows 98/ME/2000), но при этом поддерживает современные функции, такие как прозрачность PNG и качественное масштабирование. Плагин органично интегрируется с движком скинов Winamp и его системой управления окнами.

✨ Возможности
🖼️ Поддержка обложек
Встроенные обложки (Embedded Art): Извлекает изображения, зашитые в теги:

MP3 (фреймы PIC/APIC в ID3v2.2, ID3v2.3, ID3v2.4).

FLAC (блоки метаданных типа 6, PICTURE).

APE / MPC / WavPack (теги APEv2, бинарные поля).

M4A / MP4 (атом covr в метаданных iTunes).

Внешние файлы: Автоматически ищет файлы обложек в папке с треком:

Имена файлов: cover, folder, front, main, AlbumArt и другие.

Форматы: JPG, PNG, BMP, GIF.

🚀 Производительность и Совместимость
Работа на старых ОС: Нативная поддержка Windows 98 SE, ME, 2000, XP и новее.

Умный движок рендеринга:

Использует OLE IPicture для базовых форматов (нативная поддержка в Win98).

Динамически подгружает GDI+ (если библиотека доступна в системе) для качественной отрисовки PNG и масштабирования.

Эффективность памяти: Использование "абстракций с нулевой стоимостью" для бинарного парсинга и RAII-оберток для файловых дескрипторов.

Стабильность: Использование локальных хуков (thread-local hooks) и безопасная инъекция в меню предотвращают конфликты с другими плагинами и падения.

🎨 Интеграция
Поддержка скинов: Фон окна автоматически подстраивается под цвета текущего скина Winamp (работает как с Classic, так и с Modern скинами).

Встраивание: Работает как нативное инструментальное окно Winamp (Tool Window).

Горячие клавиши: Нажмите Alt+A, чтобы мгновенно показать или скрыть окно обложки.

Портативность: Все настройки сохраняются в файл plugin.ini рядом с DLL плагина, реестр не засоряется.

🛠️ Установка
Скачайте gen_art.dll из раздела Releases.

Скопируйте файл в папку плагинов Winamp:

Например: C:\Program Files\Winamp\Plugins\

Перезапустите Winamp.

Плагин можно включить/отключить через меню: Options -> Preferences -> Plug-ins -> General Purpose.

🎮 Использование
Показать/Скрыть окно: Нажмите Alt+A или выберите пункт Album Art в меню Options.

Масштабирование: Окно можно растягивать мышкой; изображение масштабируется с сохранением пропорций.

Контекстное меню: (В планах) Правый клик по окну для доступа к настройкам.

🏗️ Сборка из исходников
Проект настроен для совместимости со старыми компиляторами (Visual Studio 2003 / VC6), чтобы гарантировать работу бинарников на Windows 98 без дополнительных зависимостей.

Требования
Visual Studio 2003, 2005, 2008 или новее (с соответствующим toolset).

Winamp SDK headers (включены в папку SDK/).

Windows SDK (совместимый с целевой ОС).

Шаги сборки
Откройте файл решения gen_art.sln.

Выберите конфигурацию Release.

Соберите решение (Build Solution).

Готовый файл будет находиться в папке Release/gen_art.dll.

📂 Структура проекта
plugin_main.cpp: Точка входа DLL и структура экспорта для Winamp.

ui_host.cpp: Ядро менеджера плагина. Управляет жизненным циклом, меню и хуками сообщений.

cover_window.cpp: Реализация окна интерфейса. Отрисовка и логика поиска файлов.

image_loader.cpp: Гибридный декодер изображений (GDI+/OLE).

*_reader.cpp: Бинарные парсеры для специфичных форматов тегов (MP3, FLAC, APE, MP4).

skin_util.cpp: Хелперы для извлечения цветов из скинов Winamp.

hotkeys.cpp: Безопасная реализация локального клавиатурного хука.

📝 Лицензия
Этот проект является открытым (Open Source). Вы можете свободно использовать, модифицировать и распространять его.
