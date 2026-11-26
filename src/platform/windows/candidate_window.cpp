#define NOMINMAX
#include "candidate_window.h"

#include <gdiplus.h>
#include <algorithm>

namespace {

constexpr wchar_t kClassName[] = L"JianhuanCandidateWindow";
constexpr int kPaddingX = 8;
constexpr int kItemHeight = 36;
constexpr int kItemPadding = 6;
constexpr int kNumGap = 2;
constexpr int kSpacing = 2;
constexpr float kRadius = 10.0f;

bool HasEmoji(const std::wstring& text) {
  for (wchar_t ch : text) {
    if (ch >= 0xD800 && ch <= 0xDFFF) return true;
    if (ch >= 0x2600 && ch <= 0x27BF) return true;
    if (ch >= 0xFE00 && ch <= 0xFE0F) return true;
  }
  return false;
}

std::wstring ToWide(const std::string& text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
  return result;
}

void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius) {
  const float d = radius * 2;
  path.AddArc(rect.X, rect.Y, d, d, 180, 90);
  path.AddArc(rect.GetRight() - d, rect.Y, d, d, 270, 90);
  path.AddArc(rect.GetRight() - d, rect.GetBottom() - d, d, d, 0, 90);
  path.AddArc(rect.X, rect.GetBottom() - d, d, d, 90, 90);
  path.CloseFigure();
}

}  // namespace

CandidateWindow::~CandidateWindow() {
  Hide();
  if (hwnd_) DestroyWindow(hwnd_);
  delete font_;
  delete emoji_font_;
  if (gdiplus_ready_) Gdiplus::GdiplusShutdown(gdiplus_token_);
}

void CandidateWindow::EnsureWindow() {
  if (hwnd_) return;
  WNDCLASSW wc = {};
  wc.lpfnWndProc = CandidateWindow::WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                          kClassName, L"", WS_POPUP, 0, 0, 1, 1,
                          nullptr, nullptr, wc.hInstance, this);
}

bool CandidateWindow::EnsureGdiplus() {
  if (gdiplus_ready_) return true;
  Gdiplus::GdiplusStartupInput input;
  gdiplus_ready_ = Gdiplus::GdiplusStartup(&gdiplus_token_, &input, nullptr) == Gdiplus::Ok;
  if (!gdiplus_ready_) return false;
  font_ = new Gdiplus::Font(L"Microsoft YaHei UI", 22, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  emoji_font_ = new Gdiplus::Font(L"Segoe UI Emoji", 22, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  const auto read_metrics = [](Gdiplus::Font* f, FontMetrics& m) {
    Gdiplus::FontFamily family;
    f->GetFamily(&family);
    m.em = static_cast<float>(family.GetEmHeight(Gdiplus::FontStyleRegular));
    m.ascent = static_cast<float>(family.GetCellAscent(Gdiplus::FontStyleRegular));
    m.descent = static_cast<float>(family.GetCellDescent(Gdiplus::FontStyleRegular));
    m.size = f->GetSize();
  };
  read_metrics(font_, font_m_);
  read_metrics(emoji_font_, emoji_m_);
  return true;
}

void CandidateWindow::Show(int x, int y, const std::vector<std::string>& items, int highlighted) {
  EnsureWindow();
  if (!EnsureGdiplus()) return;
  items_.clear();
  for (const auto& item : items) items_.push_back(ToWide(item));
  highlighted_ = highlighted;
  Render(x, y);
}

void CandidateWindow::Hide() {
  if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void CandidateWindow::Render(int x, int y) {
  HDC screen = GetDC(nullptr);
  Gdiplus::Graphics measure(screen);
  measure.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

  const std::size_t count = items_.size();
  std::vector<bool> emoji(count);
  std::vector<float> widths(count);
  std::vector<float> num_widths(count);
  float items_width = 0;
  for (std::size_t i = 0; i < count; ++i) {
    emoji[i] = HasEmoji(items_[i]);
    const std::wstring num = std::to_wstring(i + 1);
    Gdiplus::RectF num_bounds;
    measure.MeasureString(num.c_str(), static_cast<INT>(num.size()), font_, Gdiplus::PointF(0, 0), &num_bounds);
    Gdiplus::RectF word_bounds;
    measure.MeasureString(items_[i].c_str(), static_cast<INT>(items_[i].size()),
                          emoji[i] ? emoji_font_ : font_, Gdiplus::PointF(0, 0), &word_bounds);
    widths[i] = num_bounds.Width + kNumGap + word_bounds.Width + kItemPadding * 2;
    num_widths[i] = num_bounds.Width;
    items_width += widths[i] + kSpacing;
  }
  const int width = static_cast<int>(std::max(items_width, 40.0f)) + kPaddingX * 2;
  const int height = kItemHeight + 12;
  x = std::max(0, std::min(x, GetSystemMetrics(SM_CXSCREEN) - width));
  y = std::max(0, std::min(y, GetSystemMetrics(SM_CYSCREEN) - height));

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC memory = CreateCompatibleDC(screen);
  HBITMAP bitmap = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ old = SelectObject(memory, bitmap);

  Gdiplus::Graphics graphics(memory);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
  graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

  Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255));
  Gdiplus::Pen border(Gdiplus::Color(140, 140, 240), 2.0f);
  Gdiplus::GraphicsPath background;
  AddRoundedRect(background, Gdiplus::RectF(1.0f, 1.0f, width - 2.0f, height - 2.0f), kRadius);
  graphics.FillPath(&white, &background);
  graphics.DrawPath(&border, &background);

  float cursor_x = kPaddingX;
  const float cursor_y = 6;
  const float center_y = cursor_y + kItemHeight / 2.0f;
  for (std::size_t i = 0; i < count; ++i) {
    const Gdiplus::RectF item_rect(cursor_x, cursor_y, widths[i], static_cast<float>(kItemHeight));
    Gdiplus::SolidBrush text_brush(Gdiplus::Color(45, 160, 60));
    if (static_cast<int>(i) == highlighted_) {
      Gdiplus::SolidBrush highlight(Gdiplus::Color(205, 205, 250));
      Gdiplus::GraphicsPath pill;
      AddRoundedRect(pill, item_rect, 8.0f);
      graphics.FillPath(&highlight, &pill);
      text_brush.SetColor(Gdiplus::Color(105, 105, 215));
    }
    const float text_x = item_rect.X + kItemPadding;
    const std::wstring num = std::to_wstring(i + 1);
    graphics.DrawString(num.c_str(), static_cast<INT>(num.size()), font_,
                        Gdiplus::PointF(text_x, font_m_.MidlineY(center_y)), &text_brush);
    Gdiplus::Font* word_font = emoji[i] ? emoji_font_ : font_;
    const FontMetrics& m = emoji[i] ? emoji_m_ : font_m_;
    const float word_y = m.MidlineY(center_y) + (emoji[i] ? -2.0f : 0.0f);
    graphics.DrawString(items_[i].c_str(), static_cast<INT>(items_[i].size()), word_font,
                        Gdiplus::PointF(text_x + num_widths[i] + kNumGap, word_y), &text_brush);
    cursor_x += widths[i] + kSpacing;
  }

  POINT position = {x, y};
  SIZE size = {width, height};
  POINT source = {0, 0};
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(hwnd_, screen, &position, &size, memory, &source, 0, &blend, ULW_ALPHA);
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

  SelectObject(memory, old);
  DeleteObject(bitmap);
  DeleteDC(memory);
  ReleaseDC(nullptr, screen);
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  return DefWindowProcW(hwnd, message, wparam, lparam);
}
