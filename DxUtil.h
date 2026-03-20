#pragma once

#include <windows.h>
#include <tchar.h>
#include <comdef.h>
#include <stdexcept>
#include <string>

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        _com_error err(hr);
        const TCHAR* tmsg = err.ErrorMessage();

#ifdef UNICODE
        std::wstring text = L"HRESULT failed: 0x" + std::to_wstring(static_cast<unsigned long>(hr));
        if (tmsg)
        {
            text += L" - ";
            text += tmsg; // TCHAR == wchar_t
        }
        throw std::runtime_error(std::string(text.begin(), text.end()));
#else
        std::string text = "HRESULT failed: 0x" + std::to_string(static_cast<unsigned long>(hr));
        if (tmsg)
        {
            text += " - ";
            text += tmsg; // TCHAR == char
        }
        throw std::runtime_error(text);
#endif
    }
}