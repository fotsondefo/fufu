#pragma once

namespace Delos 
{
	enum class EventType
	{
		None = 0,
		
		WindowClose, 
		WindowResize, 
		WindowFocus, 
		WindowLostFocus,

		KeyPressed, 
		KeyReleased, 
		KeyTyped,
		
		MouseButtonPressed, 
		MouseButtonReleased, 
		MouseMoved, 
		MouseScrolled
	};

	enum EventCategory : int
	{
		EventCategoryNone = 0,
		EventCategoryApplication = 1 << 0,
		EventCategoryInput = 1 << 1,
		EventCategoryKeyboard = 1 << 2,
		EventCategoryMouse = 1 << 3,
		EventCategoryMouseButton = 1 << 4
	};

	// Macros pour éviter le boilerplate dans chaque sous-classe
#define DELOS_EVENT_TYPE(type) \
    static EventType getStaticType() { return EventType::type; } \
    virtual EventType getEventType() const override { return getStaticType(); } \
    virtual const char* getName() const override { return #type; }

#define DELOS_EVENT_CATEGORY(category) \
    virtual int getCategoryFlags() const override { return category; }

	class Event
	{
	public:
		virtual ~Event() = default;

		virtual EventType getEventType() const = 0;
		virtual const char* getName() const = 0;
		virtual int getCategoryFlags() const = 0;
		virtual std::string toString() const { return getName(); }

		bool isInCategory(EventCategory category) const
		{
			return getCategoryFlags() & category;
		}

		bool handled = false;
	};

	class EventDispatcher
	{
	public:
		explicit EventDispatcher(Event& event) : m_Event(event) {}

		template<typename T, typename F>
		bool dispatch(const F& func)
		{
			if (m_Event.getEventType() == T::getStaticType())
			{
				m_Event.handled |= func(static_cast<T&>(m_Event));
				return true;
			}
			return false;
		}

	private:
		Event& m_Event;
	};
}