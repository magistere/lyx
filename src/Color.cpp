/**
 * \file Color.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Asger Alstrup
 * \author Lars Gullik Bjønnes
 * \author Matthias Ettrich
 * \author Jean-Marc Lasgouttes
 * \author John Levon
 * \author André Pönitz
 * \author Martin Vermeer
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Color.h"
#include "ColorSet.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/gettext.h"
#include "support/lstrings.h"
#include "support/lassert.h"

#include <map>
#include <cmath>
#include <sstream>
#include <iomanip>


using namespace std;
using namespace lyx::support;

namespace lyx {


struct ColorSet::ColorEntry {
	ColorCode lcolor;
	char const * guiname;
	char const * latexname;
	char const * x11hexname;
	char const * x11darkhexname;
	char const * lyxname;
};


static int hexstrToInt(string const & str)
{
	int val = 0;
	istringstream is(str);
	is >> setbase(16) >> val;
	return val;
}


/////////////////////////////////////////////////////////////////////
//
// RGBColor
//
/////////////////////////////////////////////////////////////////////


string const X11hexname(RGBColor const & col)
{
	ostringstream ostr;

	ostr << '#' << setbase(16) << setfill('0')
	     << setw(2) << col.r
	     << setw(2) << col.g
	     << setw(2) << col.b;

	return ostr.str();
}


RGBColor rgbFromHexName(string const & x11hexname)
{
	RGBColor c;
	LASSERT(x11hexname.size() == 7 && x11hexname[0] == '#',
		return c);
	c.r = hexstrToInt(x11hexname.substr(1, 2));
	c.g = hexstrToInt(x11hexname.substr(3, 2));
	c.b = hexstrToInt(x11hexname.substr(5, 2));
	return c;
}


string const outputLaTeXColor(RGBColor const & color)
{
	// this routine returns a LaTeX readable color string in the form
	// "red, green, blue" where the colors are a number in the range 0-1
	int red = color.r;
	int green = color.g;
	int blue = color.b;
#ifdef USE_CORRECT_RGB_CONVERSION
	int const scale = 255;
#else
	// the color values are given in the range of 0-255, so to get
	// an output of "0.5" for the value 127 we need to do the following
	// FIXME: This is wrong, since it creates a nonlinear mapping:
	//        There is a gap between 0/256 and 2/256!
	//        0.5 cannot be represented in 8bit hex RGB, it would be 127.5.
	if (red != 0)
		++red;
	if (green != 0)
		++green;
	if (blue != 0)
		++blue;
	int const scale = 256;
#endif
	string output;
	output = convert<string>(float(red) / scale) + ", "
			 + convert<string>(float(green) / scale) + ", "
			 + convert<string>(float(blue) / scale);
	return output;
}


RGBColor const RGBColorFromLaTeX(string const & color)
{
	vector<string> rgb = getVectorFromString(color);
	while (rgb.size() < 3)
		rgb.push_back("0");
	RGBColor c;
	for (int i = 0; i < 3; ++i) {
		rgb[i] = trim(rgb[i]);
		if (!isStrDbl(rgb[i]))
			return c;
	}
#ifdef USE_CORRECT_RGB_CONVERSION
	int const scale = 255;
#else
	// FIXME: This is wrong, since it creates a nonlinear mapping:
	//        Both 0/256 and 1/256 are mapped to 0!
	//        The wrong code exists only to match outputLaTeXColor().
	int const scale = 256;
#endif
	c.r = static_cast<unsigned int>(scale * convert<double>(rgb[0]) + 0.5);
	c.g = static_cast<unsigned int>(scale * convert<double>(rgb[1]) + 0.5);
	c.b = static_cast<unsigned int>(scale * convert<double>(rgb[2]) + 0.5);
#ifndef USE_CORRECT_RGB_CONVERSION
	if (c.r != 0)
		c.r--;
	if (c.g != 0)
		c.g--;
	if (c.b != 0)
		c.b--;
#endif
	return c;
}


RGBColor const inverseRGBColor(RGBColor color)
{
	color.r = 255 - color.r;
	color.g = 255 - color.g;
	color.b = 255 - color.b;

	return color;
}


Color::Color(ColorCode base_color) : baseColor(base_color),
	mergeColor(Color_ignore)
{}


bool Color::operator==(Color const & color) const
{
	return baseColor == color.baseColor;
}


bool Color::operator!=(Color const & color) const
{
	return baseColor != color.baseColor;
}


bool Color::operator<(Color const & color) const
{
	return baseColor < color.baseColor;
}


bool Color::operator<=(Color const & color) const
{
	return baseColor <= color.baseColor;
}


std::ostream & operator<<(std::ostream & os, Color color)
{
	os << to_ascii(lcolor.getGUIName(color.baseColor));
	if (color.mergeColor != Color_ignore)
		os << "[merged with:"
			<< to_ascii(lcolor.getGUIName(color.mergeColor)) << "]";
	return os;
}


ColorSet::ColorSet()
{
	char const * grey40 = "#666666";
	char const * grey60 = "#999999";
	char const * grey80 = "#cccccc";
	// latex colors (xcolor package)
	char const * black = "#000000";
	char const * white = "#ffffff";
	char const * blue = "#0000ff";
	char const * brown = "#bf8040";
	char const * cyan = "#00ffff";
	char const * darkgray = "#404040";
	char const * gray = "#808080";
	char const * green = "#00ff00";
	char const * lightgray = "#bfbfbf";
	char const * lime = "#bfff00";
	char const * magenta = "#ff00ff";
	char const * olive = "#808000";
	char const * orange = "#ff8000";
	char const * pink = "#ffbfbf";
	char const * purple = "#bf0040";
	char const * red = "#ff0000";
	char const * teal = "#008080";
	char const * violet = "#800080";
	char const * yellow = "#ffff00";
	// svg colors
	char const * Brown = "#a52a2a";
	char const * DarkRed = "#8b0000";
	char const * Green = "#008000";
	char const * IndianRed = "#cd5c5c";
	char const * Linen = "#faf0e6";
	char const * RoyalBlue = "#4169e1";

	//char const * grey90 = "#e5e5e5";
	//  ColorCode, gui, latex, x11hexname, x11darkhexname, lyx
	// Warning: several of these entries are overridden in GuiApplication constructor
	static ColorEntry const items[] = {
	{ Color_none, N_("none"), "none", black, black, "none" },
	{ Color_black, N_("black"), "black", black, black, "black" },
	{ Color_white, N_("white"), "white", white, white, "white" },
	{ Color_blue, N_("blue"), "blue", blue, blue, "blue" },
	{ Color_brown, N_("brown"), "brown", brown, brown, "brown" },
	{ Color_cyan, N_("cyan"), "cyan", cyan, cyan, "cyan" },
	{ Color_darkgray, N_("darkgray"), "darkgray", darkgray, darkgray, "darkgray" },
	{ Color_gray, N_("gray"), "gray", gray, gray, "gray" },
	{ Color_green, N_("green"), "green", green, green, "green" },
	{ Color_lightgray, N_("lightgray"), "lightgray", lightgray, lightgray, "lightgray" },
	{ Color_lime, N_("lime"), "lime", lime, lime, "lime" },
	{ Color_magenta, N_("magenta"), "magenta", magenta, magenta, "magenta" },
	{ Color_olive, N_("olive"), "olive", olive, olive, "olive" },
	{ Color_orange, N_("orange"), "orange", orange, orange, "orange" },
	{ Color_pink, N_("pink"), "pink", pink, pink, "pink" },
	{ Color_purple, N_("purple"), "purple", purple, purple, "purple" },
	{ Color_red, N_("red"), "red", red, red, "red" },
	{ Color_teal, N_("teal"), "teal", teal, teal, "teal" },
	{ Color_violet, N_("violet"), "violet", violet, violet, "violet" },
	{ Color_yellow, N_("yellow"), "yellow", yellow, yellow, "yellow" },
	{ Color_cursor, N_("cursor"), "cursor", black, Linen, "cursor" },
	{ Color_background, N_("background"), "background", Linen, black, "background" },
	{ Color_foreground, N_("text"), "foreground", black, Linen, "foreground" },
	{ Color_selection, N_("selection"), "selection", "#add8e6", "#add8e6", "selection" },
	{ Color_selectiontext, N_("selected text"), "selectiontext", black, black, "selectiontext" },
	{ Color_latex, N_("LaTeX text"), "latex", DarkRed, "#D66613", "latex" },
	{ Color_textlabel1, N_("Text label 1"), "textlabel1", blue, "#86a4ff", "textlabel1" },
	{ Color_textlabel2, N_("Text label 2"), "textlabel2", Green, green, "textlabel2" },
	{ Color_textlabel3, N_("Text label 3"), "textlabel3", magenta, magenta, "textlabel3" },
	{ Color_inlinecompletion, N_("inline completion"),
		"inlinecompletion", grey60, grey40, "inlinecompletion" },
	{ Color_nonunique_inlinecompletion, N_("non-unique inline completion"),
		"nonuniqueinlinecompletion", grey80, grey60, "nonuniqueinlinecompletion" },
	{ Color_preview, N_("previewed snippet"), "preview", black, Linen, "preview" },
	{ Color_notelabel, N_("note label"), "note", yellow, "#FF6200", "note" },
	{ Color_notebg, N_("note background"), "notebg", yellow, "#5b5903", "notebg" },
	{ Color_commentlabel, N_("comment label"), "comment", magenta, olive, "comment" },
	{ Color_commentbg, N_("comment background"), "commentbg", Linen, black, "commentbg" },
	{ Color_greyedoutlabel, N_("greyedout inset label"), "greyedout", "#ff0080", "#ff0080", "greyedout" },
	{ Color_greyedouttext, N_("greyedout inset text"), "greyedouttext", grey80, grey40, "greyedouttext" },
	{ Color_greyedoutbg, N_("greyedout inset background"), "greyedoutbg", Linen, black, "greyedoutbg" },
	{ Color_phantomtext, N_("phantom inset text"), "phantomtext", "#7f7f7f", "#7f7f7f", "phantomtext" },
	{ Color_shadedbg, N_("shaded box"), "shaded", "#ff0000", "#f2af7d", "shaded" },
	{ Color_listingsbg, N_("listings background"), "listingsbg", white, black, "listingsbg" },
	{ Color_branchlabel, N_("branch label"), "branchlabel", "#c88000", "#c88000", "branchlabel" },
	{ Color_footlabel, N_("footnote label"), "footlabel", "#00aaff", blue, "footlabel" },
	{ Color_indexlabel, N_("index label"), "indexlabel", Green, teal, "indexlabel" },
	{ Color_marginlabel, N_("margin note label"), "marginlabel", "#aa55ff", violet, "marginlabel" },
	{ Color_urllabel, N_("URL label"), "urllabel", blue, blue, "urllabel" },
	{ Color_urltext, N_("URL text"), "urltext", blue, "#86a4ff", "urltext" },
	{ Color_depthbar, N_("depth bar"), "depthbar", IndianRed, IndianRed, "depthbar" },
	{ Color_scroll, N_("scroll indicator"), "scroll", IndianRed, IndianRed, "scroll" },
	{ Color_language, N_("language"), "language", blue, "#86a4ff", "language" },
	{ Color_command, N_("command inset"), "command", black, black, "command" },
	{ Color_commandbg, N_("command inset background"), "commandbg", "#f0ffff", "#f0ffff", "commandbg" },
	{ Color_commandframe, N_("command inset frame"), "commandframe", black, Linen, "commandframe" },
	{ Color_command_broken, N_("command inset (broken reference)"), "command", white, white, "command_broken" },
	{ Color_buttonbg_broken, N_("button background (broken reference)"), "commandbg", red, red, "commandbg_broken" },
	{ Color_buttonframe_broken, N_("button frame (broken reference)"), "commandframe", red, red, "commandframe_broken" },
	{ Color_buttonhoverbg_broken, N_("button background (broken reference) under focus"), "buttonhoverbg", "#DB0B0B", "#DB0B0B", "buttonhoverbg_broken" },
	{ Color_special, N_("special character"), "special", RoyalBlue, RoyalBlue, "special" },
	{ Color_math, N_("math"), "math", "#00008B", "#85F0FE", "math" },
	{ Color_mathbg, N_("math background"), "mathbg", Linen, black, "mathbg" },
	{ Color_graphicsbg, N_("graphics background"), "graphicsbg", Linen, black, "graphicsbg" },
	{ Color_mathmacrobg, N_("math macro background"), "mathmacrobg", Linen, black, "mathmacrobg" },
	{ Color_mathframe, N_("math frame"), "mathframe", magenta, magenta, "mathframe" },
	{ Color_mathcorners, N_("math corners"), "mathcorners", Linen, black, "mathcorners" },
	{ Color_mathline, N_("math line"), "mathline", blue, "#86a4ff", "mathline" },
	{ Color_mathmacrobg, N_("math macro background"), "mathmacrobg", "#ede2d8", black, "mathmacrobg" },
	{ Color_mathmacrohoverbg, N_("math macro hovered background"), "mathmacrohoverbg", "#cdc3b8", grey80, "mathmacrohoverbg" },
	{ Color_mathmacrolabel, N_("math macro label"), "mathmacrolabel", "#a19992", "#a19992", "mathmacrolabel" },
	{ Color_mathmacroframe, N_("math macro frame"), "mathmacroframe", "#ede2d8", black, "mathmacroframe" },
	{ Color_mathmacroblend, N_("math macro blended out"), "mathmacroblend", black, Linen, "mathmacroblend" },
	{ Color_mathmacrooldarg, N_("math macro old parameter"), "mathmacrooldarg", grey80, grey40, "mathmacrooldarg" },
	{ Color_mathmacronewarg, N_("math macro new parameter"), "mathmacronewarg", black, Linen, "mathmacronewarg" },
	{ Color_collapsible, N_("collapsible inset text"), "collapsible", DarkRed, DarkRed, "collapsible" },
	{ Color_collapsibleframe, N_("collapsible inset frame"), "collapsibleframe", IndianRed, IndianRed, "collapsibleframe" },
	{ Color_insetbg, N_("inset background"), "insetbg", grey80, grey80, "insetbg" },
	{ Color_insetframe, N_("inset frame"), "insetframe", IndianRed, IndianRed, "insetframe" },
	{ Color_error, N_("LaTeX error"), "error", red, DarkRed, "error" },
	{ Color_eolmarker, N_("end-of-line marker"), "eolmarker", Brown, Brown, "eolmarker" },
	{ Color_appendix, N_("appendix marker"), "appendix", Brown, Brown, "appendix" },
	{ Color_changebar, N_("change bar"), "changebar", blue, "#86a4ff", "changebar" },
	{ Color_deletedtext, N_("deleted text (output)"), "deletedtext", "#ff0000", "#ff0000", "deletedtext" },
	{ Color_addedtext, N_("added text (output)"), "addedtext", "#0000ff", "#0000ff", "addedtext" },
	{ Color_addedtextauthor1, N_("added text (workarea, 1st author)"), "changedtextauthor1", "#0000ff", "#86a4ff", "changedtextauthor1" },
	{ Color_addedtextauthor2, N_("added text (workarea, 2nd author)"), "changedtextauthor2", "#ff00ff", "#ee86ee", "changedtextauthor2" },
	{ Color_addedtextauthor3, N_("added text (workarea, 3rd author)"), "changedtextauthor3", "#ff0000", "#ea8989", "changedtextauthor3" },
	{ Color_addedtextauthor4, N_("added text (workarea, 4th author)"), "changedtextauthor4", "#aa00ff", "#c371ec", "changedtextauthor4" },
	{ Color_addedtextauthor5, N_("added text (workarea, 5th author)"), "changedtextauthor5", "#55aa00", "#acd780", "changedtextauthor5" },
	{ Color_deletedtextmodifier, N_("deleted text modifier (workarea)"), "deletedtextmodifier", white, white, "deletedtextmodifier" },
	{ Color_added_space, N_("added space markers"), "added_space", Brown, Brown, "added_space" },
	{ Color_tabularline, N_("table line"), "tabularline", black, Linen, "tabularline" },
	{ Color_tabularonoffline, N_("table on/off line"), "tabularonoffline", "#b0c4de", "#23497b", "tabularonoffline" },
	{ Color_bottomarea, N_("bottom area"), "bottomarea", grey40, grey80, "bottomarea" },
	{ Color_newpage, N_("new page"), "newpage", blue, "#86a4ff", "newpage" },
	{ Color_pagebreak, N_("page break / line break"), "pagebreak", RoyalBlue, RoyalBlue, "pagebreak" },
	{ Color_buttonframe, N_("button frame"), "buttonframe", "#dcd2c8", "#dcd2c8", "buttonframe" },
	{ Color_buttonbg, N_("button background"), "buttonbg", "#dcd2c8", "#dcd2c8", "buttonbg" },
	{ Color_buttonhoverbg, N_("button background under focus"), "buttonhoverbg", "#C7C7CA", "#C7C7CA", "buttonhoverbg" },
	{ Color_paragraphmarker, N_("paragraph marker"), "paragraphmarker", grey80, grey40, "paragraphmarker"},
	{ Color_previewframe, N_("preview frame"), "previewframe", black, Linen, "previewframe"},
	{ Color_regexpframe, N_("regexp frame"), "regexpframe", Green, green, "regexpframe" },
	{ Color_bookmark, N_("bookmark"), "bookmark", RoyalBlue, RoyalBlue, "bookmark" },
	{ Color_inherit, N_("inherit"), "inherit", black, Linen, "inherit" },
	{ Color_ignore, N_("ignore"), "ignore", black, Linen, "ignore" },
	{ Color_ignore, nullptr, nullptr, nullptr, nullptr, nullptr }
	};

	for (int i = 0; items[i].guiname; ++i)
		fill(items[i]);
}


/// initialise a color entry
void ColorSet::fill(ColorEntry const & entry)
{
	Information in;
	in.lyxname   = entry.lyxname;
	in.latexname = entry.latexname;
	in.x11hexname   = entry.x11hexname;
	in.x11darkhexname   = entry.x11darkhexname;
	in.guiname   = entry.guiname;
	infotab[entry.lcolor] = in;
	lyxcolors[entry.lyxname] = entry.lcolor;
	latexcolors[entry.latexname] = entry.lcolor;
}


docstring const ColorSet::getGUIName(ColorCode c) const
{
	InfoTab::const_iterator it = infotab.find(c);
	if (it != infotab.end())
		return _(it->second.guiname);
	return from_ascii("none");
}


string const ColorSet::getX11HexName(ColorCode c, bool const darkmode) const
{
	InfoTab::const_iterator it = infotab.find(c);
	if (it != infotab.end())
		return darkmode ? it->second.x11darkhexname : it->second.x11hexname;

	lyxerr << "LyX internal error: Missing color"
		  " entry in Color.cpp for " << c << '\n'
	       << "Using black." << endl;
	return darkmode ? "#faf0e6" : "black";
}


pair<string, string> const ColorSet::getAllX11HexNames(ColorCode c) const
{
	InfoTab::const_iterator it = infotab.find(c);
	if (it != infotab.end())
		return make_pair(it->second.x11hexname, it->second.x11darkhexname);

	lyxerr << "LyX internal error: Missing color"
		  " entry in Color.cpp for " << c << '\n'
	       << "Using black." << endl;
	return make_pair("black", "#faf0e6");
}


string const ColorSet::getLaTeXName(ColorCode c) const
{
	InfoTab::const_iterator it = infotab.find(c);
	if (it != infotab.end())
		return it->second.latexname;
	return "black";
}


string const ColorSet::getLyXName(ColorCode c) const
{
	InfoTab::const_iterator it = infotab.find(c);
	if (it != infotab.end())
		return it->second.lyxname;
	return "black";
}


bool ColorSet::setColor(ColorCode col, string const & x11hexname,
			string const & x11darkhexname)
{
	InfoTab::iterator it = infotab.find(col);
	if (it == infotab.end()) {
		LYXERR0("Color " << col << " not found in database.");
		return false;
	}

	// "inherit" is returned for colors not in the database
	// (and anyway should not be redefined)
	if (col == Color_none || col == Color_inherit || col == Color_ignore) {
		LYXERR0("Color " << getLyXName(col) << " may not be redefined.");
		return false;
	}

	if (!x11hexname.empty())
		it->second.x11hexname = x11hexname;
	it->second.x11darkhexname = (x11darkhexname.empty()) ? x11hexname : x11darkhexname;
	return true;
}


bool ColorSet::setColor(string const & lyxname, string const & x11hexname,
			string const & x11darkhexname)
{
	string const lcname = ascii_lowercase(lyxname);
	if (lyxcolors.find(lcname) == lyxcolors.end()) {
		LYXERR(Debug::GUI, "ColorSet::setColor: Unknown color \""
		       << lyxname << '"');
		addColor(static_cast<ColorCode>(infotab.size()), lcname);
	}

	return setColor(lyxcolors[lcname], x11hexname, x11darkhexname);
}


bool ColorSet::setLaTeXName(string const & lyxname, string const & latexname)
{
	string const lcname = ascii_lowercase(lyxname);
	if (lyxcolors.find(lcname) == lyxcolors.end()) {
		LYXERR(Debug::GUI, "ColorSet::setLaTeXName: Unknown color \""
		       << lyxname << '"');
		addColor(static_cast<ColorCode>(infotab.size()), lcname);
	}

	ColorCode col = lyxcolors[lcname];
	InfoTab::iterator it = infotab.find(col);
	if (it == infotab.end()) {
		LYXERR0("Color " << col << " not found in database.");
		return false;
	}

	// "inherit" is returned for colors not in the database
	// (and anyway should not be redefined)
	if (col == Color_none || col == Color_inherit || col == Color_ignore) {
		LYXERR0("Color " << getLyXName(col) << " may not be redefined.");
		return false;
	}

	if (!latexname.empty())
		it->second.latexname = latexname;
	return true;
}


bool ColorSet::setGUIName(string const & lyxname, string const & guiname)
{
	string const lcname = ascii_lowercase(lyxname);
	if (lyxcolors.find(lcname) == lyxcolors.end()) {
		LYXERR(Debug::GUI, "ColorSet::setGUIName: Unknown color \""
		       << lyxname << '"');
		return false;
	}

	ColorCode col = lyxcolors[lcname];
	InfoTab::iterator it = infotab.find(col);
	if (it == infotab.end()) {
		LYXERR0("Color " << col << " not found in database.");
		return false;
	}

	// "inherit" is returned for colors not in the database
	// (and anyway should not be redefined)
	if (col == Color_none || col == Color_inherit || col == Color_ignore) {
		LYXERR0("Color " << getLyXName(col) << " may not be redefined.");
		return false;
	}

	if (!guiname.empty())
		it->second.guiname = guiname;
	return true;
}


void ColorSet::addColor(ColorCode c, string const & lyxname)
{
	ColorEntry ce = { c, "", "", "", "", lyxname.c_str() };
	fill(ce);
}


ColorCode ColorSet::getFromLyXName(string const & lyxname) const
{
	string const lcname = ascii_lowercase(lyxname);
	Transform::const_iterator it = lyxcolors.find(lcname);
	if (it == lyxcolors.end()) {
		LYXERR0("ColorSet::getFromLyXName: Unknown color \""
		       << lyxname << '"');
		return Color_none;
	}

	return it->second;
}


ColorCode ColorSet::getFromLaTeXName(string const & latexname) const
{
	Transform::const_iterator it = latexcolors.find(latexname);
	if (it == latexcolors.end()) {
		lyxerr << "ColorSet::getFromLaTeXName: Unknown color \""
		       << latexname << '"' << endl;
		return Color_none;
	}

	return it->second;
}


// The evil global Color instance
ColorSet lcolor;
// An equally evil global system Color instance
ColorSet system_lcolor;


} // namespace lyx
