#pragma once

#include "color_value.h"

// Owner-drawn color button
class MColorBox
{
public:
    HWND m_hWnd = nullptr;
    COLORREF m_rgbColor = RGB(0, 0, 0);

    COLORREF *get_color_table()
    {
        static COLORREF s_rgbColorTable[16] =
        {
            RGB(0, 0, 0),
            RGB(0x33, 0x33, 0x33),
            RGB(0x66, 0x66, 0x66),
            RGB(0x99, 0x99, 0x99),
            RGB(0xCC, 0xCC, 0xCC),
            RGB(0xFF, 0xFF, 0xFF),
            RGB(0xFF, 0xFF, 0xCC),
            RGB(0xFF, 0xCC, 0xFF),
            RGB(0xFF, 0xCC, 0xCC),
            RGB(0xCC, 0xFF, 0xFF),
            RGB(0xCC, 0xFF, 0xCC),
            RGB(0xCC, 0xCC, 0xFF),
            RGB(0xCC, 0xCC, 0xCC),
            RGB(0, 0, 0xCC),
            RGB(0, 0xCC, 0),
            RGB(0xCC, 0, 0)
        };
        return s_rgbColorTable;
    }

    MColorBox(HWND hwnd = nullptr) noexcept
    {
        m_hWnd = hwnd;
    }

    COLORREF get_color_value() const noexcept
    {
        return m_rgbColor;
    }

    void set_color_value(COLORREF rgb) noexcept
    {
        m_rgbColor = rgb;
        InvalidateRect(m_hWnd, nullptr, TRUE);
    }

    bool choose_color() noexcept
    {
        CHOOSECOLOR cc = { sizeof(cc), GetParent(m_hWnd) };
        cc.rgbResult = get_color_value();
        cc.lpCustColors = get_color_table();
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (::ChooseColor(&cc))
        {
            set_color_value(cc.rgbResult);
            return true;
        }
        return false;
    }

    LPCTSTR get_color_text() const noexcept
    {
        static TCHAR s_szText[16];
        StringCchPrintf(s_szText, _countof(s_szText), L"#%02X%02X%02X",
            GetRValue(m_rgbColor), GetGValue(m_rgbColor), GetBValue(m_rgbColor));
        return s_szText;
    }

    void set_color_text(const char *color_name) noexcept
    {
        m_rgbColor = color_value_fix(color_value_parse(color_name));
        InvalidateRect(m_hWnd, nullptr, TRUE);
    }

    void set_color_text(const wchar_t *color_name) noexcept
    {
        char buf[64];
        WideCharToMultiByte(CP_ACP, 0, color_name, -1, buf, _countof(buf), nullptr, nullptr);
        buf[_countof(buf) - 1] = 0;
        set_color_text(buf);
    }

    virtual void OnOwnerDrawItem(const DRAWITEMSTRUCT *pdis) noexcept
    {
        if (pdis->hwndItem != m_hWnd || pdis->CtlType != ODT_BUTTON)
            return;

        BOOL bSelected = !!(pdis->itemState & ODS_SELECTED);
        BOOL bFocus = !!(pdis->itemState & ODS_FOCUS);
        RECT rcItem = pdis->rcItem;

        HDC hdc = pdis->hDC;
        ::DrawFrameControl(hdc, &rcItem, DFC_BUTTON, DFCS_BUTTONPUSH | (bSelected ? DFCS_PUSHED : 0));

        ::InflateRect(&rcItem, -4, -4);
        HBRUSH hbr = ::CreateSolidBrush(m_rgbColor);
        ::FillRect(hdc, &rcItem, hbr);
        ::DeleteObject(hbr);

        if (bFocus)
        {
            ::InflateRect(&rcItem, 2, 2);
            ::DrawFocusRect(hdc, &rcItem);
        }
    }
};
