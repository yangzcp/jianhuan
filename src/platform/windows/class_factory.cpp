#include "class_factory.h"

extern LONG g_dll_ref;

STDMETHODIMP JianhuanClassFactory::QueryInterface(REFIID riid, void** object) {
  if (!object) {
    return E_POINTER;
  }
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_IClassFactory) {
    *object = static_cast<IClassFactory*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) JianhuanClassFactory::AddRef() {
  return InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) JianhuanClassFactory::Release() {
  const LONG value = InterlockedDecrement(&ref_);
  if (value == 0) {
    delete this;
  }
  return value;
}

STDMETHODIMP JianhuanClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** object) {
  if (!object) {
    return E_POINTER;
  }
  *object = nullptr;
  if (outer) {
    return CLASS_E_NOAGGREGATION;
  }
  return CreateTextServiceInstance(riid, object);
}

STDMETHODIMP JianhuanClassFactory::LockServer(BOOL lock) {
  if (lock) {
    InterlockedIncrement(&g_dll_ref);
  } else {
    InterlockedDecrement(&g_dll_ref);
  }
  return S_OK;
}