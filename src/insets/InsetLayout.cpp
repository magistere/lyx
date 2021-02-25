// -*- C++ -*-
/**
 * \file InsetLayout.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Martin Vermeer
 * \author Richard Kimberly Heck
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetLayout.h"

#include "ColorSet.h"
#include "Lexer.h"
#include "TextClass.h"

#include "support/debug.h"
#include "support/lstrings.h"
#include "support/textutils.h"

#include <vector>

using std::string;
using std::set;
using std::vector;

using namespace lyx::support;

namespace lyx {

InsetDecoration translateDecoration(std::string const & str)
{
	if (compare_ascii_no_case(str, "classic") == 0)
		return InsetDecoration::CLASSIC;
	if (compare_ascii_no_case(str, "minimalistic") == 0)
		return InsetDecoration::MINIMALISTIC;
	if (compare_ascii_no_case(str, "conglomerate") == 0)
		return InsetDecoration::CONGLOMERATE;
	return InsetDecoration::DEFAULT;
}

namespace {

InsetLaTeXType translateLaTeXType(std::string const & str)
{
	if (compare_ascii_no_case(str, "command") == 0)
		return InsetLaTeXType::COMMAND;
	if (compare_ascii_no_case(str, "environment") == 0)
		return InsetLaTeXType::ENVIRONMENT;
	if (compare_ascii_no_case(str, "none") == 0)
		return InsetLaTeXType::NOLATEXTYPE;
	return InsetLaTeXType::ILT_ERROR;
}

} // namespace


bool InsetLayout::read(Lexer & lex, TextClass const & tclass,
	bool validating)
{
	enum {
		IL_ADDTOTOC,
		IL_ARGUMENT,
		IL_BABELPREAMBLE,
		IL_BGCOLOR,
		IL_CONTENTASLABEL,
		IL_COPYSTYLE,
		IL_COUNTER,
		IL_CUSTOMPARS,
		IL_DECORATION,
		IL_DISPLAY,
		IL_EDITEXTERNAL,
		IL_FIXEDWIDTH_PREAMBLE_ENCODING,
		IL_FONT,
		IL_FORCE_LOCAL_FONT_SWITCH,
		IL_FORCELTR,
		IL_FORCEOWNLINES,
		IL_FORCEPLAIN,
		IL_FREESPACING,
		IL_HTMLTAG,
		IL_HTMLATTR,
		IL_HTMLFORCECSS,
		IL_HTMLINNERTAG,
		IL_HTMLINNERATTR,
		IL_HTMLISBLOCK,
		IL_HTMLLABEL,
		IL_HTMLSTYLE,
		IL_HTMLPREAMBLE,
		IL_DOCBOOKTAG,
		IL_DOCBOOKATTR,
		IL_DOCBOOKTAGTYPE,
		IL_DOCBOOKSECTION,
		IL_DOCBOOKININFO,
		IL_DOCBOOKARGUMENTBEFOREMAINTAG,
		IL_DOCBOOKARGUMENTAFTERMAINTAG,
		IL_DOCBOOKNOTINPARA,
		IL_DOCBOOKWRAPPERTAG,
		IL_DOCBOOKWRAPPERTAGTYPE,
		IL_DOCBOOKWRAPPERATTR,
		IL_DOCBOOKITEMTAG,
		IL_DOCBOOKITEMTAGTYPE,
		IL_DOCBOOKITEMATTR,
		IL_DOCBOOKITEMWRAPPERTAG,
		IL_DOCBOOKITEMWRAPPERTAGTYPE,
		IL_DOCBOOKITEMWRAPPERATTR,
        IL_DOCBOOKNOFONTINSIDE,
		IL_INTOC,
		IL_ISTOCCAPTION,
		IL_LABELFONT,
		IL_LABELSTRING,
		IL_LANGPREAMBLE,
		IL_LATEXNAME,
		IL_LATEXPARAM,
		IL_LATEXTYPE,
		IL_LEFTDELIM,
		IL_LYXTYPE,
		IL_OBSOLETEDBY,
		IL_KEEPEMPTY,
		IL_MENUSTRING,
		IL_MULTIPAR,
		IL_NEEDCPROTECT,
		IL_NEEDMBOXPROTECT,
		IL_NEEDPROTECT,
		IL_NEWLINE_CMD,
		IL_PASSTHRU,
		IL_PASSTHRU_CHARS,
		IL_PARBREAKIGNORED,
		IL_PARBREAKISNEWLINE,
		IL_PREAMBLE,
		IL_REQUIRES,
		IL_RIGHTDELIM,
		IL_REFPREFIX,
		IL_RESETARGS,
		IL_RESETSFONT,
		IL_SPELLCHECK,
		IL_END
	};


	LexerKeyword elementTags[] = {
		{ "addtotoc", IL_ADDTOTOC },
		{ "argument", IL_ARGUMENT },
		{ "babelpreamble", IL_BABELPREAMBLE },
		{ "bgcolor", IL_BGCOLOR },
		{ "contentaslabel", IL_CONTENTASLABEL },
		{ "copystyle", IL_COPYSTYLE },
		{ "counter", IL_COUNTER},
		{ "custompars", IL_CUSTOMPARS },
		{ "decoration", IL_DECORATION },
		{ "display", IL_DISPLAY },
		{ "docbookargumentaftermaintag", IL_DOCBOOKARGUMENTAFTERMAINTAG },
		{ "docbookargumentbeforemaintag", IL_DOCBOOKARGUMENTBEFOREMAINTAG },
		{ "docbookattr", IL_DOCBOOKATTR },
		{ "docbookininfo", IL_DOCBOOKININFO },
		{ "docbookitemattr", IL_DOCBOOKITEMATTR },
		{ "docbookitemtag", IL_DOCBOOKITEMTAG },
		{ "docbookitemtagtype", IL_DOCBOOKITEMTAGTYPE },
		{ "docbookitemwrapperattr", IL_DOCBOOKITEMWRAPPERATTR },
		{ "docbookitemwrappertag", IL_DOCBOOKITEMWRAPPERTAG },
		{ "docbookitemwrappertagtype", IL_DOCBOOKITEMWRAPPERTAGTYPE },
		{ "docbooknofontinside", IL_DOCBOOKNOFONTINSIDE },
		{ "docbooknotinpara", IL_DOCBOOKNOTINPARA },
		{ "docbooksection", IL_DOCBOOKSECTION },
		{ "docbooktag", IL_DOCBOOKTAG },
		{ "docbooktagtype", IL_DOCBOOKTAGTYPE },
		{ "docbookwrapperattr", IL_DOCBOOKWRAPPERATTR },
		{ "docbookwrappertag", IL_DOCBOOKWRAPPERTAG },
		{ "docbookwrappertagtype", IL_DOCBOOKWRAPPERTAGTYPE },
		{ "editexternal", IL_EDITEXTERNAL },
		{ "end", IL_END },
		{ "fixedwidthpreambleencoding", IL_FIXEDWIDTH_PREAMBLE_ENCODING },
		{ "font", IL_FONT },
		{ "forcelocalfontswitch", IL_FORCE_LOCAL_FONT_SWITCH },
		{ "forceltr", IL_FORCELTR },
		{ "forceownlines", IL_FORCEOWNLINES },
		{ "forceplain", IL_FORCEPLAIN },
		{ "freespacing", IL_FREESPACING },
		{ "htmlattr", IL_HTMLATTR },
		{ "htmlforcecss", IL_HTMLFORCECSS },
		{ "htmlinnerattr", IL_HTMLINNERATTR},
		{ "htmlinnertag", IL_HTMLINNERTAG},
		{ "htmlisblock", IL_HTMLISBLOCK},
		{ "htmllabel", IL_HTMLLABEL },
		{ "htmlpreamble", IL_HTMLPREAMBLE },
		{ "htmlstyle", IL_HTMLSTYLE },
		{ "htmltag", IL_HTMLTAG },
		{ "intoc", IL_INTOC },
		{ "istoccaption", IL_ISTOCCAPTION },
		{ "keepempty", IL_KEEPEMPTY },
		{ "labelfont", IL_LABELFONT },
		{ "labelstring", IL_LABELSTRING },
		{ "langpreamble", IL_LANGPREAMBLE },
		{ "latexname", IL_LATEXNAME },
		{ "latexparam", IL_LATEXPARAM },
		{ "latextype", IL_LATEXTYPE },
		{ "leftdelim", IL_LEFTDELIM },
		{ "lyxtype", IL_LYXTYPE },
		{ "menustring", IL_MENUSTRING },
		{ "multipar", IL_MULTIPAR },
		{ "needcprotect", IL_NEEDCPROTECT },
		{ "needmboxprotect", IL_NEEDMBOXPROTECT },
		{ "needprotect", IL_NEEDPROTECT },
		{ "newlinecmd", IL_NEWLINE_CMD },
		{ "obsoletedby", IL_OBSOLETEDBY },
		{ "parbreakignored", IL_PARBREAKIGNORED },
		{ "parbreakisnewline", IL_PARBREAKISNEWLINE },
		{ "passthru", IL_PASSTHRU },
		{ "passthruchars", IL_PASSTHRU_CHARS },
		{ "preamble", IL_PREAMBLE },
		{ "refprefix", IL_REFPREFIX },
		{ "requires", IL_REQUIRES },
		{ "resetargs", IL_RESETARGS },
		{ "resetsfont", IL_RESETSFONT },
		{ "rightdelim", IL_RIGHTDELIM },
		{ "spellcheck", IL_SPELLCHECK }
	};

	lex.pushTable(elementTags);

	if (labelfont_ == sane_font)
		labelfont_ = inherit_font;
	if (bgcolor_ == Color_error)
		bgcolor_ = Color_none;
	bool getout = false;
	// whether we've read the CustomPars or ForcePlain tag
	// for issuing a warning in case MultiPars comes later
	bool readCustomOrPlain = false;

	string tmp;
	while (!getout && lex.isOK()) {
		int le = lex.lex();
		switch (le) {
		case Lexer::LEX_UNDEF:
			lex.printError("Unknown InsetLayout tag");
			if (validating)
				return false;
			continue;
		default:
			break;
		}
		switch (le) {
		// FIXME
		// Perhaps a more elegant way to deal with the next two would be the
		// way this sort of thing is handled in Layout::read(), namely, by
		// using the Lexer.
		case IL_LYXTYPE: {
			// make sure that we have the right sort of name.
			if (name_ != from_ascii("undefined")
			    && name_.substr(0,5) != from_ascii("Flex:")) {
				LYXERR0("Flex insets must have names of the form `Flex:<name>'.\n"
				        "This one has the name `" << to_utf8(name_) << "'\n"
				        "Ignoring LyXType declaration.");
				// this is not really a reason to abort
				if (validating)
					return false;
				break;
			}
			string lt;
			lex >> lt;
			lyxtype_ = translateLyXType(lt);
			if (lyxtype_  == InsetLyXType::NOLYXTYPE) {
				LYXERR0("Unknown LyXType `" << lt << "'.");
				// this is not really a reason to abort
				if (validating)
					return false;
			}
			if (lyxtype_ == InsetLyXType::CHARSTYLE) {
				// by default, charstyles force the plain layout
				multipar_ = false;
				forceplain_ = true;
			}
			break;
		}
		case IL_LATEXTYPE:  {
			string lt;
			lex >> lt;
			latextype_ = translateLaTeXType(lt);
			if (latextype_  == InsetLaTeXType::ILT_ERROR) {
				LYXERR0("Unknown LaTeXType `" << lt << "'.");
				// this is not really a reason to abort
				if (validating)
					return false;
			}
			break;
		}
		case IL_LABELSTRING:
			lex >> labelstring_;
			break;
		case IL_MENUSTRING:
			lex >> menustring_;
			break;
		case IL_DECORATION:
			lex >> tmp;
			decoration_ = translateDecoration(tmp);
			break;
		case IL_LATEXNAME:
			lex >> latexname_;
			break;
		case IL_LATEXPARAM:
			lex >> tmp;
			latexparam_ = subst(tmp, "&quot;", "\"");
			break;
		case IL_LEFTDELIM:
			lex >> leftdelim_;
			leftdelim_ = subst(leftdelim_, from_ascii("<br/>"),
						    from_ascii("\n"));
			break;
		case IL_FIXEDWIDTH_PREAMBLE_ENCODING:
			lex >> fixedwidthpreambleencoding_;
			break;
		case IL_FORCE_LOCAL_FONT_SWITCH:
			lex >> forcelocalfontswitch_;
			break;
		case IL_RIGHTDELIM:
			lex >> rightdelim_;
			rightdelim_ = subst(rightdelim_, from_ascii("<br/>"),
						     from_ascii("\n"));
			break;
		case IL_LABELFONT:
			labelfont_ = lyxRead(lex, inherit_font);
			break;
		case IL_FORCELTR:
			lex >> forceltr_;
			break;
		case IL_FORCEOWNLINES:
			lex >> forceownlines_;
			break;
		case IL_INTOC:
			lex >> intoc_;
			break;
		case IL_MULTIPAR:
			lex >> multipar_;
			// the defaults for these depend upon multipar_
			if (readCustomOrPlain)
				LYXERR0("Warning: Read MultiPar after CustomPars or ForcePlain. "
				        "Previous value may be overwritten!");
			readCustomOrPlain = false;
			custompars_ = multipar_;
			forceplain_ = !multipar_;
			break;
		case IL_COUNTER:
			lex >> counter_;
			break;
		case IL_CUSTOMPARS:
			lex >> custompars_;
			readCustomOrPlain = true;
			break;
		case IL_FORCEPLAIN:
			lex >> forceplain_;
			readCustomOrPlain = true;
			break;
		case IL_PASSTHRU:
			lex >> passthru_;
			break;
		case IL_PASSTHRU_CHARS:
			lex >> passthru_chars_;
			break;
		case IL_NEWLINE_CMD:
			lex >> newline_cmd_;
			break;
		case IL_PARBREAKIGNORED:
			lex >> parbreakignored_;
			break;
		case IL_PARBREAKISNEWLINE:
			lex >> parbreakisnewline_;
			break;
		case IL_KEEPEMPTY:
			lex >> keepempty_;
			break;
		case IL_FREESPACING:
			lex >> freespacing_;
			break;
		case IL_NEEDPROTECT:
			lex >> needprotect_;
			break;
		case IL_NEEDCPROTECT:
			lex >> needcprotect_;
			break;
		case IL_NEEDMBOXPROTECT:
			lex >> needmboxprotect_;
			break;
		case IL_CONTENTASLABEL:
			lex >> contentaslabel_;
			break;
		case IL_COPYSTYLE: {
			// initialize with a known style
			docstring style;
			lex >> style;
			style = subst(style, '_', ' ');

			// We don't want to apply the algorithm in DocumentClass::insetLayout()
			// here. So we do it the long way.
			TextClass::InsetLayouts::const_iterator it =
					tclass.insetLayouts().find(style);
			if (it != tclass.insetLayouts().end()) {
				docstring const tmpname = name_;
				this->operator=(it->second);
				name_ = tmpname;
			} else {
				LYXERR0("Cannot copy unknown InsetLayout `"
					<< style << "' to InsetLayout `"
					<< name() << "'\n"
					<< "All InsetLayouts so far:");
				TextClass::InsetLayouts::const_iterator lit =
						tclass.insetLayouts().begin();
				TextClass::InsetLayouts::const_iterator len =
						tclass.insetLayouts().end();
				for (; lit != len; ++lit)
					lyxerr << lit->second.name() << "\n";
				// this is not really a reason to abort
				if (validating)
					return false;
			}
			break;
		}
		case IL_OBSOLETEDBY: {
			docstring style;
			lex >> style;
			style = subst(style, '_', ' ');

			// We don't want to apply the algorithm in DocumentClass::insetLayout()
			// here. So we do it the long way.
			TextClass::InsetLayouts::const_iterator it =
					tclass.insetLayouts().find(style);
			if (it != tclass.insetLayouts().end()) {
				docstring const tmpname = name_;
				this->operator=(it->second);
				name_ = tmpname;
				if (obsoleted_by().empty())
					obsoleted_by_ = style;
			} else {
				LYXERR0("Cannot replace InsetLayout `"
					<< name()
					<< "' with unknown InsetLayout `"
					<< style << "'\n"
					<< "All InsetLayouts so far:");
				TextClass::InsetLayouts::const_iterator lit =
						tclass.insetLayouts().begin();
				TextClass::InsetLayouts::const_iterator len =
						tclass.insetLayouts().end();
				for (; lit != len; ++lit)
					lyxerr << lit->second.name() << "\n";
				// this is not really a reason to abort
				if (validating)
					return false;
			}
			break;
		}

		case IL_FONT: {
			font_ = lyxRead(lex, inherit_font);
			// If you want to define labelfont, you need to do so after
			// font is defined.
			labelfont_ = font_;
			break;
		}
		case IL_RESETARGS:
			bool reset;
			lex >> reset;
			if (reset) {
				latexargs_.clear();
				postcommandargs_.clear();
			}
			break;
		case IL_ARGUMENT:
			readArgument(lex);
			break;
		case IL_BGCOLOR:
			lex >> tmp;
			bgcolor_ = lcolor.getFromLyXName(tmp);
			break;
		case IL_PREAMBLE:
			preamble_ = lex.getLongString(from_ascii("EndPreamble"));
			break;
		case IL_BABELPREAMBLE:
			babelpreamble_ = lex.getLongString(from_ascii("EndBabelPreamble"));
			break;
		case IL_LANGPREAMBLE:
			langpreamble_ = lex.getLongString(from_ascii("EndLangPreamble"));
			break;
		case IL_REFPREFIX:
			lex >> refprefix_;
			break;
		case IL_HTMLTAG:
			lex >> htmltag_;
			break;
		case IL_HTMLATTR:
			lex >> htmlattr_;
			break;
		case IL_HTMLFORCECSS:
			lex >> htmlforcecss_;
			break;
		case IL_HTMLINNERTAG:
			lex >> htmlinnertag_;
			break;
		case IL_HTMLINNERATTR:
			lex >> htmlinnerattr_;
			break;
		case IL_HTMLLABEL:
			lex >> htmllabel_;
			break;
		case IL_HTMLISBLOCK:
			lex >> htmlisblock_;
			break;
		case IL_HTMLSTYLE:
			htmlstyle_ = lex.getLongString(from_ascii("EndHTMLStyle"));
			break;
		case IL_HTMLPREAMBLE:
			htmlpreamble_ = lex.getLongString(from_ascii("EndPreamble"));
			break;
		case IL_DOCBOOKTAG:
			lex >> docbooktag_;
			break;
		case IL_DOCBOOKATTR:
			lex >> docbookattr_;
			break;
		case IL_DOCBOOKTAGTYPE:
			lex >> docbooktagtype_;
			break;
		case IL_DOCBOOKININFO:
			lex >> docbookininfo_;
			break;
		case IL_DOCBOOKARGUMENTBEFOREMAINTAG:
			lex >> docbookargumentbeforemaintag_;
			break;
		case IL_DOCBOOKARGUMENTAFTERMAINTAG:
			lex >> docbookargumentaftermaintag_;
			break;
		case IL_DOCBOOKNOTINPARA:
			lex >> docbooknotinpara_;
			break;
		case IL_DOCBOOKSECTION:
			lex >> docbooksection_;
			break;
		case IL_DOCBOOKITEMTAG:
			lex >> docbookitemtag_;
			break;
		case IL_DOCBOOKITEMTAGTYPE:
			lex >> docbookitemtagtype_;
			break;
		case IL_DOCBOOKITEMATTR:
			lex >> docbookitemattr_;
			break;
		case IL_DOCBOOKITEMWRAPPERTAG:
			lex >> docbookitemwrappertag_;
			break;
		case IL_DOCBOOKITEMWRAPPERTAGTYPE:
			lex >> docbookitemwrappertagtype_;
			break;
		case IL_DOCBOOKITEMWRAPPERATTR:
			lex >> docbookitemwrapperattr_;
			break;
		case IL_DOCBOOKWRAPPERTAG:
			lex >> docbookwrappertag_;
			break;
		case IL_DOCBOOKWRAPPERTAGTYPE:
			lex >> docbookwrappertagtype_;
			break;
		case IL_DOCBOOKWRAPPERATTR:
			lex >> docbookwrapperattr_;
			break;
        case IL_DOCBOOKNOFONTINSIDE:
            lex >> docbooknofontinside_;
            break;
		case IL_REQUIRES: {
			lex.eatLine();
			vector<string> const req
				= getVectorFromString(lex.getString(true));
			required_.insert(req.begin(), req.end());
			break;
		}
		case IL_SPELLCHECK:
			lex >> spellcheck_;
			break;
		case IL_RESETSFONT:
			lex >> resetsfont_;
			break;
		case IL_DISPLAY:
			lex >> display_;
			break;
		case IL_ADDTOTOC:
			lex >> toc_type_;
			add_to_toc_ = !toc_type_.empty();
			break;
		case IL_ISTOCCAPTION:
			lex >> is_toc_caption_;
			break;
		case IL_EDITEXTERNAL:
			lex >> edit_external_;
			break;
		case IL_END:
			getout = true;
			break;
		}
	}

	// Here add element to list if getout == true
	if (!getout)
		return false;

	// The label font is generally used as-is without
	// any realization against a given context.
	labelfont_.realize(sane_font);

	lex.popTable();
	return true;
}


InsetLyXType translateLyXType(std::string const & str)
{
	if (compare_ascii_no_case(str, "charstyle") == 0)
		return InsetLyXType::CHARSTYLE;
	if (compare_ascii_no_case(str, "custom") == 0)
		return InsetLyXType::CUSTOM;
	if (compare_ascii_no_case(str, "end") == 0)
		return InsetLyXType::END;
	if (compare_ascii_no_case(str, "standard") == 0)
		return InsetLyXType::STANDARD;
	return InsetLyXType::NOLYXTYPE;
}


string const & InsetLayout::htmltag() const
{
	if (htmltag_.empty())
		htmltag_ = multipar_ ? "div" : "span";
	return htmltag_;
}


string const & InsetLayout::htmlattr() const
{
	if (htmlattr_.empty())
		htmlattr_ = "class=\"" + defaultCSSClass() + "\"";
	return htmlattr_;
}


string const & InsetLayout::htmlinnerattr() const
{
	if (htmlinnerattr_.empty())
		htmlinnerattr_ = "class=\"" + defaultCSSClass() + "_inner\"";
	return htmlinnerattr_;
}


string InsetLayout::defaultCSSClass() const
{
	if (!defaultcssclass_.empty())
		return defaultcssclass_;
	string d;
	string n = to_utf8(name());
	string::const_iterator it = n.begin();
	string::const_iterator en = n.end();
	for (; it != en; ++it) {
		if (!isAlphaASCII(*it))
			d += "_";
		else if (isLower(*it))
			d += *it;
		else
			d += lowercase(*it);
	}
	// are there other characters we need to remove?
	defaultcssclass_ = d;
	return defaultcssclass_;
}


void InsetLayout::makeDefaultCSS() const
{
	if (!htmldefaultstyle_.empty())
		return;
	docstring const mainfontCSS = font_.asCSS();
	if (!mainfontCSS.empty())
		htmldefaultstyle_ =
				from_ascii(htmltag() + "." + defaultCSSClass() + " {\n") +
				mainfontCSS + from_ascii("\n}\n");
}


docstring InsetLayout::htmlstyle() const
{
	if (!htmlstyle_.empty() && !htmlforcecss_)
		return htmlstyle_;
	if (htmldefaultstyle_.empty())
		makeDefaultCSS();
	docstring retval = htmldefaultstyle_;
	if (!htmlstyle_.empty())
		retval += '\n' + htmlstyle_ + '\n';
	return retval;
}


std::string const & InsetLayout::docbookininfo() const
{
	// Same as Layout::docbookininfo.
	// Indeed, a trilean. Only titles should be "maybe": otherwise, metadata is "always", content is "never".
	if (docbookininfo_.empty() || (docbookininfo_ != "never" && docbookininfo_ != "always" && docbookininfo_ != "maybe"))
		docbookininfo_ = "never";
	return docbookininfo_;
}


std::string InsetLayout::docbooktagtype() const
{
	if (docbooktagtype_ != "block" && docbooktagtype_ != "paragraph" && docbooktagtype_ != "inline")
		docbooktagtype_ = "block";
	return docbooktagtype_;
}


std::string InsetLayout::docbookwrappertagtype() const
{
	if (docbookwrappertagtype_ != "block" && docbookwrappertagtype_ != "paragraph" && docbookwrappertagtype_ != "inline")
		docbookwrappertagtype_ = "block";
	return docbookwrappertagtype_;
}


std::string InsetLayout::docbookitemtagtype() const
{
	if (docbookitemtagtype_ != "block" && docbookitemtagtype_ != "paragraph" && docbookitemtagtype_ != "inline")
		docbookitemtagtype_ = "block";
	return docbookitemtagtype_;
}


std::string InsetLayout::docbookitemwrappertagtype() const
{
	if (docbookitemwrappertagtype_ != "block" && docbookitemwrappertagtype_ != "paragraph" && docbookitemwrappertagtype_ != "inline")
		docbookitemwrappertagtype_ = "block";
	return docbookitemwrappertagtype_;
}


void InsetLayout::readArgument(Lexer & lex)
{
	Layout::latexarg arg;
	arg.mandatory = false;
	arg.autoinsert = false;
	arg.insertcotext = false;
	arg.insertonnewline = false;
	bool error = false;
	bool finished = false;
	arg.font = inherit_font;
	arg.labelfont = inherit_font;
	arg.is_toc_caption = false;
	arg.free_spacing = false;
	arg.passthru = PT_INHERITED;
	arg.nodelims = false;
	string nr;
	lex >> nr;
	bool const postcmd = prefixIs(nr, "post:");
	while (!finished && lex.isOK() && !error) {
		lex.next();
		string const tok = ascii_lowercase(lex.getString());

		if (tok.empty()) {
			continue;
		} else if (tok == "endargument") {
			finished = true;
		} else if (tok == "labelstring") {
			lex.next();
			arg.labelstring = lex.getDocString();
		} else if (tok == "menustring") {
			lex.next();
			arg.menustring = lex.getDocString();
		} else if (tok == "mandatory") {
			lex.next();
			arg.mandatory = lex.getBool();
		} else if (tok == "autoinsert") {
			lex.next();
			arg.autoinsert = lex.getBool();
		} else if (tok == "insertcotext") {
			lex.next();
			arg.insertcotext = lex.getBool();
		} else if (tok == "insertonnewline") {
			lex.next();
			arg.insertonnewline = lex.getBool();
		} else if (tok == "leftdelim") {
			lex.next();
			arg.ldelim = lex.getDocString();
			arg.ldelim = subst(arg.ldelim,
						    from_ascii("<br/>"), from_ascii("\n"));
		} else if (tok == "rightdelim") {
			lex.next();
			arg.rdelim = lex.getDocString();
			arg.rdelim = subst(arg.rdelim,
						    from_ascii("<br/>"), from_ascii("\n"));
		} else if (tok == "defaultarg") {
			lex.next();
			arg.defaultarg = lex.getDocString();
		} else if (tok == "presetarg") {
			lex.next();
			arg.presetarg = lex.getDocString();
		} else if (tok == "tooltip") {
			lex.next();
			arg.tooltip = lex.getDocString();
		} else if (tok == "requires") {
			lex.next();
			arg.required = lex.getString();
		} else if (tok == "decoration") {
			lex.next();
			arg.decoration = lex.getString();
		} else if (tok == "newlinecmd") {
			lex.next();
			arg.newlinecmd = lex.getString();
		} else if (tok == "font") {
			arg.font = lyxRead(lex, arg.font);
		} else if (tok == "labelfont") {
			arg.labelfont = lyxRead(lex, arg.labelfont);
		} else if (tok == "passthruchars") {
			lex.next();
			arg.pass_thru_chars = lex.getDocString();
		} else if (tok == "passthru") {
			lex.next();
			docstring value = lex.getDocString();
			if (value == "true" || value == "1")
				arg.passthru = PT_TRUE;
			else if (value == "false" || value == "0")
				arg.passthru = PT_FALSE;
			else
				arg.passthru = PT_INHERITED;
		} else if (tok == "istoccaption") {
			lex.next();
			arg.is_toc_caption = lex.getBool();
		} else if (tok == "freespacing") {
			lex.next();
			arg.free_spacing = lex.getBool();
		} else if (tok == "docbooktag") {
			lex.next();
			arg.docbooktag = lex.getDocString();
		} else if (tok == "docbookattr") {
			lex.next();
			arg.docbookattr = lex.getDocString();
		} else if (tok == "docbooktagtype") {
			lex.next();
			arg.docbooktagtype = lex.getDocString();
		} else if (tok == "docbookargumentbeforemaintag") {
			lex.next();
			arg.docbookargumentbeforemaintag = lex.getBool();
		} else if (tok == "docbookargumentaftermaintag") {
			lex.next();
			arg.docbookargumentaftermaintag = lex.getBool();
		} else {
			lex.printError("Unknown tag");
			error = true;
		}
	}
	if (arg.labelstring.empty())
		LYXERR0("Incomplete Argument definition!");
	else if (postcmd)
		postcommandargs_[nr] = arg;
	else
		latexargs_[nr] = arg;
}


Layout::LaTeXArgMap InsetLayout::args() const
{
	Layout::LaTeXArgMap args = latexargs_;
	if (!postcommandargs_.empty())
		args.insert(postcommandargs_.begin(), postcommandargs_.end());
	return args;
}


int InsetLayout::optArgs() const
{
	int nr = 0;
	Layout::LaTeXArgMap const args = InsetLayout::args();
	Layout::LaTeXArgMap::const_iterator it = args.begin();
	for (; it != args.end(); ++it) {
		if (!(*it).second.mandatory)
			++nr;
	}
	return nr;
}


int InsetLayout::requiredArgs() const
{
	int nr = 0;
	Layout::LaTeXArgMap const args = InsetLayout::args();
	Layout::LaTeXArgMap::const_iterator it = args.begin();
	for (; it != args.end(); ++it) {
		if ((*it).second.mandatory)
			++nr;
	}
	return nr;
}


} //namespace lyx
