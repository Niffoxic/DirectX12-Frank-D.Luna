#pragma once

#include "base_exception.h"
#include <windows.h>
#include <comdef.h>

namespace framework
{
	class DxException final : public BaseException
	{
	public:
		DxException(
			_In_z_ const char* file,
			_In_   int			line,
			_In_z_ const char* function,
			_In_   HRESULT		hr
		)
			:	BaseException(file, line, function, "None"),
				m_nErrorCode(hr)
		{
			_com_error err(m_nErrorCode);
			int size = WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1, nullptr, 0, nullptr, nullptr);
			std::string ansi(size, 0);
			WideCharToMultiByte(CP_UTF8, 0, msg.c_str(), -1, ansi.data(), size, nullptr, nullptr);
			m_szErrorMessage = ansi.c_str();
		}

		_NODISCARD _Ret_z_ _Ret_valid_ _Check_return_
		const char* what() const noexcept override
		{
			if (m_szWhatBuffer.empty())
			{
				m_szWhatBuffer =
					"[WinException] "	 + m_szErrorMessage				 +
					"\nOn File Path: "	 + m_szFilePath					 +
					"\nAt Line Number: " + std::to_string(m_nLineNumber) +
					"\nFunction: "		 + m_szFunctionName;
			}
			return m_szWhatBuffer.c_str();
		}

	private:
		HRESULT m_nErrorCode{};
	};
} // namespace framework

#define THROW_DX_IF_FAILS(_hr_expr) \
    do \
	{ \
		HRESULT _hr_internal = (_hr_expr); \
		if (FAILED(_hr_internal)) \
		{ \
			throw framework::DxException(__FILE__, __LINE__, __FUNCTION__, _hr_internal); \
    } } while(0)
