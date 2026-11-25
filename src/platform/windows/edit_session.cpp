#include "edit_session.h"
#include "text_service.h"

JianhuanEditSession::JianhuanEditSession(JianhuanTextService* service, ITfContext* context, WPARAM wparam, LPARAM lparam)
    : service_(service), context_(context), wparam_(wparam), lparam_(lparam) {
  if (context_) {
    context_->AddRef();
  }
}

STDMETHODIMP JianhuanEditSession::QueryInterface(REFIID riid, void** object) {
  if (!object) {
    return E_POINTER;
  }
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
    *object = static_cast<ITfEditSession*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) JianhuanEditSession::AddRef() {
  return InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) JianhuanEditSession::Release() {
  const LONG value = InterlockedDecrement(&ref_);
  if (value == 0) {
    if (context_) {
      context_->Release();
    }
    delete this;
  }
  return value;
}

STDMETHODIMP JianhuanEditSession::DoEditSession(TfEditCookie cookie) {
  return service_->HandleKey(cookie, context_, wparam_, lparam_);
}