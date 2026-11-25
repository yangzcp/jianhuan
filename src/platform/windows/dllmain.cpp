#include <windows.h>
#include <new>
#include "class_factory.h"
#include "guids.h"
#include "registration.h"

HINSTANCE g_instance = nullptr;
LONG g_dll_ref = 0;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_instance = instance;
    DisableThreadLibraryCalls(instance);
  }
  return TRUE;
}

extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE JianhuanDllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* object) {
  if (!object) {
    return E_POINTER;
  }
  *object = nullptr;
  if (clsid != CLSID_JianhuanTextService) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }
  auto* factory = new (std::nothrow) JianhuanClassFactory();
  if (!factory) {
    return E_OUTOFMEMORY;
  }
  const HRESULT hr = factory->QueryInterface(riid, object);
  factory->Release();
  return hr;
}

extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE JianhuanDllCanUnloadNow() {
  return g_dll_ref == 0 ? S_OK : S_FALSE;
}

extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE JianhuanDllRegisterServer() {
  return RegisterJianhuanServer();
}

extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE JianhuanDllUnregisterServer() {
  return UnregisterJianhuanServer();
}