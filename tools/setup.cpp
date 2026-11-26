#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

bool LoadResourceData(int id, std::vector<char>& data) {
  HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!resource) return false;
  HGLOBAL loaded = LoadResource(nullptr, resource);
  if (!loaded) return false;
  const void* pointer = LockResource(loaded);
  const DWORD size = SizeofResource(nullptr, resource);
  if (!pointer || size == 0) return false;
  data.assign(static_cast<const char*>(pointer), static_cast<const char*>(pointer) + size);
  return true;
}

bool ExtractPayload(const fs::path& target, std::wstring* error = nullptr) {
  std::vector<char> manifest_data;
  if (!LoadResourceData(1, manifest_data)) {
    if (error) *error = L"无法读取安装清单资源。";
    return false;
  }
  std::string manifest(manifest_data.begin(), manifest_data.end());
  std::istringstream stream(manifest);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    const int id = std::stoi(line.substr(0, tab));
    if (id < 1000) continue;
    const std::string relative = line.substr(tab + 1);
    std::vector<char> data;
    if (!LoadResourceData(id, data)) {
      if (error) *error = L"无法读取资源: " + fs::path(relative).wstring();
      return false;
    }
    const fs::path output = target / relative;
    std::error_code ec;
    fs::create_directories(output.parent_path(), ec);
    if (ec) {
      if (error) *error = L"无法创建目录: " + output.parent_path().wstring();
      return false;
    }
    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      if (error) *error = L"无法写入文件: " + output.wstring() + L"\n可能文件被占用或权限不足。";
      return false;
    }
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
  }
  return true;
}

DWORD RunProcess(const fs::path& exe, const std::wstring& args) {
  std::wstring command = L"\"" + exe.wstring() + L"\" " + args;
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
    return 1;
  }
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(process.hProcess, &exit_code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return exit_code;
}

fs::path InstallTarget() {
  wchar_t local[MAX_PATH] = {};
  GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
  return fs::path(local) / "Jianhuan";
}

void WriteUninstallEntry(const fs::path& target) {
  HKEY key = nullptr;
  const wchar_t* path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Jianhuan";
  if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
    return;
  }
  const std::wstring uninstall = L"\"" + (target / "JianhuanSetup.exe").wstring() + L"\" /uninstall";
  const std::wstring display = L"简焕输入法";
  const std::wstring version = L"0.1.0";
  const std::wstring location = target.wstring();
  RegSetValueExW(key, L"DisplayName", 0, REG_SZ, reinterpret_cast<const BYTE*>(display.c_str()), static_cast<DWORD>((display.size() + 1) * sizeof(wchar_t)));
  RegSetValueExW(key, L"DisplayVersion", 0, REG_SZ, reinterpret_cast<const BYTE*>(version.c_str()), static_cast<DWORD>((version.size() + 1) * sizeof(wchar_t)));
  RegSetValueExW(key, L"InstallLocation", 0, REG_SZ, reinterpret_cast<const BYTE*>(location.c_str()), static_cast<DWORD>((location.size() + 1) * sizeof(wchar_t)));
  RegSetValueExW(key, L"UninstallString", 0, REG_SZ, reinterpret_cast<const BYTE*>(uninstall.c_str()), static_cast<DWORD>((uninstall.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
}

void RemoveUninstallEntry() {
  RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Jianhuan");
}

void MoveToPending(const fs::path& path) {
  wchar_t temp[MAX_PATH] = {};
  GetTempPathW(MAX_PATH, temp);
  const auto pending = fs::path(temp) / ("Jianhuan_pending_" + std::to_string(GetTickCount64()));
  std::error_code ec;
  fs::create_directories(pending, ec);
  MoveFileExW(path.c_str(), (pending / path.filename()).c_str(), MOVEFILE_REPLACE_EXISTING);
}

// Nudge running processes to re-evaluate the input language so they re-create
// the text service (loading the newly registered DLL) without a reboot.
void RefreshInputEnvironment() {
  SendNotifyMessageW(HWND_BROADCAST, WM_INPUTLANGCHANGE, 0, 0);
}

bool HasFlag(int argc, wchar_t** argv, const wchar_t* flag) {
  for (int i = 1; i < argc; ++i) {
    if (_wcsicmp(argv[i], flag) == 0) return true;
  }
  return false;
}

std::wstring GetDirArg(int argc, wchar_t** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::wstring arg = argv[i];
    if (_wcsnicmp(arg.c_str(), L"/DIR=", 5) == 0) {
      return arg.substr(5);
    }
  }
  return {};
}

int CALLBACK BrowseCallback(HWND hwnd, UINT message, LPARAM, LPARAM data) {
  if (message == BFFM_INITIALIZED) {
    SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, data);
  }
  return 0;
}

bool ChooseInstallPath(fs::path& target) {
  const std::wstring default_path = target.wstring();
  BROWSEINFOW browse = {};
  browse.lpszTitle = L"选择简焕输入法安装目录";
  browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  browse.lpfn = BrowseCallback;
  browse.lParam = reinterpret_cast<LPARAM>(default_path.c_str());
  LPITEMIDLIST list = SHBrowseForFolderW(&browse);
  if (!list) return false;
  wchar_t path[MAX_PATH] = {};
  const bool ok = SHGetPathFromIDListW(list, path) != FALSE;
  CoTaskMemFree(list);
  if (!ok) return false;
  target = path;
  return true;
}

struct InstallDialog {
  HWND edit = nullptr;
  fs::path* target = nullptr;
  bool accepted = false;
};

LRESULT CALLBACK SetupWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }
  auto* dialog = reinterpret_cast<InstallDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (message == WM_COMMAND && dialog) {
    const int id = LOWORD(wparam);
    if (id == 101) {
      wchar_t current[MAX_PATH] = {};
      GetWindowTextW(dialog->edit, current, MAX_PATH);
      fs::path chosen = current;
      if (ChooseInstallPath(chosen)) {
        SetWindowTextW(dialog->edit, chosen.c_str());
      }
      return 0;
    }
    if (id == 102) {
      wchar_t path[MAX_PATH] = {};
      GetWindowTextW(dialog->edit, path, MAX_PATH);
      *dialog->target = path;
      dialog->accepted = true;
      DestroyWindow(hwnd);
      return 0;
    }
    if (id == 103) {
      DestroyWindow(hwnd);
      return 0;
    }
  }
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool ShowInstallDialog(fs::path& target) {
  WNDCLASSW wc = {};
  wc.lpfnWndProc = SetupWndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"JianhuanSetupDialog";
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  InstallDialog dialog;
  dialog.target = &target;
  const wchar_t* title = fs::exists(target / "jianhuan_tsf.dll") ? L"简焕输入法 覆盖安装" : L"简焕输入法 安装";
  HWND hwnd = CreateWindowExW(0, L"JianhuanSetupDialog", title,
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              0, 0, 520, 180, nullptr, nullptr, wc.hInstance, &dialog);
  if (!hwnd) return false;

  HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  HWND label = CreateWindowExW(0, L"STATIC", L"选择安装目录：", WS_CHILD | WS_VISIBLE,
                               16, 16, 300, 20, hwnd, nullptr, wc.hInstance, nullptr);
  dialog.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", target.c_str(),
                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                16, 42, 380, 24, hwnd, reinterpret_cast<HMENU>(100), wc.hInstance, nullptr);
  HWND browse = CreateWindowExW(0, L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                405, 41, 85, 26, hwnd, reinterpret_cast<HMENU>(101), wc.hInstance, nullptr);
  HWND install = CreateWindowExW(0, L"BUTTON", L"安装", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                 300, 95, 90, 28, hwnd, reinterpret_cast<HMENU>(102), wc.hInstance, nullptr);
  HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                400, 95, 90, 28, hwnd, reinterpret_cast<HMENU>(103), wc.hInstance, nullptr);
  for (HWND control : {label, dialog.edit, browse, install, cancel}) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  }

  RECT rect = {};
  GetWindowRect(hwnd, &rect);
  const int x = (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2;
  const int y = (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2;
  SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  ShowWindow(hwnd, SW_SHOW);
  SetFocus(dialog.edit);

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (!IsDialogMessageW(hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  return dialog.accepted;
}

fs::path ReadInstallLocation() {
  wchar_t path[MAX_PATH] = {};
  DWORD size = sizeof(path);
  const LSTATUS status = RegGetValueW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Jianhuan",
      L"InstallLocation", RRF_RT_REG_SZ, nullptr, path, &size);
  if (status != ERROR_SUCCESS) return {};
  return fs::path(path);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  const bool quiet = HasFlag(argc, argv, L"/quiet");

  if (HasFlag(argc, argv, L"/uninstall")) {
    fs::path target = ReadInstallLocation();
    if (target.empty()) target = InstallTarget();
    const fs::path registrar = target / "jianhuan_register.exe";
    if (fs::exists(registrar)) {
      RunProcess(registrar, L"/unregister");
    }
    const fs::path registrar32 = target / "jianhuan_register32.exe";
    if (fs::exists(registrar32)) {
      RunProcess(registrar32, L"/unregister");
    }
    RefreshInputEnvironment();
    RemoveUninstallEntry();
    {
      HKEY run_key = nullptr;
      if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, KEY_SET_VALUE, &run_key) == ERROR_SUCCESS) {
        RegDeleteValueW(run_key, L"JianhuanTray");
        RegCloseKey(run_key);
      }
    }
    if (HWND tray = FindWindowW(L"JianhuanTrayWindow", nullptr)) {
      PostMessageW(tray, WM_CLOSE, 0, 0);
    }
    if (HWND settings = FindWindowW(L"JianhuanSettings", nullptr)) {
      PostMessageW(settings, WM_CLOSE, 0, 0);
    }
    Sleep(300);
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(target)) {
      std::error_code item_ec;
      if (entry.is_directory()) {
        fs::remove_all(entry.path(), item_ec);
      } else {
        fs::remove(entry.path(), item_ec);
      }
      if (item_ec) {
        MoveToPending(entry.path());
      }
    }
    fs::remove(target, ec);
    if (!quiet) {
      MessageBoxW(nullptr, L"简焕输入法已卸载。", L"简焕", MB_OK | MB_ICONINFORMATION);
    }
    return 0;
  }

  fs::path target = InstallTarget();
  const std::wstring dir_arg = GetDirArg(argc, argv);
  if (!dir_arg.empty()) {
    target = dir_arg;
  } else if (!quiet && !HasFlag(argc, argv, L"/extract")) {
    if (!ShowInstallDialog(target)) {
      return 0;
    }
  }

  if (HasFlag(argc, argv, L"/extract")) {
    std::wstring error;
    if (ExtractPayload(target, &error)) return 0;
    std::error_code ec;
    fs::create_directories(target, ec);
    std::wofstream log(target / "setup_error.txt", std::ios::trunc);
    log << error;
    return 1;
  }

  const fs::path staging = target / "staging";
  std::error_code ec;
  fs::remove_all(staging, ec);
  if (HWND tray = FindWindowW(L"JianhuanTrayWindow", nullptr)) {
    PostMessageW(tray, WM_CLOSE, 0, 0);
  }
  if (HWND settings = FindWindowW(L"JianhuanSettings", nullptr)) {
    PostMessageW(settings, WM_CLOSE, 0, 0);
  }
  Sleep(300);
  std::wstring extract_error;
  if (!ExtractPayload(staging, &extract_error)) {
    if (!quiet) MessageBoxW(nullptr, extract_error.c_str(), L"简焕", MB_OK | MB_ICONERROR);
    return 1;
  }

  fs::create_directories(target, ec);
  for (const wchar_t* dir : {L"licenses"}) {
    const fs::path source = staging / dir;
    if (fs::exists(source)) {
      fs::copy(source, target / dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
      if (ec) {
        if (!quiet) MessageBoxW(nullptr, (L"无法更新目录: " + (target / dir).wstring()).c_str(), L"简焕", MB_OK | MB_ICONERROR);
        return 1;
      }
    }
  }

  const std::wstring stamp = std::to_wstring(GetTickCount64());
  const fs::path dll64 = target / (L"jianhuan_tsf_" + stamp + L".dll");
  const fs::path dll32 = target / (L"jianhuan_tsf32_" + stamp + L".dll");
  fs::copy_file(staging / "jianhuan_tsf.dll", dll64, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    if (!quiet) MessageBoxW(nullptr, (L"无法释放 DLL: " + dll64.wstring()).c_str(), L"简焕", MB_OK | MB_ICONERROR);
    return 1;
  }
  const bool has32 = fs::exists(staging / "jianhuan_tsf32.dll");
  if (has32) {
    fs::copy_file(staging / "jianhuan_tsf32.dll", dll32, fs::copy_options::overwrite_existing, ec);
  }

  const std::wstring data_name = L"data_" + stamp;
  const fs::path data_dir = target / data_name;
  fs::copy(staging / "data", data_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  if (ec) {
    if (!quiet) MessageBoxW(nullptr, (L"无法释放数据目录: " + data_dir.wstring()).c_str(), L"简焕", MB_OK | MB_ICONERROR);
    return 1;
  }
  {
    std::ofstream pointer(target / "active_data.txt", std::ios::trunc);
    pointer << std::string(data_name.begin(), data_name.end());
  }
  {
    std::error_code junction_ec;
    fs::remove(target / "data", junction_ec);
    if (!fs::exists(target / "data")) {
      const std::wstring link_args = L"/c mklink /J \"" + (target / "data").wstring() +
                                     L"\" \"" + data_dir.wstring() + L"\"";
      RunProcess(L"cmd.exe", link_args);
    }
  }

  fs::copy_file(staging / "jianhuan_register.exe", target / "jianhuan_register.exe",
                fs::copy_options::overwrite_existing, ec);
  if (fs::exists(staging / "jianhuan_register32.exe")) {
    fs::copy_file(staging / "jianhuan_register32.exe", target / "jianhuan_register32.exe",
                  fs::copy_options::overwrite_existing, ec);
  }
  if (fs::exists(staging / "jianhuan_tray.exe")) {
    fs::copy_file(staging / "jianhuan_tray.exe", target / "jianhuan_tray.exe",
                  fs::copy_options::overwrite_existing, ec);
  }

  std::wstring args = L"/register /DLL64=\"" + dll64.wstring() + L"\"";
  if (has32) {
    args += L" /DLL32=\"" + dll32.wstring() + L"\"";
  }
  const DWORD result = RunProcess(staging / "jianhuan_register.exe", args);
  if (result != 0) {
    Sleep(500);
    if (RunProcess(staging / "jianhuan_register.exe", args) != 0) {
      if (!quiet) MessageBoxW(nullptr, L"输入法注册失败。", L"简焕", MB_OK | MB_ICONERROR);
      return 1;
    }
  }
  if (has32 && fs::exists(staging / "jianhuan_register32.exe")) {
    const std::wstring args32 = L"/register /DLL64=\"" + dll32.wstring() + L"\"";
    RunProcess(staging / "jianhuan_register32.exe", args32);
  }
  RefreshInputEnvironment();

  wchar_t self[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  CopyFileW(self, (target / "JianhuanSetup.exe").c_str(), FALSE);

  WriteUninstallEntry(target);
  {
    HKEY run_key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &run_key, nullptr) == ERROR_SUCCESS) {
      const std::wstring tray = (target / "jianhuan_tray.exe").wstring();
      RegSetValueExW(run_key, L"JianhuanTray", 0, REG_SZ, reinterpret_cast<const BYTE*>(tray.c_str()),
                     static_cast<DWORD>((tray.size() + 1) * sizeof(wchar_t)));
      RegCloseKey(run_key);
    }
    ShellExecuteW(nullptr, L"open", (target / "jianhuan_tray.exe").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  }
  for (const auto& entry : fs::directory_iterator(target)) {
    const std::wstring name = entry.path().filename().wstring();
    std::error_code remove_ec;
    if (name.rfind(L"jianhuan_tsf", 0) == 0 && entry.path().extension() == L".dll" &&
        entry.path() != dll64 && entry.path() != dll32) {
      fs::remove(entry.path(), remove_ec);
    }
  }
  fs::remove_all(staging, ec);
  if (!quiet) {
    MessageBoxW(nullptr, L"简焕输入法安装完成，无需重启电脑。\n请在 Windows 输入法设置中切换到“简焕”。\n已打开的程序切换一次输入法即加载新版。", L"简焕", MB_OK | MB_ICONINFORMATION);
  }
  return 0;
}