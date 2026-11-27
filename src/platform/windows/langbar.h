#pragma once

#include <windows.h>
#include <msctf.h>

class JianhuanLangBarItem : public ITfLangBarItemButton {
 public:
  JianhuanLangBarItem();

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* info) override;
  STDMETHODIMP GetStatus(DWORD* status) override;
  STDMETHODIMP Show(BOOL show) override;
  STDMETHODIMP GetTooltipString(BSTR* text) override;
  STDMETHODIMP GetIcon(HICON* icon) override;
  STDMETHODIMP GetText(BSTR* text) override;
  STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* area) override;
  STDMETHODIMP InitMenu(ITfMenu* menu) override;
  STDMETHODIMP OnMenuSelect(UINT id) override;

 private:
  ~JianhuanLangBarItem();
  void ApplyCommand(UINT id);
  LONG ref_ = 1;
  HICON icon_ = nullptr;
};