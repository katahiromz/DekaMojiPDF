// デカ文字PDF by katahiromz
// Copyright (C) 2022-2024 片山博文MZ. All Rights Reserved.
// See README.txt and LICENSE.txt.

// For detecting memory leak (for MSVC only)
#if defined(_MSC_VER) && !defined(NDEBUG) && !defined(_CRTDBG_MAP_ALLOC)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

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
#include <algorithm>        // std::reverse。
#include <cmath>            // C言語の数学ライブラリ。
#include <cassert>          // assertマクロ。
#include "MImageViewEx.h"   // イメージプレビュー用のウィンドウ コントロール。
#include "MTempFile.hpp"    // 一時ファイル操作用のヘッダ。
#include "MCenterWindow.h"  // ウィンドウの中央寄せ。
#include "MPageMgr.h"       // ページマネージャ。
#include "MString.hpp"      // 文字列を扱うヘッダ。
#include "MColorBox.h"      // 色ボタン。
#include "resource.h"       // リソースIDの定義ヘッダ。

// 文字列クラス。
#ifdef UNICODE
    typedef std::wstring string_t;
#else
    typedef std::string string_t;
#endif

// シフトJIS コードページ（Shift_JIS）。
#define CP932  932

// The ranges of the surrogate pairs
#define HIGH_SURROGATE_MIN 0xD800U
#define HIGH_SURROGATE_MAX 0xDBFFU
#define LOW_SURROGATE_MIN  0xDC00U
#define LOW_SURROGATE_MAX  0xDFFFU
//#define IS_HIGH_SURROGATE(ch0) (HIGH_SURROGATE_MIN <= (ch0) && (ch0) <= HIGH_SURROGATE_MAX)
//#define IS_LOW_SURROGATE(ch1)  (LOW_SURROGATE_MIN  <= (ch1) && (ch1) <=  LOW_SURROGATE_MAX)

// サロゲートペアを考慮した文字列の文字数を返す関数。
INT ucchStrLen(LPCWSTR text)
{
    INT ucch = 0;
    while (*text)
    {
        if (IS_HIGH_SURROGATE(text[0]) && IS_LOW_SURROGATE(text[1]))
            ++text;
        ++ucch;
        ++text;
    }
    return ucch;
}

// プレビューを再描画するタイマーID。
#define TIMER_ID_REFRESH_PREVIEW 999

// わかりやすい項目名を使用する。
enum
{
    IDC_GENERATE = IDOK,
    IDC_EXIT = IDCANCEL,
    IDC_PAGE_SIZE = cmb1,
    IDC_PAGE_DIRECTION = cmb2,
    IDC_FONT_NAME = cmb3,
    IDC_LETTER_ASPECT = cmb4,
    IDC_TEXT = edt1,
    IDC_TEXT_COLOR = psh1,
    IDC_BACK_COLOR = psh2,
    IDC_SETTINGS = psh5,
    IDC_README = psh6,
    IDC_V_ADJUST = edt3,
    IDC_VERTICAL = chx1,
    IDC_ONE_LETTER_PER_PAPER = chx2,
    IDC_PAGE_LEFT = psh7,
    IDC_PAGE_INFO = stc5,
    IDC_PAGE_RIGHT = psh8,
    IDC_FONT_LEFT = psh9,
    IDC_FONT_RIGHT = psh10,
};

// デカ文字PDFのメインクラス。
class DekaMoji
{
public:
    HINSTANCE m_hInstance;
    INT m_argc;
    LPTSTR *m_argv;
    std::map<string_t, string_t> m_settings;
    std::vector<string_t> m_fonts;

    // コンストラクタ。
    DekaMoji(HINSTANCE hInstance, INT argc, LPTSTR *argv);

    // デストラクタ。
    ~DekaMoji()
    {
    }

    // フォントマップを読み込む。
    BOOL LoadFonts();
    // データをリセットする。
    void Reset();
    // ダイアログを初期化する。
    void OnInitDialog(HWND hwnd);
    // ダイアログからデータへ。
    BOOL DataFromDialog(HWND hwnd, BOOL bNoError = FALSE);
    // データからダイアログへ。
    BOOL DialogFromData(HWND hwnd);
    // レジストリからデータへ。
    BOOL DataFromReg(HWND hwnd, LPCTSTR pszSubKey = NULL);
    // データからレジストリへ。
    BOOL RegFromData(HWND hwnd, LPCTSTR pszSubKey = NULL);

    // PDFを作成する処理。
    BOOL MakePDF(HWND hwnd, LPCTSTR pszPdfFileName);
};

// グローバル変数。
HINSTANCE g_hInstance = NULL; // インスタンス。
HWND g_hMainWnd = NULL; // メイン ウィンドウ。
TCHAR g_szAppName[256] = TEXT(""); // アプリ名。
HICON g_hIcon = NULL; // アイコン（大）。
HICON g_hIconSm = NULL; // アイコン（小）。
HFONT g_hTextFont = NULL; // テキストフォント。
MImageViewEx g_hwndImageView; // プレビューを表示する。
MColorBox g_textColorButton; // 文字色ボタン。
MColorBox g_backColorButton; // 背景色ボタン。
LONG g_nVAdjust = 0; // 垂直位置補正。
BOOL g_bVertical = FALSE; // 縦書きかどうか。
BOOL g_bOneLetterPerPaper = FALSE; // 用紙1枚に1文字だけ。
MPageMgr g_pageMgr; // 複数ページの管理。

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
#define SPACES L" \t\r\n\x3000"
    StrTrimW(text, SPACES);
}

// レジストリに設定名を保存するためにエンコードする。
void EncodeName(LPWSTR szName)
{
    WCHAR szEncoded[MAX_PATH], szHEX[16];
    LPWSTR pch0 = szName, pch1 = szEncoded;
    while (*pch0)
    {
        StringCchPrintfW(szHEX, _countof(szHEX), L"%04X", *pch0++);
        *pch1++ = szHEX[0];
        *pch1++ = szHEX[1];
        *pch1++ = szHEX[2];
        *pch1++ = szHEX[3];
    }
    *pch1 = 0;
    StringCchCopyW(szName, MAX_PATH, szEncoded);
}

// レジストリに保存した設定名を復元するためにデコードする。
void DecodeName(LPWSTR szName)
{
    WCHAR szDecoded[MAX_PATH], szHEX[16];
    LPWSTR pch0 = szName, pch1 = szDecoded;
    while (*pch0)
    {
        szHEX[0] = *pch0++;
        if (!*pch0) break;
        szHEX[1] = *pch0++;
        if (!*pch0) break;
        szHEX[2] = *pch0++;
        if (!*pch0) break;
        szHEX[3] = *pch0++;
        szHEX[4] = 0;
        *pch1++ = (WCHAR)wcstoul(szHEX, NULL, 16);
    }
    *pch1 = 0;
    StringCchCopyW(szName, MAX_PATH, szDecoded);
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
    MsgBoxDx(hwnd, szText, g_szAppName, MB_ICONERROR);
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
std::string ansi_from_wide(UINT codepage, LPCWSTR wide)
{
    CHAR ansi[MAX_PATH];

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

    WideCharToMultiByte(codepage, 0, wide, -1, ansi, MAX_PATH, geta, NULL);
    ansi[MAX_PATH - 2] = 0;
    ansi[MAX_PATH - 1] = 0;

    return ansi;
}

// ANSI文字列をワイド文字列に変換する。
std::wstring wide_from_ansi(UINT codepage, LPCSTR ansi)
{
    WCHAR wide[MAX_PATH];
    MultiByteToWideChar(codepage, 0, ansi, -1, wide, MAX_PATH);
    wide[MAX_PATH - 1] = 0;
    return wide;
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

static
INT CALLBACK
EnumFontFamProc(
    const LOGFONT *plf,
    const TEXTMETRIC *ptm,
    DWORD FontType,
    LPARAM lParam)
{
    if (plf->lfFaceName[0] == TEXT('@'))
        return TRUE;
    auto pList = reinterpret_cast<std::vector<string_t> *>(lParam);
    pList->push_back(plf->lfFaceName);
    return TRUE;
}

// フォントを読み込む。
BOOL DekaMoji::LoadFonts()
{
    m_fonts.clear();

    HDC hDC = CreateCompatibleDC(NULL);
    ::EnumFontFamilies(hDC, nullptr, (FONTENUMPROC)EnumFontFamProc, (LPARAM)&m_fonts);
    DeleteDC(hDC);

    std::sort(m_fonts.begin(), m_fonts.end());
    return TRUE;
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
    LoadFonts();
}

// 既定値。
#define IDC_PAGE_SIZE_DEFAULT doLoadString(IDS_A4)
#define IDC_PAGE_DIRECTION_DEFAULT doLoadString(IDS_LANDSCAPE)
#define IDC_FONT_NAME_DEFAULT doLoadString(IDS_FONT_01)
#define IDC_LETTER_ASPECT_DEFAULT doLoadString(IDS_ASPECT_250)
#define IDC_TEXT_DEFAULT doLoadString(IDS_SAMPLETEXT)
#define IDC_V_ADJUST_DEFAULT TEXT("0")
#define IDC_VERTICAL_DEFAULT TEXT("0")
#define IDC_ONE_LETTER_PER_PAPER_DEFAULT TEXT("0")
#define IDC_TEXT_COLOR_DEFAULT TEXT("#000000")
#define IDC_BACK_COLOR_DEFAULT TEXT("#FFFFFF")

// データをリセットする。
void DekaMoji::Reset()
{
#define SETTING(id) m_settings[TEXT(#id)]
    SETTING(IDC_PAGE_SIZE) = IDC_PAGE_SIZE_DEFAULT;
    SETTING(IDC_PAGE_DIRECTION) = IDC_PAGE_DIRECTION_DEFAULT;
    SETTING(IDC_FONT_NAME) = IDC_FONT_NAME_DEFAULT;
    SETTING(IDC_TEXT) = IDC_TEXT_DEFAULT;
    SETTING(IDC_TEXT_COLOR) = IDC_TEXT_COLOR_DEFAULT;
    SETTING(IDC_BACK_COLOR) = IDC_BACK_COLOR_DEFAULT;
    SETTING(IDC_V_ADJUST) = IDC_V_ADJUST_DEFAULT;
    SETTING(IDC_VERTICAL) = IDC_VERTICAL_DEFAULT;
    SETTING(IDC_ONE_LETTER_PER_PAPER) = IDC_ONE_LETTER_PER_PAPER_DEFAULT;
    SETTING(IDC_LETTER_ASPECT) = IDC_LETTER_ASPECT_DEFAULT;
}

// ダイアログを初期化する。
void DekaMoji::OnInitDialog(HWND hwnd)
{
    g_hMainWnd = hwnd;
    g_pageMgr.m_hWnd = GetDlgItem(hwnd, stc7);
    g_textColorButton.m_hWnd = GetDlgItem(hwnd, IDC_TEXT_COLOR);
    g_backColorButton.m_hWnd = GetDlgItem(hwnd, IDC_BACK_COLOR);

    // IDC_PAGE_SIZE: 用紙サイズ。
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A3));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A4));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_A5));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_B4));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_B5));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_LETTER));
    SendDlgItemMessage(hwnd, IDC_PAGE_SIZE, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_SIZE_LEGAL));

    // IDC_PAGE_DIRECTION: ページの向き。
    SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_PORTRAIT));
    SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_LANDSCAPE));

    // IDC_FONT_NAME: フォント名。
    if (m_fonts.size())
    {
        for (auto& entry : m_fonts)
        {
            SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_ADDSTRING, 0, (LPARAM)entry.c_str());
        }
    }
    SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_SETHORIZONTALEXTENT, 400, 0);

    // IDC_LETTER_ASPECT: 文字のアスペクト比。
    SendDlgItemMessage(hwnd, IDC_LETTER_ASPECT, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_ASPECT_100));
    SendDlgItemMessage(hwnd, IDC_LETTER_ASPECT, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_ASPECT_150));
    SendDlgItemMessage(hwnd, IDC_LETTER_ASPECT, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_ASPECT_200));
    SendDlgItemMessage(hwnd, IDC_LETTER_ASPECT, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_ASPECT_250));
    SendDlgItemMessage(hwnd, IDC_LETTER_ASPECT, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_ASPECT_300));
    SendDlgItemMessage(hwnd, IDC_LETTER_ASPECT, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_ASPECT_400));
    SendDlgItemMessage(hwnd, IDC_LETTER_ASPECT, CB_ADDSTRING, 0, (LPARAM)doLoadString(IDS_ASPECT_NO_LIMIT));

    // MImageViewExを使えるようにする。
    g_hwndImageView.doSubclass(GetDlgItem(hwnd, stc7));

    // 垂直位置補正のスピンコントロール。
    SendDlgItemMessage(hwnd, scr1, UDM_SETRANGE, 0, MAKELONG(100, -100));

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
    SETTING(id) = szText; \
} while (0)
    GET_COMBO_DATA(IDC_PAGE_SIZE);
    GET_COMBO_DATA(IDC_PAGE_DIRECTION);
    GET_COMBO_DATA(IDC_FONT_NAME);
    GET_COMBO_DATA(IDC_LETTER_ASPECT);
#undef GET_COMBO_DATA

    // チェックボックスからデータを取得する。
#define GET_CHECK_DATA(id) do { \
    if (IsDlgButtonChecked(hwnd, id) == BST_CHECKED) \
        SETTING(id) = doLoadString(IDS_YES); \
    else \
        SETTING(id) = doLoadString(IDS_NO); \
} while (0)
#undef GET_CHECK_DATA

    // フォント名。
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

    // 縦横比。
    if (SETTING(IDC_LETTER_ASPECT).empty())
    {
        SETTING(IDC_LETTER_ASPECT) = IDC_LETTER_ASPECT_DEFAULT;
    }

    // テキスト。
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

    // 色。
    SETTING(IDC_TEXT_COLOR) = g_textColorButton.get_color_text();
    SETTING(IDC_BACK_COLOR) = g_backColorButton.get_color_text();

    // その他。
    auto nAdjust = GetDlgItemInt(hwnd, IDC_V_ADJUST, NULL, TRUE);
    m_settings[TEXT("IDC_V_ADJUST")] = std::to_wstring(nAdjust);
    g_nVAdjust = nAdjust * -8;

    g_bVertical = (IsDlgButtonChecked(hwnd, IDC_VERTICAL) == BST_CHECKED);
    m_settings[TEXT("IDC_VERTICAL")] = std::to_wstring(g_bVertical);

    g_bOneLetterPerPaper = (IsDlgButtonChecked(hwnd, IDC_ONE_LETTER_PER_PAPER) == BST_CHECKED);
    m_settings[TEXT("IDC_ONE_LETTER_PER_PAPER")] = std::to_wstring(g_bOneLetterPerPaper);

    return TRUE;
}

// データからダイアログへ。
BOOL DekaMoji::DialogFromData(HWND hwnd)
{
    // コンボボックスへデータを設定する。
#define SET_COMBO_DATA(id) do { \
    setComboText(hwnd, (id), SETTING(id).c_str()); \
    if (SendDlgItemMessage(hwnd, (id), CB_GETCURSEL, 0, 0) == CB_ERR) { \
        SETTING(id) = id##_DEFAULT; \
        setComboText(hwnd, (id), SETTING(id).c_str()); \
    } \
} while (0)
    SET_COMBO_DATA(IDC_PAGE_SIZE);
    SET_COMBO_DATA(IDC_PAGE_DIRECTION);
    SET_COMBO_DATA(IDC_FONT_NAME);
    SET_COMBO_DATA(IDC_LETTER_ASPECT);
#undef SET_COMBO_DATA

    // チェックボックスへデータを設定する。
#define SET_CHECK_DATA(id) do { \
    if (SETTING(id) == doLoadString(IDS_YES)) \
        CheckDlgButton(hwnd, (id), BST_CHECKED); \
    else \
        CheckDlgButton(hwnd, (id), BST_UNCHECKED); \
} while (0)
#undef SET_CHECK_DATA

    // テキスト。
    ::SetDlgItemText(hwnd, IDC_TEXT, SETTING(IDC_TEXT).c_str());
    // 垂直位置調整。
    ::SetDlgItemInt(hwnd, IDC_V_ADJUST, _ttoi(SETTING(IDC_V_ADJUST).c_str()), TRUE);
    // 色。
    g_textColorButton.set_color_text(SETTING(IDC_TEXT_COLOR).c_str());
    g_backColorButton.set_color_text(SETTING(IDC_BACK_COLOR).c_str());

    // 縦書き。
    if (_ttoi(SETTING(IDC_VERTICAL).c_str()))
        CheckDlgButton(hwnd, IDC_VERTICAL, BST_CHECKED);
    else
        CheckDlgButton(hwnd, IDC_VERTICAL, BST_UNCHECKED);

    // 各用紙1文字ずつ出力。
    if (_ttoi(SETTING(IDC_ONE_LETTER_PER_PAPER).c_str()))
        CheckDlgButton(hwnd, IDC_ONE_LETTER_PER_PAPER, BST_CHECKED);
    else
        CheckDlgButton(hwnd, IDC_ONE_LETTER_PER_PAPER, BST_UNCHECKED);

    return TRUE;
}

// アプリのレジストリキー。
#define REGKEY_APP TEXT("Software\\Katayama Hirofumi MZ\\DekaMojiPDF")

// レジストリからデータへ。
BOOL DekaMoji::DataFromReg(HWND hwnd, LPCTSTR pszSubKey)
{
    TCHAR szKey[MAX_PATH];
    StringCchCopy(szKey, _countof(szKey), REGKEY_APP);
    if (pszSubKey)
        PathAppend(szKey, pszSubKey);

    // ソフト固有のレジストリキーを開く。
    HKEY hKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, szKey, 0, KEY_READ, &hKey);
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
    GET_REG_DATA(IDC_LETTER_ASPECT);
    GET_REG_DATA(IDC_TEXT);
    GET_REG_DATA(IDC_TEXT_COLOR);
    GET_REG_DATA(IDC_BACK_COLOR);
    GET_REG_DATA(IDC_V_ADJUST);
    GET_REG_DATA(IDC_VERTICAL);
    GET_REG_DATA(IDC_ONE_LETTER_PER_PAPER);
#undef GET_REG_DATA

    // 符号なし(REG_DWORD)から符号付きの値に直す。
    SETTING(IDC_V_ADJUST) = std::to_wstring(_ttoi(SETTING(IDC_V_ADJUST).c_str()));

    // レジストリキーを閉じる。
    RegCloseKey(hKey);
    return TRUE;
}

// データからレジストリへ。
BOOL DekaMoji::RegFromData(HWND hwnd, LPCTSTR pszSubKey)
{
    TCHAR szKey[MAX_PATH];
    StringCchCopy(szKey, _countof(szKey), REGKEY_APP);
    if (pszSubKey)
        PathAppend(szKey, pszSubKey);

    // ソフト固有のレジストリキーを作成または開く。
    HKEY hAppKey;
    RegCreateKeyEx(HKEY_CURRENT_USER, szKey, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hAppKey, NULL);
    if (hAppKey == NULL)
        return FALSE; // 失敗。

    // レジストリにデータを設定する。
#define SET_REG_DATA(id) do { \
    auto& str = SETTING(id); \
    DWORD cbText = (str.size() + 1) * sizeof(WCHAR); \
    RegSetValueEx(hAppKey, TEXT(#id), 0, REG_SZ, (LPBYTE)str.c_str(), cbText); \
} while(0)
    SET_REG_DATA(IDC_PAGE_SIZE);
    SET_REG_DATA(IDC_PAGE_DIRECTION);
    SET_REG_DATA(IDC_FONT_NAME);
    SET_REG_DATA(IDC_LETTER_ASPECT);
    SET_REG_DATA(IDC_TEXT);
    SET_REG_DATA(IDC_TEXT_COLOR);
    SET_REG_DATA(IDC_BACK_COLOR);
    SET_REG_DATA(IDC_V_ADJUST);
    SET_REG_DATA(IDC_VERTICAL);
    SET_REG_DATA(IDC_ONE_LETTER_PER_PAPER);
#undef SET_REG_DATA

    // レジストリキーを閉じる。
    RegCloseKey(hAppKey);
    return TRUE; // 成功。
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

// 文字列をエスケープする。
std::wstring mstr_escape(const std::wstring& text)
{
    std::wstring ret;
    for (size_t ich = 0; ich < text.size(); ++ich)
    {
        auto ch = text[ich];
        switch (ch)
        {
        case L'\t': ret += L'\\'; ret += L't'; break;
        case L'\n': ret += L'\\'; ret += L'n'; break;
        case L'\r': ret += L'\\'; ret += L'r'; break;
        case L'\f': ret += L'\\'; ret += L'f'; break;
        case L'\\': ret += L'\\'; ret += L'\\'; break;
        default: ret += ch; break;
        }
    }
    return ret;
}

// PDFを作成する処理。
BOOL DekaMoji::MakePDF(HWND hwnd, LPCTSTR pszPdfFileName)
{
    // pdfplaca.exe を使う。
    TCHAR szPDFPLACA[MAX_PATH];
    GetModuleFileName(NULL, szPDFPLACA, _countof(szPDFPLACA));
    PathRemoveFileSpec(szPDFPLACA);
    PathAppend(szPDFPLACA, TEXT("pdfplaca\\pdfplaca.exe"));
    if (!PathFileExists(szPDFPLACA))
    {
        assert(0);
        return FALSE; // 失敗。
    }

    // コマンドライン引数を構築。
    string_t params;
    params += TEXT("--font \"");
    params += SETTING(IDC_FONT_NAME).c_str();
    params += TEXT("\" --text \"");
    params += mstr_escape(SETTING(IDC_TEXT).c_str()).c_str();
    params += TEXT("\" -o \"");
    params += pszPdfFileName;
    params += TEXT("\" --text-color ");
    params += SETTING(IDC_TEXT_COLOR).c_str();
    params += TEXT(" --back-color ");
    params += SETTING(IDC_BACK_COLOR).c_str();

    // 縦書きか？
    if (SETTING(IDC_VERTICAL) != TEXT("0"))
        params += TEXT(" --vertical");

    // 用紙の向き。
    if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_PORTRAIT))
        params += TEXT(" --portrait");
    else if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_LANDSCAPE))
        params += TEXT(" --landscape");

    // ページサイズ。
    if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A3))
        params += TEXT(" --page-size A3");
    else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A4))
        params += TEXT(" --page-size A4");
    else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_A5))
        params += TEXT(" --page-size A5");
    else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_B4))
        params += TEXT(" --page-size B4");
    else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_B5))
        params += TEXT(" --page-size B5");
    else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_LETTER))
        params += TEXT(" --page-size Letter");
    else if (SETTING(IDC_PAGE_SIZE) == doLoadString(IDS_SIZE_LEGAL))
        params += TEXT(" --page-size Legal");

    // アスペクト比のしきい値。
    if (SETTING(IDC_LETTER_ASPECT) == doLoadString(IDS_ASPECT_100))
        params += TEXT(" --threshold 1");
    else if (SETTING(IDC_LETTER_ASPECT) == doLoadString(IDS_ASPECT_150))
        params += TEXT(" --threshold 1.5");
    else if (SETTING(IDC_LETTER_ASPECT) == doLoadString(IDS_ASPECT_200))
        params += TEXT(" --threshold 2");
    else if (SETTING(IDC_LETTER_ASPECT) == doLoadString(IDS_ASPECT_250))
        params += TEXT(" --threshold 2.5");
    else if (SETTING(IDC_LETTER_ASPECT) == doLoadString(IDS_ASPECT_300))
        params += TEXT(" --threshold 3");
    else if (SETTING(IDC_LETTER_ASPECT) == doLoadString(IDS_ASPECT_400))
        params += TEXT(" --threshold 4");
    else if (SETTING(IDC_LETTER_ASPECT) == doLoadString(IDS_ASPECT_NO_LIMIT))
        params += TEXT(" --threshold 1000");

    // Y方向の補正。
    params += TEXT(" --y-adjust ");
    params += std::to_wstring(_ttoi(SETTING(IDC_V_ADJUST).c_str()));

    SHELLEXECUTEINFO info = { sizeof(info) };
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
    info.hwnd = hwnd;
    info.lpFile = szPDFPLACA;
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteEx(&info))
        return FALSE; // 失敗。

    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD dwExitCode;
    GetExitCodeProcess(info.hProcess, &dwExitCode);
    CloseHandle(info.hProcess);

    return (dwExitCode == 0);
}

// WM_INITDIALOG
// ダイアログの初期化。
BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    // 中央寄せする。
    CenterWindowDx(hwnd);

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
    pDM->OnInitDialog(hwnd);

    // レジストリからデータを読み込む。
    pDM->DataFromReg(hwnd);

    // ダイアログにデータを設定。
    pDM->DialogFromData(hwnd);

    // 少し大きなフォントをテキストボックスにセット。
    LOGFONT lf;
    GetObject(GetWindowFont(hwnd), sizeof(lf), &lf);
    lf.lfHeight = lf.lfHeight * 5 / 4;
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

    // 一貫性のために再度ダイアログを更新。
    pDM->DialogFromData(hwnd);

    // 一時的なPDFファイルを作成する。
    MTempFile szPdfFile(TEXT("DeM"), TEXT(".pdf"));
    szPdfFile.make();

    // PDFを作成する処理。
    BOOL bOK = pDM->MakePDF(hwnd, szPdfFile);

    // 出力PDFファイルのパス名を構築する。
    string_t strOutputPdfFile;
    if (bOK)
    {
        // 現在の日時を取得する。
        SYSTEMTIME st;
        ::GetLocalTime(&st);

        // デスクトップのディレクトリを取得する。
        TCHAR szDesktop[MAX_PATH];
        SHGetSpecialFolderPath(hwnd, szDesktop, CSIDL_DESKTOPDIRECTORY, FALSE);

        // 現在の日付を用いて、出力ファイルパス名を構築する。
        TCHAR szPath[MAX_PATH];
        StringCchPrintf(szPath, _countof(szPath), doLoadString(IDS_OUTPUT_NAME), szDesktop, st.wYear, st.wMonth, st.wDay);
        PathAddExtension(szPath, TEXT(".pdf"));

        // パスファイル名が衝突したらファイル名の後ろに番号を付けて回避しようとする。
        INT iTry = 1;
        TCHAR szNum[64];
        while (PathFileExists(szPath))
        {
            StringCchPrintf(szPath, _countof(szPath), doLoadString(IDS_OUTPUT_NAME), szDesktop, st.wYear, st.wMonth, st.wDay);
            StringCchPrintf(szNum, _countof(szNum), TEXT(" (%d)"), (iTry + 1));
            StringCchCat(szPath, _countof(szPath), szNum);
            PathAddExtension(szPath, TEXT(".pdf"));
            ++iTry;
        }

        // 一時ファイルを実ファイルにコピー。
        strOutputPdfFile = szPath;
        bOK = CopyFile(szPdfFile, strOutputPdfFile.c_str(), FALSE);
    }

    // ボタンテキストを元に戻す。
    SetWindowText(hButton, doLoadString(IDS_GENERATE));

    if (!bOK)
    {
        // 失敗メッセージを表示する。
        MsgBoxDx(hwnd, doLoadString(IDS_GENERATEFAILED), g_szAppName, MB_ICONERROR);
        return FALSE; // 失敗。
    }

    // 成功メッセージを表示する。
    TCHAR szText[MAX_PATH];
    StringCchPrintf(szText, _countof(szText), doLoadString(IDS_SUCCEEDED),
                    PathFindFileName(strOutputPdfFile.c_str()));
    MsgBoxDx(hwnd, szText, g_szAppName, MB_ICONINFORMATION);
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

// サブ設定を削除。
void OnEraseSubSettings(HWND hwnd)
{
    // レジストリを開く。
    HKEY hAppKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, REGKEY_APP, 0, KEY_READ | KEY_WRITE, &hAppKey);
    if (!hAppKey)
        return;

    // 削除を繰り返す。
    TCHAR szName[MAX_PATH];
    while (RegEnumKey(hAppKey, 0, szName, _countof(szName)) == ERROR_SUCCESS)
    {
        RegDeleteKey(hAppKey, szName);
    }

    // レジストリを閉じる。
    RegCloseKey(hAppKey);
}

// サブ設定の名前を取得する。
BOOL getSubSettingsName(LPTSTR pszName, INT index)
{
    HKEY hAppKey;
    RegCreateKey(HKEY_CURRENT_USER, REGKEY_APP, &hAppKey);
    if (!hAppKey)
        return FALSE;

    BOOL ret = (RegEnumKey(hAppKey, index, pszName, MAX_PATH) == ERROR_SUCCESS);
    RegCloseKey(hAppKey);
    return ret;
}

// サブ設定を復元する。
void OnRestoreSubSettings(HWND hwnd, INT id)
{
    TCHAR szName[MAX_PATH];
    if (getSubSettingsName(szName, id - ID_SETTINGS_000))
    {
        DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        pDM->DataFromReg(hwnd, szName);
        pDM->DialogFromData(hwnd);
    }
}

void ChooseDelete_OnInitDialog(HWND hwnd)
{
    // レジストリを開く。
    HKEY hAppKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, REGKEY_APP, 0, KEY_READ, &hAppKey);
    if (!hAppKey)
    {
        assert(0);
        return;
    }

    // サブ設定の名前を追加する。
    TCHAR szName[MAX_PATH];
    HWND hwndLst1 = GetDlgItem(hwnd, lst1);
    for (DWORD dwIndex = 0; dwIndex <= 9999; dwIndex++)
    {
        if (RegEnumKey(hAppKey, dwIndex, szName, _countof(szName)) == ERROR_SUCCESS)
        {
            DecodeName(szName);
            ListBox_AddString(hwndLst1, szName);
        }
    }

    // レジストリを閉じる。
    RegCloseKey(hAppKey);
}

// リストボックスで選択した設定名を削除する。
void ChooseDelete_OnDeleteSettings(HWND hwnd)
{
    // レジストリを開く。
    HKEY hAppKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, REGKEY_APP, 0, KEY_READ | KEY_WRITE, &hAppKey);
    if (!hAppKey)
    {
        assert(0);
        return;
    }

    // 選択された項目を取得する。
    HWND hwndLst1 = GetDlgItem(hwnd, lst1);
    std::vector<INT> selItems;
    INT cSelections = (INT)SendMessage(hwndLst1, LB_GETSELCOUNT, 0, 0);
    selItems.resize(cSelections);
    SendMessage(hwndLst1, LB_GETSELITEMS, cSelections, (LPARAM)&selItems[0]);

    // 選択されていた項目を逆順で削除する。
    std::reverse(selItems.begin(), selItems.end());
    for (INT iItem : selItems)
    {
        TCHAR szName[MAX_PATH];
        SendMessage(hwndLst1, LB_GETTEXT, iItem, (LPARAM)szName);
        EncodeName(szName);
        RegDeleteKey(hAppKey, szName);
    }

    // レジストリを閉じる。
    RegCloseKey(hAppKey);
}

INT_PTR CALLBACK
ChooseDeleteDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        // 中央寄せする。
        CenterWindowDx(hwnd);
        ChooseDelete_OnInitDialog(hwnd);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            ChooseDelete_OnDeleteSettings(hwnd);
            EndDialog(hwnd, IDOK);
            break;
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        break;
    }
    return 0;
}

void OnChooseEraseSettings(HWND hwnd)
{
    DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_CHOOSEDELETESETTINGS), hwnd, ChooseDeleteDlgProc);
}

// 「テキストの色」ボタンが押された。
void OnTextColorButton(HWND hwnd)
{
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

// プレビューのリフレッシュを数えるカウンタ変数。
static INT s_nRefreshCounter = 0;

// プレビューのリフレッシュを始める。
void doRefreshPreview(HWND hwnd, DWORD dwDelay = 500)
{
    // フォーカスを持ったコントロールを無効化するとWindowsの不具合が生じるので、
    // フォーカスを移動して回避。
    if (GetFocus() == GetDlgItem(hwnd, IDC_PAGE_LEFT) ||
        GetFocus() == GetDlgItem(hwnd, IDC_PAGE_RIGHT))
    {
        SetFocus(GetDlgItem(hwnd, IDC_TEXT));
    }

    // いったん、ページ移動を無効化する。
    EnableWindow(GetDlgItem(hwnd, IDC_PAGE_LEFT), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_PAGE_RIGHT), FALSE);

    // 頻繁に更新するのではなく、タイマーを使ってUI/UXを改善する。
    KillTimer(hwnd, TIMER_ID_REFRESH_PREVIEW);
    SetTimer(hwnd, TIMER_ID_REFRESH_PREVIEW, dwDelay, NULL); // 少し後でプレビューを更新する。

    // リフレッシュが頻繁な場合はユーザーに待つように指示する。
    ++s_nRefreshCounter;
    if (s_nRefreshCounter >= 3)
    {
        HBITMAP hbmWait = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_WAIT));
        g_hwndImageView.setImage(hbmWait);
    }
}

// 「設定」ボタン。
void OnSettings(HWND hwnd)
{
    // ボタンの位置を取得する。
    RECT rc;
    GetWindowRect(GetDlgItem(hwnd, psh5), &rc);
    POINT pt = { rc.left, (rc.top + rc.bottom) / 2 };

    // メニューを読み込む。
    HMENU hMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_SETTINGS_MENU));
    HMENU hSubMenu = GetSubMenu(hMenu, 0);
    HMENU hSubSubMenu = GetSubMenu(hSubMenu, 3); // 「次の設定に上書き保存(&O)」

    // レジストリを開く。
    HKEY hAppKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, REGKEY_APP, 0, KEY_READ, &hAppKey);
    if (hAppKey)
    {
        // メニューに項目を追加する。
        for (DWORD dwIndex = 0; dwIndex < 9999; ++dwIndex)
        {
            TCHAR szName[MAX_PATH];
            LONG error = RegEnumKey(hAppKey, dwIndex, szName, _countof(szName));
            if (error)
                break;

            if (dwIndex == 0)
            {
                DeleteMenu(hSubMenu, 0, MF_BYPOSITION);
                DeleteMenu(hSubSubMenu, 0, MF_BYPOSITION);
            }

            DecodeName(szName);

            InsertMenu(hSubMenu, dwIndex, MF_BYPOSITION, ID_SETTINGS_000 + dwIndex, szName);
            InsertMenu(hSubSubMenu, dwIndex, MF_BYPOSITION, ID_OVERWRITE_SETTINGS_000 + dwIndex, szName);
        }

        // レジストリを閉じる。
        RegCloseKey(hAppKey);
    }

    // ポップアップメニューを表示し、コマンドが選択されるのを待つ。
    TPMPARAMS params = { sizeof(params), rc };
    SetForegroundWindow(hwnd);
    enum { flags = TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL | TPM_RETURNCMD };
    const auto id = TrackPopupMenuEx(hSubMenu, flags, pt.x, pt.y, hwnd, &params);

    // 選択項目が有効ならば、メインウィンドウにコマンドを投函する。
    if (id != 0 && id != -1)
    {
        ::PostMessageW(hwnd, WM_COMMAND, id, 0);
    }

    // メニューを破棄する。
    ::DestroyMenu(hMenu);
}

INT_PTR CALLBACK
NameSettingsDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static LPTSTR s_szName = NULL; // 名前へのポインタを保持する。
    TCHAR szText[MAX_PATH];
    switch (uMsg)
    {
    case WM_INITDIALOG:
        // 中央寄せする。
        CenterWindowDx(hwnd);
        // 名前へのポインタを保持する。
        s_szName = (LPTSTR)lParam;
        // 文字数を制限する。
        SendDlgItemMessage(hwnd, edt1, EM_SETLIMITTEXT, 40, 0);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            // 名前を取得する。
            GetDlgItemText(hwnd, edt1, szText, _countof(szText));
            // 前後の空白を無視する。
            StrTrim(szText, TEXT(" \t\r\n\x3000"));
            // 名前を格納する。
            if (szText[0])
            {
                StringCchCopy(s_szName, MAX_PATH, szText);
            }
            // ダイアログを終了する。
            EndDialog(hwnd, szText[0] ? IDOK : IDCANCEL);
            break;
        case IDCANCEL:
            // ダイアログを終了する。
            EndDialog(hwnd, IDCANCEL);
            break;
        }
    }
    return 0;
}

// 「設定に名前を付けて保存」
void OnSaveSubSettingsAs(HWND hwnd)
{
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!pDM->DataFromDialog(hwnd, FALSE))
    {
        assert(0);
        return;
    }

    TCHAR szName[MAX_PATH];
    if (DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_NAMESETTINGS), hwnd,
                       NameSettingsDlgProc, (LPARAM)szName) == IDOK)
    {
        EncodeName(szName);
        pDM->RegFromData(hwnd, szName);
    }
}

// 「設定を上書き」
void OnOverwriteSubSettings(HWND hwnd, INT id)
{
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!pDM->DataFromDialog(hwnd, FALSE))
    {
        assert(0);
        return;
    }

    TCHAR szName[MAX_PATH];
    if (getSubSettingsName(szName, id - ID_OVERWRITE_SETTINGS_000))
        pDM->RegFromData(hwnd, szName);
}

// 「すべての設定をREGファイルに保存」
void OnSaveAllToRegFile(HWND hwnd)
{
    // レジストリをダイアログに従って更新する。
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!pDM->DataFromDialog(hwnd, FALSE))
        return;
    pDM->RegFromData(hwnd);

    // フィルター文字列を取得する。
    TCHAR szFilter[MAX_PATH];
    LoadString(g_hInstance, IDS_REGFILTER, szFilter, _countof(szFilter));
    for (INT ich = 0; szFilter[ich]; ++ich)
    {
        if (szFilter[ich] == TEXT('|'))
            szFilter[ich] = 0;
    }

    // ユーザーにREGファイル名を問い合わせる。
    OPENFILENAME ofn = { OPENFILENAME_SIZE_VERSION_400, hwnd };
    TCHAR szFile[MAX_PATH] = TEXT("");
    ofn.lpstrFilter = szFilter;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = _countof(szFile);
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT |
                OFN_ENABLESIZING;
    ofn.lpstrDefExt = TEXT("reg");
    if (!GetSaveFileName(&ofn))
        return; // キャンセルか失敗。

    // "reg export"を使ってレジストリ設定をファイルに保存する。
    string_t params;
    params += TEXT("export \"HKCU\\");
    params += REGKEY_APP;
    params += TEXT("\" \"");
    params += szFile;
    params += TEXT("\"");
    SHELLEXECUTEINFO info = { sizeof(info) };
    info.hwnd = hwnd;
    info.lpFile = TEXT("reg.exe");
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    ShellExecuteEx(&info);
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
        {
            DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (pDM->DataFromDialog(hwnd, TRUE))
            {
                pDM->RegFromData(hwnd);
            }
            EndDialog(hwnd, id);
        }
        break;
    case IDC_SETTINGS: // 「設定の初期化」ボタン。
        OnSettings(hwnd);
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
    case IDC_FONT_NAME: // 「フォント名」
    case IDC_LETTER_ASPECT: // 「文字のアスペクト比」
        if (codeNotify == CBN_SELCHANGE || codeNotify == CBN_SELENDOK)
        {
            doRefreshPreview(hwnd, 0);
        }
        break;
    case IDC_V_ADJUST: // 「垂直位置補正」
        if (codeNotify == EN_CHANGE)
        {
            doRefreshPreview(hwnd, 250);
        }
        break;
    case IDC_VERTICAL:
        if (codeNotify == BN_CLICKED)
        {
            if (IsDlgButtonChecked(hwnd, IDC_VERTICAL) == BST_CHECKED)
                SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_SETCURSEL, 0, 0);
            else
                SendDlgItemMessage(hwnd, IDC_PAGE_DIRECTION, CB_SETCURSEL, 1, 0);
            doRefreshPreview(hwnd, 0);
        }
        break;
    case IDC_ONE_LETTER_PER_PAPER:
        if (codeNotify == BN_CLICKED)
        {
            doRefreshPreview(hwnd, 0);
        }
        break;
    case IDC_TEXT_COLOR:
        if (codeNotify == BN_CLICKED)
        {
            g_textColorButton.choose_color();
            doRefreshPreview(hwnd, 0);
        }
        break;
    case IDC_BACK_COLOR:
        if (codeNotify == BN_CLICKED)
        {
            g_backColorButton.choose_color();
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
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        {
            HWND hEdit = ::GetDlgItem(hwnd, edt1);
            Edit_SetSel(hEdit, 0, -1); // すべて選択。
            ::SetFocus(hEdit);
        }
        break;
    case stc4:
        ::SetFocus(::GetDlgItem(hwnd, IDC_TEXT_COLOR));
        break;
    case stc10:
        ::SetFocus(::GetDlgItem(hwnd, IDC_BACK_COLOR));
        break;
    case stc6:
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        ::SetFocus(::GetDlgItem(hwnd, cmb3));
        break;
    case stc8:
        // コンボボックスの前のラベルをクリックしたら、対応するコンボボックスにフォーカスを当てる。
        ::SetFocus(::GetDlgItem(hwnd, cmb4));
        break;
    case stc9:
        // エディットコントロールの前のラベルをクリックしたら、対応するエディットコントロールにフォーカスを当てる。
        SendDlgItemMessage(hwnd, edt3, EM_SETSEL, 0, -1);
        ::SetFocus(::GetDlgItem(hwnd, edt3));
        break;
    case ID_SAVESETTINGSAS: // 「設定に名前を付けて保存」
        OnSaveSubSettingsAs(hwnd);
        break;
    case ID_RESETSETTINGS: // 「設定のリセット」
        OnEraseSettings(hwnd);
        doRefreshPreview(hwnd, 0);
        break;
    case ID_INITAPP: // 「アプリの初期化」
        OnEraseSettings(hwnd);
        OnEraseSubSettings(hwnd);
        doRefreshPreview(hwnd, 0);
        break;
    case ID_CHOOSESETTINGSTODELETE: // 「削除したい設定名を選ぶ」
        OnChooseEraseSettings(hwnd);
        break;
    case ID_SAVEALLTOREGFILE: // 「すべての設定をREGファイルに保存」
        OnSaveAllToRegFile(hwnd);
        break;
    case IDC_PAGE_LEFT: // 「前のページへ」
        if (g_pageMgr.hasBack())
        {
            g_pageMgr.goBack();
            doRefreshPreview(hwnd, 0);
        }
        break;
    case IDC_PAGE_RIGHT: // 「次のページへ」
        if (g_pageMgr.hasNext())
        {
            g_pageMgr.goNext();
            doRefreshPreview(hwnd, 0);
        }
        break;
    case IDC_FONT_LEFT: // 「前のフォント」
        {
            INT iItem = (INT)SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_GETCURSEL, 0, 0);
            INT cItems = (INT)SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_GETCOUNT, 0, 0);
            if (iItem > 0)
            {
                SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_SETCURSEL, iItem - 1, 0);
                doRefreshPreview(hwnd, 0);
            }
        }
        break;
    case IDC_FONT_RIGHT: // 「次のフォント」
        {
            INT iItem = (INT)SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_GETCURSEL, 0, 0);
            INT cItems = (INT)SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_GETCOUNT, 0, 0);
            if (iItem + 1 < cItems)
            {
                SendDlgItemMessage(hwnd, IDC_FONT_NAME, CB_SETCURSEL, iItem + 1, 0);
                doRefreshPreview(hwnd, 0);
            }
        }
        break;
    default:
        if (ID_SETTINGS_000 <= id && id <= ID_SETTINGS_000 + 999) // サブ設定。
        {
            OnRestoreSubSettings(hwnd, id);
            doRefreshPreview(hwnd, 0);
            break;
        }
        if (ID_OVERWRITE_SETTINGS_000 <= id && id <= ID_OVERWRITE_SETTINGS_000 + 999) // サブ設定上書き。
        {
            OnOverwriteSubSettings(hwnd, id);
            break;
        }
        break;
    }
}

// WM_DRAWITEM
void OnDrawItem(HWND hwnd, const DRAWITEMSTRUCT * lpDrawItem)
{
    // 色ボタンのオーナードローを処理する。
    g_textColorButton.OnOwnerDrawItem(lpDrawItem);
    g_backColorButton.OnOwnerDrawItem(lpDrawItem);
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

// プレビューを更新する。
BOOL doUpdatePreview(HWND hwnd)
{
    s_nRefreshCounter = 0;

    // PDFファイル名。
    MTempFile szPdfFile(TEXT("DeM"), TEXT(".pdf"));
    szPdfFile.make();

    // BMPファイル名。
    MTempFile szBmpFile(TEXT("DeM"), TEXT(".bmp"));
    szBmpFile.make();

    // ダイアログからデータを取得。
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!pDM->DataFromDialog(hwnd, TRUE))
        return FALSE; // 失敗。

    // PDFを作成する処理。
    BOOL bOK = pDM->MakePDF(hwnd, szPdfFile);
    if (!bOK)
    {
        assert(0);
        return FALSE; // 失敗。
    }

    // pdf2bmp.exe を使ってPDFをBMPに変換。
    TCHAR szPDF2BMP[MAX_PATH];
    GetModuleFileName(NULL, szPDF2BMP, _countof(szPDF2BMP));
    PathRemoveFileSpec(szPDF2BMP);
    PathAppend(szPDF2BMP, TEXT("pdf2bmp\\pdf2bmp.exe"));
    if (!PathFileExists(szPDF2BMP))
    {
        assert(0);
        return FALSE; // pdf2bmp.exeが見つからない。
    }

    // pdf2bmp用のコマンドライン引数を構築する。
    string_t params;
    params += TEXT("--dpi 16 --page ");
    params += std::to_wstring(g_pageMgr.m_iPage);
    params += TEXT(" \"");
    params += szPdfFile;
    params += TEXT("\" \"");
    params += szBmpFile;
    params += TEXT("\"");
    //MessageBoxW(NULL, params.c_str(), NULL, 0);

    // シェルを用いてプログラムを実行する。
    SHELLEXECUTEINFO info = { sizeof(info) };
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
    info.hwnd = hwnd;
    info.lpFile = szPDF2BMP;
    info.lpParameters = params.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteEx(&info))
        return FALSE; // 失敗。

    // 終了を待って閉じる。
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD dwExitCode;
    GetExitCodeProcess(info.hProcess, &dwExitCode);
    CloseHandle(info.hProcess);

    // 終了コードがゼロでなければ失敗。
    if (dwExitCode != 0)
        return FALSE; // 失敗。

    // pdf2bmpの実行結果で得られたBMPを読み込む。
    HBITMAP hbmImage = (HBITMAP)LoadImage(NULL, szBmpFile, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    BITMAP bm;
    if (!GetObject(hbmImage, sizeof(bm), &bm))
        return FALSE; // 失敗。

    // 周りの黒線の幅（ピクセル単位）。
    const INT margin = 2;

    // プレビューのイメージを作成する。
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
    HBITMAP hbmPreview = CreateDIBSection(hdc1, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (hbmPreview)
    {
        // イメージを転送する。
        HGDIOBJ hbm1Old = SelectObject(hdc1, hbmImage);
        HGDIOBJ hbm2Old = SelectObject(hdc2, hbmPreview);
        BitBlt(hdc2, margin, margin, bm.bmWidth, bm.bmHeight, hdc1, 0, 0, SRCCOPY);
        SelectObject(hdc1, hbm1Old);
        SelectObject(hdc2, hbm2Old);

        // プレビューイメージをセットする。hbmPreviewの所有権はg_hwndImageViewに移動する。
        g_hwndImageView.setImage(hbmPreview);
    }
    DeleteObject(hbmImage);

    // ページのナビゲーションUIを更新する。
    EnableWindow(GetDlgItem(hwnd, IDC_PAGE_LEFT), g_pageMgr.hasBack());
    EnableWindow(GetDlgItem(hwnd, IDC_PAGE_RIGHT), g_pageMgr.hasNext());

    if (hbmPreview) // プレビューが用意できたら
    {
        // ページ情報をセットする。
        g_pageMgr.setPageCount(1);
        g_pageMgr.setPage(1);

        // ページ情報のテキストをセットする。
        auto page_info = g_pageMgr.print();
        SetDlgItemText(hwnd, IDC_PAGE_INFO, page_info.c_str());
    }
    else
    {
        // ページ情報をクリアする。
        g_pageMgr.init();
        SetDlgItemText(hwnd, IDC_PAGE_INFO, NULL);
    }

    return TRUE; // 成功。
}

void OnTimer(HWND hwnd, UINT id)
{
    if (id != TIMER_ID_REFRESH_PREVIEW)
        return;

    KillTimer(hwnd, id);

    if (!doUpdatePreview(hwnd))
    {
        g_hwndImageView.setImage(NULL);
        g_pageMgr.init();
        SetDlgItemText(hwnd, IDC_PAGE_INFO, NULL);
    }
}

// ダイアログプロシージャ。
static INT_PTR CALLBACK
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

// デカ文字PDFのダイアログ メイン関数。
INT DekaMoji_DialogMain(HINSTANCE hInstance, INT argc, LPTSTR *argv)
{
    // アプリのインスタンスを保持する。
    g_hInstance = hInstance;

    // 共通コントロール群を初期化する。
    InitCommonControls();

    // ユーザーデータを保持する。
    DekaMoji dm(hInstance, argc, argv);

    // ユーザーデータをパラメータとしてダイアログを開く。
    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAINDLG), NULL, DialogProc, (LPARAM)&dm);

    return 0;
}

// デカ文字PDFのメイン関数。
INT DekaMoji_Main(HINSTANCE hInstance, INT argc, LPTSTR *argv)
{
    INT ret = DekaMoji_DialogMain(hInstance, argc, argv);

#if (WINVER >= 0x0500) && !defined(NDEBUG)
    // Check handle leak (for Windows only)
    {
        TCHAR szText[MAX_PATH];
        HANDLE hProcess = GetCurrentProcess();
        #if 1
            #undef OutputDebugString
            #define OutputDebugString(str) _fputts((str), stderr);
        #endif
        wnsprintf(szText, _countof(szText),
            TEXT("GDI objects: %ld\n")
            TEXT("USER objects: %ld\n"),
            GetGuiResources(hProcess, GR_GDIOBJECTS),
            GetGuiResources(hProcess, GR_USEROBJECTS));
        OutputDebugString(szText);
    }
#endif

#if defined(_MSC_VER) && !defined(NDEBUG)
    // Detect memory leak (for MSVC only)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    return ret; // 終了。
}

// Windowsアプリのメイン関数。
INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    // wWinMainをサポートしていないコンパイラのために、コマンドラインの処理を行う。
    INT argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    INT ret = DekaMoji_Main(hInstance, argc, argv);
    LocalFree(argv);
    return ret;
}
