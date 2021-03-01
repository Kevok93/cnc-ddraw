#include <windows.h>
#include "dd.h"
#include "hook.h"
#include "config.h"
#include "utils.h"
#include "mouse.h"
#include "wndproc.h"


BOOL WINAPI fake_GetCursorPos(LPPOINT lpPoint)
{
    POINT pt, realpt;
    
    if (!real_GetCursorPos(&pt) || !g_ddraw)
        return FALSE;
    
    realpt.x = pt.x;
    realpt.y = pt.y;
    
    if(g_ddraw->locked && (!g_ddraw->windowed || real_ScreenToClient(g_ddraw->hwnd, &pt)))
    {
        //fallback solution for possible ClipCursor failure
        int diffx = 0, diffy = 0;

        int max_width = g_ddraw->adjmouse ? g_ddraw->render.viewport.width : g_ddraw->width;
        int max_height = g_ddraw->adjmouse ? g_ddraw->render.viewport.height : g_ddraw->height;

        if (pt.x < 0)
        {
            diffx = pt.x;
            pt.x = 0;
        }

        if (pt.y < 0)
        {
            diffy = pt.y;
            pt.y = 0;
        }

        if (pt.x > max_width)
        {
            diffx = pt.x - max_width;
            pt.x = max_width;
        }

        if (pt.y > max_height)
        {
            diffy = pt.y - max_height;
            pt.y = max_height;
        }

        if (diffx || diffy)
            real_SetCursorPos(realpt.x - diffx, realpt.y - diffy);


        if(g_ddraw->adjmouse)
        {
            g_ddraw->cursor.x = pt.x * g_ddraw->render.unscale_w;
            g_ddraw->cursor.y = pt.y * g_ddraw->render.unscale_h;
        }
        else
        {
            g_ddraw->cursor.x = pt.x;
            g_ddraw->cursor.y = pt.y;
        }

        if (g_ddraw->vhack && InterlockedExchangeAdd(&g_ddraw->incutscene, 0))
        {
            diffx = 0;
            diffy = 0;

            if (g_ddraw->cursor.x > CUTSCENE_WIDTH)
            {
                diffx = g_ddraw->cursor.x - CUTSCENE_WIDTH;
                g_ddraw->cursor.x = CUTSCENE_WIDTH;
            }
                
            if (g_ddraw->cursor.y > CUTSCENE_HEIGHT)
            {
                diffy = g_ddraw->cursor.y - CUTSCENE_HEIGHT;
                g_ddraw->cursor.y = CUTSCENE_HEIGHT;
            }

            if (diffx || diffy)
                real_SetCursorPos(realpt.x - diffx, realpt.y - diffy);
        }
    }

    if (lpPoint)
    {
        lpPoint->x = (int)g_ddraw->cursor.x;
        lpPoint->y = (int)g_ddraw->cursor.y;
    }
    
    return TRUE;
}

BOOL WINAPI fake_ClipCursor(const RECT *lpRect)
{
    if(lpRect)
    {
        /* hack for 640x480 mode */
        if (lpRect->bottom == 400 && g_ddraw && g_ddraw->height == 480)
            g_ddraw->mouse_y_adjust = 40;
    }
    return TRUE;
}

int WINAPI fake_ShowCursor(BOOL bShow)
{
    static int count;


        return real_ShowCursor(bShow);

    return bShow ? ++count : --count;
}

HCURSOR WINAPI fake_SetCursor(HCURSOR hCursor)
{
        return real_SetCursor(hCursor); 
    
    return NULL;
}

BOOL WINAPI fake_GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    if (lpRect && g_ddraw)
    {
        if (g_ddraw->hwnd == hWnd)
        {
            lpRect->bottom = g_ddraw->height;
            lpRect->left = 0;
            lpRect->right = g_ddraw->width;
            lpRect->top = 0;

            return TRUE;
        }
        else
        {
            if (real_GetWindowRect(hWnd, lpRect))
            {
                MapWindowPoints(HWND_DESKTOP, g_ddraw->hwnd, (LPPOINT)lpRect, 2);

                return TRUE;
            }

            return FALSE;
        }
    }

    return real_GetWindowRect(hWnd, lpRect);
}

BOOL WINAPI fake_GetClientRect(HWND hWnd, LPRECT lpRect)
{
    if (lpRect && g_ddraw && g_ddraw->hwnd == hWnd)
    {
        lpRect->bottom = g_ddraw->height;
        lpRect->left = 0;
        lpRect->right = g_ddraw->width;
        lpRect->top = 0;

        return TRUE;
    }

    return real_GetClientRect(hWnd, lpRect);
}

BOOL WINAPI fake_ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    if (g_ddraw && g_ddraw->hwnd != hWnd)
        return real_ClientToScreen(hWnd, lpPoint) && real_ScreenToClient(g_ddraw->hwnd, lpPoint);

    return TRUE;
}

BOOL WINAPI fake_ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    if (g_ddraw && g_ddraw->hwnd != hWnd)
        return real_ClientToScreen(g_ddraw->hwnd, lpPoint) && real_ScreenToClient(hWnd, lpPoint);

    return TRUE;
}

BOOL WINAPI fake_SetCursorPos(int X, int Y)
{
    if (g_ddraw && !g_ddraw->locked && !g_ddraw->devmode)
        return TRUE;

    POINT pt = { X, Y };

    if (g_ddraw && g_ddraw->adjmouse)
    {
        pt.x *= g_ddraw->render.scale_w;
        pt.y *= g_ddraw->render.scale_h;
    }

    return g_ddraw && real_ClientToScreen(g_ddraw->hwnd, &pt) && real_SetCursorPos(pt.x, pt.y);
}

HWND WINAPI fake_WindowFromPoint(POINT Point)
{
    POINT pt = { Point.x, Point.y };
    return g_ddraw && real_ClientToScreen(g_ddraw->hwnd, &pt) ? real_WindowFromPoint(pt) : NULL;
}

BOOL WINAPI fake_GetClipCursor(LPRECT lpRect)
{
    if (lpRect && g_ddraw)
    {
        lpRect->bottom = g_ddraw->height;
        lpRect->left = 0;
        lpRect->right = g_ddraw->width;
        lpRect->top = 0;

        return TRUE;
    }

    return FALSE;
}

BOOL WINAPI fake_GetCursorInfo(PCURSORINFO pci)
{
    return pci && g_ddraw && real_GetCursorInfo(pci) && real_ScreenToClient(g_ddraw->hwnd, &pci->ptScreenPos);
}

int WINAPI fake_GetSystemMetrics(int nIndex)
{
    if (g_ddraw)
    {
        if (nIndex == SM_CXSCREEN)
            return g_ddraw->width;

        if (nIndex == SM_CYSCREEN)
            return g_ddraw->height;
    }

    return real_GetSystemMetrics(nIndex);
}

BOOL WINAPI fake_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    UINT req_flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER;

    if (g_ddraw && g_ddraw->hwnd == hWnd && (uFlags & req_flags) != req_flags)
        return TRUE;

    return real_SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

BOOL WINAPI fake_MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd)
        return TRUE;

    return real_MoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
}

LRESULT WINAPI fake_SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = real_SendMessageA(hWnd, Msg, wParam, lParam);

    if (result && g_ddraw && Msg == CB_GETDROPPEDCONTROLRECT)
    {
        RECT *rc = (RECT *)lParam;
        if (rc)
            MapWindowPoints(HWND_DESKTOP, g_ddraw->hwnd, (LPPOINT)rc, 2);
    }

    return result;
}

LONG WINAPI fake_SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd && nIndex == GWL_STYLE)
        return 0;

    return real_SetWindowLongA(hWnd, nIndex, dwNewLong);
}

BOOL WINAPI fake_EnableWindow(HWND hWnd, BOOL bEnable)
{
    if (g_ddraw && g_ddraw->hwnd == hWnd)
    {
        return FALSE;
    }

    return real_EnableWindow(hWnd, bEnable);
}

int WINAPI fake_GetDeviceCaps(HDC hdc, int index)
{
    if (g_ddraw && g_ddraw->bpp && index == BITSPIXEL)
    {
        return g_ddraw->bpp;
    }

    return real_GetDeviceCaps(hdc, index);
}

HMODULE WINAPI fake_LoadLibraryA(LPCSTR lpLibFileName)
{
    HMODULE hmod = real_LoadLibraryA(lpLibFileName);

    hook_init();

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryW(LPCWSTR lpLibFileName)
{
    HMODULE hmod = real_LoadLibraryW(lpLibFileName);

    hook_init();

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod = real_LoadLibraryExA(lpLibFileName, hFile, dwFlags);

    hook_init();

    return hmod;
}

HMODULE WINAPI fake_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    HMODULE hmod = real_LoadLibraryExW(lpLibFileName, hFile, dwFlags);

    hook_init();

    return hmod;
}

BOOL WINAPI fake_DestroyWindow(HWND hWnd)
{
    BOOL result = real_DestroyWindow(hWnd);

    if (g_ddraw && g_ddraw->hwnd != hWnd && g_ddraw->bnet_active)
    {
        RedrawWindow(NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

        if (!FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL))
        {
            g_ddraw->bnet_active = FALSE;
            SetFocus(g_ddraw->hwnd);
            mouse_lock();

            if (g_ddraw->windowed)
            {
                g_ddraw->bnet_pos.x = g_ddraw->bnet_pos.y = 0;
                real_ClientToScreen(g_ddraw->hwnd, &g_ddraw->bnet_pos);

                if (!g_ddraw->bnet_was_upscaled)
                {
                    int width = g_ddraw->bnet_win_rect.right - g_ddraw->bnet_win_rect.left;
                    int height = g_ddraw->bnet_win_rect.bottom - g_ddraw->bnet_win_rect.top;

                    UINT flags = width != g_ddraw->width || height != g_ddraw->height ? 0 : SWP_NOMOVE;

                    util_set_window_rect(g_ddraw->bnet_win_rect.left, g_ddraw->bnet_win_rect.top, width, height, flags);
                }

                g_ddraw->fullscreen = g_ddraw->bnet_was_upscaled;

                SetTimer(g_ddraw->hwnd, IDT_TIMER_LEAVE_BNET, 1000, (TIMERPROC)NULL);

                g_ddraw->resizable = TRUE;
            }
        }
    }

    return result;
}

HWND WINAPI fake_CreateWindowExA(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
    int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    if (lpClassName && _strcmpi(lpClassName, "SDlgDialog") == 0 && g_ddraw)
    {
        if (!g_ddraw->bnet_active)
        {
            g_ddraw->bnet_was_upscaled = g_ddraw->fullscreen;
            g_ddraw->fullscreen = FALSE;

            if (!g_ddraw->windowed && !g_ddraw->bnet_was_fullscreen)
            {
                int ws = g_config.window_state;
                util_toggle_fullscreen();
                g_config.window_state = ws;
                g_ddraw->bnet_was_fullscreen = TRUE;
            }

            real_GetClientRect(g_ddraw->hwnd, &g_ddraw->bnet_win_rect);
            MapWindowPoints(g_ddraw->hwnd, HWND_DESKTOP, (LPPOINT)&g_ddraw->bnet_win_rect, 2);

            int width = g_ddraw->bnet_win_rect.right - g_ddraw->bnet_win_rect.left;
            int height = g_ddraw->bnet_win_rect.bottom - g_ddraw->bnet_win_rect.top;

            int x = g_ddraw->bnet_pos.x || g_ddraw->bnet_pos.y ? g_ddraw->bnet_pos.x : -32000;
            int y = g_ddraw->bnet_pos.x || g_ddraw->bnet_pos.y ? g_ddraw->bnet_pos.y : -32000;

            UINT flags = width != g_ddraw->width || height != g_ddraw->height ? 0 : SWP_NOMOVE;

            util_set_window_rect(x, y, g_ddraw->width, g_ddraw->height, flags);
            g_ddraw->resizable = FALSE;

            g_ddraw->bnet_active = TRUE;
            mouse_unlock();
        }

        POINT pt = { 0, 0 };
        real_ClientToScreen(g_ddraw->hwnd, &pt);

        X += pt.x;
        Y += pt.y;

        dwStyle |= WS_CLIPCHILDREN;
    }

    return real_CreateWindowExA(
        dwExStyle,
        lpClassName,
        lpWindowName,
        dwStyle,
        X,
        Y,
        nWidth,
        nHeight,
        hWndParent,
        hMenu,
        hInstance,
        lpParam);
}
