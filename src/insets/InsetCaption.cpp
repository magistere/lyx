/**
 * \file InsetCaption.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetCaption.h"
#include "InsetFloat.h"
#include "InsetWrap.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Cursor.h"
#include "Dimension.h"
#include "Floating.h"
#include "FloatList.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetList.h"
#include "Language.h"
#include "LyXRC.h"
#include "MetricsInfo.h"
#include "xml.h"
#include "output_latex.h"
#include "output_xhtml.h"
#include "Paragraph.h"
#include "ParIterator.h"
#include "TexRow.h"
#include "texstream.h"
#include "TextClass.h"
#include "TextMetrics.h"
#include "TocBackend.h"

#include "frontends/FontMetrics.h"
#include "frontends/Painter.h"

#include "support/gettext.h"
#include "support/lstrings.h"

#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {


InsetCaption::InsetCaption(Buffer * buf, string const & type)
    : InsetText(buf, InsetText::PlainLayout), type_(type)
{
	setDrawFrame(true);
	setFrameColor(Color_collapsibleframe);
}


void InsetCaption::write(ostream & os) const
{
	os << "Caption";
	if (!type_.empty())
		os << ' ' << type_;
	os << '\n';
	text().write(os);
}


docstring InsetCaption::layoutName() const
{
	if (type_.empty())
		return from_ascii("Caption");
	return from_utf8("Caption:" + type_);
}


void InsetCaption::cursorPos(BufferView const & bv,
		CursorSlice const & sl, bool boundary, int & x, int & y) const
{
	InsetText::cursorPos(bv, sl, boundary, x, y);
	if (!rtl_)
		x += labelwidth_;
}


void InsetCaption::addToToc(DocIterator const & cpit, bool output_active,
							UpdateType utype, TocBackend & backend) const
{
	string const & type = floattype_.empty() ? "senseless" : floattype_;
	DocIterator pit = cpit;
	pit.push_back(CursorSlice(const_cast<InsetCaption &>(*this)));
	int length = (utype == OutputUpdate) ?
		// For output (e.g. xhtml) all (bug #8603) or nothing
		(output_active ? INT_MAX : 0) :
		// TOC for LyX interface
		TOC_ENTRY_LENGTH;
	docstring str;
	if (length > 0) {
		str = full_label_;
		text().forOutliner(str, length);
	}
	backend.builder(type).captionItem(pit, str, output_active);
	// Proceed with the rest of the inset.
	InsetText::addToToc(cpit, output_active, utype, backend);
}


void InsetCaption::metrics(MetricsInfo & mi, Dimension & dim) const
{
	FontInfo tmpfont = mi.base.font;
	mi.base.font = mi.base.bv->buffer().params().getFont().fontInfo();
	labelwidth_ = theFontMetrics(mi.base.font).width(full_label_);
	// add some space to separate the label from the inset text
	labelwidth_ += leftOffset(mi.base.bv) + rightOffset(mi.base.bv);
	dim.wid = labelwidth_;
	Dimension textdim;
	// Correct for button and label width
	mi.base.textwidth -= dim.wid;
	InsetText::metrics(mi, textdim);
	mi.base.font = tmpfont;
	mi.base.textwidth += dim.wid;
	dim.des = max(dim.des - textdim.asc + dim.asc, textdim.des);
	dim.asc = textdim.asc;
	dim.wid += textdim.wid;
}


void InsetCaption::drawBackground(PainterInfo & pi, int x, int y) const
{
	TextMetrics & tm = pi.base.bv->textMetrics(&text());
	int const h = tm.height() + topOffset(pi.base.bv) + bottomOffset(pi.base.bv);
	int const yy = y - topOffset(pi.base.bv) - tm.ascent();
	if (rtl_)
		x+= + dimension(*pi.base.bv).wid - labelwidth_;
	pi.pain.fillRectangle(x, yy, labelwidth_, h, pi.backgroundColor(this));
}


void InsetCaption::draw(PainterInfo & pi, int x, int y) const
{
	// We must draw the label, we should get the label string
	// from the enclosing float inset.
	// The question is: Who should draw the label, the caption inset,
	// the text inset or the paragraph?
	// We should also draw the float number (Lgb)

	// Answer: the text inset (in buffer_funcs.cpp: setCaption).

	rtl_ = !pi.ltr_pos;
	FontInfo tmpfont = pi.base.font;
	pi.base.font = pi.base.bv->buffer().params().getFont().fontInfo();
	pi.base.font.setColor(pi.textColor(pi.base.font.color()).baseColor);
	if (is_deleted_)
		pi.base.font.setStrikeout(FONT_ON);
	else if (isChanged() && lyxrc.ct_additions_underlined)
		pi.base.font.setUnderbar(FONT_ON);
	int const lo = leftOffset(pi.base.bv);
	if (rtl_) {
		InsetText::draw(pi, x, y);
		pi.pain.text(x + dimension(*pi.base.bv).wid - labelwidth_ + lo,
		             y, full_label_, pi.base.font);
	} else {
		pi.pain.text(x + lo, y, full_label_, pi.base.font);
		InsetText::draw(pi, x + labelwidth_, y);
	}
	pi.base.font = tmpfont;
}


void InsetCaption::edit(Cursor & cur, bool front, EntryDirection entry_from)
{
	cur.push(*this);
	InsetText::edit(cur, front, entry_from);
}


Inset * InsetCaption::editXY(Cursor & cur, int x, int y)
{
	cur.push(*this);
	return InsetText::editXY(cur, x, y);
}


bool InsetCaption::insetAllowed(InsetCode code) const
{
	switch (code) {
	// code that is not allowed in a caption
	case CAPTION_CODE:
	case FLOAT_CODE:
	case FOOT_CODE:
	case NEWPAGE_CODE:
	case MARGIN_CODE:
	case MATHMACRO_CODE:
	case TABULAR_CODE:
	case WRAP_CODE:
		return false;
	default:
		return InsetText::insetAllowed(code);
	}
}


void InsetCaption::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		if (cmd.getArg(0) == "changetype") {
			cur.recordUndoInset(this);
			type_ = cmd.getArg(1);
			cur.forceBufferUpdate();
			break;
		}
	}
	// no "changetype":
	// fall through

	default:
		InsetText::doDispatch(cur, cmd);
		break;
	}
}


bool InsetCaption::getStatus(Cursor & cur, FuncRequest const & cmd,
	FuncStatus & status) const
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		string const first_arg = cmd.getArg(0);
		if (first_arg == "changetype") {
			string const type = cmd.getArg(1);
			status.setOnOff(type == type_);
			bool varia = type != "Unnumbered";
			// check if the immediate parent inset allows caption variation
			if (cur.depth() > 1) {
				varia = cur[cur.depth() - 2].inset().allowsCaptionVariation(type);
			}
			status.setEnabled(!is_subfloat_ && varia
					  && buffer().params().documentClass().hasInsetLayout(
						from_ascii("Caption:" + type)));
			return true;
		}
		return InsetText::getStatus(cur, cmd, status);
	}

	case LFUN_INSET_TOGGLE:
		// pass back to owner
		cur.undispatched();
		return false;

	default:
		return InsetText::getStatus(cur, cmd, status);
	}
}


void InsetCaption::latex(otexstream & os,
			 OutputParams const & runparams_in) const
{
	if (runparams_in.inFloat == OutputParams::SUBFLOAT)
		// caption is output as an optional argument
		return;
	// This is a bit too simplistic to take advantage of
	// caption options we must add more later. (Lgb)
	// This code is currently only able to handle the simple
	// \caption{...}, later we will make it take advantage
	// of the one of the caption packages. (Lgb)
	OutputParams runparams = runparams_in;
	// Some fragile commands (labels, index entries)
	// are output after the caption (#2154)
	runparams.postpone_fragile_stuff = buffer().masterParams().postpone_fragile_content;
	InsetText::latex(os, runparams);
	if (!runparams.post_macro.empty()) {
		// Output the stored fragile commands (labels, indices etc.)
		// that need to be output after the caption.
		os << runparams.post_macro;
		runparams.post_macro.clear();
	}
	// Backwards compatibility: We always had a linebreak after
	// the caption (see #8514)
	os << breakln;
	runparams_in.encoding = runparams.encoding;
}


int InsetCaption::plaintext(odocstringstream & os,
			    OutputParams const & runparams, size_t max_length) const
{
	os << '[' << full_label_ << "\n";
	InsetText::plaintext(os, runparams, max_length);
	os << "\n]";

	return PLAINTEXT_NEWLINE + 1; // one char on a separate line
}


void InsetCaption::docbook(XMLStream &, OutputParams const &) const
{
	// This function should never be called (rather InsetFloat::docbook, the titles should be skipped in floats).
}


docstring InsetCaption::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	if (rp.html_disable_captions)
		return docstring();
	InsetLayout const & il = getLayout();
	string const & tag = il.htmltag();
	string attr = il.htmlattr();
	if (!type_.empty()) {
		string const our_class = "float-caption-" + type_;
		size_t const loc = attr.find("class='");
		if (loc != string::npos)
			attr.insert(loc + 7, our_class + " ");
		else
			attr = attr + " class='" + our_class + "'";
	}
	xs << xml::StartTag(tag, attr);
	docstring def = getCaptionAsHTML(xs, rp);
	xs << xml::EndTag(tag);
	return def;
}


void InsetCaption::getArgument(otexstream & os,
			OutputParams const & runparams) const
{
	InsetLayout const & il = getLayout();

	if (!il.leftdelim().empty())
		os << il.leftdelim();

	OutputParams rp = runparams;
	if (isPassThru())
		rp.pass_thru = true;
	if (il.isNeedProtect())
		rp.moving_arg = true;
	if (il.isNeedMBoxProtect())
		++rp.inulemcmd;
	rp.par_begin = 0;
	rp.par_end = paragraphs().size();

	// Output the contents of the inset
	if (!paragraphs().empty())
		os.texrow().forceStart(paragraphs()[0].id(), 0);
	latexParagraphs(buffer(), text(), os, rp);
	runparams.encoding = rp.encoding;

	if (!il.rightdelim().empty())
		os << il.rightdelim();
}


int InsetCaption::getCaptionAsPlaintext(odocstream & os,
			OutputParams const & runparams) const
{
	os << full_label_ << ' ';
	odocstringstream ods;
	int const retval = InsetText::plaintext(ods, runparams);
	os << ods.str();
	return retval;
}


void InsetCaption::getCaptionAsDocBook(XMLStream & xs,
										 OutputParams const & runparams) const
{
	if (runparams.docbook_in_float)
		return;

	// Ignore full_label_, as the DocBook processor will deal with the numbering.
	InsetText::XHTMLOptions opts = InsetText::WriteInnerTag;
	InsetText::docbook(xs, runparams, opts);
}


docstring InsetCaption::getCaptionAsHTML(XMLStream & xs,
			OutputParams const & runparams) const
{
	xs << full_label_ << ' ';
	InsetText::XHTMLOptions const opts =
		InsetText::WriteLabel | InsetText::WriteInnerTag;
	return InsetText::insetAsXHTML(xs, runparams, opts);
}


void InsetCaption::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	Buffer const & master = *buffer().masterBuffer();
	DocumentClass const & tclass = master.params().documentClass();
	string const & lang = it.paragraph().getParLanguage(master.params())->code();
	Counters & cnts = tclass.counters();
	string const & type = cnts.current_float();
	if (utype == OutputUpdate) {
		// counters are local to the caption
		cnts.saveLastCounter();
	}
	is_deleted_ = deleted;
	// Memorize type for addToToc().
	floattype_ = type;
	if (type.empty() || type == "senseless")
		full_label_ = master.B_("Senseless!!! ");
	else {
		// FIXME: life would be _much_ simpler if listings was
		// listed in Floating.
		docstring name;
		if (type == "listing")
			name = master.B_("Listing");
		else
			name = master.B_(tclass.floats().getType(type).name());
		docstring counter = from_utf8(type);
		is_subfloat_ = cnts.isSubfloat();
		if (is_subfloat_) {
			// only standard captions allowed in subfloats
			type_ = "Standard";
			counter = "sub-" + from_utf8(type);
			name = bformat(_("Sub-%1$s"),
				       master.B_(tclass.floats().getType(type).name()));
		}
		docstring sec;
		docstring const lstring = getLayout().labelstring();
		docstring const labelstring = isAscii(lstring) ?
				master.B_(to_ascii(lstring)) : lstring;
		if (cnts.hasCounter(counter)) {
			int val = cnts.value(counter);
			// for longtables, we step the counter upstream
			if (!cnts.isLongtable())
				cnts.step(counter, utype);
			sec = cnts.theCounter(counter, lang);
			if (deleted && !cnts.isLongtable())
				// un-step after deleted counter
				cnts.set(counter, val);
		}
		if (labelstring != master.B_("standard")) {
			if (!sec.empty())
				sec += from_ascii(" ");
			sec += bformat(from_ascii("(%1$s)"), labelstring);
		}
		if (sec.empty())
			sec = from_ascii("#");
		full_label_ = bformat(master.B_("%1$s %2$s: [[Caption label (ex. Figure 1: )]]"), name, sec);
	}

	// Do the real work now.
	InsetText::updateBuffer(it, utype, deleted);
	if (utype == OutputUpdate)
		cnts.restoreLastCounter();
}


string InsetCaption::contextMenuName() const
{
	return "context-caption";
}


} // namespace lyx
