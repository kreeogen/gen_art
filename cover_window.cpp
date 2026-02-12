/**
 * @file cover_window.cpp
 * @brief Album cover art viewer implementation (Optimized)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h> 
#include <stdio.h>

#pragma comment(lib, "shlwapi.lib")

#include "SDK\wa_dlg.h"
#include "SwitchLangUI.h"
#include "cover_window.h"
#include "image_loader.h"
#include "skin_util.h"
#include "ini_store.h"

// Provided by plugin_main.cpp
extern HWND UIHost_GetWinampWnd();
extern HINSTANCE UIHost_GetHInstance();

#include "Extensions\id3v2_reader.h"
#include "Extensions\flac_reader.h"
#include "Extensions\ape_reader.h"
#include "Extensions\mp4_reader.h"

// ============================================================================
// Constants and Macros
// ============================================================================

#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)
#endif

#ifndef IPC_GETLISTPOS
#define IPC_GETLISTPOS 125  
#endif

#ifndef IPC_GETPLAYLISTFILE
#define IPC_GETPLAYLISTFILE 211  
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

#define TAG_RETRY_TIMER_ID 2  

// ============================================================================
// Global State
// ============================================================================

static HWND  s_view          = NULL;  
static HBITMAP s_hbm          = NULL;  
static SIZE    s_bm           = {0,0}; 
static UINT    s_timer        = 0;     
static char    s_lastPath[MAX_PATH] = {0}; 
static int     s_retryTries   = 0;     
static ATOM    s_cls          = 0;     // window class atom / атом класса окна

// ============================================================================
// Helper Functions
// ============================================================================

static HWND FindWinamp() {
    HWND wa = UIHost_GetWinampWnd();
    if (wa && IsWindow(wa)) return wa;
    return FindWindowA("Winamp v1.x", NULL);
}

static int ascii_icmp(const char* a, const char* b) { 
    return lstrcmpiA(a, b);
}

static BOOL IsTagReadingSupported(const char* lpszPath)
{
    if (!lpszPath || !*lpszPath) return FALSE;
    const char* pszExt = PathFindExtensionA(lpszPath);
    if (!pszExt || *pszExt == 0) return FALSE;

    const char* szSupported[] = {
        ".mp3", ".flac", ".fla", ".m4a", ".m4b", 
        ".mp4", ".m4v", ".mov", ".ape", ".mpc", ".wv"
    };

    for (int i = 0; i < ARRAYSIZE(szSupported); i++) {
        if (ascii_icmp(pszExt, szSupported[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL GetCurrentSongPathA(char* out, int cch)
{
    HWND wa = FindWinamp(); 
    if (!wa) return FALSE;
    int pos = (int)SendMessageA(wa, WM_WA_IPC, 0, IPC_GETLISTPOS); 
    if (pos < 0) return FALSE;
    const char* p = (const char*)SendMessageA(wa, WM_WA_IPC, pos, IPC_GETPLAYLISTFILE);
    if (!p || !*p) return FALSE; 
    lstrcpynA(out, p, cch); 
    return TRUE;
}

static void SafeResetBitmap() { 
    if (s_hbm) { 
        DeleteObject(s_hbm); 
        s_hbm = NULL; 
    } 
    s_bm.cx = s_bm.cy = 0; 
}

static BOOL IsHttpUrl(const char* path) {
    if (!path) return FALSE;
    return PathIsURLA(path);
}

static void StopRetry() { 
    if (s_view && IsWindow(s_view)) KillTimer(s_view, TAG_RETRY_TIMER_ID); 
    s_retryTries = 0; 
}

static void StartRetry() { 
    if (!s_view || !IsWindow(s_view)) return; 
    s_retryTries = 8; 
    SetTimer(s_view, TAG_RETRY_TIMER_ID, 300, NULL); 
}

// ============================================================================
// Cover Art Search Logic
// ============================================================================

static BOOL TryLoadBesideA(const char* audioPath)
{
    if (IsHttpUrl(audioPath)) return FALSE;
    char dir[MAX_PATH];
    lstrcpynA(dir, audioPath, MAX_PATH);
    PathRemoveFileSpecA(dir); 

    static const char* kNames[] = { "cover", "folder", "front", "main", "AlbumArtSmall", "AlbumArt" };
    static const char* kExts[]  = { ".jpg", ".jpeg", ".png", ".bmp" };
    char testPath[MAX_PATH];

    for (int i = 0; i < ARRAYSIZE(kNames); ++i) {
        for (int j = 0; j < ARRAYSIZE(kExts); ++j) {
            wsprintfA(testPath, "%s\\%s%s", dir, kNames[i], kExts[j]);
            if (PathFileExistsA(testPath)) {
                HBITMAP hb; SIZE s;
                if (Img_LoadFromFileA(testPath, &hb, &s)) {
                    SafeResetBitmap();
                    s_hbm = hb; s_bm = s;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

static void LoadForPathA(const char* path)
{
    if (!path || !*path) return;
    
    if (IsHttpUrl(path)) {
        lstrcpynA(s_lastPath, path, MAX_PATH);
        SafeResetBitmap();
        if (s_view) InvalidateRect(s_view, NULL, TRUE);
        return;
    }

    if (s_hbm && ascii_icmp(path, s_lastPath) == 0) return;

    HBITMAP hb = 0;
    SIZE    sz = {0,0};
    BOOL    loaded = FALSE;

    if (IsTagReadingSupported(path)) 
    {
        if      (ID3v2_LoadCoverToBitmapA(path, &hb, &sz)) loaded = TRUE;
        else if (FLAC_LoadCoverToBitmapA (path, &hb, &sz)) loaded = TRUE;
        else if (MP4_LoadCoverToBitmapA  (path, &hb, &sz)) loaded = TRUE;
        else if (APE_LoadCoverToBitmapA  (path, &hb, &sz)) loaded = TRUE;
    }

    if (loaded && hb) {
        SafeResetBitmap();
        s_hbm = hb; s_bm = sz;
        StopRetry();
    } 
    else if (TryLoadBesideA(path)) {
        StopRetry();
    } 
    else {
        SafeResetBitmap();
        StartRetry();
    }

    lstrcpynA(s_lastPath, path, MAX_PATH);

    // УДАЛЕНО: WADlg_init и Skin_RefreshDialogBrush. 
    // Эти функции вызывали SendMessage к главному окну, что приводило к Deadlock 
    // в Modern скинах при запуске файла из Медиатеки.
    
    if (s_view && IsWindow(s_view)) {
        InvalidateRect(s_view, NULL, TRUE);
        // UpdateWindow убран для предотвращения лишней синхронности
    }
}

// ============================================================================
// Window Procedure
// ============================================================================

static LRESULT CALLBACK ViewProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_CREATE:
        s_timer = SetTimer(h, 1, 700, NULL);
        // Инициализируем кисть скина один раз при создании
        WADlg_init(FindWinamp());
        Skin_RefreshDialogBrush();
        return 0;

    case WM_TIMER:
        if (w == 1) {
            char cur[MAX_PATH];
            if (GetCurrentSongPathA(cur, MAX_PATH)) {
                if (ascii_icmp(cur, s_lastPath) != 0) {
                    LoadForPathA(cur);
                }
            }
            return 0;
        }
        
        if (w == TAG_RETRY_TIMER_ID) {
            if (s_retryTries > 0 && s_lastPath[0] && !IsHttpUrl(s_lastPath) && IsTagReadingSupported(s_lastPath)) {
                HBITMAP hb = NULL; SIZE sz;
                if (ID3v2_LoadCoverToBitmapA(s_lastPath, &hb, &sz) ||
                    FLAC_LoadCoverToBitmapA (s_lastPath, &hb, &sz) ||
                    MP4_LoadCoverToBitmapA  (s_lastPath, &hb, &sz) ||
                    APE_LoadCoverToBitmapA  (s_lastPath, &hb, &sz))
                {
                    SafeResetBitmap();
                    s_hbm = hb; s_bm = sz;
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
        return 1;

    case WM_SIZE: 
        InvalidateRect(h, NULL, TRUE); 
        return 0;

    case WM_SYSCOLORCHANGE:
    case WM_DISPLAYCHANGE:
        // Обновляем цвета только когда скин реально меняется
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

        if (W > 0 && H > 0 && dc)
        {
            HDC mem = CreateCompatibleDC(dc);
            if (mem)
            {
                HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
                if (bmp)
                {
                    HGDIOBJ old = SelectObject(mem, bmp);

                    HBRUSH br = Skin_GetDialogBrush();
                    if (br) FillRect(mem, &rc, br);
                    else    FillRect(mem, &rc, (HBRUSH)(COLOR_WINDOW + 1));

                    if (s_hbm && s_bm.cx > 0 && s_bm.cy > 0)
                    {
                        double sx = (double)W / (double)s_bm.cx;
                        double sy = (double)H / (double)s_bm.cy;
                        double s = (sx < sy) ? sx : sy;

                        int wdst = (int)(s_bm.cx * s);
                        int hdst = (int)(s_bm.cy * s);
                        int x = (W - wdst) / 2;
                        int y = (H - hdst) / 2;

                        HDC src = CreateCompatibleDC(dc);
                        if (src)
                        {
                            HGDIOBJ oS = SelectObject(src, s_hbm);

                            int mode = (s < 1.0) ? HALFTONE : COLORONCOLOR;
                            SetStretchBltMode(mem, mode);
                            if (mode == HALFTONE) SetBrushOrgEx(mem, 0, 0, NULL);

                            StretchBlt(mem, x, y, wdst, hdst, src, 0, 0, s_bm.cx, s_bm.cy, SRCCOPY);

                            SelectObject(src, oS);
                            DeleteDC(src);
                        }
                    }
                    else
                    {
                        SetBkMode(mem, TRANSPARENT);
                        SetTextColor(mem, RGB(160, 160, 160));
                        DrawTextA(mem, STR_NO_COVER, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }

                    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);

                    SelectObject(mem, old);
                    DeleteObject(bmp);
                }
                DeleteDC(mem);
            }
        }

        EndPaint(h, &ps);
        return 0;
    }
case WM_NCDESTROY:
        // Final cleanup: if our DLL is unloaded, class must not remain registered
        // Финальная очистка: при выгрузке DLL класс не должен оставаться зарегистрированным
        if (h == s_view) {
            s_view = NULL;
        }
        // If class was registered, unregister it now (safe when last window is gone)
        if (s_cls) {
            HINSTANCE hi = UIHost_GetHInstance();
            if (!hi) hi = GetModuleHandleA(NULL);
            UnregisterClassA("APT_CoverArtView", hi);
            s_cls = 0;
        }
        return 0;

    case WM_DESTROY:
        if (s_timer) KillTimer(h, s_timer);
        StopRetry(); 
        if (h == s_view) s_view = NULL;
        SafeResetBitmap();
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

// ============================================================================
// Public API Implementation
// ============================================================================

void CoverView_Attach(HWND parent)
{
    if (!parent || !IsWindow(parent)) return;

    if (!s_view || !IsWindow(s_view))
    {
        if (!s_cls)
        {
            WNDCLASSA wc = {0};
            wc.lpfnWndProc   = ViewProc;
            wc.hInstance     = UIHost_GetHInstance();
            if (!wc.hInstance) wc.hInstance = GetModuleHandleA(NULL);
            wc.lpszClassName = "APT_CoverArtView";
            wc.style         = CS_HREDRAW | CS_VREDRAW;
            s_cls = RegisterClassA(&wc);
        }

        RECT rc; 
        GetClientRect(parent, &rc);

        HINSTANCE hi = UIHost_GetHInstance();
        if (!hi) hi = GetModuleHandleA(NULL);

        s_view = CreateWindowExA(0, "APT_CoverArtView", "",
                                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                0, 0, rc.right, rc.bottom,
                                parent, NULL, hi, NULL);

        CoverView_ReloadFromCurrent();
    }
}

void CoverView_ReloadFromCurrent()

{
    char cur[MAX_PATH];
    if (GetCurrentSongPathA(cur, MAX_PATH)) {
        LoadForPathA(cur);
    } else { 
        s_lastPath[0] = 0; 
        SafeResetBitmap(); 
        if (s_view && IsWindow(s_view)) InvalidateRect(s_view, NULL, TRUE);
    }
}

HWND CoverView_FindOn(HWND parent)
{
    return FindWindowExA(parent, NULL, "APT_CoverArtView", NULL);
}