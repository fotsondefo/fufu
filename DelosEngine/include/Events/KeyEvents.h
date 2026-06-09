#pragma once
#include "Event.h"

namespace Delos 
{

	class KeyEvent : public Event
	{
	public:
		int getKeyCode() const { return m_KeyCode; }
		DELOS_EVENT_CATEGORY(EventCategoryInput | EventCategoryKeyboard)

	protected:
		explicit KeyEvent(int keyCode) : m_KeyCode(keyCode) {}
		int m_KeyCode;
	};

	class KeyPressedEvent : public KeyEvent
	{
	public:
		KeyPressedEvent(int keyCode, bool isRepeat = false)
			: KeyEvent(keyCode), m_IsRepeat(isRepeat) {}

		bool isRepeat() const { return m_IsRepeat; }

		DELOS_EVENT_TYPE(KeyPressed)

	private:
		bool m_IsRepeat;
	};

	class KeyReleasedEvent : public KeyEvent
	{
	public:
		explicit KeyReleasedEvent(int keyCode) : KeyEvent(keyCode) {}
		DELOS_EVENT_TYPE(KeyReleased)
	};

	class KeyTypedEvent : public KeyEvent
	{
	public:
		explicit KeyTypedEvent(int keyCode) : KeyEvent(keyCode) {}
		DELOS_EVENT_TYPE(KeyTyped)
	};

}