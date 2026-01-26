# gen_art

---

## 🇬🇧 English

**gen_art** is a lightweight cover art viewer plug-in for Winamp (focused on the classic Winamp experience).  
It extracts and displays embedded album artwork using dedicated tag readers, with minimal overhead and a Winamp-style UI.

The goal is simple: **fast cover art preview** without turning Winamp into a “spaceship cockpit”.

---

### Features

- Displays cover art in a separate window (Winamp-friendly look)
- Reads embedded artwork from common formats:
  - **ID3v2** (MP3)
  - **APE tags**
  - **FLAC PICTURE blocks**
  - **MP4/M4A** cover atoms
- Caches / reuses decoded images to reduce CPU and disk usage
- Remembers window position (INI-based settings)
- Skin-aware helpers (better integration with different Winamp skins)

---

### Install

1. Copy `gen_art.dll` into: `Winamp\Plugins\`
2. Restart Winamp
3. Open the cover window from the plug-in’s menu item inside Winamp

---

### Build

- Visual Studio 2003 (VC7.1)
- Windows XP and newer (recommended)

---

### Legal

This project **does not include Winamp source code**.  
It is a standalone plug-in that works through the public Winamp plug-in API.

---

------------------

## 🇷🇺 Описание (RU)

**gen_art** — это лёгкий плагин для Winamp, который показывает **обложку (Album Art)** в отдельном окне.  
Он читает встроенные картинки из тегов через отдельные ридеры форматов и старается работать **быстро и без лишнего “жира”**.

Цель простая: **быстрый просмотр обложки** в стиле Winamp, без превращения плеера в “кабину звездолёта”.

---

### Возможности

- Окно просмотра обложки (интерфейс в духе Winamp)
- Чтение встроенных обложек из популярных форматов:
  - **ID3v2** (MP3)
  - **APE теги**
  - **FLAC PICTURE блоки**
  - **MP4/M4A** обложка в контейнере
- Кэширование / повторное использование декодированных изображений (меньше нагрузки на CPU/диск)
- Запоминает позицию окна (настройки через INI)
- Утилиты для лучшей интеграции со скинами

---

### Установка

1. Скопируй `gen_art.dll` в: `Winamp\Plugins\`
2. Перезапусти Winamp
3. Открой окно обложки через добавленный пункт меню плагина

---

### Сборка

- Visual Studio 2003 (VC7.1)
- Windows XP и новее (рекомендуется)

---

### Юридическая информация

Проект **не содержит исходный код Winamp**.  
Это отдельный plug-in, работающий через публичный Winamp plug-in API.
