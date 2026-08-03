#pragma once

namespace Fufu
{
	enum class LengthUnit { Meters, Centimeters, Millimeters };

	struct WorldSettings
	{
		LengthUnit lengthUnit = LengthUnit::Meters;

		float unitsPerMeter() const
		{
			switch (lengthUnit)
			{
			case LengthUnit::Centimeters: return 100.f;
			case LengthUnit::Millimeters: return 1000.f;
			default: return 1.f;
			}
		}

		const char* suffix() const
		{
			switch (lengthUnit)
			{
			case LengthUnit::Centimeters: return "cm";
			case LengthUnit::Millimeters: return "mm";
			default: return "m";
			}
		}

		// Drag step for position widgets in the current unit
		float dragStep() const { return unitsPerMeter() * 0.01f; }

		float toDisplay(float meters) const { return meters * unitsPerMeter(); }
		float fromDisplay(float displayVal) const { return displayVal / unitsPerMeter(); }
	};

} // namespace Fufu
