// �f�J����PDF by katahiromz
// Copyright (C) 2022-2023 �ЎR����MZ. All Rights Reserved.
// See README.txt and LICENSE.txt.
#include <windows.h>        // Windows�̕W���w�b�_�B
#include <windowsx.h>       // Windows�̃}�N���w�b�_�B
#include <commctrl.h>       // ���ʃR���g���[���̃w�b�_�B
#include <commdlg.h>        // ���ʃ_�C�A���O�̃w�b�_�B
#include <shlobj.h>         // �V�F��API�̃w�b�_�B
#include <shlwapi.h>        // �V�F���y��API�̃w�b�_�B
#include <tchar.h>          // �W�F�l���b�N�e�L�X�g�}�b�s���O�p�̃w�b�_�B
#include <strsafe.h>        // ���S�ȕ����񑀍�p�̃w�b�_ (StringC*)
#include <string>           // std::string ����� std::wstring �N���X�B
#include <vector>           // std::vector �N���X�B
#include <map>              // std::map �N���X�B
#include <stdexcept>        // std::runtime_error �N���X�B
#include <cassert>          // assert�}�N���B
#include <hpdf.h>           // PDF�o�͗p�̃��C�u����libharu�̃w�b�_�B
#include "TempFile.hpp"     // �ꎞ�t�@�C������p�̃w�b�_�B
#include "resource.h"       // ���\�[�XID�̒�`�w�b�_�B

// �V�F�A�E�F�A���B
#ifndef NO_SHAREWARE
    #include "Shareware.hpp"

    SW_Shareware g_shareware(
        /* company registry key */      TEXT("Katayama Hirofumi MZ"),
        /* application registry key */  TEXT("DekaMojiPDF"),
        /* password hash */
        "e218f83f070a186f886c6dc82bd7ecf3d6c3ea4224fd7d213aa06e9c9713b395",
        /* trial days */                10,
        /* salt string */               "mJpDxx2D",
        /* version string */            "0.9.7");
#endif

#define UTF8_SUPPORT // UTF-8�T�|�[�g�B

// ������N���X�B
#ifdef UNICODE
    typedef std::wstring string_t;
#else
    typedef std::string string_t;
#endif

// �V�t�gJIS �R�[�h�y�[�W�iShift_JIS�j�B
#define CP932  932

struct FONT_ENTRY
{
    string_t m_font_name;
    string_t m_pathname;
    int m_index = -1;
};

// �킩��₷�����ږ����g�p����B
enum
{
    IDC_GENERATE = IDOK,
    IDC_EXIT = IDCANCEL,
};

// �f�J����PDF�̃��C���N���X�B
class DekaMoji
{
public:
    HINSTANCE m_hInstance;
    INT m_argc;
    LPTSTR *m_argv;
    std::map<string_t, string_t> m_settings;
    std::vector<FONT_ENTRY> m_font_map;

    // �R���X�g���N�^�B
    DekaMoji(HINSTANCE hInstance, INT argc, LPTSTR *argv);

    // �f�X�g���N�^�B
    ~DekaMoji()
    {
    }

    // �t�H���g�}�b�v��ǂݍ��ށB
    BOOL LoadFontMap();
    // �f�[�^�����Z�b�g����B
    void Reset();
    // �_�C�A���O������������B
    void InitDialog(HWND hwnd);
    // �_�C�A���O����f�[�^�ցB
    BOOL DataFromDialog(HWND hwnd);
    // �f�[�^����_�C�A���O�ցB
    BOOL DialogFromData(HWND hwnd);
    // ���W�X�g������f�[�^�ցB
    BOOL DataFromReg(HWND hwnd);
    // �f�[�^���烌�W�X�g���ցB
    BOOL RegFromData(HWND hwnd);

    // ���C���f�B�b�V�������B
    string_t JustDoIt(HWND hwnd);
};

// �O���[�o���ϐ��B
HINSTANCE g_hInstance = NULL; // �C���X�^���X�B
TCHAR g_szAppName[256] = TEXT(""); // �A�v�����B
HICON g_hIcon = NULL; // �A�C�R���i��j�B
HICON g_hIconSm = NULL; // �A�C�R���i���j�B

// ���\�[�X�������ǂݍ��ށB
LPTSTR doLoadString(INT nID)
{
    static TCHAR s_szText[1024];
    s_szText[0] = 0;
    LoadString(NULL, nID, s_szText, _countof(s_szText));
    return s_szText;
}

// ������̑O��̋󔒂��폜����B
void str_trim(LPWSTR text)
{
    StrTrimW(text, L" \t\r\n\x3000");
}

// ���[�J���̃t�@�C���̃p�X�����擾����B
LPCTSTR findLocalFile(LPCTSTR filename)
{
    // ���݂̃v���O�����̃p�X�t�@�C�������擾����B
    static TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, _countof(szPath));

    // �t�@�C���^�C�g����filename�Œu��������B
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    // ���̃t�H���_�ցB
    PathRemoveFileSpec(szPath);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    // ����Ɉ��̃t�H���_�ցB
    PathRemoveFileSpec(szPath);
    PathRemoveFileSpec(szPath);
    PathRemoveFileSpec(szPath);
    PathAppend(szPath, filename);
    if (PathFileExists(szPath))
        return szPath;

    return NULL; // ������Ȃ������B
}

// �s���ȕ����񂪓��͂��ꂽ�B
void OnInvalidString(HWND hwnd, INT nItemID, INT nFieldId, INT nReasonId)
{
    SetFocus(GetDlgItem(hwnd, nItemID));
    string_t field = doLoadString(nFieldId);
    string_t reason = doLoadString(nReasonId);
    TCHAR szText[256];
    StringCchPrintf(szText, _countof(szText), doLoadString(IDS_INVALIDSTRING), field.c_str(), reason.c_str());
    MessageBox(hwnd, szText, g_szAppName, MB_ICONERROR);
}

// �R���{�{�b�N�X�̃e�L�X�g���擾����B
BOOL getComboText(HWND hwnd, INT id, LPTSTR text, INT cchMax)
{
    text[0] = 0;

    HWND hCombo = GetDlgItem(hwnd, id);
    INT iSel = ComboBox_GetCurSel(hCombo);
    if (iSel == CB_ERR) // �R���{�{�b�N�X�ɑI�����ڂ��Ȃ����
    {
        // ���̂܂܃e�L�X�g���擾����B
        ComboBox_GetText(hCombo, text, cchMax);
    }
    else
    {
        // ���X�g����e�L�X�g���擾����B�����̃`�F�b�N����B
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

// �R���{�{�b�N�X�̃e�L�X�g��ݒ肷��B
BOOL setComboText(HWND hwnd, INT id, LPCTSTR text)
{
    // �e�L�X�g�Ɉ�v���鍀�ڂ��擾����B
    HWND hCombo = GetDlgItem(hwnd, id);
    INT iItem = ComboBox_FindStringExact(hCombo, -1, text);
    if (iItem == CB_ERR) // ��v���鍀�ڂ��Ȃ����
        ComboBox_SetText(hCombo, text); // ���̂܂܃e�L�X�g��ݒ�B
    else
        ComboBox_SetCurSel(hCombo, iItem); // ��v���鍀�ڂ�I���B
    return TRUE;
}

// ���C�h�������ANSI������ɕϊ�����B
LPSTR ansi_from_wide(UINT codepage, LPCWSTR wide)
{
    static CHAR s_ansi[1024];

    // �R�[�h�y�[�W�ŕ\���ł��Ȃ������̓Q�^�����i���j�ɂ���B
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
    return s_ansi;
}

// ANSI����������C�h������ɕϊ�����B
LPWSTR wide_from_ansi(UINT codepage, LPCSTR ansi)
{
    static WCHAR s_wide[1024];
    MultiByteToWideChar(codepage, 0, ansi, -1, s_wide, _countof(s_wide));
    return s_wide;
}

// mm�P�ʂ���s�N�Z���P�ʂւ̕ϊ��B
double pixels_from_mm(double mm, double dpi = 72)
{
    return dpi * mm / 25.4;
}

// �s�N�Z���P�ʂ���mm�P�ʂւ̕ϊ��B
double mm_from_pixels(double pixels, double dpi = 72)
{
    return 25.4 * pixels / dpi;
}

// �t�@�C�����Ɏg���Ȃ����������������ɒu��������B
void validate_filename(string_t& filename)
{
    for (auto& ch : filename)
    {
        if (wcschr(L"\\/:*?\"<>|", ch) != NULL)
            ch = L'_';
    }
}

// �t�H���g�}�b�v��ǂݍ��ށB
BOOL DekaMoji::LoadFontMap()
{
    // ����������B
    m_font_map.clear();

    // ���[�J���t�@�C���́ufontmap.dat�v��T���B
    auto filename = findLocalFile(TEXT("fontmap.dat"));
    if (filename == NULL)
        return FALSE; // ������Ȃ������B

    // �t�@�C���ufontmap.dat�v���J���B
    if (FILE *fp = _tfopen(filename, TEXT("rb")))
    {
        // ��s���ǂݍ��ށB
        char buf[512];
        while (fgets(buf, _countof(buf), fp))
        {
            // UTF-8����������C�h������ɕϊ�����B
            WCHAR szText[512];
            MultiByteToWideChar(CP_UTF8, 0, buf, -1, szText, _countof(szText));

            // �O��̋󔒂���菜���B
            str_trim(szText);

            // �s�R�����g���폜����B
            if (auto pch = wcschr(szText, L';'))
            {
                *pch = 0;
            }

            // ������x�O��̋󔒂���菜���B
            str_trim(szText);

            // �u=�v��T���B
            if (auto pch = wcschr(szText, L'='))
            {
                // �������؂蕪����B
                *pch++ = 0;
                auto font_name = szText;
                auto font_file = pch;

                // �O��̋󔒂���菜���B
                str_trim(font_name);
                str_trim(font_file);

                // �u,�v������΃C���f�b�N�X��ǂݍ��݁A�؂蕪����B
                pch = wcschr(pch, L',');
                int index = -1;
                if (pch)
                {
                    *pch++ = 0;
                    index = _wtoi(pch);
                }

                // ����ɑO��̋󔒂���菜���B
                str_trim(font_name);
                str_trim(font_file);

                // �t�H���g�t�@�C���̃p�X�t�@�C�������\�z����B
                TCHAR font_pathname[MAX_PATH];
                GetWindowsDirectory(font_pathname, _countof(font_pathname));
                PathAppend(font_pathname, TEXT("Fonts"));
                PathAppend(font_pathname, font_file);

                // �p�X�t�@�C���������݂��邩�H
                if (PathFileExists(font_pathname))
                {
                    // ���݂���΁A�t�H���g�̃G���g���[��ǉ��B
                    FONT_ENTRY entry;
                    entry.m_font_name = font_name;
                    entry.m_pathname = font_pathname;
                    entry.m_index = index;
                    m_font_map.push_back(entry);
                }
            }
        }

        // �t�@�C�������B
        fclose(fp);
    }

    return m_font_map.size() > 0;
}

// �R���X�g���N�^�B
DekaMoji::DekaMoji(HINSTANCE hInstance, INT argc, LPTSTR *argv)
    : m_hInstance(hInstance)
    , m_argc(argc)
    , m_argv(argv)
{
    // �f�[�^�����Z�b�g����B
    Reset();

    // �t�H���g�}�b�v��ǂݍ��ށB
    LoadFontMap();
}

// �f�[�^�����Z�b�g����B
void DekaMoji::Reset()
{
#define SETTING(id) m_settings[TEXT(#id)]
}

// �_�C�A���O������������B
void DekaMoji::InitDialog(HWND hwnd)
{
}

// �_�C�A���O����f�[�^�ցB
BOOL DekaMoji::DataFromDialog(HWND hwnd)
{
    return TRUE;
}

// �f�[�^����_�C�A���O�ցB
BOOL DekaMoji::DialogFromData(HWND hwnd)
{
    return TRUE;
}

// ���W�X�g������f�[�^�ցB
BOOL DekaMoji::DataFromReg(HWND hwnd)
{
    return TRUE;
}

// �f�[�^���烌�W�X�g���ցB
BOOL DekaMoji::RegFromData(HWND hwnd)
{
    return TRUE;
}

// �����񒆂Ɍ���������������������ׂĒu��������B
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

// �t�@�C���T�C�Y���擾����B
DWORD get_file_size(const string_t& filename)
{
    WIN32_FIND_DATA find;
    HANDLE hFind = FindFirstFile(filename.c_str(), &find);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0; // �G���[�B
    FindClose(hFind);
    if (find.nFileSizeHigh)
        return 0; // �傫������̂ŃG���[�B
    return find.nFileSizeLow; // �t�@�C���T�C�Y�B
}

// libHaru�̃G���[�n���h���̎����B
void hpdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void* user_data) {
    char message[1024];
    StringCchPrintfA(message, _countof(message), "error: error_no = %04X, detail_no = %d",
                     UINT(error_no), INT(detail_no));
    throw std::runtime_error(message);
}

// �����`��`�悷��B
void hpdf_draw_box(HPDF_Page page, double x, double y, double width, double height)
{
    HPDF_Page_MoveTo(page, x, y);
    HPDF_Page_LineTo(page, x, y + height);
    HPDF_Page_LineTo(page, x + width, y + height);
    HPDF_Page_LineTo(page, x + width, y);
    HPDF_Page_ClosePath(page);
    HPDF_Page_Stroke(page);
}

// �e�L�X�g��`�悷��B
void hpdf_draw_text(HPDF_Page page, HPDF_Font font, double font_size,
                    const char *text,
                    double x, double y, double width, double height,
                    int draw_box = 0)
{
    // �t�H���g�T�C�Y�𐧌��B
    if (font_size > HPDF_MAX_FONTSIZE)
        font_size = HPDF_MAX_FONTSIZE;

    // �����`��`�悷��B
    if (draw_box == 1)
    {
        hpdf_draw_box(page, x, y, width, height);
    }

    // �����`�Ɏ��܂�t�H���g�T�C�Y���v�Z����B
    double text_width, text_height;
    for (;;)
    {
        // �t�H���g�ƃt�H���g�T�C�Y���w��B
        HPDF_Page_SetFontAndSize(page, font, font_size);

        // �e�L�X�g�̕��ƍ������擾����B
        text_width = HPDF_Page_TextWidth(page, text);
        text_height = HPDF_Page_GetCurrentFontSize(page);

        // �e�L�X�g�������`�Ɏ��܂邩�H
        if (text_width <= width && text_height <= height)
        {
            // x,y�𒆉����낦�B
            x += (width - text_width) / 2;
            y += (height - text_height) / 2;
            break;
        }

        // �t�H���g�T�C�Y���������������čČv�Z�B
        font_size *= 0.8;
    }

    // �e�L�X�g��`�悷��B
    HPDF_Page_BeginText(page);
    {
        // �x�[�X���C������descent�������炷�B
        double descent = -HPDF_Font_GetDescent(font) * font_size / 1000.0;
        HPDF_Page_TextOut(page, x, y + descent, text);
    }
    HPDF_Page_EndText(page);

    // �����`��`�悷��B
    if (draw_box == 2)
    {
        hpdf_draw_box(page, x, y, text_width, text_height);
    }
}

// ���C���f�B�b�V�������B
string_t DekaMoji::JustDoIt(HWND hwnd)
{
    string_t ret;
    // PDF�I�u�W�F�N�g���쐬����B
    HPDF_Doc pdf = HPDF_New(hpdf_error_handler, NULL);
    if (!pdf)
        return L"";

    try
    {
        // �G���R�[�f�B���O 90ms-RKSJ-H, 90ms-RKSJ-V, 90msp-RKSJ-H, EUC-H, EUC-V �����p�\�ƂȂ�
        HPDF_UseJPEncodings(pdf);

#ifdef UTF8_SUPPORT
        // �G���R�[�f�B���O "UTF-8" �����p�\�ɁH�H�H
        HPDF_UseUTFEncodings(pdf);
        HPDF_SetCurrentEncoder(pdf, "UTF-8");
#endif

        // ���{��t�H���g�� MS-(P)Mincyo, MS-(P)Gothic �����p�\�ƂȂ�
        //HPDF_UseJPFonts(pdf);

        // �p���̌����B
        HPDF_PageDirection direction;
        if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_PORTRAIT))
            direction = HPDF_PAGE_PORTRAIT;
        else if (SETTING(IDC_PAGE_DIRECTION) == doLoadString(IDS_LANDSCAPE))
            direction = HPDF_PAGE_LANDSCAPE;
        else
            direction = HPDF_PAGE_PORTRAIT;

        // �y�[�W�T�C�Y�B
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
        else
            page_size = HPDF_PAGE_SIZE_A4;

        // �y�[�W�]���B
        double margin = pixels_from_mm(10);

        // �t�H���g���B
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

        // �t�H���g�T�C�Y�ipt�j�B
        double font_size = HPDF_MAX_FONTSIZE;

        // �o�̓t�@�C�����B
        string_t output_name = TEXT("DekaMojiPDF");

        HPDF_Page page; // �y�[�W�I�u�W�F�N�g�B
        HPDF_Font font; // �t�H���g�I�u�W�F�N�g�B
        double page_width, page_height; // �y�[�W�T�C�Y�B
        double content_x, content_y, content_width, content_height; // �y�[�W���e�̈ʒu�ƃT�C�Y�B
        for (INT iPage = 0; iPage < 1; ++iPage)
        {
            // �y�[�W��ǉ�����B
            page = HPDF_AddPage(pdf);

            // �y�[�W�T�C�Y�Ɨp���̌������w��B
            HPDF_Page_SetSize(page, page_size, direction);

            // �y�[�W�T�C�Y�i�s�N�Z���P�ʁj�B
            page_width = HPDF_Page_GetWidth(page);
            page_height = HPDF_Page_GetHeight(page);

            // �y�[�W���e�̈ʒu�ƃT�C�Y�B
            content_x = margin;
            content_y = margin;
            content_width = page_width - margin * 2;
            content_height = page_height - margin * 2;

            // ���̕����w��B
            HPDF_Page_SetLineWidth(page, 2);

            // ���̐F�� RGB �Őݒ肷��BPDF �ł� RGB �e�l�� [0,1] �Ŏw�肷�邱�ƂɂȂ��Ă���B
            HPDF_Page_SetRGBStroke(page, 0, 0, 0);

            /* �h��Ԃ��̐F�� RGB �Őݒ肷��BPDF �ł� RGB �e�l�� [0,1] �Ŏw�肷�邱�ƂɂȂ��Ă���B*/
            HPDF_Page_SetRGBFill(page, 0, 0, 0);

            // �t�H���g���w�肷��B
            auto font_name_a = ansi_from_wide(CP932, font_name.c_str());
#ifdef UTF8_SUPPORT
            font = HPDF_GetFont(pdf, font_name_a, "UTF-8");
#else
            font = HPDF_GetFont(pdf, font_name_a, "90ms-RKSJ-H");
#endif

#ifndef NO_SHAREWARE
            // �V�F�A�E�F�A���o�^�Ȃ�΁A���S�������`�悷��B
            if (!g_shareware.IsRegistered())
            {
#ifdef UTF8_SUPPORT
                auto logo_a = ansi_from_wide(CP_UTF8, doLoadString(IDS_LOGO));
#else
                auto logo_a = ansi_from_wide(CP932, doLoadString(IDS_LOGO));
#endif
                double logo_x = content_x, logo_y = content_y;

                // �t�H���g�ƃt�H���g�T�C�Y���w��B
                HPDF_Page_SetFontAndSize(page, font, font_size);

                // �e�L�X�g��`�悷��B
                HPDF_Page_BeginText(page);
                {
                    HPDF_Page_TextOut(page, logo_x, logo_y, logo_a);
                }
                HPDF_Page_EndText(page);
            }
#endif

            // ANSI������ɕϊ����ăe�L�X�g��`�悷��B
            string_t text = TEXT("This is a test.");
#ifdef UTF8_SUPPORT
            auto text_a = ansi_from_wide(CP_UTF8, text.c_str());
#else
            auto text_a = ansi_from_wide(CP932, text.c_str());
#endif
            hpdf_draw_text(page, font, font_size, text_a, content_x, content_y, content_width, font_size);
        }

        // PDF�o�́B
        {
            // PDF���ꎞ�t�@�C���ɕۑ�����B
            TempFile temp_file(TEXT("DM2"), TEXT(".pdf"));
            std::string temp_file_a = ansi_from_wide(CP_ACP, temp_file.make());
            HPDF_SaveToFile(pdf, temp_file_a.c_str());

            // �f�X�N�g�b�v�Ƀt�@�C�����R�s�[�B
            TCHAR szPath[MAX_PATH];
            SHGetSpecialFolderPath(hwnd, szPath, CSIDL_DESKTOPDIRECTORY, FALSE);
            PathAppend(szPath, output_name.c_str());
            StringCchCat(szPath, _countof(szPath), TEXT(".pdf"));
            if (!CopyFile(temp_file.get(), szPath, FALSE))
            {
                auto err_msg = ansi_from_wide(CP_ACP, doLoadString(IDS_COPYFILEFAILED));
                throw std::runtime_error(err_msg);
            }

            // �������b�Z�[�W��\���B
            StringCchCopy(szPath, _countof(szPath), output_name.c_str());
            StringCchCat(szPath, _countof(szPath), TEXT(".pdf"));
            TCHAR szText[MAX_PATH];
            StringCchPrintf(szText, _countof(szText), doLoadString(IDS_SUCCEEDED), szPath);
            ret = szText;
        }
    }
    catch (std::runtime_error& err)
    {
        // ���s�B
        auto wide = wide_from_ansi(CP_ACP, err.what());
        MessageBoxW(hwnd, wide, NULL, MB_ICONERROR);
        return TEXT("");
    }

    // PDF�I�u�W�F�N�g���������B
    HPDF_Free(pdf);

    return ret;
}

// WM_INITDIALOG
// �_�C�A���O�̏������B
BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    // ���[�U�f�[�^�B
    DekaMoji* pDM = (DekaMoji*)lParam;

    // ���[�U�[�f�[�^���E�B���h�E�n���h���Ɋ֘A�t����B
    SetWindowLongPtr(hwnd, GWLP_USERDATA, lParam);

    // �A�v���̖��O�B
    LoadString(NULL, IDS_APPNAME, g_szAppName, _countof(g_szAppName));

    // �A�C�R���̐ݒ�B
    g_hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(1));
    g_hIconSm = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (WPARAM)g_hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (WPARAM)g_hIconSm);

    // �������B
    pDM->InitDialog(hwnd);

    // ���W�X�g������f�[�^��ǂݍ��ށB
    pDM->DataFromReg(hwnd);

    // �_�C�A���O�Ƀf�[�^��ݒ�B
    pDM->DialogFromData(hwnd);

    return TRUE;
}

// �uOK�v�{�^���������ꂽ�B
BOOL OnOK(HWND hwnd)
{
    // ���[�U�f�[�^�B
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // �u������...�v�ƃ{�^���ɕ\������B
    HWND hButton = GetDlgItem(hwnd, IDC_GENERATE);
    SetWindowText(hButton, doLoadString(IDS_PROCESSINGNOW));

    // �_�C�A���O����f�[�^���擾�B
    if (!pDM->DataFromDialog(hwnd)) // ���s�B
    {
        // �{�^���e�L�X�g�����ɖ߂��B
        SetWindowText(hButton, doLoadString(IDS_GENERATE));

        return FALSE; // ���s�B
    }

    // �ݒ�����W�X�g���ɕۑ��B
    pDM->RegFromData(hwnd);

    // ���C���f�B�b�V�������B
    string_t success = pDM->JustDoIt(hwnd);

    // �{�^���e�L�X�g�����ɖ߂��B
    SetWindowText(hButton, doLoadString(IDS_GENERATE));

    // �K�v�Ȃ猋�ʂ����b�Z�[�W�{�b�N�X�Ƃ��ĕ\������B
    if (success.size())
    {
        MessageBox(hwnd, success.c_str(), g_szAppName, MB_ICONINFORMATION);
    }

    return TRUE; // �����B
}

// �u�ݒ�̏������v�{�^���B
void OnEraseSettings(HWND hwnd)
{
    // ���[�U�[�f�[�^�B
    DekaMoji* pDM = (DekaMoji*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    // �f�[�^�����Z�b�g����B
    pDM->Reset();

    // �f�[�^����_�C�A���O�ցB
    pDM->DialogFromData(hwnd);

    // �f�[�^���烌�W�X�g���ցB
    pDM->RegFromData(hwnd);
}

// WM_COMMAND
// �R�}���h�B
void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDC_GENERATE: // �uPDF�����v�{�^���B
        OnOK(hwnd);
        break;
    case IDC_EXIT: // �u�I���v�{�^���B
        EndDialog(hwnd, id);
        break;
    }
}

// WM_DESTROY
// �E�B���h�E���j�����ꂽ�B
void OnDestroy(HWND hwnd)
{
    // �A�C�R����j���B
    DestroyIcon(g_hIcon);
    DestroyIcon(g_hIconSm);
    g_hIcon = g_hIconSm = NULL;
}

// �_�C�A���O�v���V�[�W���B
INT_PTR CALLBACK
DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_MSG(hwnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    }
    return 0;
}

// �f�J����PDF�̃��C���֐��B
INT DekaMoji_Main(HINSTANCE hInstance, INT argc, LPTSTR *argv)
{
    // �A�v���̃C���X�^���X��ێ�����B
    g_hInstance = hInstance;

    // ���ʃR���g���[���Q������������B
    InitCommonControls();

#ifndef NO_SHAREWARE
    // �f�o�b�K�\���L���A�܂��̓V�F�A�E�F�A���J�n�ł��Ȃ��Ƃ���
    if (IsDebuggerPresent() || !g_shareware.Start(NULL))
    {
        // ���s�B�A�v���P�[�V�������I������B
        return -1;
    }
#endif

    // ���[�U�[�f�[�^��ێ�����B
    DekaMoji dm(hInstance, argc, argv);

    // ���[�U�[�f�[�^���p�����[�^�Ƃ��ă_�C�A���O���J���B
    DialogBoxParam(hInstance, MAKEINTRESOURCE(1), NULL, DialogProc, (LPARAM)&dm);

    // ����I���B
    return 0;
}

// Windows�A�v���̃��C���֐��B
INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
#ifdef UNICODE
    // wWinMain���T�|�[�g���Ă��Ȃ��R���p�C���̂��߂ɁA�R�}���h���C���̏������s���B
    INT argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    INT ret = DekaMoji_Main(hInstance, argc, argv);
    LocalFree(argv);
    return ret;
#else
    return DekaMoji_Main(hInstance, __argc, __argv);
#endif
}
