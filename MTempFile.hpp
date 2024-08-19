// MTempFile.hpp --- 一時ファイルを自動化する by katahiromz
#pragma once

#include <strsafe.h> // 文字列を安全に扱う関数(StringCc*)。
#include <cassert> // assertマクロ。

// 一時ファイルを扱うクラス。
class MTempFile
{
protected:
    DWORD m_old_value;
    TCHAR m_tempfile[MAX_PATH]; // ファイル名。
    TCHAR m_temppath[MAX_PATH]; // パスファイル名。
    TCHAR m_prefix[4]; // プレフィックス。
    TCHAR m_dot_ext[8]; // 拡張子（ドットで始まる）。

    // ファイル名を多様にするための調味料。
    DWORD get_salt()
    {
        // 現在の時刻データを利用する。
        SYSTEMTIME st;
        ::GetLocalTime(&st);

        DWORD dw = m_old_value; // 古い値も利用する。
        dw += st.wYear;
        dw += st.wMonth;
        dw += st.wDay;
        dw += st.wHour;
        dw += st.wMinute;
        dw += st.wSecond;
        dw += st.wMilliseconds;
        m_old_value = dw; // 値を保存する。
        return LOWORD(dw) ^ HIWORD(dw); // 値は符号なし2バイト整数の範囲。
    }

public:
    // コンストラクタ。
    MTempFile() : m_old_value(::GetTickCount())
    {
        m_tempfile[0] = m_temppath[0] = m_prefix[0] = m_dot_ext[0] = 0;
    }

    // コンストラクタ。
    MTempFile(LPCTSTR prefix, LPCTSTR dot_ext) : m_old_value(::GetTickCount())
    {
        init(prefix, dot_ext);
    }

    // デストラクタ。
    ~MTempFile()
    {
        // 自動的に削除する。
        erase();
    }

    // 初期化。第一引数はプレフィックス。第二引数は拡張子。
    void init(LPCTSTR prefix, LPCTSTR dot_ext)
    {
        m_tempfile[0] = 0;
        ::GetTempPath(_countof(m_temppath), m_temppath);
        StringCchCopy(m_prefix, _countof(m_prefix), prefix);
        StringCchCopy(m_dot_ext, _countof(m_dot_ext), dot_ext);
        assert(dot_ext[0] == TEXT('.') || dot_ext[0] == 0);
    }

    // 一時ファイルを作成し、パスファイル名を返す。
    LPCTSTR make()
    {
        for (UINT i = 0; i < 1000; ++i) // 1000回やってダメならあきらめよう。
        {
            // 調味料を使ってパスファイル名を作成する。
            StringCchPrintf(m_tempfile, _countof(m_tempfile),
                            TEXT("%s%s-%04lX%s"),
                            m_temppath, m_prefix, get_salt(), m_dot_ext);
            // 新しく作成できたら、それを採用。
            HANDLE hFile = ::CreateFile(m_tempfile, GENERIC_WRITE, FILE_SHARE_READ,
                                        NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(hFile);
                return m_tempfile;
            }
            // 作成できなかったらやり直す。
        }

        // 作成失敗。
        assert(0);
        m_tempfile[0] = 0;
        return NULL;
    }

    // パスファイル名を返す。
    LPTSTR get()
    {
        assert(m_tempfile[0] != 0); // make first!

        if (m_tempfile[0] != 0)
            return m_tempfile;

        return NULL;
    }

    operator LPTSTR()
    {
        return get();
    }

    // 一時ファイルを削除する。
    void erase()
    {
        if (m_tempfile[0])
            ::DeleteFile(m_tempfile);
    }

    // 名前をクリアする。削除されてなければこのクラスは一時ファイルを削除しない。
    void clear()
    {
        m_tempfile[0] = 0;
    }
};
