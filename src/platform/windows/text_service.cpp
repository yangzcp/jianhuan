#include "text_service.h"

#include <objbase.h>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <new>
#include <string>
#include "edit_session.h"
#include "guids.h"
#include "jianhuan/key_policy.h"
#include "jianhuan/settings.h"
#include "langbar.h"

extern LONG g_dll_ref;
std::uint64_t g_current_session = 0;

namespace {

void TsfLog(const std::wstring& message) {
  wchar_t appdata[MAX_PATH] = {};
  GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
  const auto dir = std::filesystem::path(appdata) / "Jianhuan";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::wofstream log(dir / "tsf.log", std::ios::app);
  if (log.is_open()) {
    log << GetTickCount64() << L" " << message << L"\n";
  }
}

}  // namespace

namespace {

std::wstring ToWide(const std::string& text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
  return result;
}

std::string ToUtf8(const std::wstring& text) {
  if (text.empty()) return {};
  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length, nullptr, nullptr);
  return result;
}

bool IsPunctToggle(WPARAM wparam) {
  return wparam == VK_OEM_PERIOD && (GetKeyState(VK_CONTROL) & 0x8000);
}

bool MapKey(WPARAM wparam, int& keycode, int& mask) {
  const UINT vk = static_cast<UINT>(wparam);
  mask = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000) mask |= 1;
  if (GetKeyState(VK_CONTROL) & 0x8000) mask |= 4;
  if (GetKeyState(VK_MENU) & 0x8000) mask |= 8;

  switch (vk) {
    case VK_BACK: keycode = 0xff08; return true;
    case VK_TAB: keycode = 0xff09; return true;
    case VK_RETURN: keycode = 0xff0d; return true;
    case VK_ESCAPE: keycode = 0xff1b; return true;
    case VK_SPACE: keycode = 0x20; return true;
    case VK_DELETE: keycode = 0xffff; return true;
    case VK_HOME: keycode = 0xff50; return true;
    case VK_LEFT: keycode = 0xff51; return true;
    case VK_UP: keycode = 0xff52; return true;
    case VK_RIGHT: keycode = 0xff53; return true;
    case VK_DOWN: keycode = 0xff54; return true;
    case VK_PRIOR: keycode = 0xff55; return true;
    case VK_NEXT: keycode = 0xff56; return true;
    case VK_END: keycode = 0xff57; return true;
    case VK_INSERT: keycode = 0xff63; return true;
    default: break;
  }

  const UINT mapped = MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR);
  if (mapped == 0) {
    return false;
  }
  wchar_t ch = static_cast<wchar_t>(mapped & 0x7fff);
  if (ch >= L'A' && ch <= L'Z') {
    ch = static_cast<wchar_t>(ch - L'A' + L'a');
  }
  keycode = static_cast<int>(ch);
  return keycode >= 0x20 && keycode <= 0x7e;
}

}  // namespace

JianhuanTextService::JianhuanTextService() {
  InterlockedIncrement(&g_dll_ref);
}

JianhuanTextService::~JianhuanTextService() {
  if (composition_) {
    composition_->Release();
  }
  if (thread_mgr_) {
    thread_mgr_->Release();
  }
  InterlockedDecrement(&g_dll_ref);
}

STDMETHODIMP JianhuanTextService::QueryInterface(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor) {
    *object = static_cast<ITfTextInputProcessor*>(this);
  } else if (riid == IID_ITfKeyEventSink) {
    *object = static_cast<ITfKeyEventSink*>(this);
  } else if (riid == IID_ITfCompositionSink) {
    *object = static_cast<ITfCompositionSink*>(this);
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) JianhuanTextService::AddRef() {
  return InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) JianhuanTextService::Release() {
  const LONG value = InterlockedDecrement(&ref_);
  if (value == 0) {
    delete this;
  }
  return value;
}

bool JianhuanTextService::EnsureEngine() {
  auto& engine = jianhuan::Engine::instance();
  if (engine.initialized()) {
    return true;
  }
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&g_dll_ref), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  const std::filesystem::path dll_path(path);
  std::filesystem::path data_dir = dll_path.parent_path() / "data";
  std::ifstream pointer(dll_path.parent_path() / "active_data.txt");
  if (pointer.is_open()) {
    std::string name;
    std::getline(pointer, name);
    if (!name.empty()) {
      data_dir = dll_path.parent_path() / name;
    }
  }
  const std::string shared_dir = data_dir.string();

  wchar_t appdata[MAX_PATH] = {};
  GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
  const std::string user_dir = (std::filesystem::path(appdata) / "Jianhuan").string();
  return engine.initialize(shared_dir, user_dir);
}

STDMETHODIMP JianhuanTextService::Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) {
  if (!thread_mgr) return E_INVALIDARG;
  thread_mgr_ = thread_mgr;
  thread_mgr_->AddRef();
  client_id_ = client_id;

  if (!EnsureEngine()) {
    return E_FAIL;
  }
  session_ = jianhuan::Engine::instance().create_session();
  if (session_ == 0) {
    return E_FAIL;
  }

  jianhuan::Settings settings;
  jianhuan::load_settings(jianhuan::Engine::instance().user_data_dir(), settings);
  jianhuan::Engine::instance().select_schema(session_, settings.schema_id);
  jianhuan::Engine::instance().set_option(session_, "ascii_mode", settings.ascii_mode);
  jianhuan::Engine::instance().set_option(session_, "full_shape", settings.full_shape);
  jianhuan::Engine::instance().set_option(session_, "ascii_punct", settings.ascii_punct);
  ascii_mode_ = false;
  jianhuan::Engine::instance().set_option(session_, "ascii_mode", false);
  g_current_session = session_;
  TsfLog(L"Activate session=" + std::to_wstring(session_) + L" schema=" +
         std::wstring(settings.schema_id.begin(), settings.schema_id.end()));

  ITfLangBarItemMgr* langbar_mgr = nullptr;
  if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfLangBarItemMgr, reinterpret_cast<void**>(&langbar_mgr)))) {
    langbar_item_ = new (std::nothrow) JianhuanLangBarItem();
    if (langbar_item_) {
      langbar_mgr->AddItem(langbar_item_);
    }
    langbar_mgr->Release();
  }

  ITfKeystrokeMgr* keystroke = nullptr;
  if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystroke)))) {
    keystroke->AdviseKeyEventSink(client_id_, this, TRUE);
    keystroke->Release();
  }
  return S_OK;
}

STDMETHODIMP JianhuanTextService::Deactivate() {
  TsfLog(L"Deactivate");
  if (thread_mgr_) {
    ITfKeystrokeMgr* keystroke = nullptr;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keystroke)))) {
      keystroke->UnadviseKeyEventSink(client_id_);
      keystroke->Release();
    }
    if (langbar_item_) {
      ITfLangBarItemMgr* langbar_mgr = nullptr;
      if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfLangBarItemMgr, reinterpret_cast<void**>(&langbar_mgr)))) {
        langbar_mgr->RemoveItem(langbar_item_);
        langbar_mgr->Release();
      }
      langbar_item_->Release();
      langbar_item_ = nullptr;
    }
  }
  g_current_session = 0;
  if (session_) {
    jianhuan::Settings settings;
    jianhuan::load_settings(jianhuan::Engine::instance().user_data_dir(), settings);
    settings.ascii_mode = ascii_mode_;
    settings.schema_id = jianhuan::Engine::instance().current_schema(session_);
    jianhuan::save_settings(jianhuan::Engine::instance().user_data_dir(), settings);
    jianhuan::Engine::instance().destroy_session(session_);
    session_ = 0;
  }
  candidate_.Hide();
  composing_ = false;
  return S_OK;
}

STDMETHODIMP JianhuanTextService::OnSetFocus(BOOL) {
  return S_OK;
}

bool JianhuanTextService::ShouldEatKey(WPARAM wparam) const {
  const UINT vk = static_cast<UINT>(wparam);
  int keycode = 0;
  int mask = 0;
  MapKey(wparam, keycode, mask);
  return jianhuan::ShouldEatKeyPolicy(
      ascii_mode_, jianhuan::Engine::instance().is_composing(session_),
      jianhuan::Engine::instance().get_option(session_, "ascii_punct"),
      vk,
      (GetKeyState(VK_CONTROL) & 0x8000) != 0,
      (GetKeyState(VK_MENU) & 0x8000) != 0,
      (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0,
      keycode);
}

STDMETHODIMP JianhuanTextService::OnTestKeyDown(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
  if (!eaten) return E_INVALIDARG;
  *eaten = (!ascii_mode_ && IsPunctToggle(wparam)) || ShouldEatKey(wparam) ? TRUE : FALSE;
  TsfLog(L"TestKeyDown vk=" + std::to_wstring(wparam) + L" eaten=" + std::to_wstring(*eaten));
  return *eaten ? S_OK : S_FALSE;
}

STDMETHODIMP JianhuanTextService::OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) {
  if (!eaten) return E_INVALIDARG;
  *eaten = (!ascii_mode_ && IsPunctToggle(wparam)) || ShouldEatKey(wparam) ? TRUE : FALSE;
  if (!*eaten || !context) {
    return S_OK;
  }
  if (!ascii_mode_ && IsPunctToggle(wparam)) {
    auto& engine = jianhuan::Engine::instance();
    const bool value = !engine.get_option(session_, "ascii_punct");
    engine.set_option(session_, "ascii_punct", value);
    jianhuan::Settings settings;
    jianhuan::load_settings(engine.user_data_dir(), settings);
    settings.ascii_punct = value;
    jianhuan::save_settings(engine.user_data_dir(), settings);
    return S_OK;
  }
  if (!ascii_mode_ && jianhuan::Engine::instance().is_composing(session_) &&
      (wparam == VK_OEM_PLUS || wparam == VK_OEM_MINUS || wparam == VK_ADD || wparam == VK_SUBTRACT)) {
    const bool backward = (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT);
    jianhuan::Engine::instance().change_page(session_, backward);
    auto* refresh = new (std::nothrow) JianhuanEditSession(this, context, 0, 0);
    if (refresh) {
      HRESULT hr = S_OK;
      context->RequestEditSession(client_id_, refresh, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
      refresh->Release();
    }
    return S_OK;
  }
  auto* session = new (std::nothrow) JianhuanEditSession(this, context, wparam, lparam);
  if (!session) return E_OUTOFMEMORY;
  HRESULT hr = S_OK;
  context->RequestEditSession(client_id_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  session->Release();
  TsfLog(L"KeyDown vk=" + std::to_wstring(wparam) + L" edit_hr=" + std::to_wstring((long)hr));
  return *eaten ? S_OK : S_FALSE;
}

STDMETHODIMP JianhuanTextService::OnTestKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
  if (!eaten) return E_INVALIDARG;
  *eaten = (wparam == VK_SHIFT && !jianhuan::Engine::instance().is_composing(session_)) ? TRUE : FALSE;
  return S_OK;
}

STDMETHODIMP JianhuanTextService::OnKeyUp(ITfContext*, WPARAM wparam, LPARAM, BOOL* eaten) {
  if (!eaten) return E_INVALIDARG;
  *eaten = FALSE;
  if (wparam == VK_SHIFT && !jianhuan::Engine::instance().is_composing(session_)) {
    ascii_mode_ = !ascii_mode_;
    if (session_ != 0) {
      jianhuan::Engine::instance().set_option(session_, "ascii_mode", ascii_mode_);
    }
    *eaten = TRUE;
  }
  return S_OK;
}

STDMETHODIMP JianhuanTextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) {
  if (eaten) *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP JianhuanTextService::OnCompositionTerminated(TfEditCookie, ITfComposition* composition) {
  if (composition_ == composition) {
    composition_->Release();
    composition_ = nullptr;
  }
  candidate_.Hide();
  composing_ = false;
  return S_OK;
}

HRESULT JianhuanTextService::HandleKey(TfEditCookie cookie, ITfContext* context, WPARAM wparam, LPARAM) {
  TsfLog(L"HandleKey vk=" + std::to_wstring(wparam));
  if (!context || session_ == 0) return E_FAIL;
  if (wparam == 0) {
    jianhuan::Context engine_context;
    if (jianhuan::Engine::instance().get_context(session_, engine_context)) {
      UpdateComposition(cookie, context, engine_context);
    }
    return S_OK;
  }
  int keycode = 0;
  int mask = 0;
  if (!MapKey(wparam, keycode, mask)) {
    return S_FALSE;
  }

  auto& engine = jianhuan::Engine::instance();
  if (keycode >= '1' && keycode <= '9') {
    jianhuan::Context c;
    engine.get_context(session_, c);
    const std::string raw = engine.raw_input(session_);
    if (!raw.empty() && (keycode - '0') == static_cast<int>(c.candidates.size()) + 1) {
      InsertText(cookie, context, raw);
      engine.clear_composition(session_);
      EndComposition(cookie);
      candidate_.Hide();
      composing_ = false;
      return S_OK;
    }
  }
  engine.process_key(session_, keycode, mask);

  std::string commit;
  if (engine.get_commit(session_, commit)) {
    InsertText(cookie, context, commit);
    EndComposition(cookie);
    candidate_.Hide();
    composing_ = false;
  }

  jianhuan::Context engine_context;
  if (engine.get_context(session_, engine_context)) {
    UpdateComposition(cookie, context, engine_context);
  }
  return S_OK;
}

HRESULT JianhuanTextService::InsertText(TfEditCookie cookie, ITfContext* context, const std::string& text) {
  TF_SELECTION selection = {};
  ULONG count = 0;
  if (FAILED(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &count)) || count == 0) {
    return E_FAIL;
  }
  const std::wstring wide = ToWide(text);
  const HRESULT hr = selection.range->SetText(cookie, 0, wide.c_str(), static_cast<LONG>(wide.size()));
  selection.range->Release();
  return hr;
}

void JianhuanTextService::EndComposition(TfEditCookie cookie) {
  if (composition_) {
    composition_->EndComposition(cookie);
    composition_->Release();
    composition_ = nullptr;
  }
}

HRESULT JianhuanTextService::UpdateComposition(TfEditCookie cookie, ITfContext* context, const jianhuan::Context& engine_context) {
  if (engine_context.preedit.empty()) {
    EndComposition(cookie);
    candidate_.Hide();
    composing_ = false;
    return S_OK;
  }
  composing_ = true;

  if (!composition_) {
    ITfContextOwnerCompositionServices* services = nullptr;
    if (FAILED(context->QueryInterface(IID_ITfContextOwnerCompositionServices, reinterpret_cast<void**>(&services)))) {
      return E_FAIL;
    }
    TF_SELECTION selection = {};
    ULONG count = 0;
    ITfRange* start_range = nullptr;
    if (SUCCEEDED(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &selection, &count)) && count > 0) {
      start_range = selection.range;
    }
    services->StartComposition(cookie, start_range, this, &composition_);
    if (start_range) {
      start_range->Release();
    }
    services->Release();
  }
  if (!composition_) {
    return E_FAIL;
  }

  ITfRange* range = nullptr;
  if (FAILED(composition_->GetRange(&range)) || !range) {
    return E_FAIL;
  }
  const std::wstring preedit = ToWide(engine_context.preedit);
  range->SetText(cookie, 0, preedit.c_str(), static_cast<LONG>(preedit.size()));

  RECT rect = {};
  BOOL clipped = FALSE;
  bool have_rect = false;
  ITfContextView* view = nullptr;
  if (SUCCEEDED(context->GetActiveView(&view)) && view) {
    if (SUCCEEDED(view->GetTextExt(cookie, range, &rect, &clipped))) {
      have_rect = true;
    }
    view->Release();
  }
  if (!have_rect) {
    POINT cursor = {};
    GetCursorPos(&cursor);
    rect = {cursor.x, cursor.y, cursor.x, cursor.y + 20};
  }

  std::vector<std::string> candidates;
  for (const auto& candidate : engine_context.candidates) {
    candidates.push_back(candidate.text);
  }
  const std::string raw = jianhuan::Engine::instance().raw_input(session_);
  if (!raw.empty()) {
    candidates.push_back(raw);
  }
  candidate_.Show(rect.left, rect.bottom, candidates, engine_context.highlighted,
                  engine_context.page_no, engine_context.is_last_page, engine_context.preedit);
  TsfLog(L"Show candidates=" + std::to_wstring(candidates.size()) + L" preedit_len=" +
         std::to_wstring(engine_context.preedit.size()));
  range->Release();
  return S_OK;
}

HRESULT CreateTextServiceInstance(REFIID riid, void** object) {
  if (!object) return E_POINTER;
  *object = nullptr;
  auto* service = new (std::nothrow) JianhuanTextService();
  if (!service) return E_OUTOFMEMORY;
  const HRESULT hr = service->QueryInterface(riid, object);
  service->Release();
  return hr;
}