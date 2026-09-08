#pragma once

#include <wx/colour.h>
#include <wx/settings.h>

#include <algorithm>

// Flips how light a colour is without touching its hue, by shifting every channel by
// the same amount: black becomes white and silver becomes charcoal, but green stays
// green. Inverting each channel instead would rotate the hue half a turn as well.
inline wxColour invertLightness(const wxColour& c)
{
	const int lo = std::min({(int)c.Red(), (int)c.Green(), (int)c.Blue()});
	const int hi = std::max({(int)c.Red(), (int)c.Green(), (int)c.Blue()});
	const int shift = 255 - lo - hi;

	return wxColour((unsigned char)(c.Red() + shift), (unsigned char)(c.Green() + shift),
					(unsigned char)(c.Blue() + shift), c.Alpha());
}

inline wxColour themed(const wxColour& lightThemeColour)
{
	return wxSystemSettings::SelectLightDark(lightThemeColour, invertLightness(lightThemeColour));
}

// For colours the flip cannot help: a fully saturated one is already mid lightness, so
// it comes back unchanged and has to be chosen by hand instead.
inline wxColour themed(const wxColour& lightThemeColour, const wxColour& darkThemeColour)
{
	return wxSystemSettings::SelectLightDark(lightThemeColour, darkThemeColour);
}

// Stands in for a yellow highlight, which stays glaring when its lightness is flipped.
inline wxColour darkHighlight()
{
	return wxColour(96, 80, 0);
}
