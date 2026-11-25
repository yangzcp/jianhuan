#include <windows.h>
#include <msctf.h>
#include <iostream>
#include <string>
#include "registration.h"

namespace {

std::wstring GetArg(int argc, wchar_t** argv, const wchar_t* prefix) {
  const std::wstring key = prefix;
  for (int i = 1; i < argc; ++i) {
    const std::wstring arg = argv[i];
    if (arg.rfind(key, 0) == 0) {
      return arg.substr(key.size());
    }
  }
  return {};
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 2) {
    std::wcerr << L"usage: jianhuan_register /register|/unregister [/DLL64=path /DLL32=path]\n";
    return 1;
  }
  const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(co)) {
    return 1;
  }
  HRESULT hr = E_FAIL;
  if (wcscmp(argv[1], L"/register") == 0) {
    const std::wstring dll64 = GetArg(argc, argv, L"/DLL64=");
    const std::wstring dll32 = GetArg(argc, argv, L"/DLL32=");
    if (!dll64.empty()) {
      hr = RegisterJianhuanServer(dll64, dll32);
    } else {
      hr = RegisterJianhuanServer();
    }
  } else if (wcscmp(argv[1], L"/unregister") == 0) {
    hr = UnregisterJianhuanServer();
  }
  CoUninitialize();
  if (FAILED(hr)) {
    std::wcerr << L"operation failed: 0x" << std::hex << hr << L"\n";
    return 1;
  }
  std::wcout << L"ok\n";
  return 0;
}