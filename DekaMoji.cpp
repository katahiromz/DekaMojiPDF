// デカ文字PDF by katahiromz
// Copyright (C) 2022-2023 片山博文MZ. All Rights Reserved.
// See README.txt and LICENSE.txt.
#include <windows.h>        // Windowsの標準ヘッダ。
#include <windowsx.h>       // Windowsのマクロヘッダ。
#include <commctrl.h>       // 共通コントロールのヘッダ。
#include <commdlg.h>        // 共通ダイアログのヘッダ。
#include <shlobj.h>         // シェルAPIのヘッダ。
#include <shlwapi.h>        // シェル軽量APIのヘッダ。
#include <tchar.h>          // ジェネリックテキストマッピング用のヘッダ。
#include <strsafe.h>        // 安全な文字列操作用のヘッダ (StringC*)
#include <string>           // std::string および std::wstring クラス。
#include <vector>           // std::vector クラス。
#include <map>              // std::map クラス。
#include <stdexcept>        // std::runtime_error クラス。
#include <cassert>          // assertマクロ。
#include <hpdf.h>           // PDF出力用のライブラリlibharuのヘッダ。
#include <gdiplus.h>        // GDI+
#include "color_value.h"    // 色をパースする。
#include "MImageView.h"     // イメージプレビュー用のウィンドウ コントロール。
#include "TempFile.hpp"     // 一時ファイル操作用のヘッダ。
#include "resource.h"       // リソースIDの定義ヘッダ。

// 文字列クラス。
#ifdef UNICODE
    typedef std::wstring string_t;
#else
    typedef std::string string_t;
#endif

// シフトJIS コードページ（Shift_JIS）。
#define CP932  932

#define TIMER_ID_REFRESH_PREVIEW 999

struct FONT_ENTRY
{
    string_t m_font_name;
    string_t m_pathname;
    int m_index = -1;
};

// わかりやすい項目名を使用する。
enum
{
    IDC_GENERATE = IDOK,
    IDC_EXIT = IDCANCEL,
    IDC_PAGE_SIZE = cmb1,
    IDC_PAGE_DIRECTION = cmb2,
    IDC_FONT_NAME = cmb3,
    IDC_METHOD = cmb5,
    IDC_TEXT = edt1,
    IDC_TEXT_COLOR = edt2,
    IDC_TEXT_COLOR_BUTTON = psh1,
    IDC_ERASESETTINGS = psh5,
    IDC_README = psh6,
};

// デカ文字PDFのメインクラス。
class DekaMoji
{
public:
    HINSTANCE m_hInstance;
    INT m_argc;
    LPTSTR *m_argv;
    std::map<string_t, string_t> m_settings;
    std::vector<FONT_ENTRY> m_font_map;

    // コンストラクタ。
    DekaMoji(HINSTANCE hInstance, INT argc, LPTSTR *argv);

    // デストラクタ。
    ~DekaMoji()
    {
    }

    // フォントマップを読み込む。
    BOOL LoadFontMap();
    // データをリセットする。
    void Reset();
    // ダイアログを初期化する。
    void InitDialog(HWND hwnd);
    // ダイアログからデータへ。
    BOOL DataFromDialog(HWND hwnd, BOOL bNoError = FALSE);
    // データからダイアログへ。
    BOOL DialogFromData(HWND hwnd);
    // レジストリからデータへ。
    BOOL DataFromReg(HWND hwnd);
    // データからレジストリへ。
    BOOL RegFromData(HWND hwnd);

    // メインディッシュ処理。
    string_t JustDoIt(HWND hwnd, LPCTSTR pszPdfFileName = NULL);
};

// グローバル変数。
HINSTANCE g_hInstance = NULL; // インスタンス。
TCHAR g_szAppName[256] = TEXT(""); // アプリ名。
HICON g_hIcon = NULL; // アイコン（大）。
HICON g_hIconSm = NULL; // アイコン（小）。
HFONT g_hTextFont = NULL; // テキストフォント。
MImageView g_hwndImageView;

// リソース文字列を読み込む。
LPTSTR doLoadString(INT nID)
{
    static TCHAR s_szText[1024];
    s_szText[0] = 0;
    LoadString(NULL, nID, s_szText, _countof(s_szText));
    return s_szText;
}

// 文字列の前後の空白を削除する。
void str_trim(LPWSTR text)
{
    StrTrimW(text, L" \t\r\n\x3000");
}

// ローカルのファイルのパス名を取得する。
LPCTSTR findLocalFile(LPCTSTR filename)
{
    // 現在のプログラムのパスファイル名を取得する。
    static TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));

    // 現在のプログラムのあるディレクトリを取得。
    TCHAR dir[MAX_PATH];
    StringCchCopy(dir, _countof(dir), szPath);
    PathRemoveFileSpec(dir);

    /// ファイルの存在確認。
    StringCchCopy(szPath, _countof(szPath), dir);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    // 一つ上のフォルダでファイルの存在確認。
    StringCchCopy(szPath, _countof(szPath), dir);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    // さらに一つ上のフォルダへ。
    StringCchCopy(szPath, _countof(szPath), dir);
    PathRemoveFileSpec(szPath);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    return NULL; // 見つからなかった。
}

// 不正な文字列が入力された。
void OnInvalidString(HWND hwnd, INT nItemID, INT nFieldId, INT nReasonId)
{
    SetFocus(GetDlgItem(hwnd, nItemID));
    string_t field = doLoadString(nFieldId);
    string_t reason = doLoadString(nReasonId);
    TCHAR szText[256];
    StringCchPrintf(szText, _countof(szText), doLoadString(IDS_INVALIDSTRING), field.c_str(), reason.c_str());
    MessageBox(hwnd, szText, g_szAppName, MB_ICONERROR);
}

// コンボボックスのテキストを取得する。
BOOL getComboText(HWND hwnd, INT id, LPTSTR text, INT cchMax)
{
    text[0] = 0;

    HWND hCombo = GetDlgItem(hwnd, id);
    INT iSel = ComboBox_GetCurSel(hCombo);
    if (iSel == CB_ERR) // コンボボックスに選択項目がなければ
    {
        // そのままテキストを取得する。
        ComboBox_GetText(hCombo, text, cchMax);
    }
    else
    {
        // リストからテキストを取得する。長さのチェックあり。
        if (ComboBox_GetLBTextLen(hCombo, iSel) >= cchMax)
        {
            StringCchCopy(text, cchMax, doLoadString(IDS_TEXTTOOLONG));
            return FALSE;
        }
        else
        {
            ComboBox_GetLBText(hCombo, iSel, text);
        }
    }

    return TRUE;
}

// コンボボックスのテキストを設定する。
BOOL setComboText(HWND hwnd, INT id, LPCTSTR text)
{
    // テキストに一致する項目を取得する。
    HWND hCombo = GetDlgItem(hwnd, id);
    INT iItem = ComboBox_FindStringExact(hCombo, -1, text);
    if (iItem == CB_ERR) // 一致する項目がなければ
        ComboBox_SetText(hCombo, text); // そのままテキストを設定。
    else
        ComboBox_SetCurSel(hCombo, iItem); // 一致する項目を選択。
    return TRUE;
}

// ワイド文字列をANSI文字列に変換する。
LPSTR ansi_from_wide(UINT codepage, LPCWSTR wide)
{
    static CHAR s_ansi[1024];

    // コードページで表示できない文字はゲタ文字（〓）にする。
    static const char utf8_geta[] = "\xE3\x80\x93";
    static const char cp932_geta[] = "\x81\xAC";
    const char *geta = NULL;
    if (codepage == CP_ACP || codepage == CP932)
    {
        geta = cp932_geta;
    }
    else if (codepage == CP_UTF8)
    {
        geta = utf8_geta;
    }

    WideCharToMultiByte(codepage, 0, wide, -1, s_ansi, _countof(s_ansi), geta, NULL);
    s_ansi[_countof(s_ansi) - 2] = 0;
    s_ansi[_countof(s_ansi) - 1] = 0;
    return s_ansi;
}

// ANSI文字列をワイド文字列に変換する。
LPWSTR wide_from_ansi(UINT codepage, LPCSTR ansi)
{
    static WCHAR s_wide[1024];
    MultiByteToWideChar(codepage, 0, ansi, -1, s_wide, _countof(s_wide));
    s_wide[_countof(s_wide) - 1] = 0;
    return s_wide;
}

// mm単位からピクセル単位への変換。
double pixels_from_mm(double mm, double dpi = 72)
{
    return dpi * mm / 25.4;
}

// ピクセル単位からmm単位への変換。
double mm_from_pixels(double pixels, double dpi = 72)
{
    return 25.4 * pixels / dpi;
}

// ファイル名に使えない文字を下線文字に置き換える。
void validate_filename(string_t& filename)
{
    for (auto& ch : filename)
    {
        if (wcschr(L"\\/:*?\"<>|", ch) != NULL)
            ch = L'_';
    }
}

// フォントマップを読み込む。
BOOL DekaMoji::LoadFontMap()
{
    // 初期化する。
    m_font_map.clear();

    // ローカルファイルの「fontmap.dat」を探す。
    auto filename = findLocalFile(TEXT("fontmap.dat"));
    if (filename == NULL)
        return FALSE; // 見つからなかった。

    // ファイル「fontmap.dat」を開く。
    if (FILE *fp = _tfopen(filename, TEXT("rb")))
    {
        // 一行ずつ読み込む。
        char buf[512];
        while (fgets(buf, _countof(buf), fp))
        {
            // UTF-8文字列をワイド文字列に変換する。
            WCHAR szText[512];
            MultiByteToWideChar(CP_UTF8, 0, buf, -1, szText, _countof(szText));
            szText[_countof(szText) - 1] = 0;

            // 前後の空白を取り除く。
            str_trim(szText);

            // 行コメントを削除する。
            if (auto pch = wcschr(szText, L';'))
            {
                *pch = 0;
            }

            // もう一度前後の空白を取り除く。
            str_trim(szText);

            // 「=」を探す。
            if (auto pch = wcschr(szText, L'='))
            {
                // 文字列を切り分ける。
                *pch++ = 0;
                auto font_name = szText;
                auto font_file = pch;

                // 前後の空白を取り除く。
                str_trim(font_name);
                str_trim(font_file);

                // 「,」があればインデックスを読み込み、切り分ける。
                pch = wcschr(pch, L',');
                int index = -1;
                if (pch)
                {
                    *pch++ = 0;
                    index = _wtoi(pch);
                }

                // さらに前後の空白を取り除く。
                str_trim(font_name);
                str_trim(font_file);

                // フォントファイルのパスファイル名を構築する。
                TCHAR font_pathname[MAX_PATH];
                GetWindowsDirectory(font_pathname, _countof(font_pathname));
                PathAppend(font_pathname, TEXT("Fonts"));
                PathAppend(font_pathname, font_file);

                // 存在しなかった？ ローカルフォントを試す。
                if (!PathFileExists(font_pathname))
                {
                    ExpandEnvironmentStrings(TEXT("%LOCALAPPDATA%\\Microsoft\\Windows\\Fonts"), font_pathname, _countof(font_pathname));
                    PathAppend(font_pathname, font_file);
                }

                // パスファイル名が存在するか？
                if (PathFileExists(font_pathname))
                {
                    // 存在すれば、フォントのエントリーを追加。
                    FONT_ENTRY entry;
                    entry.m_font_name = font_name;
                    entry.m_pathname = font_pathname;
                    entry.m_index = index;
                    m_font_map.push_back(entry);
                }
            }
        }

        // ファイルを閉じる。
        fclose(fp);
    }

    return m_font_map.size() > 0;
}

// コンストラクタ。
DekaMoji::DekaMoji(HINSTANCE hInstance, INT argc, LPTSTR *argv)
    : m_hInstance(hInstance)
    , m_argc(argc)
    , m_argv(argv)
{
    // データをリセットする。
    Reset();

    // フォントマップを読み込む。
    LoadFontMap();
}

// 既定値。
#define IDC_PAGE_SIZE_DEFAULT doLoadString(IDS_A4)
#define IDC_PAGE_DIRECTION_DEFAULT doLoadString(IDS_LANDSCAPE)
#define IDC_FONT_NAME_DEFAULT doLoadString(IDS_FONT_01)
#define IDC_METHOD_DEFAULT doLoadString(IDS_METHOD_02)
#define IDC_TEXT_DEFAULT doLoadString(IDS_SAMPLETEXT)
#define IDC_TEXT_COLOR_DEFAULT TEXT("#000000")

// データをリセットする。
void DekaMoji::Reset()
{
#define SETTING(id) m_settings[TEXT(#id)]
    SETTING(IDC_PAGE_SIZE) = IDC_PAGE_SIZE_DEFAULT;
    SETTING(IDC_PAGE_DIRECTION) = IDC_PAGE_DIRECTION_DEFAULT;
    SETTING(IDC_FONT_NAME) = IDC_FONT_NAME_DEFAULT;
    SETTING(IDC_METHOD) = IDC_METHOD_DEFAULT;
    SETTING(IDC_TEXT) = IDC_TEXT_DEFAULT;
    SETTING(IDC_TEXT_COLOR) = IDC_TEXT_COLOR_DEFAULT;
}

// ダイアログを初期化する。
void DekaMoji::InitDialog(HWND hwnd)
{
    // IDC_PAGE_SIZE: 用紙サイズ。
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A3));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A4));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A5));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_B4));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_B5));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_LETTER));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_LEGAL));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_EXECUTIVE));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_US4X6));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_US4X8));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_US5X7));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_COMM10));

    // IDC_PAGE_DIRECTION: ページの向き。
    SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PORTRAIT));
    SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_LANDSCAPE));

    // IDC_FONT_NAME: フォント名。
    if (m_font_map.size())
    {
        for (auto& entry : m_font_map)
        {
            SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_ADDSTRING, 0, (LPARAM)entry.m_font_name.c_str());
        }
    }

    // IDC_METHOD: テキストの配置方法。
    SendDlgItemMessage(hwnd, IDC_METHOD, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_METHOD_01));
    SendDlgItemMessage(hwnd, IDC_METHOD, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_METHOD_02));
    SendDlgItemMessage(hwnd, IDC_METHOD, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_METHOD_03));

    g_hwndImageView.doSubclass(GetDlgItem(hwnd, stc7));

    // プレビューを更新する。
    SetTimer(hwnd, TIMER_ID_REFRESH_PREVIEW, 0, NULL);
}

// ダイアログからデータへ。
BOOL DekaMoji::DataFromDialog(HWND hwnd, BOOL bNoError)
{
    TCHAR szText[MAX_PATH];
    // コンボボックスからデータを取得する。
#define GET_COMBO_DATA(id) do { \
    getComboText(hwnd, (id), szText, _countof(szText)); \
    str_trim(szText); \
    m_settings[TEXT(#id)] = szText; \
} while (0)

    GET_COMBO_DATA(IDC_PAGE_SIZE);
    GET_COMBO_DATA(IDC_PAGE_DIRECTION);
    GET_COMBO_DATA(IDC_FONT_NAME);
    GET_COMBO_DATA(IDC_METHOD);

    // チェックボックスからデータを取得する。
#define GET_CHECK_DATA(id) do { \
    if (IsDlgButtonChecked(hwnd, id) == BST_CHECKED) \
        m_settings[TEXT(#id)] = doLoadString(IDS_YES); \
    else \
        m_settings[TEXT(#id)] = doLoadString(IDS_NO); \
} while (0)
#undef GET_CHECK_DATA

    if (SETTING(IDC_FONT_NAME).empty())
    {
        if (!bNoError)
        {
            SETTING(IDC_FONT_NAME) = IDC_FONT_NAME_DEFAULT;
            ::SetFocus(::GetDlgItem(hwnd, IDC_FONT_NAME));
            OnInvalidString(hwnd, IDC_TEXT, IDS_FIELD_FONT_NAME, IDS_REASON_EMPTY_TEXT);
        }
        return FALSE;
    }

    GetDlgItemText(hwnd, IDC_TEXT, szText, _countof(szText));
    //str_trim(szText);
    if (szText[0] == 0)
    {
        if (!bNoError)
        {
            m_settings[TEXT("IDC_TEXT")] = IDC_TEXT_DEFAULT;
            ::SetFocus(::GetDlgItem(hwnd, IDC_TEXT));
            OnInvalidString(hwnd, IDC_TEXT, IDS_FIELD_TEXT, IDS_REASON_EMPTY_TEXT);
        }
        return FALSE;
    }
    m_settings[TEXT("IDC_TEXT")] = szText;

    GetDlgItemText(hwnd, IDC_TEXT_COLOR, szText, _countof(szText));
    str_trim(szText);
    if (szText[0] == 0)
    {
        if (!bNoError)
        {
            m_settings[TEXT("IDC_TEXT_COLOR")] = IDC_TEXT_COLOR_DEFAULT;
            ::SetFocus(::GetDlgItem(hwnd, IDC_TEXT_COLOR));
            OnInvalidString(hwnd, IDC_TEXT_COLOR, IDS_FIELD_TEXT_COLOR, IDS_REASON_EMPTY_TEXT);
        }
        return FALSE;
    }
    auto ansi = ansi_from_wide(CP_ACP, szText);
    auto color_value = color_value_parse(ansi);
    if (color_value == -1)
    {
        if (!bNoError)
        {
            m_settings[TEXT("IDC_TEXT_COLOR")] = IDC_TEXT_COLOR_DEFAULT;
            ::SetFocus(::GetDlgItem(hwnd, IDC_TEXT_COLOR));
            OnInvalidString(hwnd, IDC_TEXT_COLOR, IDS_FIELD_TEXT_COLOR, IDS_REASON_VALID_COLOR);
        }
        return FALSE;
    }
    m_settings[TEXT("IDC_TEXT_COLOR")] = szText;

    return TRUE;
}

// データからダイアログへ。
BOOL DekaMoji::DialogFromData(HWND hwnd)
{
    // コンボボックスへデータを設定する。
#define SET_COMBO_DATA(id) \
    setComboText(hwnd, (id), m_settings[TEXT(#id)].c_str());
    SET_COMBO_DATA(IDC_PAGE_SIZE);
    SET_COMBO_DATA(IDC_PAGE_DIRECTION);
    SET_COMBO_DATA(IDC_FONT_NAME);
    SET_COMBO_DATA(IDC_METHOD);
#undef SET_COMBO_DATA

    // チェックボックスへデータを設定する。
#define SET_CHECK_DATA(id) do { \
    if (m_settings[TEXT(#id)] == doLoadString(IDS_YES)) \
        CheckDlgButton(hwnd, (id), BST_CHECKED); \
    else \
        CheckDlgButton(hwnd, (id), BST_UNCHECKED); \
} while (0)
#undef SET_CHECK_DATA

    ::SetDlgItemText(hwnd, IDC_TEXT, SETTING(IDC_TEXT).c_str());
    ::SetDlgItemText(hwnd, IDC_TEXT_COLOR, SETTING(IDC_TEXT_COLOR).c_str());

    return TRUE;
}

// レジストリからデータへ。
BOOL DekaMoji::DataFromReg(HWND hwnd)
{
    // ソフト固有のレジストリキーを開く。
    HKEY hKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Katayama Hirofumi MZ\\DekaMojiPDF"), 0, KEY_READ, &hKey);
    if (!hKey)
        return FALSE; // 開けなかった。

    // レジストリからデータを取得する。
    TCHAR szText[MAX_PATH];
#define GET_REG_DATA(id) do { \
    szText[0] = 0; \
    DWORD cbText = sizeof(szText); \
    LONG error = RegQueryValueEx(hKey, TEXT(#id), NULL, NULL, (LPBYTE)szText, &cbText); \
    if (error == ERROR_SUCCESS) { \
        SETTING(id) = szText; \
    } \
} while(0)
    GET_REG_DATA(IDC_PAGE_SIZE);
    GET_REG_DATA(IDC_PAGE_DIRECTION);
    GET_REG_DATA(IDC_FONT_NAME);
    GET_REG_DATA(IDC_METHOD);
    GET_REG_DATA(IDC_TEXT);
    GET_REG_DATA(IDC_TEXT_COLOR);
#undef GET_REG_DATA

    // レジストリキーを閉じる。
    RegCloseKey(hKey);
    return TRUE;
}

// データからレジストリへ。
BOOL DekaMoji::RegFromData(HWND hwnd)
{
    HKEY hCompanyKey = NULL, hAppKey = NULL;

    // 会社固有のレジストリキーを作成または開く。
    RegCreateKey(HKEY_CURRENT_USER, TEXT("Software\\Katayama Hirofumi MZ"), &hCompanyKey);
    if (hCompanyKey == NULL)
        return FALSE; // 失敗。

    // ソフト固有のレジストリキーを作成または開く。
    RegCreateKey(hCompanyKey, TEXT("DekaMojiPDF"), &hAppKey);
    if (hAppKey == NULL)
    {
        RegCloseKey(hCompanyKey);
        return FALSE; // 失敗。
    }

    // レジストリにデータを設定する。
#define SET_REG_DATA(id) do { \
    auto& str = m_settings[TEXT(#id)]; \
    DWORD cbText = (str.size() + 1) * sizeof(WCHAR); \
    RegSetValueEx(hAppKey, TEXT(#id), 0, REG_SZ, (LPBYTE)str.c_str(), cbText); \
} while(0)
    SET_REG_DATA(IDC_PAGE_SIZE);
    SET_REG_DATA(IDC_PAGE_DIRECTION);
    SET_REG_DATA(IDC_FONT_NAME);
    SET_REG_DATA(IDC_METHOD);
    SET_REG_DATA(IDC_TEXT);
    SET_REG_DATA(IDC_TEXT_COLOR);
#undef SET_REG_DATA

    // レジストリキーを閉じる。
    RegCloseKey(hAppKey);
    RegCloseKey(hCompanyKey);

    return TRUE; // 成功。
}

// 文字列中に見つかった部分文字列をすべて置き換える。
template <typename T_STR>
inline bool
str_replace(T_STR& str, const T_STR& from, const T_STR& to)
{
    bool ret = false;
    size_t i = 0;
    for (;;) {
        i = str.find(from, i);
        if (i == T_STR::npos)
            break;
        ret = true;
        str.replace(i, from.size(), to);
        i += to.size();
    }
    return ret;
}
template <typename T_STR>
inline bool
str_replace(T_STR& str,
            const typename T_STR::value_type *from,
            const typename T_STR::value_type *to)
{
    return str_replace(str, T_STR(from), T_STR(to));
}

// ファイルサイズを取得する。
DWORD get_file_size(const string_t& filename)
{
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(filename.c_str(), &find);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0; // エラー。
    FindClose(hFind);
    if (find.nFileSizeHigh)
        return 0; // 大きすぎるのでエラー。
    return find.nFileSizeLow; // ファイルサイズ。
}

// libHaruのエラーハンドラの実装。
void hpdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void* user_data) {
    char message[1024];
    StringCchPrintfA(message, _countof(message), "error: error_no = %04X, detail_no = %d",
                     UINT(error_no), INT(detail_no));
    throw std::runtime_error(message);
}

// 長方形を描画する。
void hpdf_draw_box(HPDF_Page page, double x, double y, double width, double height)
{
    HPDF_Page_MoveTo(page, x, y);
    HPDF_Page_LineTo(page, x, y + height);
    HPDF_Page_LineTo(page, x + width, y + height);
    HPDF_Page_LineTo(page, x + width, y);
    HPDF_Page_ClosePath(page);
    HPDF_Page_Stroke(page);
}

// テキストを描画する。縦横比を考慮。
void hpdf_draw_text_1(HPDF_Page page, HPDF_Font font, double font_size,
                      const char *text,
                      double x, double y, double width, double height,
                      int draw_box = 0)
{
    // フォントサイズを制限。
    if (font_size > HPDF_MAX_FONTSIZE)
        font_size = HPDF_MAX_FONTSIZE;

    // 長方形を描画する。
    if (draw_box == 1)
    {
        hpdf_draw_box(page, x, y, width, height);
    }

    // 長方形に収まるフォントサイズを計算する。
    double text_width, text_height;
    double ratio2 = 1.0;
    for (;;)
    {
        // フォントとフォントサイズを指定。
        HPDF_Page_SetFontAndSize(page, font, font_size);

        // テキストの幅と高さを取得する。
        text_width = HPDF_Page_TextWidth(page, text);
        text_height = HPDF_Page_GetCurrentFontSize(page);

        text_width *= ratio2;
        text_height *= ratio2;

        if (text_height > height || text_width > width)
        {
            // フォントサイズを少し小さくして再計算。
            ratio2 *= 0.95;
            continue;
        }

        if (text_height * 1.1 < height && text_width * 1.1 < width)
        {
            // フォントサイズを少し大きくして再計算。
            ratio2 *= 1.1;
            continue;
        }

        // x,yを中央そろえ。
        x += (width - text_width) / 2;
        y += (height - text_height) / 2;
        break;
    }

    // テキストを描画する。
    HPDF_Page_BeginText(page);
    {
        // ベースラインからdescentだけずらす。
        double descent = -HPDF_Font_GetDescent(font) * font_size / 1000.0;
        descent *= ratio2;

        // 文字をページいっぱいにする。
        HPDF_Page_SetTextMatrix(page, ratio2, 0, 0, ratio2, x, y + descent);

        // テキストを描画する。
        HPDF_Page_ShowText(page, text);
    }
    HPDF_Page_EndText(page);

    // 長方形を描画する。
    if (draw_box == 2)
    {
        hpdf_draw_box(page, x, y, text_width, text_height);
    }
}

// テキストを描画する。縦横比を無視。
void hpdf_draw_text_2(HPDF_Page page, HPDF_Font font, double font_size,
                      const char *text,
                      double x, double y, double width, double height,
                      int draw_box = 0)
{
    // フォントサイズを制限。
    if (font_size > HPDF_MAX_FONTSIZE)
        font_size = HPDF_MAX_FONTSIZE;

    // 長方形を描画する。
    if (draw_box == 1)
    {
        hpdf_draw_box(page, x, y, width, height);
    }

    // 長方形に収まるフォントサイズを計算する。
    double text_width, text_height;
    double aspect1, aspect2, ratio1, ratio2 = 1.0;
    for (;;)
    {
        // フォントとフォントサイズを指定。
        HPDF_Page_SetFontAndSize(page, font, font_size);

        // テキストの幅と高さを取得する。
        text_width = HPDF_Page_TextWidth(page, text);
        text_height = HPDF_Page_GetCurrentFontSize(page);

        // アスペクト比を調整する。
        aspect1 = text_height / text_width;
        aspect2 = height / width;
        ratio1 = aspect2 / aspect1;
        text_height *= ratio1;

        text_width *= ratio2;
        text_height *= ratio2;

        if (text_width > width && text_height > height)
        {
            // フォントサイズを少し小さくして再計算。
            ratio2 *= 0.95;
            continue;
        }

        if (text_width * 1.1 < width && text_height * 1.1 < height)
        {
            // フォントサイズを少し大きくして再計算。
            ratio2 *= 1.1;
            continue;
        }

        // x,yを中央そろえ。
        x += (width - text_width) / 2;
        y += (height - text_height) / 2;
        break;
    }

    // テキストを描画する。
    HPDF_Page_BeginText(page);
    {
        // ベースラインからdescentだけずらす。
        double descent = -HPDF_Font_GetDescent(font) * font_size / 1000.0;
        descent *= ratio2 * ratio1;

        // 文字をページいっぱいにする。
        HPDF_Page_SetTextMatrix(page, ratio2, 0, 0, ratio1 * ratio2, x, y + descent);

        // テキストを描画する。
        HPDF_Page_ShowText(page, text);
    }
    HPDF_Page_EndText(page);

    // 長方形を描画する。
    if (draw_box == 2)
    {
        hpdf_draw_box(page, x, y, text_width, text_height);
    }
}

template <typename T_STR_CONTAINER>
inline void
str_split(T_STR_CONTAINER& container,
          const typename T_STR_CONTAINER::value_type& str,
          const typename T_STR_CONTAINER::value_type& chars)
{
    container.clear();
    size_t i = 0, k = str.find_first_of(chars);
    while (k != T_STR_CONTAINER::value_type::npos)
    {
        container.push_back(str.substr(i, k - i));
        i = k + 1;
        k = str.find_first_of(chars, i);
    }
    container.push_back(str.substr(i));
}

// テキストを描画する。
void hpdf_draw_multiline_text(HPDF_Page page, HPDF_Font font, double font_size,
                              const char *text,
                              double x, double y, double width, double height,
                              bool ignore_aspect = true)
{
    char buf[1024];
    StringCchCopyA(buf, _countof(buf), text);
    //StrTrimA(buf, " \t\r\n");

    std::string str = buf;
    str_replace(str, "\r\n", "\n"); // 改行コード。
    str_replace(str, "\xE3\x80\x80", "  "); // 全角空白「　」

    // 改行で分割。
    std::vector<std::string> lines;
    str_split(lines, str, std::string("\n"));

    auto rows = lines.size();
    if (rows == 0)
        return;

    for (size_t i = 0; i < rows; ++i)
    {
        auto line = lines[i];
        StringCchCopyA(buf, _countof(buf), line.c_str());
        //StrTrimA(buf, " \t\r\n");

        double line_x = x;
        double line_y = y + (height / rows) * (rows - i - 1);
        double line_width = width;
        double line_height = height / rows;

        if (ignore_aspect)
        {
            hpdf_draw_text_2(page, font, font_size, buf, 
                             line_x, line_y, line_width, line_height);
        }
        else
        {
            hpdf_draw_text_1(page, font, font_size, buf, 
                             line_x, line_y, line_width, line_height);
        }
    }
}

// 1文字ずつ分割する。空白文字は無視する。
void split_text_data(std::vector<string_t>& chars, const string_t& text)
{
    string_t data = text;
    str_replace(data, L" ", L"");
    str_replace(data, L"\t", L"");
    str_replace(data, L"\r", L"");
    str_replace(data, L"\n", L"");
    str_replace(data, L"\r", L"");
    str_replace(data, L"\x3000", L""); // 全角空白「　」

    for (auto& ch : data)
    {
        WCHAR sz[2];
        sz[0] = ch;
        sz[1] = 0;
        chars.push_back(sz);
    }
}

// メインディッシュ処理。
string_t DekaMoji::JustDoIt(HWND hwnd, LPCTSTR pszPdfFileName)
{
    string_t ret;
    // PDFオブジェクトを作成する。
    HPDF_Doc pdf = HPDF_New(hpdf_error_handler, NULL);
    if (!pdf)
        return L"";

    try
    {
        // エンコーディング 90ms-RKSJ-H, 90ms-RKSJ-V, 90msp-RKSJ-H, EUC-H, EUC-V が利用可能となる
        HPDF_UseJPEncodings(pdf);

        // エンコーディング "UTF-8" が利用可能に？？？
        HPDF_UseUTFEncodings(pdf);
        HPDF_SetCurrentEncoder(pdf, "UTF-8");

        // 日本語フォントの MS-(P)Mincyo, MS-(P)Gothic が利用可能となる
        //HPDF_UseJPFonts(pdf);

        // 用紙の向き。
        HPDF_PageDirection direction;
        if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_PORTRAIT))
            direction = HPDF_PAGE_PORTRAIT;
        else if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_LANDSCAPE))
            direction = HPDF_PAGE_LANDSCAPE;
        else
            direction = HPDF_PAGE_PORTRAIT;

        // ページサイズ。
        HPDF_PageSizes page_size;
        if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A3))
            page_size = HPDF_PAGE_SIZE_A3;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A4))
            page_size = HPDF_PAGE_SIZE_A4;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A5))
            page_size = HPDF_PAGE_SIZE_A5;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_B4))
            page_size = HPDF_PAGE_SIZE_B4;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_B5))
            page_size = HPDF_PAGE_SIZE_B5;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_LETTER))
            page_size = HPDF_PAGE_SIZE_LETTER;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_LEGAL))
            page_size = HPDF_PAGE_SIZE_LEGAL;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_EXECUTIVE))
            page_size = HPDF_PAGE_SIZE_EXECUTIVE;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_US4X6))
            page_size = HPDF_PAGE_SIZE_US4x6;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_US4X8))
            page_size = HPDF_PAGE_SIZE_US4x8;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_US5X7))
            page_size = HPDF_PAGE_SIZE_US5x7;
        else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_COMM10))
            page_size = HPDF_PAGE_SIZE_COMM10;
        else
            page_size = HPDF_PAGE_SIZE_A4;

        // ページ余白。
        double margin = pixels_from_mm(10);

        // フォント名。
        string_t font_name;
        for (auto& entry : m_font_map)
        {
            if (entry.m_font_name != SETTING(IDC_FONT_NAME))
                continue;

            auto ansi = ansi_from_wide(CP_ACP, entry.m_pathname.c_str());
            if (entry.m_index != -1)
            {
                std::string font_name_a = HPDF_LoadTTFontFromFile2(pdf, ansi, entry.m_index, HPDF_TRUE);
                font_name = wide_from_ansi(CP_UTF8, font_name_a.c_str());
            }
            else
            {
                std::string font_name_a = HPDF_LoadTTFontFromFile(pdf, ansi, HPDF_TRUE);
                font_name = wide_from_ansi(CP_UTF8, font_name_a.c_str());
            }
        }

        // フォントサイズ（pt）。
        double font_size = HPDF_MAX_FONTSIZE;

        // アスペクト比を無視する。
        bool ignore_aspect = true;
        // 1ページに1文字ずつ配置する。
        bool one_char_per_page = false;
        if (SETTING(IDC_METHOD) == doLoadString(IDS_METHOD_01))
        {
            ignore_aspect = false;
            one_char_per_page = false;
        }
        else if (SETTING(IDC_METHOD) == doLoadString(IDS_METHOD_02))
        {
            ignore_aspect = true;
            one_char_per_page = false;
        }
        else if (SETTING(IDC_METHOD) == doLoadString(IDS_METHOD_03))
        {
            ignore_aspect = false;
            one_char_per_page = true;
        }
        else
        {
            ignore_aspect = true;
            one_char_per_page = false;
        }

        // 出力ファイル名。
        string_t output_name;
        {
            SYSTEMTIME st;
            ::GetLocalTime(&st);
            TCHAR szText[MAX_PATH];
            StringCchPrintf(szText, _countof(szText), doLoadString(IDS_OUTPUT_NAME),
                            st.wYear, st.wMonth, st.wDay);
            output_name = szText;
        }

        // テキストデータ。
        string_t text_data = SETTING(IDC_TEXT);

        // テキストの色。
        double r_value, g_value, b_value;
        {
            auto text = SETTING(IDC_TEXT_COLOR);
            auto ansi = ansi_from_wide(CP_ACP, text.c_str());
            uint32_t text_color = color_value_parse(ansi);
            text_color = color_value_fix(text_color);
            r_value = GetRValue(text_color) / 255.5;
            g_value = GetGValue(text_color) / 255.5;
            b_value = GetBValue(text_color) / 255.5;
        }

        HPDF_Page page; // ページオブジェクト。
        HPDF_Font font; // フォントオブジェクト。
        double page_width, page_height; // ページサイズ。
        double content_x, content_y, content_width, content_height; // ページ内容の位置とサイズ。
        if (one_char_per_page)
        {
            // 1文字ずつ分割する。空白文字は無視する。
            std::vector<string_t> chars;
            split_text_data(chars, text_data);

            for (size_t iPage = 0; iPage < chars.size(); ++iPage)
            {
                // 文字列。
                string_t& str = chars[iPage];

                // ページを追加する。
                page = HPDF_AddPage(pdf);

                // ページサイズと用紙の向きを指定。
                HPDF_Page_SetSize(page, page_size, direction);

                // ページサイズ（ピクセル単位）。
                page_width = HPDF_Page_GetWidth(page);
                page_height = HPDF_Page_GetHeight(page);

                // ページ内容の位置とサイズ。
                content_x = margin;
                content_y = margin;
                content_width = page_width - margin * 2;
                content_height = page_height - margin * 2;

                // 線の幅を指定。
                HPDF_Page_SetLineWidth(page, 2);

                // 線の色を RGB で設定する。PDF では RGB 各値を [0,1] で指定することになっている。
                HPDF_Page_SetRGBStroke(page, r_value, g_value, b_value);

                /* 塗りつぶしの色を RGB で設定する。PDF では RGB 各値を [0,1] で指定することになっている。*/
                HPDF_Page_SetRGBFill(page, r_value, g_value, b_value);

                // フォントを指定する。
                auto font_name_a = ansi_from_wide(CP932, font_name.c_str());
                font = HPDF_GetFont(pdf, font_name_a, "UTF-8");

                // ANSI文字列に変換してテキストを描画する。
                auto text_a = ansi_from_wide(CP_UTF8, str.c_str());
                hpdf_draw_multiline_text(page, font, font_size, text_a,
                    content_x, content_y, content_width, content_height, ignore_aspect);
            }
        }
        else
        {
            // ページを追加する。
            page = HPDF_AddPage(pdf);

            // ページサイズと用紙の向きを指定。
            HPDF_Page_SetSize(page, page_size, direction);

            // ページサイズ（ピクセル単位）。
            page_width = HPDF_Page_GetWidth(page);
            page_height = HPDF_Page_GetHeight(page);

            // ページ内容の位置とサイズ。
            content_x = margin;
            content_y = margin;
            content_width = page_width - margin * 2;
            content_height = page_height - margin * 2;

            // 線の幅を指定。
            HPDF_Page_SetLineWidth(page, 2);

            // 線の色を RGB で設定する。PDF では RGB 各値を [0,1] で指定することになっている。
            HPDF_Page_SetRGBStroke(page, r_value, g_value, b_value);

            /* 塗りつぶしの色を RGB で設定する。PDF では RGB 各値を [0,1] で指定することになっている。*/
            HPDF_Page_SetRGBFill(page, r_value, g_value, b_value);

            // フォントを指定する。
            auto font_name_a = ansi_from_wide(CP932, font_name.c_str());
            font = HPDF_GetFont(pdf, font_name_a, "UTF-8");

            // ANSI文字列に変換してテキストを描画する。
            auto text_a = ansi_from_wide(CP_UTF8, text_data.c_str());
            hpdf_draw_multiline_text(page, font, font_size, text_a,
                content_x, content_y, content_width, content_height, ignore_aspect);
        }

        // PDF出力。
        {
            // PDFを一時ファイルに保存する。
            TempFile temp_file(TEXT("DM2"), TEXT(".pdf"));
            std::string temp_file_a = ansi_from_wide(CP_ACP, temp_file.make());
            HPDF_SaveToFile(pdf, temp_file_a.c_str());

            TCHAR szPath[MAX_PATH];
            if (pszPdfFileName) // ファイル名指定があれば
            {
                // それを使う。
                StringCchCopy(szPath, _countof(szPath), pszPdfFileName);
            }
            else // 指定がなければ
            {
                // デスクトップにファイルをコピー。
                SHGetSpecialFolderPath(hwnd, szPath, CSIDL_DESKTOPDIRECTORY, FALSE);
                PathAppend(szPath, output_name.c_str());
                LPTSTR pch = PathFindExtension(szPath);
                UINT iTry = 1;
                TCHAR szNum[32];
                while (PathFileExists(szPath))
                {
                    *pch = 0;
                    StringCchPrintf(szNum, _countof(szNum), TEXT(" (%u)"), iTry);
                    StringCchCat(szPath, _countof(szPath), szNum);
                    PathAddExtension(szPath, TEXT(".pdf"));
                    ++iTry;
                }
            }
            if (!CopyFile(temp_file.get(), szPath, FALSE))
            {
                auto err_msg = ansi_from_wide(CP_ACP, doLoadString(IDS_COPYFILEFAILED));
                throw std::runtime_error(err_msg);
            }

            // 成功メッセージを表示。
            TCHAR szText[MAX_PATH];
            StringCchPrintf(szText, _countof(szText), doLoadString(IDS_SUCCEEDED), PathFindFileName(szPath));
            ret = szText;
        }
    }
    catch (std::runtime_error& err)
    {
        // 失敗。
        auto wide = wide_from_ansi(CP_ACP, err.what());
        MessageBoxW(hwnd, wide, NULL, MB_ICONERROR);
        return TEXT("");
    }

    // PDFオブジェクトを解放する。
    HPDF_Free(pdf);

    return ret;
}

// WM_INITDIALOG
// ダイアログの初期化。
BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    // ユーザデータ。
    DekaMoji* pDM = (DekaMoji*)lParam;

    // ユーザーデータをウィンドウハンドルに関連付ける。
    SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);

    // アプリの名前。
    LoadString(NULL, IDS_APPNAME, g_szAppName, _countof(g_szAppName));

    // アイコンの設定。
    g_hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(1));
    g_hIconSm = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (WPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (WPARAM)g_hIconSm);

    // 初期化。
    pDM->InitDialog(hwnd);

    // レジストリからデータを読み込む。
    pDM->DataFromReg(hwnd);

    // ダイアログにデータを設定。
    pDM->DialogFromData(hwnd);

    // 少し大きなフォントをテキストボックスにセット。
    LOGFONT lf;
    GetObject(GetWindowFont(hwnd), sizeof(lf), &lf);
    lf.lfHeight = lf.lfHeight * 4 / 3;
    g_hTextFont = CreateFontIndirect(&lf);
    SetWindowFont(GetDlgItem(hwnd, IDC_TEXT), g_hTextFont, TRUE);

    // まれにテキストボックスが再描画されないことがあるのでここで再描画。
    InvalidateRect(GetDlgItem(hwnd, IDC_TEXT), NULL, TRUE);

    return TRUE;
}

// 「OK」ボタンが押された。
BOOL OnOK(HWND hwnd)
{
    // ユーザデータ。
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // 「処理中...」とボタンに表示する。
    HWND hButton = GetDlgItem(hwnd, IDC_GENERATE);
    SetWindowText(hButton, doLoadString(IDS_PROCESSINGNOW));

    // ダイアログからデータを取得。
    if (!pDM->DataFromDialog(hwnd)) // 失敗。
    {
        // ボタンテキストを元に戻す。
        SetWindowText(hButton, doLoadString(IDS_GENERATE));

        return FALSE; // 失敗。
    }

    // 設定をレジストリに保存。
    pDM->RegFromData(hwnd);

    // メインディッシュ処理。
    string_t success = pDM->JustDoIt(hwnd);

    // ボタンテキストを元に戻す。
    SetWindowText(hButton, doLoadString(IDS_GENERATE));

    // 必要なら結果をメッセージボックスとして表示する。
    if (success.size())
    {
        MessageBox(hwnd, success.c_str(), g_szAppName, MB_ICONINFORMATION);
    }

    return TRUE; // 成功。
}

// 「設定の初期化」ボタン。
void OnEraseSettings(HWND hwnd)
{
    // ユーザーデータ。
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // データをリセットする。
    pDM->Reset();

    // データからダイアログへ。
    pDM->DialogFromData(hwnd);

    // データからレジストリへ。
    pDM->RegFromData(hwnd);
}

// 「テキストの色」ボタンが押された。
void OnTextColorButton(HWND hwnd)
{
    // テキストの色を取得する。
    TCHAR szText[64];
    GetDlgItemText(hwnd, IDC_TEXT_COLOR, szText, _countof(szText));
    StrTrim(szText, TEXT(" \t\r\n"));
    auto ansi = ansi_from_wide(CP_ACP, szText);
    auto text_color = color_value_parse(ansi);
    if (text_color == -1)
        text_color = 0;
    text_color = color_value_fix(text_color);

    static COLORREF custom_colors[16] = {
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
    };
    CHOOSECOLOR cc = { sizeof(cc), hwnd };
    cc.rgbResult = text_color;
    cc.lpCustColors = custom_colors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (::ChooseColor(&cc))
    {
        StringCchPrintf(szText, _countof(szText), TEXT("#%02X%02X%02X"),
            GetRValue(cc.rgbResult),
            GetGValue(cc.rgbResult),
            GetBValue(cc.rgbResult)
        );
        SetDlgItemText(hwnd, IDC_TEXT_COLOR, szText);
        InvalidateRect(GetDlgItem(hwnd, IDC_TEXT_COLOR_BUTTON), NULL, TRUE);
    }
}

void OnReadMe(HWND hwnd)
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("README.txt"));
    if (!PathFileExists(szPath))
    {
        PathRemoveFileSpec(szPath);
        PathRemoveFileSpec(szPath);
        PathAppend(szPath, TEXT("README.txt"));
    }
    ShellExecute(hwnd, NULL, szPath, NULL, NULL, SW_SHOWNORMAL);
}

static INT s_nRefreshCounter = 0;

void doRefreshPreview(HWND hwnd, DWORD dwDelay = 500)
{
    ++s_nRefreshCounter;
    KillTimer(hwnd, TIMER_ID_REFRESH_PREVIEW);
    SetTimer(hwnd, TIMER_ID_REFRESH_PREVIEW, dwDelay, NULL); // 少し後でプレビューを更新する。

    if (s_nRefreshCounter >= 3)
    {
        HBITMAP hbmWait = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_WAIT));
        g_hwndImageView.setImage(hbmWait);
    }
}

// WM_COMMAND
// コマンド。
void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDC_GENERATE: // 「PDF生成」ボタン。
        OnOK(hwnd);
        break;
    case IDC_EXIT: // 「終了」ボタン。
        EndDialog(hwnd, id);
        break;
    case IDC_ERASESETTINGS: // 「設定の初期化」ボタン。
        OnEraseSettings(hwnd);
        doRefreshPreview(hwnd, 0);
        break;
    case IDC_README: // 「README」ボタン。
        OnReadMe(hwnd);
        break;
    case IDC_TEXT: // テキスト
        if (codeNotify == EN_CHANGE)
        {
            doRefreshPreview(hwnd);
        }
        break;
    case IDC_PAGE_SIZE: // 「用紙サイズ」
    case IDC_PAGE_DIRECTION: // 「ページの向き」
    case IDC_METHOD: // 「レイアウト」
    case IDC_FONT_NAME: // 「フォント名」
        if (codeNotify == CBN_SELCHANGE || codeNotify == CBN_SELENDOK)
        {
            doRefreshPreview(hwnd, 0);
        }
        break;
    case stc1:
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        ::SetFocus(::GetDlgItem(hwnd, cmb1));
        break;
    case stc2:
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        ::SetFocus(::GetDlgItem(hwnd, cmb2));
        break;
    case stc3:
        // テキストボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        {
            HWND hEdit = ::GetDlgItem(hwnd, edt1);
            Edit_SetSel(hEdit, 0, -1); // すべて選択。
            ::SetFocus(hEdit);
        }
        break;
    case stc4:
        // テキストボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        {
            HWND hEdit = ::GetDlgItem(hwnd, edt2);
            Edit_SetSel(hEdit, 0, -1); // すべて選択。
            ::SetFocus(hEdit);
        }
        break;
    case stc5:
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        ::SetFocus(::GetDlgItem(hwnd, cmb5));
        break;
    case stc6:
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        ::SetFocus(::GetDlgItem(hwnd, cmb3));
        break;
    case IDC_TEXT_COLOR: // 「テキストの色」テキストボックス。
        if (codeNotify == EN_CHANGE)
        {
            ::InvalidateRect(::GetDlgItem(hwnd, IDC_TEXT_COLOR_BUTTON), NULL, TRUE);
            doRefreshPreview(hwnd, 0);
            break;
        }
    case IDC_TEXT_COLOR_BUTTON: // 「テキストの色」ボタン。
        if (codeNotify == BN_CLICKED)
        {
            OnTextColorButton(hwnd);
            doRefreshPreview(hwnd, 0);
        }
        break;
    }
}

// WM_DRAWITEM
void OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT * lpDrawItem)
{
    HWND hButton = ::GetDlgItem(hwnd, IDC_TEXT_COLOR_BUTTON);
    if (lpDrawItem->hwndItem != hButton)
        return;

    // テキストの色を取得する。
    TCHAR szText[64];
    GetDlgItemText(hwnd, IDC_TEXT_COLOR, szText, _countof(szText));
    StrTrim(szText, TEXT(" \t\r\n"));
    auto ansi = ansi_from_wide(CP_ACP, szText);
    auto text_color = color_value_parse(ansi);
    if (text_color == -1)
        text_color = 0;
    text_color = color_value_fix(text_color);

    BOOL bPressed = !!(lpDrawItem->itemState & ODS_CHECKED);

    // ボタンを描画する。
    RECT rcItem = lpDrawItem->rcItem;
    UINT uState = DFCS_BUTTONPUSH | DFCS_ADJUSTRECT;
    if (bPressed)
        uState |= DFCS_PUSHED;
    ::DrawFrameControl(lpDrawItem->hDC, &rcItem, DFC_BUTTON, uState);

    // 色ボタンの内側を描画する。
    HBRUSH hbr = ::CreateSolidBrush(text_color);
    ::FillRect(lpDrawItem->hDC, &rcItem, hbr);
    ::DeleteObject(hbr);
}

// WM_DESTROY
// ウィンドウが破棄された。
void OnDestroy(HWND hwnd)
{
    // アイコンを破棄。
    DestroyIcon(g_hIcon);
    DestroyIcon(g_hIconSm);
    g_hIcon = g_hIconSm = NULL;

    // フォントを破棄。
    DeleteObject(g_hTextFont);
    g_hTextFont = NULL;
}

class MFileDeleter
{
public:
    LPCTSTR m_file;
    MFileDeleter(LPCTSTR file) : m_file(file)
    {
    }
    ~MFileDeleter()
    {
        DeleteFile(m_file);
    }
};

BOOL doUpdatePreview(HWND hwnd)
{
    s_nRefreshCounter = 0;

    // PDFファイル名。
    TCHAR szPdfFile[MAX_PATH];
    ExpandEnvironmentStrings(TEXT("%TEMP%\\dekamoji.pdf"), szPdfFile, _countof(szPdfFile));

    // PNGファイル名。
    TCHAR szPngFile[MAX_PATH];
    ExpandEnvironmentStrings(TEXT("%TEMP%\\dekamoji"), szPngFile, _countof(szPngFile));
    StringCchCat(szPngFile, _countof(szPngFile), L"%d.png");

    // ダイアログからデータを取得。
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!pDM->DataFromDialog(hwnd, TRUE))
        return FALSE; // 失敗。

    // メインディッシュ処理。
    string_t success = pDM->JustDoIt(hwnd, szPdfFile);
    if (success.empty())
        return FALSE; // 失敗。

    MFileDeleter pdfFileDeleter(szPdfFile);

    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, TEXT("mutool.exe"));

    if (!PathFileExists(szPath))
        return FALSE;

    // mutool convertでPDFをPNGに変換。
    {
        string_t params;
        params += TEXT("convert -O resolution=16 -o \"");
        params += szPngFile;
        params += TEXT("\" \"");
        params += szPdfFile;
        params += TEXT("\" 1");

        SHELLEXECUTEINFO info = { sizeof(info) };
        info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
        info.hwnd = hwnd;
        info.lpFile = szPath;
        info.lpParameters = params.c_str();
        info.nShow = SW_HIDE;
        if (!ShellExecuteEx(&info))
            return FALSE; // 失敗。

        WaitForSingleObject(info.hProcess, INFINITE);
        CloseHandle(info.hProcess);
    }

    // PNGを読み込む。最初は1ページ。
    ExpandEnvironmentStrings(TEXT("%TEMP%\\dekamoji"), szPngFile, _countof(szPngFile));
    StringCchCat(szPngFile, _countof(szPngFile), L"1.png");
    MFileDeleter pngFileDeleter(szPngFile);

    HBITMAP hbm1 = NULL, hbm2 = NULL;
    try
    {
        Gdiplus::Bitmap image(szPngFile);

        Gdiplus::Color white_color(Gdiplus::Color::White);
        image.GetHBITMAP(white_color, &hbm1);

        BITMAP bm;
        GetObject(hbm1, sizeof(bm), &bm);

        const INT margin = 4;
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = bm.bmWidth + margin * 2;
        bmi.bmiHeader.biHeight = bm.bmHeight + margin * 2;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        HDC hdc1 = CreateCompatibleDC(NULL);
        HDC hdc2 = CreateCompatibleDC(NULL);
        LPVOID pvBits;
        hbm2 = CreateDIBSection(hdc1, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
        if (hbm2)
        {
            HGDIOBJ hbm1Old = SelectObject(hdc1, hbm1);
            HGDIOBJ hbm2Old = SelectObject(hdc2, hbm2);
            BitBlt(hdc2, margin, margin, bm.bmWidth, bm.bmHeight, hdc1, 0, 0, SRCCOPY);
            SelectObject(hdc1, hbm1Old);
            SelectObject(hdc2, hbm2Old);

            g_hwndImageView.setImage(hbm2);
        }
        DeleteObject(hbm1);
    }
    catch (...)
    {
        DeleteObject(hbm1);
        DeleteObject(hbm2);
        return FALSE;
    }

    return TRUE;
}

void OnTimer(HWND hwnd, UINT id)
{
    if (id != TIMER_ID_REFRESH_PREVIEW)
        return;

    KillTimer(hwnd, id);

    if (!doUpdatePreview(hwnd))
    {
        g_hwndImageView.setImage(NULL);
    }
}

// ダイアログプロシージャ。
INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_DRAWITEM, OnDrawItem);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
        HANDLE_MSG(hwnd, WM_TIMER, OnTimer);
    }
    return 0;
}

// デカ文字PDFのメイン関数。
INT DekaMoji_Main(HINSTANCE hInstance, INT argc, LPTSTR *argv)
{
    // GDI+を初期化。
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // アプリのインスタンスを保持する。
    g_hInstance = hInstance;

    // 共通コントロール群を初期化する。
    InitCommonControls();

    // ユーザーデータを保持する。
    DekaMoji dm(hInstance, argc, argv);

    // ユーザーデータをパラメータとしてダイアログを開く。
    DialogBoxParam(hInstance, MAKEINTRESOURCE(1), NULL, DialogProc, (LPARAM)&dm);

    // GDI+を終了。
    Gdiplus::GdiplusShutdown(gdiplusToken);

    // 正常終了。
    return 0;
}

// Windowsアプリのメイン関数。
INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
#ifdef UNICODE
    // wWinMainをサポートしていないコンパイラのために、コマンドラインの処理を行う。
    INT argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    INT ret = DekaMoji_Main(hInstance, argc, argv);
    LocalFree(argv);
    return ret;
#else
    return DekaMoji_Main(hInstance, __argc, __argv);
#endif
}
