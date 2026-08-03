#include "depch.h"
#include "Project/Entity.h"
#include "Project/Scene/Scene.h"

namespace Fufu 
{

	bool Entity::isValid() const
	{
		return m_Scene && m_Scene->m_Registry.valid(m_Handle);
	}

}