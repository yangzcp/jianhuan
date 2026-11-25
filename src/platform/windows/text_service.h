#pragma once

#include <windows.h>
#include <msctf.h>
#include <cstdint>
#include "candidate_window.h"
#include "jianhuan/engine.h"

class JianhuanTextService : public ITfTextInputProcessor,
                        public ITfKeyEventSink,
                        public ITfCompositionSink {
 public:
  JianhuanTextService();

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  STDMETHODIMP Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) override;
  STDMETHODIMP Deactivate() override;

  STDMETHODIMP OnSetFocus(BOOL foreground) override;
  STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
  STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
  STDMETHODIMP OnTestKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
  STDMETHODIMP OnKeyUp(ITfContext* context, WPARAM wparam, LPARAM lparam, BOOL* eaten) override;
  STDMETHODIMP OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;

  STDMETHODIMP OnCompositionTerminated(TfEditCookie cookie, ITfComposition* composition) override;

  HRESULT HandleKey(TfEditCookie cookie, ITfContext* context, WPARAM wparam, LPARAM lparam);

 private:
  ~JianhuanTextService();
  bool EnsureEngine();
  HRESULT UpdateComposition(TfEditCookie cookie, ITfContext* context, const jianhuan::Context& engine_context);
  HRESULT InsertText(TfEditCookie cookie, ITfContext* context, const std::string& text);
  void EndComposition(TfEditCookie cookie);
  bool ShouldEatKey(WPARAM wparam) const;

  LONG ref_ = 1;
  ITfThreadMgr* thread_mgr_ = nullptr;
  TfClientId client_id_ = 0;
  std::uint64_t session_ = 0;
  ITfComposition* composition_ = nullptr;
  ITfLangBarItemButton* langbar_item_ = nullptr;
  CandidateWindow candidate_;
  bool ascii_mode_ = false;
  bool composing_ = false;
};