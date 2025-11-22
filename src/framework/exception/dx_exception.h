#pragma once

#include "base_exception.h"

#include <windows.h>
#include <comdef.h>
#include <string>
#include <sstream>
#include <iomanip>

namespace framework
{
    class DxException final : public BaseException
    {
    public:
        DxException(
            _In_z_ const char* file,
            _In_   int         line,
            _In_z_ const char* function,
            _In_   HRESULT     hr
        ) noexcept
            : BaseException(file, line, function, "DirectX call failed")
            , m_nErrorCode(hr)
        {
            BuildErrorMessage();
        }

        _NODISCARD _Ret_z_ _Ret_valid_ _Check_return_
            const char* what() const noexcept override
        {
            if (m_szWhatBuffer.empty())
            {
                std::ostringstream oss;

                oss << "[DxException] " << m_szErrorMessage
                    << "\n  HRESULT : 0x"
                    << std::hex << std::uppercase
                    << std::setw(8) << std::setfill('0')
                    << static_cast<unsigned long>(m_nErrorCode)
                    << std::dec
                    << "\n  File    : " << m_szFilePath
                    << "\n  Line    : " << m_nLineNumber
                    << "\n  Function: " << m_szFunctionName;

                m_szWhatBuffer = oss.str();
            }
            return m_szWhatBuffer.c_str();
        }

        _NODISCARD HRESULT GetErrorCode() const noexcept
        {
            return m_nErrorCode;
        }

    private:
        void BuildErrorMessage() noexcept
        {
            _com_error err(m_nErrorCode);
            const wchar_t* wideMsg = err.ErrorMessage();

            if (wideMsg != nullptr)
            {
                int requiredSize = ::WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    wideMsg,
                    -1,
                    nullptr,
                    0,
                    nullptr,
                    nullptr
                );

                if (requiredSize > 0)
                {
                    std::string utf8(static_cast<size_t>(requiredSize), '\0');

                    ::WideCharToMultiByte(
                        CP_UTF8,
                        0,
                        wideMsg,
                        -1,
                        utf8.data(),
                        requiredSize,
                        nullptr,
                        nullptr
                    );

                    if (!utf8.empty() && utf8.back() == '\0')
                        utf8.pop_back();

                    m_szErrorMessage = utf8;
                    return;
                }
            }
            m_szErrorMessage = "Unknown DirectX error.";
        }

    private:
        HRESULT m_nErrorCode{};
    };
} // namespace framework

#define THROW_DX_IF_FAILS(_hr_expr)                                  \
    do                                                               \
    {                                                                \
        const HRESULT _hr_internal_ = (_hr_expr);                    \
        if (FAILED(_hr_internal_))                                   \
        {                                                            \
            throw framework::DxException(                            \
                __FILE__,                                            \
                __LINE__,                                            \
                __FUNCTION__,                                        \
                _hr_internal_);                                      \
        }                                                            \
    } while (0)
