#pragma once

#include <wx/colour.h>
#include <wx/settings.h>

// Every colour the app picks for itself, as a light/dark pair. The values are
// VS Code's Light Modern and Dark Modern defaults; each one names the theme key
// it came from, so it can be looked up again.
namespace palette
{

inline wxColour pick(const wxColour& light, const wxColour& dark)
{
	return wxSystemSettings::SelectLightDark(light, dark);
}

// Collapsed functions and modules in the lists, and the thread in focus.
// chat.slashCommandForeground
inline wxColour collapsed()
{
	return pick(wxColour(0x26, 0x56, 0x9E), wxColour(0x85, 0xB6, 0xFF));
}

// Rows with no samples at all.
// tab.inactiveForeground, descriptionForeground
inline wxColour dimmed()
{
	return pick(wxColour(0x86, 0x86, 0x86), wxColour(0x9D, 0x9D, 0x9D));
}

// Behind highlighted rows and the current line in the source view.
// peekViewResult.matchHighlightBackground, flattened onto the editor
// background because wx has no translucent item colours.
inline wxColour highlight()
{
	return pick(wxColour(0xE4, 0xCC, 0x9D), wxColour(0x5D, 0x46, 0x16));
}

// Per line timings in the source view margin.
// chat.editedFileForeground
inline wxColour cost()
{
	return pick(wxColour(0x89, 0x55, 0x03), wxColour(0xE2, 0xC0, 0x8D));
}

// editorWidget.background
inline wxColour marginBackground()
{
	return pick(wxColour(0xF8, 0xF8, 0xF8), wxColour(0x20, 0x20, 0x20));
}

// editorLineNumber.foreground, the same grey in both themes
inline wxColour marginForeground()
{
	return pick(wxColour(0x6E, 0x76, 0x81), wxColour(0x6E, 0x76, 0x81));
}

// editor.foreground
inline wxColour codeDefault()
{
	return pick(wxColour(0x3B, 0x3B, 0x3B), wxColour(0xCC, 0xCC, 0xCC));
}

// keyword, storage
inline wxColour codeKeyword()
{
	return pick(wxColour(0x00, 0x00, 0xFF), wxColour(0x56, 0x9C, 0xD6));
}

// string
inline wxColour codeString()
{
	return pick(wxColour(0xA3, 0x15, 0x15), wxColour(0xCE, 0x91, 0x78));
}

// constant.numeric
inline wxColour codeNumber()
{
	return pick(wxColour(0x09, 0x86, 0x58), wxColour(0xB5, 0xCE, 0xA8));
}

// comment
inline wxColour codeComment()
{
	return pick(wxColour(0x00, 0x80, 0x00), wxColour(0x6A, 0x99, 0x55));
}

} // namespace palette
