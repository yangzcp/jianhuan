#pragma once

#include <windows.h>
#include <gdiplus.h>

#include <string>
#include <vector>

class CandidateWindow {
 public:
  ~CandidateWindow();
  void Show(int x, int y, const std::vector<std::string>& items, int highlighted,
            int page_no, bool is_last_page, const std::string& preedit);
  void Hide();

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  void EnsureWindow();
  bool EnsureGdiplus();
  void Render(int x, int y);
  std::wstring PageLabel() const;

  HWND hwnd_ = nullptr;
  ULONG_PTR gdiplus_token_ = 0;
  bool gdiplus_ready_ = false;

  std::vector<std::wstring> items_;
  int highlighted_ = 0;
  int page_no_ = 0;
  bool is_last_page_ = true;
};