#include <windows.h>
#include <msctf.h>
#include <fstream>
#include "text_service.h"

HRESULT CreateTextServiceInstance(REFIID riid, void** object);

namespace {

std::ofstream g_log;

int Fail(const char* msg) {
  g_log << "password test failed: " << msg << "\n";
  g_log.flush();
  return 1;
}

HWND MakeEdit(HWND parent, int id, bool password) {
  const DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | (password ? ES_PASSWORD : 0);
  return CreateWindowExW(0, L"EDIT", L"", style, 10, 10 + id * 40, 300, 28, parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

}  // namespace

int wmain() {
  g_log.open("build\\password_test.log", std::ios::trunc);
  g_log << std::unitbuf;
  g_log << "password test start\n";
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSW wc = {};
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = inst;
  wc.lpszClassName = L"JianhuanPasswordTest";
  RegisterClassW(&wc);
  HWND parent = CreateWindowExW(0, wc.lpszClassName, L"pwd test", WS_OVERLAPPEDWINDOW,
                                100, 100, 340, 140, nullptr, nullptr, inst, nullptr);
  HWND normal = MakeEdit(parent, 1, false);
  HWND pwd = MakeEdit(parent, 2, true);
  ShowWindow(parent, SW_SHOW);
  UpdateWindow(parent);
  SetForegroundWindow(parent);
  g_log << "window ready controls=" << (normal != nullptr && pwd != nullptr) << "\n";

  ITfTextInputProcessor* processor = nullptr;
  if (FAILED(CreateTextServiceInstance(IID_ITfTextInputProcessor, reinterpret_cast<void**>(&processor))) || !processor) {
    return Fail("create service");
  }
  auto* service = static_cast<JianhuanTextService*>(processor);

  BOOL eaten = FALSE;
  SetFocus(normal);
  service->OnTestKeyDown(nullptr, 'A', 0, &eaten);
  if (eaten != TRUE) return Fail("normal field should eat letter for composition");
  service->OnTestKeyUp(nullptr, VK_SHIFT, 0, &eaten);
  if (eaten != FALSE) return Fail("normal field shift keyup must not be swallowed (app hotkeys)");
  service->OnTestKeyDown(nullptr, VK_F6, 0, &eaten);
  if (eaten != FALSE) return Fail("function key (Shift+F6 rename) must pass through");
  g_log << "normal field ok\n";

  SetFocus(pwd);
  service->OnTestKeyDown(nullptr, 'A', 0, &eaten);
  if (eaten != FALSE) return Fail("password field must pass letter through");
  service->OnKeyDown(nullptr, 'A', 0, &eaten);
  if (eaten != FALSE) return Fail("password keydown must pass through");
  service->OnTestKeyDown(nullptr, '1', 0, &eaten);
  if (eaten != FALSE) return Fail("password digit must pass through");
  service->OnTestKeyUp(nullptr, VK_SHIFT, 0, &eaten);
  if (eaten != FALSE) return Fail("password shift must not toggle mode");
  const WPARAM symbols[] = {VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_1, VK_OEM_2, VK_OEM_3,
                            VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7, VK_OEM_PLUS, VK_OEM_MINUS};
  for (WPARAM vk : symbols) {
    service->OnTestKeyDown(nullptr, vk, 0, &eaten);
    if (eaten != FALSE) return Fail("password symbol must pass through as ASCII");
    service->OnKeyDown(nullptr, vk, 0, &eaten);
    if (eaten != FALSE) return Fail("password symbol keydown must pass through");
  }
  g_log << "password field ok\n";

  SetFocus(normal);
  service->OnTestKeyDown(nullptr, 'A', 0, &eaten);
  if (eaten != TRUE) return Fail("returning to normal field should compose again");
  service->OnTestKeyDown(nullptr, VK_OEM_COMMA, 0, &eaten);
  if (eaten != TRUE) return Fail("normal field comma should use Chinese punctuation");
  g_log << "return to normal ok\n";

  processor->Release();
  g_log << "password ok\n";
  g_log.flush();
  ExitProcess(0);
  return 0;
}