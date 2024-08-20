#pragma once

#include <windows.h>

class MPageMgr
{
public:
    HWND m_hWnd = NULL;
    INT m_iPage = 1;
    INT m_cPages = 0;

    MPageMgr()
    {
    }
    virtual ~MPageMgr()
    {
    }

    void init()
    {
        m_iPage = 1;
        m_cPages = 0;
        doUpdate();
    }

    void addPage()
    {
        ++m_cPages;
    }
    bool hasNext() const
    {
        return m_iPage < m_cPages;
    }
    bool hasBack() const
    {
        return 1 < m_iPage;
    }
    void fixPage()
    {
        if (m_iPage >= m_cPages)
            m_iPage = m_cPages;
    }

    std::wstring print() const
    {
        WCHAR szText[64];
        if (0 < m_iPage && m_iPage <= m_cPages)
        {
            StringCchPrintfW(szText, _countof(szText), L"%d / %d", m_iPage, m_cPages);
            return szText;
        }
        return L"";
    }

    INT getPage() const
    {
        return m_iPage;
    }
    void setPage(INT iPage)
    {
        if (1 <= iPage && iPage <= m_cPages)
        {
            m_iPage = iPage;
            doUpdate();
        }
    }

    INT getPageCount() const
    {
        return m_cPages;
    }
    void setPageCount(INT cPages)
    {
        m_cPages = cPages;
    }

    virtual void goNext()
    {
        setPage(m_iPage + 1);
    }
    virtual void goBack()
    {
        setPage(m_iPage - 1);
    }

protected:
    virtual void doUpdate()
    {
        InvalidateRect(m_hWnd, NULL, TRUE);
    }
};
