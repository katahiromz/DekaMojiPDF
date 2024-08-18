#pragma once

#include "MImageView.h"

class MImageViewEx : public MImageView
{
public:
	MImageViewEx()
    {
        m_hbrChecker = CreateCheckerboardBrush(RGB(225, 225, 225), RGB(150, 150, 150), 4);
    }
	virtual ~MImageViewEx()
    {
        DeleteObject(m_hbrChecker);
    }

protected:
    HBRUSH m_hbrChecker;

    virtual BOOL OnEraseBkgnd(HWND hwnd, HDC hdc)
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        ::FillRect(hdc, &rc, m_hbrChecker);
        return TRUE;
    }

    HBRUSH CreateCheckerboardBrush(COLORREF lightColor, COLORREF darkColor, int squareSize)
    {
        HDC hdcMem = CreateCompatibleDC(NULL);
        HBITMAP hbmp = CreateCompatibleBitmap(GetDC(NULL), squareSize * 2, squareSize * 2);
        HBRUSH hbrush = NULL;

        if (hdcMem && hbmp)
        {
            SelectObject(hdcMem, hbmp);

            HBRUSH hbrLight = CreateSolidBrush(lightColor);
            HBRUSH hbrDark = CreateSolidBrush(darkColor);

            RECT rect = { 0, 0, squareSize, squareSize };
            FillRect(hdcMem, &rect, hbrLight);

            rect = { squareSize, 0, squareSize * 2, squareSize };
            FillRect(hdcMem, &rect, hbrDark);

            rect = { 0, squareSize, squareSize, squareSize * 2 };
            FillRect(hdcMem, &rect, hbrDark);

            rect = { squareSize, squareSize, squareSize * 2, squareSize * 2 };
            FillRect(hdcMem, &rect, hbrLight);

            DeleteObject(hbrLight);
            DeleteObject(hbrDark);

            hbrush = CreatePatternBrush(hbmp);
        }

        if (hbmp) DeleteObject(hbmp);
        if (hdcMem) DeleteDC(hdcMem);

        return hbrush;
    }
};
