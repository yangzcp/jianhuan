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
  return gdiplus_ready_;
}

std::wstring CandidateWindow::PageLabel() const {
  return L"";
}

void CandidateWindow::Show(int x, int y, const std::vector<std::string>& items, int highlighted,
                           int page_no, bool is_last_page, const std::string& preedit) {
  (void)preedit;
  EnsureWindow();
  if (!EnsureGdiplus()) return;
  items_.clear();
  for (const auto& item : items) items_.push_back(ToWide(item));
  highlighted_ = highlighted;
  page_no_ = page_no;
  is_last_page_ = is_last_page;
  Render(x, y);
}

void CandidateWindow::Hide() {
  if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void CandidateWindow::Render(int x, int y) {
  HDC screen = GetDC(nullptr);
  Gdiplus::Graphics measure(screen);
  Gdiplus::Font font(L"Microsoft YaHei UI", 22, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  Gdiplus::Font emoji_font(L"Segoe UI Emoji", 22, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  Gdiplus::Font small_font(L"Microsoft YaHei UI", 16, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  measure.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

  std::vector<float> widths;
  std::vector<float> num_widths;
  float items_width = 0;
  for (std::size_t i = 0; i < items_.size(); ++i) {
    const std::wstring num = std::to_wstring(i + 1);
    Gdiplus::Font* word_font = HasEmoji(items_[i]) ? &emoji_font : &font;
    Gdiplus::RectF num_bounds;
    measure.MeasureString(num.c_str(), static_cast<INT>(num.size()), &font, Gdiplus::PointF(0, 0), &num_bounds);
    Gdiplus::RectF word_bounds;
    measure.MeasureString(items_[i].c_str(), static_cast<INT>(items_[i].size()), word_font, Gdiplus::PointF(0, 0), &word_bounds);
    const float width = num_bounds.Width + kNumGap + word_bounds.Width + kItemPadding * 2;
    widths.push_back(width);
    num_widths.push_back(num_bounds.Width);
    items_width += width + kSpacing;
  }
  const std::wstring page = PageLabel();
  Gdiplus::RectF page_bounds;
  measure.MeasureString(page.c_str(), static_cast<INT>(page.size()), &small_font, Gdiplus::PointF(0, 0), &page_bounds);
  const float page_width = page.empty() ? 0.0f : page_bounds.Width + 8;
  const float content_width = std::max(items_width + (page.empty() ? 0.0f : page_width + 12), 40.0f);
  const int width = static_cast<int>(content_width) + kPaddingX * 2;
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
  for (std::size_t i = 0; i < items_.size(); ++i) {
    const std::wstring label = std::to_wstring(i + 1) + L" " + items_[i];
    const Gdiplus::RectF item_rect(cursor_x, cursor_y, widths[i], static_cast<float>(kItemHeight));
    Gdiplus::SolidBrush text_brush(Gdiplus::Color(45, 160, 60));
    if (static_cast<int>(i) == highlighted_) {
      Gdiplus::SolidBrush highlight(Gdiplus::Color(205, 205, 250));
      Gdiplus::GraphicsPath pill;
      AddRoundedRect(pill, item_rect, 8.0f);
      graphics.FillPath(&highlight, &pill);
      text_brush.SetColor(Gdiplus::Color(105, 105, 215));
    }
    Gdiplus::RectF text_rect = item_rect;
    text_rect.X += kItemPadding;
    text_rect.Width -= kItemPadding * 2;
    Gdiplus::Font* word_font = HasEmoji(items_[i]) ? &emoji_font : &font;
    const float center_y = item_rect.Y + kItemHeight / 2.0f;
    auto draw_y = [&](Gdiplus::Font* f) {
      Gdiplus::FontFamily family;
      f->GetFamily(&family);
      const float em = static_cast<float>(family.GetEmHeight(Gdiplus::FontStyleRegular));
      const float asc = static_cast<float>(family.GetCellAscent(Gdiplus::FontStyleRegular));
      const float desc = static_cast<float>(family.GetCellDescent(Gdiplus::FontStyleRegular));
      const float fs = f->GetSize();
      return center_y - (asc + desc) * fs / em / 2.0f;
    };
    const std::wstring num = std::to_wstring(i + 1);
    graphics.DrawString(num.c_str(), static_cast<INT>(num.size()), &font,
                        Gdiplus::PointF(text_rect.X, draw_y(&font)), &text_brush);
    const float word_y = draw_y(word_font) + (word_font == &emoji_font ? -2.0f : 0.0f);
    graphics.DrawString(items_[i].c_str(), static_cast<INT>(items_[i].size()), word_font,
                        Gdiplus::PointF(text_rect.X + num_widths[i] + kNumGap, word_y), &text_brush);
    cursor_x += widths[i] + kSpacing;
  }

  Gdiplus::SolidBrush gray(Gdiplus::Color(120, 130, 140));
  graphics.DrawString(page.c_str(), static_cast<INT>(page.size()), &small_font,
                      Gdiplus::PointF(width - kPaddingX - page_width, cursor_y + 8), &gray);

  POINT position = {x, y};
  SIZE size = {width, height};
  POINT source = {0, 0};
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  const BOOL ok = UpdateLayeredWindow(hwnd_, screen, &position, &size, memory, &source, 0, &blend, ULW_ALPHA);
  (void)ok;
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
