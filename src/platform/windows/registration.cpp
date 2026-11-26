#include "registration.h"

#include <msctf.h>
#include <combaseapi.h>
#include <filesystem>
#include <string>
#include "guids.h"

namespace {

constexpr LANGID kLangId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
constexpr wchar_t kDescription[] = L"简焕";

std::wstring ClsidKeyPath(bool wow64) {
  wchar_t guid[64] = {};
  StringFromGUID2(CLSID_JianhuanTextService, guid, 64);
  const std::wstring root = wow64 ? L"Software\\Classes\\Wow6432Node\\CLSID\\"
                                  : L"Software\\Classes\\CLSID\\";
  return root + guid;
}

HRESULT WriteInprocServer(const std::wstring& key_path, const std::wstring& dll_path) {
  HKEY key = nullptr;
  LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, key_path.c_str(), 0, nullptr,
                                   REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }
  RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(L"Jianhuan Text Service"),
                 static_cast<DWORD>((wcslen(L"Jianhuan Text Service") + 1) * sizeof(wchar_t)));
  RegCloseKey(key);

  const std::wstring inproc_path = key_path + L"\\InprocServer32";
  status = RegCreateKeyExW(HKEY_CURRENT_USER, inproc_path.c_str(), 0, nullptr,
                           REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, nullptr);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }
  RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(dll_path.c_str()),
                 static_cast<DWORD>((dll_path.size() + 1) * sizeof(wchar_t)));
  RegSetValueExW(key, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(L"Apartment"),
                 static_cast<DWORD>((wcslen(L"Apartment") + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  return S_OK;
}

HRESULT RegisterComClass(const std::wstring& dll64, const std::wstring& dll32) {
  HRESULT hr = WriteInprocServer(ClsidKeyPath(false), dll64);
  if (FAILED(hr)) {
    return hr;
  }
  if (!dll32.empty()) {
    hr = WriteInprocServer(ClsidKeyPath(true), dll32);
  }
  return hr;
}

HRESULT RegisterComClass() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&UnregisterJianhuanServer), &module);
  wchar_t module_path[MAX_PATH] = {};
  GetModuleFileNameW(module, module_path, MAX_PATH);
  const auto dir = std::filesystem::path(module_path).parent_path();
  const auto dll64 = dir / "jianhuan_tsf.dll";
  const auto dll32 = dir / "jianhuan_tsf32.dll";
  return RegisterComClass(dll64.wstring(),
                          std::filesystem::exists(dll32) ? dll32.wstring() : std::wstring{});
}

void UnregisterComClass() {
  RegDeleteTreeW(HKEY_CURRENT_USER, ClsidKeyPath(false).c_str());
  RegDeleteTreeW(HKEY_CURRENT_USER, ClsidKeyPath(true).c_str());
}

}  // namespace

HRESULT RegisterProfiles() {
  ITfInputProcessorProfiles* profiles = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles));
  if (FAILED(hr)) {
    return hr;
  }

  hr = profiles->Register(CLSID_JianhuanTextService);
  if (SUCCEEDED(hr)) {
    // AddLanguageProfile updates an existing profile in place. Do NOT remove it
    // first: removal fails while the profile is actively in use, which would
    // break overwrite-install without switching IME away.
    hr = profiles->AddLanguageProfile(CLSID_JianhuanTextService, kLangId, GUID_JianhuanProfile,
                                      kDescription, static_cast<ULONG>(wcslen(kDescription)),
                                      nullptr, 0, 0);
  }
  if (SUCCEEDED(hr)) {
    profiles->EnableLanguageProfile(CLSID_JianhuanTextService, kLangId, GUID_JianhuanProfile, TRUE);
    profiles->SetDefaultLanguageProfile(kLangId, CLSID_JianhuanTextService, GUID_JianhuanProfile);
  }
  profiles->Release();
  if (FAILED(hr)) {
    return hr;
  }

  ITfCategoryMgr* categories = nullptr;
  hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                        IID_ITfCategoryMgr, reinterpret_cast<void**>(&categories));
  if (FAILED(hr)) {
    return hr;
  }
  hr = categories->RegisterCategory(CLSID_JianhuanTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_JianhuanTextService);
  categories->Release();
  return hr;
}

HRESULT RegisterJianhuanServer() {
  HRESULT hr = RegisterComClass();
  if (FAILED(hr)) return hr;
  return RegisterProfiles();
}

HRESULT RegisterJianhuanServer(const std::wstring& dll64, const std::wstring& dll32) {
  HRESULT hr = RegisterComClass(dll64, dll32);
  if (FAILED(hr)) return hr;
  return RegisterProfiles();
}

HRESULT UnregisterJianhuanServer() {
  ITfCategoryMgr* categories = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ITfCategoryMgr, reinterpret_cast<void**>(&categories));
  if (SUCCEEDED(hr)) {
    categories->UnregisterCategory(CLSID_JianhuanTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_JianhuanTextService);
    categories->Release();
  }

  ITfInputProcessorProfiles* profiles = nullptr;
  hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                        IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles));
  if (FAILED(hr)) {
    return hr;
  }
  profiles->RemoveLanguageProfile(CLSID_JianhuanTextService, kLangId, GUID_JianhuanProfile);
  hr = profiles->Unregister(CLSID_JianhuanTextService);
  profiles->Release();
  UnregisterComClass();
  return hr;
}