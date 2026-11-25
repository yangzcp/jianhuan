#include <windows.h>
#include <commctrl.h>
#include <filesystem>
#include <string>
#include "jianhuan/settings.h"

namespace {

constexpr int kIdSchema = 101;
constexpr int kIdPageSize = 102;
constexpr int kIdPunct = 103;
constexpr int kIdFull = 104;
constexpr int kIdSave = 105;
constexpr int kIdCancel = 106;

HWND g_schema = nullptr;
HWND g_page = nullptr;
HWND g_punct = nullptr;
HWND g_full = nullptr;

std::string UserDir() {
  wchar_t appdata[MAX_PATH] = {};
  GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
  return (std::filesystem::path(appdata) / "Jianhuan").string();
}

void LoadControls() {
  jianhuan::Settings settings;
  jianhuan::load_settings(UserDir(), settings);
  SendMessageW(g_schema, CB_SETCURSEL, settings.schema_id == "jianhuan_double_pinyin" ? 1 : 0, 0);
  SetWindowTextW(g_page, std::to_wstring(settings.page_size).c_str());
  SendMessageW(g_punct, BM_SETCHECK, settings.ascii_punct ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageW(g_full, BM_SETCHECK, settings.full_shape ? BST_CHECKED : BST_UNCHECKED, 0);
}

void SaveControls() {
  jianhuan::Settings settings;
  jianhuan::load_settings(UserDir(), settings);
  settings.schema_id = SendMessageW(g_schema, CB_GETCURSEL, 0, 0) == 1 ? "jianhuan_double_pinyin" : "jianhuan_pinyin";
  wchar_t buffer[16] = {};
  GetWindowTextW(g_page, buffer, 16);
  const int page = _wtoi(buffer);
  settings.page_size = page >= 1 && page <= 9 ? page : 5;
  settings.ascii_punct = SendMessageW(g_punct, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.full_shape = SendMessageW(g_full, BM_GETCHECK, 0, 0) == BST_CHECKED;
  jianhuan::save_settings(UserDir(), settings);
  MessageBoxW(nullptr, L"设置已保存。\n重新切换输入法或重启应用后生效。", L"简焕设置", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_COMMAND:
      if (LOWORD(wparam) == kIdSave) {
        SaveControls();
        return 0;
      }
      if (LOWORD(wparam) == kIdCancel) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int wmain() {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  WNDCLASSW wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"JianhuanSettings";
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, L"JianhuanSettings", L"简焕输入法 设置",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              0, 0, 420, 300, nullptr, nullptr, wc.hInstance, nullptr);
  HICON app_icon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
  SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(app_icon));
  SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app_icon));
  HFONT font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

  auto label = [&](const wchar_t* text, int x, int y, int w, int h) {
    HWND control = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h,
                                   hwnd, nullptr, wc.hInstance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return control;
  };

  label(L"输入方案：", 20, 20, 90, 22);
  g_schema = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                             110, 18, 260, 120, hwnd, reinterpret_cast<HMENU>(kIdSchema), wc.hInstance, nullptr);
  SendMessageW(g_schema, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"全拼"));
  SendMessageW(g_schema, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"双拼"));
  SendMessageW(g_schema, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  label(L"候选个数：", 20, 60, 90, 22);
  g_page = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_NUMBER,
                           110, 58, 80, 24, hwnd, reinterpret_cast<HMENU>(kIdPageSize), wc.hInstance, nullptr);
  SendMessageW(g_page, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  g_punct = CreateWindowExW(0, L"BUTTON", L"中文输入时使用英文标点", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                            20, 100, 350, 24, hwnd, reinterpret_cast<HMENU>(kIdPunct), wc.hInstance, nullptr);
  SendMessageW(g_punct, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  g_full = CreateWindowExW(0, L"BUTTON", L"全角字符", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                           20, 132, 350, 24, hwnd, reinterpret_cast<HMENU>(kIdFull), wc.hInstance, nullptr);
  SendMessageW(g_full, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  HWND save = CreateWindowExW(0, L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                              200, 190, 80, 30, hwnd, reinterpret_cast<HMENU>(kIdSave), wc.hInstance, nullptr);
  SendMessageW(save, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                290, 190, 80, 30, hwnd, reinterpret_cast<HMENU>(kIdCancel), wc.hInstance, nullptr);
  SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

  LoadControls();
  RECT rect = {};
  GetWindowRect(hwnd, &rect);
  SetWindowPos(hwnd, nullptr,
               (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2,
               (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2,
               0, 0, SWP_NOSIZE | SWP_NOZORDER);
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  DeleteObject(font);
  CoUninitialize();
  return 0;
}