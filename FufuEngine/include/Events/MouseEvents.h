#pragma once
#include "Event.h"

namespace Fufu 
{

	class MouseMovedEvent : public Event
	{
	public:
		MouseMovedEvent(float x, float y) : m_X(x), m_Y(y) {}

		float getX() const { return m_X; }
		float getY() const { return m_Y; }

		FUFU_EVENT_TYPE(MouseMoved)
			FUFU_EVENT_CATEGORY(EventCategoryInput | EventCategoryMouse)

	private:
		float m_X, m_Y;
	};

	class MouseScrolledEvent : public Event
	{
	public:
		MouseScrolledEvent(float xOffset, float yOffset)
			: m_XOffset(xOffset), m_YOffset(yOffset) {}

		float getXOffset() const { return m_XOffset; }
		float getYOffset() const { return m_YOffset; }

		FUFU_EVENT_TYPE(MouseScrolled)
			FUFU_EVENT_CATEGORY(EventCategoryInput | EventCategoryMouse)

	private:
		float m_XOffset, m_YOffset;
	};

	class MouseButtonEvent : public Event
	{
	public:
		int getButton() const { return m_Button; }
		FUFU_EVENT_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)

	protected:
		explicit MouseButtonEvent(int button) : m_Button(button) {}
		int m_Button;
	};

	class MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:
		explicit MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}
		FUFU_EVENT_TYPE(MouseButtonPressed)
	};

	class MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:
		explicit MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}
		FUFU_EVENT_TYPE(MouseButtonReleased)
	};

}