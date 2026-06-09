#pragma once
#include "Event.h"

namespace Delos {

	class WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() = default;

		DELOS_EVENT_TYPE(WindowClose)
			DELOS_EVENT_CATEGORY(EventCategoryApplication)
	};

	class WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(unsigned int width, unsigned int height)
			: m_Width(width), m_Height(height) {}

		unsigned int getWidth()  const { return m_Width; }
		unsigned int getHeight() const { return m_Height; }

		std::string toString() const override
		{
			return "WindowResizeEvent: " + std::to_string(m_Width) + "x" + std::to_string(m_Height);
		}

		DELOS_EVENT_TYPE(WindowResize)
			DELOS_EVENT_CATEGORY(EventCategoryApplication)

	private:
		unsigned int m_Width, m_Height;
	};

}