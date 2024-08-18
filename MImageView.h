#pragma once

#include <windows.h>
#include <windowsx.h>

class MImageView
{
public:
	MImageView()
    {
    }
	virtual ~MImageView()
    {
    }
    void doSubclass(HWND hwnd)
    {
        m_hWnd = hwnd;
        m_fnOldWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    }
    void doUnSubClass()
    {
        SetWindowLongPtr(m_hWnd, GWLP_USERDATA, 0);
        SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, (LONG_PTR)m_fnOldWndProc);
        m_hWnd = NULL;
    }
    BOOL doRegister()
    {
        WNDCLASS wc = { CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS };
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
        wc.lpszClassName = getWndClassName();
        return ::RegisterClass(&wc);
    }
    virtual void setImage(HBITMAP hImage)
    {
        if (m_hImage)
            DeleteObject(m_hImage);
        m_hImage = hImage;
        InvalidateRect(m_hWnd, NULL, TRUE);
    }

protected:
    HWND m_hWnd = NULL;
    WNDPROC m_fnOldWndProc = NULL;
    HBITMAP m_hImage = NULL;

    virtual BOOL OnEraseBkgnd(HWND hwnd, HDC hdc)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hbr = CreateSolidBrush(getBkColor());
        ::FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        return TRUE;
    }
    void OnPaint(HWND hwnd)
    {
        PAINTSTRUCT ps;
        if (HDC hdc = BeginPaint(hwnd, &ps))
        {
            OnDraw(hwnd, hdc, ps);
            EndPaint(hwnd, &ps);
        }
    }
    virtual void OnDraw(HWND hwnd, HDC hdc, PAINTSTRUCT& ps)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);

        BITMAP bm;
        if (!GetObject(m_hImage, sizeof(bm), &bm))
            return;

        HDC hdcMem = CreateCompatibleDC(hdc);
        HGDIOBJ hbmOld = SelectObject(hdcMem, m_hImage);

        LONG center_x = rc.right / 2;
        LONG center_y = rc.bottom / 2;

        double rate1 = bm.bmHeight / (double)bm.bmWidth;
        double rate2 = rc.bottom / (double)rc.right;
        double rate3;
        if (rate1 >= rate2)
            rate3 = rc.bottom / (double)bm.bmHeight;
        else
            rate3 = rc.right / (double)bm.bmWidth;

        SetStretchBltMode(hdc, STRETCH_HALFTONE);
        StretchBlt(hdc,
            LONG(center_x - bm.bmWidth * rate3 / 2),
            LONG(center_y - bm.bmHeight * rate3 / 2),
            LONG(bm.bmWidth * rate3),
            LONG(bm.bmHeight * rate3),
            hdcMem,
            0, 0, bm.bmWidth, bm.bmHeight,
            SRCCOPY);

        SelectObject(hdcMem, hbmOld);
    }
    virtual void OnDestroy(HWND hwnd)
    {
        DeleteObject(m_hImage);
        m_hImage = NULL;
    }

    virtual LPCTSTR getWndClassName()
    {
        return TEXT("katahiromz MImageView");
    }
    virtual COLORREF getBkColor()
    {
        return RGB(0, 0, 0);
    }

    virtual LRESULT CALLBACK
    DefWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (m_fnOldWndProc)
            return ::CallWindowProc(m_fnOldWndProc, hwnd, uMsg, wParam, lParam);
        return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    virtual LRESULT CALLBACK
    WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
            HANDLE_MSG(hwnd, WM_ERASEBKGND, OnEraseBkgnd);
            HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
            HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
        default:
            return DefWndProc(hwnd, uMsg, wParam, lParam);
        }
        return 0;
    }
    static LRESULT CALLBACK
    WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        MImageView *pThis = (MImageView *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!pThis)
            return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
        return pThis->WndProc(hwnd, uMsg, wParam, lParam);
    }
};
