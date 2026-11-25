#include <windows.h>
#include <string>
#include <vector>
#include "candidate_window.h"

int wmain() {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  CandidateWindow window;
  const std::vector<std::string> items = {"床前明月光", "窗前明月光", "窗前", "床前", "😄"};
  window.Show(200, 200, items, 0, 0, false, "chaung qian ming yue g");
  const DWORD start = GetTickCount();
  while (GetTickCount() - start < 6000) {
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
  window.Hide();
  CoUninitialize();
  return 0;
}