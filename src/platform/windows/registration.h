#pragma once

#include <windows.h>
#include <string>

HRESULT RegisterJianhuanServer();
HRESULT RegisterJianhuanServer(const std::wstring& dll64, const std::wstring& dll32);
HRESULT UnregisterJianhuanServer();