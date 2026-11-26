#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <string>
#include <vector>

class CandidateWindow {
 public:
  ~CandidateWindow();
  void Show(int x, int y, const std::vector<std::string>& items, int highlighted);
  void Hide();

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  void EnsureWindow();
  bool EnsureGdiplus();
  void Render(int x, int y);

  struct FontMetrics {
    float ascent = 0;
    float descent = 0;
    float em = 0;
    float size = 0;
    float MidlineY(float center_y) const { return center_y - (ascent + descent) * size / em / 2.0f; }
  };

  HWND hwnd_ = nullptr;
  ULONG_PTR gdiplus_token_ = 0;
  bool gdiplus_ready_ = false;
  Gdiplus::Font* font_ = nullptr;
  Gdiplus::Font* emoji_font_ = nullptr;
  FontMetrics font_m_;
  FontMetrics emoji_m_;

  std::vector<std::wstring> items_;
  int highlighted_ = 0;
};