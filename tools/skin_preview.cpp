#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "candidate_window.h"

static void Capture(int x, int y, int w, int h, const wchar_t* path) {
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
  HGDIOBJ old = SelectObject(mem, bmp);
  BitBlt(mem, 0, 0, w, h, screen, x, y, SRCCOPY | CAPTUREBLT);
  Gdiplus::Bitmap image(bmp, nullptr);
  const CLSID png = {0x557CF406, 0x1A04, 0x11D3, {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
  image.Save(path, &png, nullptr);
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
}

int wmain() {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  Gdiplus::GdiplusStartupInput input;
  ULONG_PTR token = 0;
  Gdiplus::GdiplusStartup(&token, &input, nullptr);
  CandidateWindow window;
  const std::vector<std::string> items = {"我感觉", "我感觉好", "我感觉冷", "我感觉热", "我感觉累", "woganj"};
  window.Show(200, 200, items, 0);
  const DWORD start = GetTickCount();
  while (GetTickCount() - start < 800) {
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
  Capture(190, 190, 760, 90, L"build\\skin_predict.png");
  window.Hide();
  Gdiplus::GdiplusShutdown(token);
  CoUninitialize();
  return 0;
}