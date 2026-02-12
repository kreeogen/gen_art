/**
 * @file ui_host.cpp (formerly PluginManager.cpp)
 * @brief Central plugin manager for Winamp integration
 * @brief Центральный менеджер плагина для интеграции с Winamp
 *
 * This is the heart of the plugin. It manages:
 * - Plugin lifecycle (initialization, shutdown)
 * - Winamp menu integration
 * - Window hooks for message interception
 * - Keyboard shortcuts (Alt+A)
 * - Embedded window creation and management
 * - State persistence (window position, visibility)
 * - Skin integration and color changes
 *
 * Это сердце плагина. Он управляет:
 * - Жизненным циклом плагина (инициализация, завершение)
 * - Интеграцией меню Winamp
 * - Хуками окон для перехвата сообщений
 * - Горячими клавишами (Alt+A)
 * - Созданием и управлением встраиваемыми окнами
 * - Сохранением состояния (позиция окна, видимость)
 * - Интеграцией скинов и изменением цветов
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <tchar.h>

// ============================================================================
// Dependencies / Зависимости
// ============================================================================

#include "SwitchLangUI.h"
#include "SDK\gen.h"
#define WA_DLG_IMPLEMENT
#include "SDK\wa_dlg.h"

#include "resource.h"
#include "ui_host.h"
#include "ini_store.h"
#include "skin_util.h"
#include "image_loader.h"
#include "cover_window.h"
#include "Hotkeys.h"

// ============================================================================
// External Functions / Внешние функции
// ============================================================================

extern HWND      UIHost_GetWinampWnd();
extern HINSTANCE UIHost_GetHInstance();
int UIHost_Init();

// ============================================================================
// Winamp IPC Constants / Константы IPC Winamp
// ============================================================================

#ifndef WM_WA_IPC
#define WM_WA_IPC (WM_USER)
#endif

#ifndef IPC_GET_HMENU
#define IPC_GET_HMENU 0x33
#endif

#ifndef IPC_GET_EMBEDIF
#define IPC_GET_EMBEDIF 0x39
#endif

#ifndef IPC_PLAYLIST_MODIFIED
#define IPC_PLAYLIST_MODIFIED 3002
#endif

#ifndef IPC_PLAYING_FILE
#define IPC_PLAYING_FILE 3003
#endif

#ifndef IPC_GETVERSION
#define IPC_GETVERSION 0
#endif

#ifndef IPC_ADJUST_OPTIONSMENUPOS
#define IPC_ADJUST_OPTIONSMENUPOS 0x3F
#endif

// ============================================================================
// Plugin Constants / Константы плагина
// ============================================================================

#define DEFAULT_W    275
#define DEFAULT_H    116
#define WM_APT_REFRESH (WM_USER + 0x6E01)
#define WA_CMD_EQUALIZER 40258

#define MENUID_APT 0x7001
#define EMBED_FLAGS_NOWINDOWMENU 0x04

// ============================================================================
// State Management Structure / Структура управления состоянием
// ============================================================================

typedef struct {
    HWND winampWnd;
    HWND dlg;
    HWND embed;

    WNDPROC oldProc;
    WNDPROC hostOld;

    int menuReady;
    int waVersion;
    int isOpen;
    int isQuitting;
    int isInitialized;
    int oleInited;
    int refreshPosted;

    struct {
        int x, y, w, h;
        int saved;
    } pos;

    embedWindowState* ews;
} UIHostState;

static UIHostState g_state = {0};

// ============================================================================
// Helper Functions - Window Management
// ============================================================================

static void RemoveWindowBorders(HWND hwnd)
{
    LONG exStyle;
    LONG style;

    if (!hwnd || !IsWindow(hwnd)) return;

    exStyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME);
    SetWindowLongA(hwnd, GWL_EXSTYLE, exStyle);

    style = GetWindowLongA(hwnd, GWL_STYLE);
    style &= ~(WS_BORDER | WS_THICKFRAME);
    SetWindowLongA(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void SaveWindowPosition(HWND hwnd)
{
    RECT rc;

    if (!hwnd || !IsWindow(hwnd)) return;

    if (GetWindowRect(hwnd, &rc)) {
        g_state.pos.x = rc.left;
        g_state.pos.y = rc.top;
        g_state.pos.w = rc.right - rc.left;
        g_state.pos.h = rc.bottom - rc.top;
        g_state.pos.saved = 1;

        Ini_SaveWindowPos(g_state.pos.x, g_state.pos.y,
                          g_state.pos.w, g_state.pos.h);
    }
}

static int IsWindowOpen(void)
{
    return (g_state.dlg && IsWindow(g_state.dlg)) ? 1 : 0;
}

// ============================================================================
// Menu Functions - EXACT THINGER.C APPROACH
// ============================================================================

static void InsertMenuItemInWinamp(void)
{
    int i;
    HMENU WinampMenu;
    UINT id;

    WinampMenu = (HMENU)SendMessage(g_state.winampWnd, WM_WA_IPC, 0, IPC_GET_HMENU);

    for (i = GetMenuItemCount(WinampMenu); i >= 0; i--)
    {
        if (GetMenuItemID(WinampMenu, i) == 40258)
        {
            do {
                id = GetMenuItemID(WinampMenu, ++i);
                if (id == MENUID_APT) return;
            } while (id != 0xFFFFFFFF);

            InsertMenu(WinampMenu, i - 1, MF_BYPOSITION | MF_STRING, MENUID_APT, kMenuText);
            break;
        }
    }
}

static void RemoveMenuItemFromWinamp(void)
{
    HMENU WinampMenu;
    WinampMenu = (HMENU)SendMessage(g_state.winampWnd, WM_WA_IPC, 0, IPC_GET_HMENU);
    RemoveMenu(WinampMenu, MENUID_APT, MF_BYCOMMAND);
}

static void UpdateMenuCheckmark(int checked)
{
    HMENU WinampMenu;
    MENUITEMINFO mii;

    WinampMenu = (HMENU)SendMessage(g_state.winampWnd, WM_WA_IPC, 0, IPC_GET_HMENU);
    if (!WinampMenu) return;

    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;
    mii.fState = checked ? MFS_CHECKED : MFS_UNCHECKED;
    SetMenuItemInfo(WinampMenu, MENUID_APT, FALSE, &mii);
}

// ============================================================================
// Message Handlers
// ============================================================================

static LRESULT HandleSkinChanges(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND coverView;

    WADlg_init(hwnd);
    Skin_RefreshDialogBrush();

    if (g_state.dlg && IsWindow(g_state.dlg)) {
        InvalidateRect(g_state.dlg, NULL, TRUE);
    }

    coverView = CoverView_FindOn(g_state.dlg);
    if (coverView) {
        InvalidateRect(coverView, NULL, TRUE);
    }

    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

static void RequestCoverRefreshAsync(void)
{
    if (g_state.dlg && IsWindow(g_state.dlg)) {
        if (InterlockedExchange((LONG*)&g_state.refreshPosted, 1) == 0) {
            PostMessage(g_state.dlg, WM_APT_REFRESH, 0, 0);
        }
    }
}

static LRESULT HandleIPCMessages(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (lp == IPC_PLAYING_FILE || lp == IPC_PLAYLIST_MODIFIED) {
        RequestCoverRefreshAsync();
    }
    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MINMAXINFO* mmi;
    RECT cr, wr;
    int minW, minH;

    switch (msg)
    {
    case WM_GETMINMAXINFO:
        mmi = (MINMAXINFO*)lp;
        if (mmi) {
            GetClientRect(hwnd, &cr);
            GetWindowRect(hwnd, &wr);

            minW = DEFAULT_W + (wr.right - wr.left) - (cr.right - cr.left);
            minH = DEFAULT_H + (wr.bottom - wr.top) - (cr.bottom - cr.top);

            if (mmi->ptMinTrackSize.x < minW) mmi->ptMinTrackSize.x = minW;
            if (mmi->ptMinTrackSize.y < minH) mmi->ptMinTrackSize.y = minH;
        }
        return 0;

    case WM_SIZE:
        if (g_state.dlg && IsWindow(g_state.dlg)) {
            GetClientRect(hwnd, &cr);
            MoveWindow(g_state.dlg, 0, 0,
                       cr.right - cr.left,
                       cr.bottom - cr.top, TRUE);
        }
        break;

    case WM_EXITSIZEMOVE:
        SaveWindowPosition(hwnd);
        return 0;

    // КЛЮЧЕВОЕ: отцепляемся ТОЛЬКО на WM_NCDESTROY, когда окно реально умирает.
    case WM_NCDESTROY:
        {
            WNDPROC old = g_state.hostOld;
            g_state.hostOld = NULL;
            g_state.embed = NULL;

            // вернём исходную процедуру, если мы действительно всё ещё стоим на месте
            if (old && (WNDPROC)GetWindowLongA(hwnd, GWL_WNDPROC) == HostProc) {
                SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)old);
            }

            if (old) {
                return CallWindowProcA(old, hwnd, msg, wp, lp);
            }
            return DefWindowProcA(hwnd, msg, wp, lp);
        }
    }

    // defensive: если old уже сброшен, но сообщения ещё приходят
    if (!g_state.hostOld) {
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
    return CallWindowProcA(g_state.hostOld, hwnd, msg, wp, lp);
}

static LRESULT HandleCommand(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND host;

    if (LOWORD(wp) == MENUID_APT) {
        if (IsWindowOpen()) {
            // === CLOSE WINDOW ===
            host = GetParent(g_state.dlg);

            g_state.isOpen = 0;
            Ini_SaveWindowOpen(0);

            // ВАЖНО: НЕ обнуляем embed/hostOld здесь. Окна ещё живые и идут DESTROY/NCDESTROY.
            if (host && IsWindow(host)) {
                DestroyWindow(host);
            } else if (g_state.dlg && IsWindow(g_state.dlg)) {
                DestroyWindow(g_state.dlg);
            }

            // dlg станет NULL в WM_DESTROY диалога
            UpdateMenuCheckmark(0);
        } else {
            // === OPEN WINDOW ===
            g_state.isOpen = 1;
            Ini_SaveWindowOpen(1);

            // force fresh embed on open (safe because we're not inside destroy chain)
            g_state.embed = NULL;
            g_state.hostOld = NULL;

            UIHost_Init();
            UpdateMenuCheckmark(1);
        }
        return 0;
    }

    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK WinampWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCDESTROY) {
        WNDPROC old = g_state.oldProc;
        if (g_state.oldProc) {
            SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)g_state.oldProc);
            g_state.oldProc = NULL;
        }
        if (!old) return DefWindowProcA(hwnd, msg, wp, lp);
        return CallWindowProcA(old, hwnd, msg, wp, lp);
    }

    if ((msg == WM_DISPLAYCHANGE && wp == 0 && lp == 0) ||
        msg == WM_SYSCOLORCHANGE) {
        return HandleSkinChanges(hwnd, msg, wp, lp);
    }

    if (msg == WM_WA_IPC) {
        return HandleIPCMessages(hwnd, msg, wp, lp);
    }

    if (msg == WM_APT_REFRESH) {
        RequestCoverRefreshAsync();
        return 0;
    }

    if (msg == WM_COMMAND) {
        return HandleCommand(hwnd, msg, wp, lp);
    }

    if (msg == WM_ENDSESSION || msg == WM_CLOSE) {
        g_state.isQuitting = 1;
    }

    return CallWindowProcA(g_state.oldProc, hwnd, msg, wp, lp);
}

static LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int ret;
    RECT rc;
    HWND view;
    HWND host;

    ret = WADlg_handleDialogMsgs(hwnd, msg, wp, lp);
    if (ret) return ret;

    switch (msg)
    {
    case WM_INITDIALOG:
        g_state.pos.saved = 0;

        WADlg_init(UIHost_GetWinampWnd());
        Skin_RefreshDialogBrush();

        host = GetParent(hwnd);
        SetWindowText(host ? host : hwnd, kAptTitle);
        ShowWindow(host ? host : hwnd, SW_SHOWNORMAL);

        CoverView_Attach(hwnd);

        g_state.isOpen = 1;
        Ini_SaveWindowOpen(1);

        UpdateMenuCheckmark(1);
        return 0;

    case WM_APT_REFRESH:
        g_state.refreshPosted = 0;
        CoverView_ReloadFromCurrent();
        return 0;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        if (Skin_GetDialogBrush()) {
            SetBkMode((HDC)wp, TRANSPARENT);
            return (INT_PTR)Skin_GetDialogBrush();
        }
        return 0;

    case WM_ERASEBKGND:
        if (Skin_GetDialogBrush()) {
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wp, &rc, Skin_GetDialogBrush());
            return 1;
        }
        return 0;

    case WM_SIZE:
        view = CoverView_FindOn(hwnd);
        if (view) {
            GetClientRect(hwnd, &rc);
            MoveWindow(view, 0, 0, rc.right, rc.bottom, TRUE);
        }
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_SYSCOLORCHANGE:
        WADlg_init(UIHost_GetWinampWnd());
        Skin_RefreshDialogBrush();
        InvalidateRect(hwnd, NULL, TRUE);

        view = CoverView_FindOn(hwnd);
        if (view) {
            SendMessage(view, msg, wp, lp);
            InvalidateRect(view, NULL, TRUE);
        }
        return 0;

    case WM_CLOSE:
        if (!g_state.isQuitting) {
            g_state.isOpen = 0;
            Ini_SaveWindowOpen(0);
        }

        host = GetParent(hwnd);
        SaveWindowPosition(host ? host : hwnd);

        // ВАЖНО: тут тоже НЕ обнуляем embed/hostOld. Пусть HostProc отпишется на WM_NCDESTROY.
        DestroyWindow(host ? host : hwnd);
        return 0;

    case WM_DESTROY:
        if (!g_state.pos.saved) {
            host = GetParent(hwnd);
            SaveWindowPosition(host ? host : hwnd);
        }

        WADlg_close();
        g_state.dlg = NULL;

        UpdateMenuCheckmark(0);
        return 0;
    }

    return 0;
}

// ============================================================================
// Public API Implementation / Реализация публичного API
// ============================================================================

int UIHost_Init(void)
{
    int shouldOpen;
    RECT rcWinamp;
    HWND host;

    if (g_state.isInitialized && g_state.dlg && IsWindow(g_state.dlg)) {
        return 0;
    }

    g_state.winampWnd = UIHost_GetWinampWnd();

    // Initialize OLE/COM once (required for OleLoadPicture in image_loader)
    if (!g_state.oleInited) {
        HRESULT hrOle = OleInitialize(NULL);
        if (hrOle == S_OK || hrOle == S_FALSE) {
            g_state.oleInited = 1;
        }
    }

    g_state.waVersion = (int)SendMessage(g_state.winampWnd, WM_WA_IPC, 0, IPC_GETVERSION);

    if (!g_state.oldProc) {
        g_state.oldProc = (WNDPROC)SetWindowLongA(g_state.winampWnd, GWL_WNDPROC, (LONG)WinampWndProc);
    }

    Hotkeys_Init(g_state.winampWnd, MENUID_APT);

    if (!g_state.menuReady) {
        InsertMenuItemInWinamp();
        SendMessage(g_state.winampWnd, WM_WA_IPC, 1, IPC_ADJUST_OPTIONSMENUPOS);
        g_state.menuReady = 1;
    }

    if (!Ini_LoadWindowPos(g_state.pos.x, g_state.pos.y, g_state.pos.w, g_state.pos.h)) {
        g_state.pos.x = -1;
        g_state.pos.y = -1;
        g_state.pos.w = -1;
        g_state.pos.h = -1;
    }

    shouldOpen = 1;
    if (!Ini_LoadWindowOpen(shouldOpen)) shouldOpen = 1;
    g_state.isOpen = shouldOpen ? 1 : 0;

    // ------------------------------------------------------------------------
    // FIX (Wasabi assert):
    // Do NOT create/embed Wasabi host windows unless the plugin UI is open.
    // ------------------------------------------------------------------------
    if (!g_state.isOpen) {
        UpdateMenuCheckmark(0);

        // If stale handles exist - clean them
        if (g_state.embed && !IsWindow(g_state.embed)) {
            g_state.embed = NULL;
            g_state.hostOld = NULL;
        }
        return 0;
    }

    // If embed exists but stale, drop it
    if (g_state.embed && !IsWindow(g_state.embed)) {
        g_state.embed = NULL;
        g_state.hostOld = NULL;
    }

    if (g_state.ews) {
        GlobalFree(g_state.ews);
        g_state.ews = NULL;
    }

    g_state.ews = (embedWindowState*)GlobalAlloc(GPTR, sizeof(embedWindowState));
    if (!g_state.ews) return 0;

    g_state.ews->flags = (g_state.waVersion >= 0x5000) ? EMBED_FLAGS_NOWINDOWMENU : 0;

    if (g_state.pos.x != -1) {
        g_state.ews->r.left = g_state.pos.x;
        g_state.ews->r.top = g_state.pos.y;
        g_state.ews->r.right = g_state.pos.x + g_state.pos.w;
        g_state.ews->r.bottom = g_state.pos.y + g_state.pos.h;
    } else {
        GetWindowRect(g_state.winampWnd, &rcWinamp);
        g_state.ews->r.left = rcWinamp.left;
        g_state.ews->r.top = rcWinamp.bottom;
        g_state.ews->r.right = rcWinamp.left + DEFAULT_W;
        g_state.ews->r.bottom = rcWinamp.bottom + DEFAULT_H;
    }

    // Always request a fresh embed when opening UI (stability for Modern skins)
    // Стабильнее для Modern: получаем embed заново при открытии окна.
    g_state.embed = (HWND)SendMessage(g_state.winampWnd, WM_WA_IPC, (WPARAM)g_state.ews, IPC_GET_EMBEDIF);
    if (!g_state.embed) return 0;

    SetWindowLongA(g_state.embed, GWL_STYLE,
                   GetWindowLongA(g_state.embed, GWL_STYLE) |
                   WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

    g_state.hostOld = (WNDPROC)SetWindowLongA(g_state.embed, GWL_WNDPROC, (LONG)HostProc);
    RemoveWindowBorders(g_state.embed);

    if (!g_state.dlg || !IsWindow(g_state.dlg)) {
        g_state.dlg = CreateDialog(UIHost_GetHInstance(),
                                   MAKEINTRESOURCE(IDD_DIALOG1),
                                   g_state.embed,
                                   (DLGPROC)DlgProc);
        if (g_state.dlg) {
            Ini_SaveWindowOpen(1);

            host = GetParent(g_state.dlg);
            RemoveWindowBorders(host ? host : g_state.dlg);

            g_state.isInitialized = 1;
        }
    }

    UpdateMenuCheckmark(g_state.isOpen);
    return 0;
}

void UIHost_Config(void)
{
    MessageBoxA(UIHost_GetWinampWnd(),
               APP_CONFIG,
               APP_CONFIG_TITLE,
               MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

void UIHost_Quit(void)
{
    HWND host;

    Hotkeys_Uninit();

    if (g_state.pos.x != -1) {
        Ini_SaveWindowPos(g_state.pos.x, g_state.pos.y,
                          g_state.pos.w, g_state.pos.h);
    }
    Ini_SaveWindowOpen(g_state.isOpen ? 1 : 0);

    g_state.isQuitting = 1;

    if (g_state.dlg && IsWindow(g_state.dlg)) {
        host = GetParent(g_state.dlg);
        if (host && IsWindow(host)) {
            DestroyWindow(host);
        } else {
            DestroyWindow(g_state.dlg);
        }
        g_state.dlg = NULL;
    }

    if (g_state.ews) {
        GlobalFree(g_state.ews);
        g_state.ews = NULL;
    }

    if (g_state.winampWnd && g_state.oldProc) {
        SetWindowLongA(g_state.winampWnd, GWL_WNDPROC, (LONG)g_state.oldProc);
        g_state.oldProc = NULL;
    }

    if (g_state.menuReady && g_state.winampWnd && IsWindow(g_state.winampWnd)) {
        RemoveMenuItemFromWinamp();
    }
    g_state.menuReady = 0;

    Skin_DeleteDialogBrush();
    Img_Cleanup();

    {
        HINSTANCE hi = UIHost_GetHInstance();
        if (hi) {
            UnregisterClassA("APT_CoverArtView", hi);
        }
        UnregisterClassA("APT_CoverArtView", GetModuleHandleA(NULL));
    }

    if (g_state.oleInited) {
        OleUninitialize();
        g_state.oleInited = 0;
    }

    g_state.isInitialized = 0;
}
