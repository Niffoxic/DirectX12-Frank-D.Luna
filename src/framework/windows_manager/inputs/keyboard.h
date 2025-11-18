#pragma once

#include <windows.h>
#include <initializer_list>
#include <cstdint>
#include <algorithm>
#include <sal.h>

namespace framework
{
	static constexpr unsigned int MAX_KEYBOARD_INPUTS = 256u;
#define _FOX_VK_VALID _In_range_(0, MAX_KEYBOARD_INPUTS - 1) _Valid_

	enum DxKeyboardMode : uint8_t
	{
		None  = 0,
		Ctrl  = 1,
		Shift = 1 << 1,
		Alt   = 1 << 2,
		Super = 1 << 3
	};

	class DxKeyboardInputs
	{
	public:
		DxKeyboardInputs () noexcept;
		~DxKeyboardInputs() noexcept = default;

		//~ No Copy or Move
		DxKeyboardInputs(_In_ const DxKeyboardInputs&) = delete;
		DxKeyboardInputs(_Inout_ DxKeyboardInputs&&)   = delete;

		DxKeyboardInputs& operator=(_In_ const DxKeyboardInputs&) = delete;
		DxKeyboardInputs& operator=(_Inout_ DxKeyboardInputs&&)   = delete;

		_NODISCARD _Check_return_ bool Initialize() noexcept;
		_NODISCARD _Check_return_ bool Release   () noexcept;

		_Check_return_ _NODISCARD bool ProcessMessage(
			_In_ UINT   message,
			_In_ WPARAM wParam,
			_In_ LPARAM lParam) noexcept;

		void OnFrameBegin(_In_ float deltaTime) noexcept;
		void OnFrameEnd  ()					    noexcept;

		//~ Queries
		_Check_return_ _NODISCARD bool IsKeyPressed  (_FOX_VK_VALID int virtualKey) const noexcept;
		_Check_return_ _NODISCARD bool WasKeyPressed (_FOX_VK_VALID int virtualKey) const noexcept;
		_Check_return_ _NODISCARD bool WasKeyReleased(_FOX_VK_VALID int virtualKey) const noexcept;

		_Check_return_ _NODISCARD bool WasChordPressed(
			_FOX_VK_VALID int key,
			_In_	const DxKeyboardMode& mode = DxKeyboardMode::None) const noexcept;

		_Check_return_ _NODISCARD bool WasMultipleKeyPressed(
			_In_reads_(keys.size()) std::initializer_list<int> keys) const noexcept;

	private:
		//~ Internal Helpers
		void ClearAll() noexcept;
		_Check_return_ _NODISCARD bool IsSetAutoRepeat(_In_ LPARAM lParam) noexcept;

		_Check_return_ _NODISCARD bool IsCtrlPressed () const noexcept;
		_Check_return_ _NODISCARD bool IsShiftPressed() const noexcept;
		_Check_return_ _NODISCARD bool IsAltPressed  () const noexcept;
		_Check_return_ _NODISCARD bool IsSuperPressed() const noexcept;

		_Check_return_ _NODISCARD bool IsInside(_FOX_VK_VALID int virtualKey) const noexcept { return virtualKey >= 0 && virtualKey < MAX_KEYBOARD_INPUTS; }

	private:
		bool m_keyDown	  [ MAX_KEYBOARD_INPUTS ];
		bool m_keyPressed [ MAX_KEYBOARD_INPUTS ];
		bool m_keyReleased[ MAX_KEYBOARD_INPUTS ];
	};
} // namespace framework
