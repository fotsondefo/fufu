#pragma once

namespace Fufu 
{

	enum class RenderMode
	{
		Accumulation,  // Each frame refines the image — maximum quality
		Realtime       // One pass per frame — maximum interactivity
	};

}