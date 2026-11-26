#include "langbar.h"

#include <shellapi.h>
#include <filesystem>
#include "guids.h"
#include "jianhuan/engine.h"
#include "jianhuan/settings.h"

extern std::uint64_t g_current_session;

namespace {
int langbar_anchor = 0;
constexpr UINT kCmdPinyin = 1;
constexpr UINT kCmdDouble = 2;
constexpr UINT kCmdPunct = 3;
constexpr UINT kCmdFull = 4;
constexpr UINT kCmdSettings = 5;
constexpr UINT kCmdAbout = 6;
constexpr UINT kCmdAscii = 7;
}  // namespace

JianhuanLangBarItem::JianhuanLangBarItem() = default;
JianhuanLangBarItem::~JianhuanLangBarItem() = default;

STDMETHODIMP JianhuanLangBarItem::QueryInterface(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_ITfLangBarItem || riid == IID_ITfLangBarItemButton) {
    *object = static_cast<ITfLangBarItemButton*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) JianhuanLangBarItem::AddRef() {
  return InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) JianhuanLangBarItem::Release() {
  const LONG value = InterlockedDecrement(&ref_);
  if (value == 0) delete this;
  return value;
}

STDMETHODIMP JianhuanLangBarItem::GetInfo(TF_LANGBARITEMINFO* info) {
  if (!info) return E_INVALIDARG;
  info->clsidService = CLSID_JianhuanTextService;
  info->guidItem = GUID_JianhuanLangBarItem;
  info->dwStyle = TF_LBI_STYLE_BTN_BUTTON;
  info->ulSort = 0;
  wcscpy_s(info->szDescription, L"简焕");
  return S_OK;
}

STDMETHODIMP JianhuanLangBarItem::GetStatus(DWORD* status) {
  if (!status) return E_INVALIDARG;
  *status = 0;
  return S_OK;
}

STDMETHODIMP JianhuanLangBarItem::Show(BOOL) {
  return S_OK;
}

STDMETHODIMP JianhuanLangBarItem::GetTooltipString(BSTR* text) {
  if (!text) return E_INVALIDARG;
  *text = SysAllocString(L"简焕输入法");
  return *text ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP JianhuanLangBarItem::GetIcon(HICON* icon) {
  if (!icon) return E_INVALIDARG;
  *icon = nullptr;
  return S_OK;
}

STDMETHODIMP JianhuanLangBarItem::GetText(BSTR* text) {
  if (!text) return E_INVALIDARG;
  const bool ascii = jianhuan::Engine::instance().get_option(g_current_session, "ascii_mode");
  *text = SysAllocString(ascii ? L"简焕·英" : L"简焕·中");
  return *text ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP JianhuanLangBarItem::OnClick(TfLBIClick, POINT pt, const RECT*) {
  auto& engine = jianhuan::Engine::instance();
  HMENU menu = CreatePopupMenu();
  const std::string schema = engine.current_schema(g_current_session);
  AppendMenuW(menu, MF_STRING | (schema == "jianhuan_pinyin" ? MF_CHECKED : 0), kCmdPinyin, L"全拼");
  AppendMenuW(menu, MF_STRING | (schema == "jianhuan_double_pinyin" ? MF_CHECKED : 0), kCmdDouble, L"双拼");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (engine.get_option(g_current_session, "ascii_punct") ? MF_CHECKED : 0),
              kCmdPunct, L"英文标点");
  AppendMenuW(menu, MF_STRING | (engine.get_option(g_current_session, "full_shape") ? MF_CHECKED : 0),
              kCmdFull, L"全角");
  AppendMenuW(menu, MF_STRING | (engine.get_option(g_current_session, "ascii_mode") ? MF_CHECKED : 0),
              kCmdAscii, L"英文模式");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCmdSettings, L"设置…");
  AppendMenuW(menu, MF_STRING, kCmdAbout, L"关于简焕");
  const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                  pt.x, pt.y, 0, GetForegroundWindow(), nullptr);
  DestroyMenu(menu);
  if (cmd) ApplyCommand(cmd);
  return S_OK;
}

STDMETHODIMP JianhuanLangBarItem::InitMenu(ITfMenu*) {
  return E_NOTIMPL;
}

STDMETHODIMP JianhuanLangBarItem::OnMenuSelect(UINT id) {
  ApplyCommand(id);
  return S_OK;
}

void JianhuanLangBarItem::ApplyCommand(UINT id) {
  auto& engine = jianhuan::Engine::instance();
  jianhuan::Settings settings;
  jianhuan::load_settings(engine.user_data_dir(), settings);
  switch (id) {
    case kCmdPinyin:
      engine.select_schema(g_current_session, "jianhuan_pinyin");
      settings.schema_id = "jianhuan_pinyin";
      jianhuan::save_settings(engine.user_data_dir(), settings);
      break;
    case kCmdDouble:
      engine.select_schema(g_current_session, "jianhuan_double_pinyin");
      settings.schema_id = "jianhuan_double_pinyin";
      jianhuan::save_settings(engine.user_data_dir(), settings);
      break;
    case kCmdPunct: {
      const bool value = !engine.get_option(g_current_session, "ascii_punct");
      engine.set_option(g_current_session, "ascii_punct", value);
      settings.ascii_punct = value;
      jianhuan::save_settings(engine.user_data_dir(), settings);
      break;
    }
    case kCmdFull: {
      const bool value = !engine.get_option(g_current_session, "full_shape");
      engine.set_option(g_current_session, "full_shape", value);
      settings.full_shape = value;
      jianhuan::save_settings(engine.user_data_dir(), settings);
      break;
    }
    case kCmdAscii: {
      const bool value = !engine.get_option(g_current_session, "ascii_mode");
      engine.set_option(g_current_session, "ascii_mode", value);
      settings.ascii_mode = value;
      jianhuan::save_settings(engine.user_data_dir(), settings);
      break;
    }
    case kCmdSettings: {
      HMODULE module = nullptr;
      GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&langbar_anchor), &module);
      wchar_t path[MAX_PATH] = {};
      GetModuleFileNameW(module, path, MAX_PATH);
      const auto exe = std::filesystem::path(path).parent_path() / "jianhuan_settings.exe";
      ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      break;
    }
    case kCmdAbout:
      MessageBoxW(nullptr, L"简焕输入法\n本地离线 · 可闭源商用", L"关于简焕", MB_OK | MB_ICONINFORMATION);
      break;
    default:
      break;
  }
}