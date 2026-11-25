#pragma once

#include <windows.h>
#include <msctf.h>

class JianhuanTextService;

class JianhuanEditSession : public ITfEditSession {
 public:
  JianhuanEditSession(JianhuanTextService* service, ITfContext* context, WPARAM wparam, LPARAM lparam);

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP DoEditSession(TfEditCookie cookie) override;

 private:
  ~JianhuanEditSession() = default;
  LONG ref_ = 1;
  JianhuanTextService* service_;
  ITfContext* context_;
  WPARAM wparam_;
  LPARAM lparam_;
};