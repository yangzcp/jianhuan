#pragma once

#include <windows.h>
#include <unknwn.h>

HRESULT CreateTextServiceInstance(REFIID riid, void** object);

class JianhuanClassFactory : public IClassFactory {
 public:
  JianhuanClassFactory() = default;

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override;
  STDMETHODIMP LockServer(BOOL lock) override;

 private:
  LONG ref_ = 1;
};