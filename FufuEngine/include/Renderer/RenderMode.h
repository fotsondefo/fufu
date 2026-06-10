#pragma once

namespace Fufu 
{

	enum class RenderMode
	{
		Accumulation,  // Chaque frame affine l'image — qualité maximale
		Realtime       // Une passe par frame — interactivité maximale
	};

}