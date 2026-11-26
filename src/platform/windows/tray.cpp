#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include "jianhuan/settings.h"

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr wchar_t kClassName[] = L"JianhuanTrayWindow";
constexpr UINT kCmdSettings = 1;
constexpr UINT kCmdPinyin = 2;
constexpr UINT kCmdDouble = 3;
constexpr UINT kCmdPunct = 4;
constexpr UINT kCmdFull = 5;
constexpr UINT kCmdAbout = 6;
constexpr UINT kCmdExit = 7;
constexpr UINT kCmdAscii = 8;

std::string UserDir() {
  wchar_t appdata[MAX_PATH] = {};
  GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
  return (std::filesystem::path(appdata) / "Jianhuan").string();
}

std::wstring ExeDir() {
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return std::filesystem::path(path).parent_path().wstring();
}

void OpenSettings() {
  const auto exe = std::filesystem::path(ExeDir()) / "jianhuan_settings.exe";
  ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ShowMenu(HWND hwnd) {
  jianhuan::Settings settings;
  jianhuan::load_settings(UserDir(), settings);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, kCmdSettings, L"个性化设置…");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (settings.schema_id == "jianhuan_pinyin" ? MF_CHECKED : 0), kCmdPinyin, L"全拼");
  AppendMenuW(menu, MF_STRING | (settings.schema_id == "jianhuan_double_pinyin" ? MF_CHECKED : 0), kCmdDouble, L"双拼");
  AppendMenuW(menu, MF_STRING | (settings.ascii_punct ? MF_CHECKED : 0), kCmdPunct, L"英文标点");
  AppendMenuW(menu, MF_STRING | (settings.full_shape ? MF_CHECKED : 0), kCmdFull, L"全角");
  AppendMenuW(menu, MF_STRING | (settings.ascii_mode ? MF_CHECKED : 0), kCmdAscii, L"英文模式");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCmdAbout, L"关于简焕");
  AppendMenuW(menu, MF_STRING, kCmdExit, L"退出托盘");
  POINT pt = {};
  GetCursorPos(&pt);
  SetForegroundWindow(hwnd);
  const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
  DestroyMenu(menu);
  if (cmd == 0) return;
  if (cmd == kCmdSettings) {
    OpenSettings();
    return;
  }
  if (cmd == kCmdAbout) {
    MessageBoxW(hwnd, L"简焕输入法\n本地离线 · 可闭源商用", L"关于简焕", MB_OK | MB_ICONINFORMATION);
    return;
  }
  if (cmd == kCmdExit) {
    DestroyWindow(hwnd);
    return;
  }
  if (cmd == kCmdPinyin) settings.schema_id = "jianhuan_pinyin";
  if (cmd == kCmdDouble) settings.schema_id = "jianhuan_double_pinyin";
  if (cmd == kCmdPunct) settings.ascii_punct = !settings.ascii_punct;
  if (cmd == kCmdFull) settings.full_shape = !settings.full_shape;
  if (cmd == kCmdAscii) settings.ascii_mode = !settings.ascii_mode;
  jianhuan::save_settings(UserDir(), settings);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == kTrayMessage) {
    if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
      ShowMenu(hwnd);
      return 0;
    }
    if (lparam == WM_LBUTTONUP) {
      OpenSettings();
      return 0;
    }
  }
  if (message == WM_DESTROY) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

int wmain() {
  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"JianhuanTraySingleInstance");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    return 0;
  }
  WNDCLASSW wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kClassName;
  wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
  RegisterClassW(&wc);
  HWND hwnd = CreateWindowExW(0, kClassName, L"JianhuanTray", WS_OVERLAPPED, 0, 0, 0, 0,
                              nullptr, nullptr, wc.hInstance, nullptr);
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  nid.uID = 1;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = kTrayMessage;
  nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
  wcscpy_s(nid.szTip, L"简焕输入法");
  Shell_NotifyIconW(NIM_ADD, &nid);

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  ReleaseMutex(mutex);
  CloseHandle(mutex);
  return 0;
}