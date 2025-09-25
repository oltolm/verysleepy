#pragma once

#include <wx/colour.h>
#include <wx/settings.h>


inline wxColour complimentary(wxColour c)
{
	return wxColour(255 - c.Red(), 255 - c.Green(), 255 - c.Blue());
}

inline wxColour lightOrDark(wxColour lightColour)
{
	return wxSystemSettings::SelectLightDark(lightColour, complimentary(lightColour));
}
