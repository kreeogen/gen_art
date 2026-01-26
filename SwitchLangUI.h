/*******************************************************************************
 * LANGUAGE SELECTION / ВЫБОР ЯЗЫКА
 * 
 * Set to 0 for English, 1 for Russian
 * Установите 0 для английского, 1 для русского
 ******************************************************************************/
#ifndef BUILD_LANG
#define BUILD_LANG 0
#endif


/*******************************************************************************
 * LOCALIZATION STRINGS / СТРОКИ ЛОКАЛИЗАЦИИ
 ******************************************************************************/

#if BUILD_LANG == 0
	/* ========== ENGLISH ========== */
	static const TCHAR kMenuText[] = TEXT("Album Art\tAlt+A");
	static const TCHAR kAptTitle[] = TEXT("ALBUM ART");
	#define APP_NAME         "Album Art Viewer for Winamp"
	#define STR_NO_COVER     "no cover"
	#define APP_CONFIG       "Album Art Viewer for Winamp\nReads covers from tags: \n\n * MP3 ID3v2.3\\ID3v2.4\\APE\\APEv2\n * FLAC\n * AAC\n\nor from cover image files in the track's folder\n(cover, folder, front, AlbumArtSmall, AlbumArt)\nby extension bmp/jpg/jpeg/png.\n\nSpecial for Winamp PE & IF \nkreeogen & IFkO (2026)"
	#define APP_CONFIG_TITLE "About"

#elif BUILD_LANG == 1
	/* ========== RUSSIAN ========== */
	static const char kMenuText[] = "Обложка альбома\tAlt+A";
	static const char kAptTitle[] = "APT";
	#define APP_NAME         "Просмотр обложек альбомов для Winamp"
	#define STR_NO_COVER     "Нет обложки"
	#define APP_CONFIG       "Просмотр обложек альбомов для Winamp\nСчитывает обложки из файлов: \n\n * MP3 ID3v2.3\\ID3v2.4\\APE\\APEv2\n * FLAC\n * AAC\n\nили из папки с аудиофайлом с названиями\n(cover, folder, front, AlbumArtSmall, AlbumArt)\nс расширениями bmp/jpg/jpeg/png.\n\nСпециально для IF и Пиратской Версии \nkreeogen & IFkO (2026)"
	#define APP_CONFIG_TITLE "О модуле"

#endif