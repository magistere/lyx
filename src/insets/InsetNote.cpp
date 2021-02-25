/**
 * \file InsetNote.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author Martin Vermeer
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetNote.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "ColorSet.h"
#include "Cursor.h"
#include "Exporter.h"
#include "FontInfo.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetLayout.h"
#include "LaTeXFeatures.h"
#include "Lexer.h"
#include "LyXRC.h"
#include "output_docbook.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/lstrings.h"
#include "support/Translator.h"

#include "frontends/Application.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace lyx {

namespace {

typedef Translator<string, InsetNoteParams::Type> NoteTranslator;

NoteTranslator const init_notetranslator()
{
	NoteTranslator translator("Note", InsetNoteParams::Note);
	translator.addPair("Comment", InsetNoteParams::Comment);
	translator.addPair("Greyedout", InsetNoteParams::Greyedout);
	return translator;
}


NoteTranslator const & notetranslator()
{
	static NoteTranslator const translator = init_notetranslator();
	return translator;
}


} // namespace


InsetNoteParams::InsetNoteParams()
	: type(Note)
{}


void InsetNoteParams::write(ostream & os) const
{
	string const label = notetranslator().find(type);
	os << "Note " << label << "\n";
}


void InsetNoteParams::read(Lexer & lex)
{
	string label;
	lex >> label;
	if (lex)
		type = notetranslator().find(label);
}


/////////////////////////////////////////////////////////////////////
//
// InsetNote
//
/////////////////////////////////////////////////////////////////////

InsetNote::InsetNote(Buffer * buf, string const & label)
	: InsetCollapsible(buf)
{
	params_.type = notetranslator().find(label);
}


InsetNote::~InsetNote()
{
	hideDialogs("note", this);
}


docstring InsetNote::layoutName() const
{
	return from_ascii("Note:" + notetranslator().find(params_.type));
}


void InsetNote::write(ostream & os) const
{
	params_.write(os);
	InsetCollapsible::write(os);
}


void InsetNote::read(Lexer & lex)
{
	params_.read(lex);
	InsetCollapsible::read(lex);
}


bool InsetNote::showInsetDialog(BufferView * bv) const
{
	bv->showDialog("note", params2string(params()),
		const_cast<InsetNote *>(this));
	return true;
}


void InsetNote::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		// Do not do anything if converting to the same type of Note.
		// A quick break here is done instead of disabling the LFUN
		// because disabling the LFUN would lead to a greyed out
		// entry, which might confuse users.
		// In the future, we might want to have a radio button for
		// switching between notes.
		InsetNoteParams params;
		string2params(to_utf8(cmd.argument()), params);
		if (params_.type == params.type)
		  break;

		cur.recordUndoInset(this);
		string2params(to_utf8(cmd.argument()), params_);
		setButtonLabel();
		// what we really want here is a TOC update, but that means
		// a full buffer update
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_INSET_DIALOG_UPDATE:
		cur.bv().updateDialog("note", params2string(params()));
		break;

	default:
		InsetCollapsible::doDispatch(cur, cmd);
		break;
	}
}


bool InsetNote::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY:
		if (cmd.getArg(0) == "note") {
			InsetNoteParams params;
			string2params(to_utf8(cmd.argument()), params);
			flag.setOnOff(params_.type == params.type);
		}
		return true;

	case LFUN_INSET_DIALOG_UPDATE:
		flag.setEnabled(true);
		return true;

	default:
		return InsetCollapsible::getStatus(cur, cmd, flag);
	}
}


bool InsetNote::isMacroScope() const
{
	// LyX note has no latex output
	if (params_.type == InsetNoteParams::Note)
		return true;

	return InsetCollapsible::isMacroScope();
}


void InsetNote::latex(otexstream & os, OutputParams const & runparams_in) const
{
	if (params_.type == InsetNoteParams::Note)
		return;

	OutputParams runparams(runparams_in);
	if (params_.type == InsetNoteParams::Comment) {
		runparams.inComment = true;
		// Ignore files that are exported inside a comment
		runparams.exportdata.reset(new ExportData);
	}

	// the space after the comment in 'a[comment] b' will be eaten by the
	// comment environment since the space before b is ignored with the
	// following latex output:
	//
	// a%
	// \begin{comment}
	// comment
	// \end{comment}
	//  b
	//
	// Adding {} before ' b' fixes this.
	// The {} will be automatically added, but only if needed, for all
	// insets whose InsetLayout Display tag is false. This is achieved
	// by telling otexstream to protect an immediately following space
	// and is done for both comment and greyedout insets.
	InsetCollapsible::latex(os, runparams);

	runparams_in.encoding = runparams.encoding;
}


int InsetNote::plaintext(odocstringstream & os,
			 OutputParams const & runparams_in, size_t max_length) const
{
	if (params_.type == InsetNoteParams::Note)
		return 0;

	OutputParams runparams(runparams_in);
	if (params_.type == InsetNoteParams::Comment) {
		runparams.inComment = true;
		// Ignore files that are exported inside a comment
		runparams.exportdata.reset(new ExportData);
	}
	os << '[' << buffer().B_("note") << ":\n";
	InsetText::plaintext(os, runparams, max_length);
	os << "\n]";

	return PLAINTEXT_NEWLINE + 1; // one char on a separate line
}


void InsetNote::docbook(XMLStream & xs, OutputParams const & runparams_in) const
{
	if (params_.type == InsetNoteParams::Note)
		return;

	OutputParams runparams(runparams_in);
	if (params_.type == InsetNoteParams::Comment) {
		xs << xml::StartTag("remark");
		xs << xml::CR();
		runparams.inComment = true;
		// Ignore files that are exported inside a comment
		runparams.exportdata.reset(new ExportData);
	}
	// Greyed out text is output as such (no way to mark text as greyed out with DocBook).

	InsetText::docbook(xs, runparams);

	if (params_.type == InsetNoteParams::Comment) {
		xs << xml::CR();
		xs << xml::EndTag("remark");
		xs << xml::CR();
	}
}


docstring InsetNote::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	if (params_.type == InsetNoteParams::Note)
		return docstring();

	return InsetCollapsible::xhtml(xs, rp);
}


void InsetNote::validate(LaTeXFeatures & features) const
{
	switch (params_.type) {
	case InsetNoteParams::Comment:
		if (features.runparams().flavor == Flavor::Html)
			// we do output this but set display to "none" by default,
			// but people might want to use it.
			InsetCollapsible::validate(features);
		else
			// Only do the requires
			features.useInsetLayout(getLayout());
		break;
	case InsetNoteParams::Greyedout:
		if (features.hasRTLLanguage())
			features.require("environ");
		InsetCollapsible::validate(features);
		break;
	case InsetNoteParams::Note:
		break;
	}
}


string InsetNote::contextMenuName() const
{
	return "context-note";
}

bool InsetNote::allowSpellCheck() const
{
	return (params_.type == InsetNoteParams::Greyedout || lyxrc.spellcheck_notes);
}

FontInfo InsetNote::getFont() const
{
	FontInfo font = getLayout().font();
	if (params_.type == InsetNoteParams::Greyedout
	    && buffer().params().isnotefontcolor) {
		ColorCode c = lcolor.getFromLyXName("notefontcolor");
		if (c != Color_none)
			font.setColor(c);
		// This is the local color (not overridden by other documents)
		ColorCode lc = lcolor.getFromLyXName("notefontcolor@" + buffer().fileName().absFileName());
		if (lc != Color_none)
			font.setPaintColor(lc);
	}
	return font;
}


string InsetNote::params2string(InsetNoteParams const & params)
{
	ostringstream data;
	data << "note" << ' ';
	params.write(data);
	return data.str();
}


void InsetNote::string2params(string const & in, InsetNoteParams & params)
{
	params = InsetNoteParams();

	if (in.empty())
		return;

	istringstream data(in);
	Lexer lex;
	lex.setStream(data);
	lex.setContext("InsetNote::string2params");
	lex >> "note";
	// There are cases, such as when we are called via getStatus() from
	// Dialog::canApply(), where we are just called with "note" rather
	// than a full "note Note TYPE".
	if (!lex.isOK())
		return;
	lex >> "Note";

	params.read(lex);
}


} // namespace lyx
