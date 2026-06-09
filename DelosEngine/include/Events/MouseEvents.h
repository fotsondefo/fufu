#pragma once
#include "Event.h"

namespace Delos 
{

	class MouseMovedEvent : public Event
	{
	public:
		MouseMovedEvent(float x, float y) : m_X(x), m_Y(y) {}

		float getX() const { return m_X; }
		float getY() const { return m_Y; }

		DELOS_EVENT_TYPE(MouseMoved)
			DELOS_EVENT_CATEGORY(EventCategoryInput | EventCategoryMouse)

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

		DELOS_EVENT_TYPE(MouseScrolled)
			DELOS_EVENT_CATEGORY(EventCategoryInput | EventCategoryMouse)

	private:
		float m_XOffset, m_YOffset;
	};

	class MouseButtonEvent : public Event
	{
	public:
		int getButton() const { return m_Button; }
		DELOS_EVENT_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)

	protected:
		explicit MouseButtonEvent(int button) : m_Button(button) {}
		int m_Button;
	};

	class MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:
		explicit MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}
		DELOS_EVENT_TYPE(MouseButtonPressed)
	};

	class MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:
		explicit MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}
		DELOS_EVENT_TYPE(MouseButtonReleased)
	};

}