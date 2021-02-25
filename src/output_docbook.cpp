/**
 * \file output_docbook.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author José Matos
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "output_docbook.h"

#include "Buffer.h"
#include "buffer_funcs.h"
#include "BufferParams.h"
#include "Font.h"
#include "InsetList.h"
#include "Paragraph.h"
#include "ParagraphList.h"
#include "ParagraphParameters.h"
#include "xml.h"
#include "Text.h"
#include "TextClass.h"

#include "insets/InsetBibtex.h"
#include "insets/InsetBibitem.h"
#include "insets/InsetLabel.h"
#include "mathed/InsetMath.h"
#include "insets/InsetNote.h"

#include "support/debug.h"
#include "support/lassert.h"
#include "support/textutils.h"

#include <stack>
#include <iostream>
#include <algorithm>
#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace {

std::string fontToDocBookTag(xml::FontTypes type)
{
	switch (type) {
	case xml::FontTypes::FT_EMPH:
	case xml::FontTypes::FT_BOLD:
		return "emphasis";
	case xml::FontTypes::FT_NOUN:
		return "personname";
	case xml::FontTypes::FT_UBAR:
	case xml::FontTypes::FT_WAVE:
	case xml::FontTypes::FT_DBAR:
	case xml::FontTypes::FT_SOUT:
	case xml::FontTypes::FT_XOUT:
	case xml::FontTypes::FT_ITALIC:
	case xml::FontTypes::FT_UPRIGHT:
	case xml::FontTypes::FT_SLANTED:
	case xml::FontTypes::FT_SMALLCAPS:
	case xml::FontTypes::FT_ROMAN:
	case xml::FontTypes::FT_SANS:
		return "emphasis";
	case xml::FontTypes::FT_TYPE:
		return "code";
	case xml::FontTypes::FT_SIZE_TINY:
	case xml::FontTypes::FT_SIZE_SCRIPT:
	case xml::FontTypes::FT_SIZE_FOOTNOTE:
	case xml::FontTypes::FT_SIZE_SMALL:
	case xml::FontTypes::FT_SIZE_NORMAL:
	case xml::FontTypes::FT_SIZE_LARGE:
	case xml::FontTypes::FT_SIZE_LARGER:
	case xml::FontTypes::FT_SIZE_LARGEST:
	case xml::FontTypes::FT_SIZE_HUGE:
	case xml::FontTypes::FT_SIZE_HUGER:
	case xml::FontTypes::FT_SIZE_INCREASE:
	case xml::FontTypes::FT_SIZE_DECREASE:
		return "emphasis";
	default:
		return "";
	}
}


string fontToRole(xml::FontTypes type)
{
	// Specific fonts are achieved with roles. The only common ones are "" for basic emphasis,
	// and "bold"/"strong" for bold. With some specific options, other roles are copied into
	// HTML output (via the DocBook XSLT sheets); otherwise, if not recognised, they are just ignored.
	// Hence, it is not a problem to have many roles by default here.
	// See https://www.sourceware.org/ml/docbook/2003-05/msg00269.html
	switch (type) {
	case xml::FontTypes::FT_ITALIC:
	case xml::FontTypes::FT_EMPH:
		return "";
	case xml::FontTypes::FT_BOLD:
		return "bold";
	case xml::FontTypes::FT_NOUN: // Outputs a <person>
	case xml::FontTypes::FT_TYPE: // Outputs a <code>
		return "";
	case xml::FontTypes::FT_UBAR:
		return "underline";

	// All other roles are non-standard for DocBook.

	case xml::FontTypes::FT_WAVE:
		return "wave";
	case xml::FontTypes::FT_DBAR:
		return "dbar";
	case xml::FontTypes::FT_SOUT:
		return "sout";
	case xml::FontTypes::FT_XOUT:
		return "xout";
	case xml::FontTypes::FT_UPRIGHT:
		return "upright";
	case xml::FontTypes::FT_SLANTED:
		return "slanted";
	case xml::FontTypes::FT_SMALLCAPS:
		return "smallcaps";
	case xml::FontTypes::FT_ROMAN:
		return "roman";
	case xml::FontTypes::FT_SANS:
		return "sans";
	case xml::FontTypes::FT_SIZE_TINY:
		return "tiny";
	case xml::FontTypes::FT_SIZE_SCRIPT:
		return "size_script";
	case xml::FontTypes::FT_SIZE_FOOTNOTE:
		return "size_footnote";
	case xml::FontTypes::FT_SIZE_SMALL:
		return "size_small";
	case xml::FontTypes::FT_SIZE_NORMAL:
		return "size_normal";
	case xml::FontTypes::FT_SIZE_LARGE:
		return "size_large";
	case xml::FontTypes::FT_SIZE_LARGER:
		return "size_larger";
	case xml::FontTypes::FT_SIZE_LARGEST:
		return "size_largest";
	case xml::FontTypes::FT_SIZE_HUGE:
		return "size_huge";
	case xml::FontTypes::FT_SIZE_HUGER:
		return "size_huger";
	case xml::FontTypes::FT_SIZE_INCREASE:
		return "size_increase";
	case xml::FontTypes::FT_SIZE_DECREASE:
		return "size_decrease";
	default:
		return "";
	}
}


string fontToAttribute(xml::FontTypes type) {
	// If there is a role (i.e. nonstandard use of a tag), output the attribute. Otherwise, the sheer tag is sufficient
	// for the font.
	string role = fontToRole(type);
	if (!role.empty())
		return "role='" + role + "'";
	else
		return "";
}


// Higher-level convenience functions.

void openParTag(XMLStream & xs, const Paragraph * par, const Paragraph * prevpar, const OutputParams & runparams)
{
	if (par == prevpar)
		prevpar = nullptr;

	// If the previous paragraph is empty, don't consider it when opening wrappers.
	if (prevpar && prevpar->empty() && !prevpar->allowEmpty())
		prevpar = nullptr;

	// When should the wrapper be opened here? Only if the previous paragraph has the SAME wrapper tag
	// (usually, they won't have the same layout) and the CURRENT one allows merging.
	// The main use case is author information in several paragraphs: if the name of the author is the
	// first paragraph of an author, then merging with the previous tag does not make sense. Say the
	// next paragraph is the affiliation, then it should be output in the same <author> tag (different
	// layout, same wrapper tag).
	Layout const & lay = par->layout();
	bool openWrapper = lay.docbookwrappertag() != "NONE" && !runparams.docbook_ignore_wrapper;

	if (prevpar != nullptr && !runparams.docbook_ignore_wrapper) {
		Layout const & prevlay = prevpar->layout();
		if (prevlay.docbookwrappertag() != "NONE") {
			if (prevlay.docbookwrappertag() == lay.docbookwrappertag() &&
					prevlay.docbookwrapperattr() == lay.docbookwrapperattr())
				openWrapper = !lay.docbookwrappermergewithprevious();
			else
				openWrapper = true;
		}
	}

	// Main logic.
	if (openWrapper)
		xml::openTag(xs, lay.docbookwrappertag(), lay.docbookwrapperattr(), lay.docbookwrappertagtype());

	const string & tag = lay.docbooktag();
	if (tag != "NONE") {
		auto xmltag = xml::ParTag(tag, lay.docbookattr());
		if (!xs.isTagOpen(xmltag, 1)) { // Don't nest a paragraph directly in a paragraph.
			// TODO: required or not?
			// TODO: avoid creating a ParTag object just for this query...
			xml::openTag(xs, lay.docbooktag(), lay.docbookattr(), lay.docbooktagtype());
			xml::openTag(xs, lay.docbookinnertag(), lay.docbookinnerattr(), lay.docbookinnertagtype());
		}
	}

	xml::openTag(xs, lay.docbookitemwrappertag(), lay.docbookitemwrapperattr(), lay.docbookitemwrappertagtype());
	xml::openTag(xs, lay.docbookitemtag(), lay.docbookitemattr(), lay.docbookitemtagtype());
	xml::openTag(xs, lay.docbookiteminnertag(), lay.docbookiteminnerattr(), lay.docbookiteminnertagtype());
}


void closeParTag(XMLStream & xs, Paragraph const * par, Paragraph const * nextpar, const OutputParams & runparams)
{
	if (par == nextpar)
		nextpar = nullptr;

	// If the next paragraph is empty, don't consider it when closing wrappers.
	if (nextpar && nextpar->empty() && !nextpar->allowEmpty())
		nextpar = nullptr;

	// See comment in openParTag.
	Layout const & lay = par->layout();
	bool closeWrapper = lay.docbookwrappertag() != "NONE" && !runparams.docbook_ignore_wrapper;

	if (nextpar != nullptr && !runparams.docbook_ignore_wrapper) {
		Layout const & nextlay = nextpar->layout();
		if (nextlay.docbookwrappertag() != "NONE") {
			if (nextlay.docbookwrappertag() == lay.docbookwrappertag() &&
					nextlay.docbookwrapperattr() == lay.docbookwrapperattr())
				closeWrapper = !nextlay.docbookwrappermergewithprevious();
			else
				closeWrapper = true;
		}
	}

	// Main logic.
	xml::closeTag(xs, lay.docbookiteminnertag(), lay.docbookiteminnertagtype());
	xml::closeTag(xs, lay.docbookitemtag(), lay.docbookitemtagtype());
	xml::closeTag(xs, lay.docbookitemwrappertag(), lay.docbookitemwrappertagtype());
	xml::closeTag(xs, lay.docbookinnertag(), lay.docbookinnertagtype());
	xml::closeTag(xs, lay.docbooktag(), lay.docbooktagtype());
	if (closeWrapper)
		xml::closeTag(xs, lay.docbookwrappertag(), lay.docbookwrappertagtype());
}


void makeBibliography(
		Text const & text,
		Buffer const & buf,
		XMLStream & xs,
		OutputParams const & runparams,
		ParagraphList::const_iterator const & par)
{
	// If this is the first paragraph in a bibliography, open the bibliography tag.
	auto const * pbegin_before = text.paragraphs().getParagraphBefore(par);
	if (pbegin_before == nullptr || (pbegin_before && pbegin_before->layout().latextype != LATEX_BIB_ENVIRONMENT)) {
		xs << xml::StartTag("bibliography");
		xs << xml::CR();
	}

	// Start the precooked bibliography entry. This is very much like opening a paragraph tag.
	// Don't forget the citation ID!
	docstring attr;
	for (auto i = 0; i < par->size(); ++i) {
		Inset const *ip = par->getInset(i);
		if (!ip)
			continue;
		if (const auto * bibitem = dynamic_cast<const InsetBibitem*>(ip)) {
			auto id = xml::cleanID(bibitem->getParam("key"));
			attr = from_utf8("xml:id='") + id + from_utf8("'");
			break;
		}
	}
	xs << xml::StartTag(from_utf8("bibliomixed"), attr);

	// Generate the entry. Concatenate the different parts of the paragraph if any.
	auto const begin = text.paragraphs().begin();
	std::vector<docstring> pars_prepend;
	std::vector<docstring> pars;
	std::vector<docstring> pars_append;
	tie(pars_prepend, pars, pars_append) = par->simpleDocBookOnePar(buf, runparams, text.outerFont(std::distance(begin, par)), 0);

	for (auto & parXML : pars_prepend)
		xs << XMLStream::ESCAPE_NONE << parXML;
	for (auto & parXML : pars)
		xs << XMLStream::ESCAPE_NONE << parXML;
	for (auto & parXML : pars_append)
		xs << XMLStream::ESCAPE_NONE << parXML;

	// End the precooked bibliography entry.
	xs << xml::EndTag("bibliomixed");
	xs << xml::CR();

	// If this is the last paragraph in a bibliography, close the bibliography tag.
	auto const end = text.paragraphs().end();
	auto nextpar = par;
	++nextpar;
	bool endBibliography = nextpar == end || nextpar->layout().latextype != LATEX_BIB_ENVIRONMENT;

	if (endBibliography) {
		xs << xml::EndTag("bibliography");
		xs << xml::CR();
	}
}


void makeParagraph(
		Text const & text,
		Buffer const & buf,
		XMLStream & xs,
		OutputParams const & runparams,
		ParagraphList::const_iterator const & par)
{
	// Useful variables.
	auto const begin = text.paragraphs().begin();
	auto const end = text.paragraphs().end();
	auto prevpar = text.paragraphs().getParagraphBefore(par);

	// We want to open the paragraph tag if:
	//   (i) the current layout permits multiple paragraphs
	//  (ii) we are either not already inside a paragraph (HTMLIsBlock) OR
	//	   we are, but this is not the first paragraph
	//
	// But there is also a special case, and we first see whether we are in it.
	// We do not want to open the paragraph tag if this paragraph contains
	// only one item, and that item is "inline", i.e., not HTMLIsBlock (such
	// as a branch). On the other hand, if that single item has a font change
	// applied to it, then we still do need to open the paragraph.
	//
	// Obviously, this is very fragile. The main reason we need to do this is
	// because of branches, e.g., a branch that contains an entire new section.
	// We do not really want to wrap that whole thing in a <div>...</div>.
	bool special_case = false;
	Inset const *specinset = par->size() == 1 ? par->getInset(0) : nullptr;
	if (specinset && !specinset->getLayout().htmlisblock()) { // TODO: Convert htmlisblock to a DocBook parameter?
		Layout const &style = par->layout();
		FontInfo const first_font = style.labeltype == LABEL_MANUAL ?
									style.labelfont : style.font;
		FontInfo const our_font =
				par->getFont(buf.masterBuffer()->params(), 0,
							 text.outerFont(std::distance(begin, par))).fontInfo();

		if (first_font == our_font)
			special_case = true;
	}

	size_t nInsets = std::distance(par->insetList().begin(), par->insetList().end());
	auto parSize = (size_t) par->size();

	// Plain layouts must be ignored.
	special_case |= buf.params().documentClass().isPlainLayout(par->layout()) && !runparams.docbook_force_pars;
	// Equations do not deserve their own paragraph (DocBook allows them outside paragraphs).
	// Exception: any case that generates an <inlineequation> must still get a paragraph to be valid.
	special_case |= nInsets == parSize && std::all_of(par->insetList().begin(), par->insetList().end(), [](InsetList::Element inset) {
		return inset.inset && inset.inset->asInsetMath() && inset.inset->asInsetMath()->getType() != hullSimple;
	});

	// Things that should not get into their own paragraph. (Only valid for DocBook.)
	static std::set<InsetCode> lyxCodeSpecialCases = {
			TABULAR_CODE,
			FLOAT_CODE,
			BIBTEX_CODE, // Bibliographies cannot be in paragraphs. Bibitems should still be handled as paragraphs,
			// though (see makeParagraphBibliography).
			ERT_CODE, // ERTs are in comments, not paragraphs.
			LISTINGS_CODE,
			BOX_CODE,
			INCLUDE_CODE,
			NOMENCL_PRINT_CODE,
			TOC_CODE, // To be ignored in DocBook, the processor afterwards should deal with ToCs.
			NOTE_CODE // Notes do not produce any output.
	};
	auto isLyxCodeSpecialCase = [](InsetList::Element inset) {
		return lyxCodeSpecialCases.find(inset.inset->lyxCode()) != lyxCodeSpecialCases.end();
	};
	special_case |= nInsets == parSize && std::all_of(par->insetList().begin(), par->insetList().end(), isLyxCodeSpecialCase);

	// Flex elements (InsetLayout) have their own parameter to control the special case.
	auto isFlexSpecialCase = [](InsetList::Element inset) {
		if (inset.inset->lyxCode() != FLEX_CODE)
			return false;

		// Standard condition: check the parameter.
		if (inset.inset->getLayout().docbooknotinpara())
			return true;

		// If the parameter is not set, maybe the flex inset only contains things that should match the standard
		// condition. In this case, isLyxCodeSpecialCase must also check for bibitems...
		auto isLyxCodeSpecialCase = [](InsetList::Element inset) {
			return lyxCodeSpecialCases.find(inset.inset->lyxCode()) != lyxCodeSpecialCases.end() ||
					inset.inset->lyxCode() == BIBITEM_CODE;
		};
		if (InsetText * text = inset.inset->asInsetText()) {
			for (auto const & par : text->paragraphs()) {
				size_t nInsets = std::distance(par.insetList().begin(), par.insetList().end());
				auto parSize = (size_t) par.size();

				if (nInsets == 1 && par.insetList().begin()->inset->lyxCode() == BIBITEM_CODE)
					return true;
				if (nInsets != parSize)
					return false;
				if (!std::all_of(par.insetList().begin(), par.insetList().end(), isLyxCodeSpecialCase))
					return false;
			}
			return true;
		}

		// No case matched: give up.
		return false;
	};
	special_case |= nInsets == parSize && std::all_of(par->insetList().begin(), par->insetList().end(), isFlexSpecialCase);

	// Open a paragraph if it is allowed, we are not already within a paragraph, and the insets in the paragraph do
	// not forbid paragraphs (aka special cases).
	bool const open_par = runparams.docbook_make_pars
						  && !runparams.docbook_in_par
						  && !special_case;

	// We want to issue the closing tag if either:
	//   (i)  We opened it, and either docbook_in_par is false,
	//		or we're not in the last paragraph, anyway.
	//   (ii) We didn't open it and docbook_in_par is true,
	//		but we are in the first par, and there is a next par.
	bool const close_par = open_par && !runparams.docbook_in_par;

	// Determine if this paragraph has some real content. Things like new pages are not caught
	// by Paragraph::empty(), even though they do not generate anything useful in DocBook.
	// Thus, remove all spaces (including new lines: \r, \n) before checking for emptiness.
	// std::all_of allows doing this check without having to copy the string.
	// Open and close tags around each contained paragraph.
	auto nextpar = par;
	++nextpar;

	std::vector<docstring> pars_prepend;
	std::vector<docstring> pars;
	std::vector<docstring> pars_append;
	tie(pars_prepend, pars, pars_append) = par->simpleDocBookOnePar(buf, runparams, text.outerFont(distance(begin, par)), 0, nextpar == end, special_case);

	for (docstring const & parXML : pars_prepend)
	    xs << XMLStream::ESCAPE_NONE << parXML;
	for (docstring const & parXML : pars) {
		if (!xml::isNotOnlySpace(parXML))
			continue;

		if (open_par)
			openParTag(xs, &*par, prevpar, runparams);

		xs << XMLStream::ESCAPE_NONE << parXML;

		if (close_par)
			closeParTag(xs, &*par, (nextpar != end) ? &*nextpar : nullptr, runparams);
	}
	for (docstring const & parXML : pars_append)
	    xs << XMLStream::ESCAPE_NONE << parXML;
}


void makeEnvironment(Text const &text,
					 Buffer const &buf,
                     XMLStream &xs,
                     OutputParams const &runparams,
                     ParagraphList::const_iterator const & par)
{
	// Useful variables.
	auto const end = text.paragraphs().end();
	auto nextpar = par;
	++nextpar;

	// Special cases for listing-like environments provided in layouts. This is quite ad-hoc, but provides a useful
	// default. This should not be used by too many environments (only LyX-Code right now).
	// This would be much simpler if LyX-Code was implemented as InsetListings...
	bool mimicListing = false;
	bool ignoreFonts = false;
	if (par->layout().docbooktag() == "programlisting") {
		mimicListing = true;
		ignoreFonts = true;
	}

	// Output the opening tag for this environment, but only if it has not been previously opened (condition
	// implemented in openParTag).
	auto prevpar = text.paragraphs().getParagraphBefore(par);
	openParTag(xs, &*par, prevpar, runparams);

	// Generate the contents of this environment. There is a special case if this is like some environment.
	Layout const & style = par->layout();
	if (style.latextype == LATEX_COMMAND) {
		// Nothing to do (otherwise, infinite loops).
	} else if (style.latextype == LATEX_ENVIRONMENT) {
		// Generate the paragraph, if need be.
		std::vector<docstring> pars_prepend;
        std::vector<docstring> pars;
        std::vector<docstring> pars_append;
        tie(pars_prepend, pars, pars_append) = par->simpleDocBookOnePar(buf, runparams, text.outerFont(std::distance(text.paragraphs().begin(), par)), 0, false, ignoreFonts);

        for (docstring const & parXML : pars_prepend)
            xs << XMLStream::ESCAPE_NONE << parXML;
		if (mimicListing) {
			auto p = pars.begin();
			while (p != pars.end()) {
				xml::openTag(xs, par->layout().docbookiteminnertag(), par->layout().docbookiteminnerattr(),
				             par->layout().docbookiteminnertagtype());
				xs << XMLStream::ESCAPE_NONE << *p;
				xml::closeTag(xs, par->layout().docbookiteminnertag(), par->layout().docbookiteminnertagtype());
				++p;

				// Insert a new line after each "paragraph" (i.e. line in the listing), except for the last one.
				// Otherwise, there would one more new line in the output than in the LyX document.
				if (p != pars.end())
					xs << xml::CR();
			}
		} else {
			for (auto const & p : pars) {
				xml::openTag(xs, par->layout().docbookiteminnertag(), par->layout().docbookiteminnerattr(),
				             par->layout().docbookiteminnertagtype());
				xs << XMLStream::ESCAPE_NONE << p;
				xml::closeTag(xs, par->layout().docbookiteminnertag(), par->layout().docbookiteminnertagtype());
			}
		}
        for (docstring const & parXML : pars_append)
            xs << XMLStream::ESCAPE_NONE << parXML;
	} else {
		makeAny(text, buf, xs, runparams, par);
	}

	// Close the environment.
	closeParTag(xs, &*par, (nextpar != end) ? &*nextpar : nullptr, runparams);
}


ParagraphList::const_iterator findEndOfEnvironment(
		ParagraphList::const_iterator const & pstart,
		ParagraphList::const_iterator const & pend)
{
	// Copy-paste from XHTML. Should be factored out at some point...
	ParagraphList::const_iterator p = pstart;
	Layout const & bstyle = p->layout();
	size_t const depth = p->params().depth();
	for (++p; p != pend; ++p) {
		Layout const & style = p->layout();
		// It shouldn't happen that e.g. a section command occurs inside
		// a quotation environment, at a higher depth, but as of 6/2009,
		// it can happen. We pretend that it's just at lowest depth.
		if (style.latextype == LATEX_COMMAND)
			return p;

		// If depth is down, we're done
		if (p->params().depth() < depth)
			return p;

		// If depth is up, we're not done
		if (p->params().depth() > depth)
			continue;

		// FIXME I am not sure about the first check.
		// Surely we *could* have different layouts that count as
		// LATEX_PARAGRAPH, right?
		if (style.latextype == LATEX_PARAGRAPH || style != bstyle)
			return p;
	}
	return pend;
}


ParagraphList::const_iterator makeListEnvironment(Text const &text,
												  Buffer const &buf,
		                                          XMLStream &xs,
		                                          OutputParams const &runparams,
		                                          ParagraphList::const_iterator const & begin)
{
	// Useful variables.
	auto par = begin;
	auto const end = text.paragraphs().end();
	auto const envend = findEndOfEnvironment(par, end);

	// Output the opening tag for this environment.
	Layout const & envstyle = par->layout();
	xml::openTag(xs, envstyle.docbookwrappertag(), envstyle.docbookwrapperattr(), envstyle.docbookwrappertagtype());
	xml::openTag(xs, envstyle.docbooktag(), envstyle.docbookattr(), envstyle.docbooktagtype());

	// Handle the content of the list environment, item by item.
	while (par != envend) {
		// Skip this paragraph if it is both empty and the last one (otherwise, there may be deeper paragraphs after).
		auto nextpar = par;
		++nextpar;
		if (par->empty() && nextpar == envend)
			break;

		// Open the item wrapper.
		Layout const & style = par->layout();
		xml::openTag(xs, style.docbookitemwrappertag(), style.docbookitemwrapperattr(),
		             style.docbookitemwrappertagtype());

		// Generate the label, if need be. If it is taken from the text, sep != 0 and corresponds to the first
		// character after the label.
		pos_type sep = 0;
		if (style.labeltype != LABEL_NO_LABEL && style.docbookitemlabeltag() != "NONE") {
			if (style.labeltype == LABEL_MANUAL) {
				// Only variablelist gets here (or similar items defined as an extension in the layout).
				xml::openTag(xs, style.docbookitemlabeltag(), style.docbookitemlabelattr(),
				             style.docbookitemlabeltagtype());
				sep = 1 + par->firstWordDocBook(xs, runparams);
				xml::closeTag(xs, style.docbookitemlabeltag(), style.docbookitemlabeltagtype());
			} else {
				// Usual cases: maybe there is something specified at the layout level. Highly unlikely, though.
				docstring const lbl = par->params().labelString();

				if (!lbl.empty()) {
					xml::openTag(xs, style.docbookitemlabeltag(), style.docbookitemlabelattr(),
					             style.docbookitemlabeltagtype());
					xs << lbl;
					xml::closeTag(xs, style.docbookitemlabeltag(), style.docbookitemlabeltagtype());
				}
			}
		}

		// Open the item (after the wrapper and the label).
		xml::openTag(xs, style.docbookitemtag(), style.docbookitemattr(), style.docbookitemtagtype());

		// Generate the content of the item.
		if (sep < par->size()) {
            std::vector<docstring> pars_prepend;
            std::vector<docstring> pars;
            std::vector<docstring> pars_append;
            tie(pars_prepend, pars, pars_append) = par->simpleDocBookOnePar(buf, runparams,
			                                     text.outerFont(std::distance(text.paragraphs().begin(), par)), sep);
            for (docstring const & parXML : pars_prepend)
                xs << XMLStream::ESCAPE_NONE << parXML;
			for (auto &p : pars) {
				xml::openTag(xs, par->layout().docbookiteminnertag(), par->layout().docbookiteminnerattr(),
				             par->layout().docbookiteminnertagtype());
				xs << XMLStream::ESCAPE_NONE << p;
				xml::closeTag(xs, par->layout().docbookiteminnertag(), par->layout().docbookiteminnertagtype());
			}
            for (docstring const & parXML : pars_append)
                xs << XMLStream::ESCAPE_NONE << parXML;
		} else {
			// DocBook doesn't like emptiness.
			xml::compTag(xs, par->layout().docbookiteminnertag(), par->layout().docbookiteminnerattr(),
			             par->layout().docbookiteminnertagtype());
		}

		// If the next item is deeper, it must go entirely within this item (do it recursively).
		// By construction, with findEndOfEnvironment, depth can only stay constant or increase, never decrease.
		depth_type currentDepth = par->getDepth();
		++par;
		while (par != envend && par->getDepth() != currentDepth)
			par = makeAny(text, buf, xs, runparams, par);
		// Usually, this loop only makes one iteration, except in complex scenarios, like an item with a paragraph,
		// a list, and another paragraph; or an item with two types of list (itemise then enumerate, for instance).

		// Close the item.
		xml::closeTag(xs, style.docbookitemtag(), style.docbookitemtagtype());
		xml::closeTag(xs, style.docbookitemwrappertag(), style.docbookitemwrappertagtype());
	}

	// Close this environment in exactly the same way as it was opened.
	xml::closeTag(xs, envstyle.docbooktag(), envstyle.docbooktagtype());
	xml::closeTag(xs, envstyle.docbookwrappertag(), envstyle.docbookwrappertagtype());

	return envend;
}


void makeCommand(
		Text const & text,
		Buffer const & buf,
		XMLStream & xs,
		OutputParams const & runparams,
		ParagraphList::const_iterator const & par)
{
	// Useful variables.
	// Unlike XHTML, no need for labels, as they are handled by DocBook tags.
	auto const begin = text.paragraphs().begin();
	auto const end = text.paragraphs().end();
	auto nextpar = par;
	++nextpar;

	// Generate this command.
	auto prevpar = text.paragraphs().getParagraphBefore(par);

    std::vector<docstring> pars_prepend;
    std::vector<docstring> pars;
    std::vector<docstring> pars_append;
    tie(pars_prepend, pars, pars_append) = par->simpleDocBookOnePar(buf, runparams,text.outerFont(distance(begin, par)));

    for (docstring const & parXML : pars_prepend)
        xs << XMLStream::ESCAPE_NONE << parXML;

    openParTag(xs, &*par, prevpar, runparams);
	for (auto & parXML : pars)
		// TODO: decide what to do with openParTag/closeParTag in new lines.
		xs << XMLStream::ESCAPE_NONE << parXML;
    closeParTag(xs, &*par, (nextpar != end) ? &*nextpar : nullptr, runparams);

    for (docstring const & parXML : pars_append)
        xs << XMLStream::ESCAPE_NONE << parXML;
}


bool isLayoutSectioning(Layout const & lay)
{
	if (lay.docbooksection()) // Special case: some DocBook styles must be handled as sections.
		return true;
	else if (lay.category() == from_utf8("Sectioning") || lay.docbooktag() == "section") // Generic case.
		return lay.toclevel != Layout::NOT_IN_TOC;
	return false;
}


bool isLayoutSectioningOrSimilar(Layout const & lay)
{
	return isLayoutSectioning(lay) || lay.docbooktag() == "bridgehead";
}


using DocBookDocumentSectioning = tuple<bool, pit_type>;


struct DocBookInfoTag
{
	const set<pit_type> shouldBeInInfo;
	const set<pit_type> mustBeInInfo; // With the notable exception of the abstract!
	const set<pit_type> abstract;
	const bool abstractLayout;
	pit_type bpit;
	pit_type epit;

	DocBookInfoTag(const set<pit_type> & shouldBeInInfo, const set<pit_type> & mustBeInInfo,
				   const set<pit_type> & abstract, bool abstractLayout, pit_type bpit, pit_type epit) :
				   shouldBeInInfo(shouldBeInInfo), mustBeInInfo(mustBeInInfo), abstract(abstract),
				   abstractLayout(abstractLayout), bpit(bpit), epit(epit) {}
};


DocBookDocumentSectioning hasDocumentSectioning(ParagraphList const &paragraphs, pit_type bpit, pit_type const epit) {
	bool documentHasSections = false;

	while (bpit < epit) {
		Layout const &style = paragraphs[bpit].layout();
		documentHasSections |= isLayoutSectioningOrSimilar(style);

		if (documentHasSections)
			break;
		bpit += 1;
	}
	// Paragraphs before the first section: [ runparams.par_begin ; eppit )

	return make_tuple(documentHasSections, bpit);
}


bool hasOnlyNotes(Paragraph const & par)
{
	// Precondition: the paragraph is not empty. Otherwise, the function will always return true...
	for (int i = 0; i < par.size(); ++i)
		// If you find something that is not an inset (like actual text) or an inset that is not a note,
		// return false.
		if (!par.isInset(i) || par.getInset(i)->lyxCode() != NOTE_CODE)
			return false;

	// An empty paragraph may still require some output.
	if (par.layout().docbooksection())
		return false;

	// There should be really no content here.
	return true;
}


DocBookInfoTag getParagraphsWithInfo(ParagraphList const &paragraphs,
									 pit_type bpit, pit_type const epit,
									 // Typically, bpit is the beginning of the document and epit the end of the
									 // document *or* the first section.
									 bool documentHasSections,
									 bool detectUnlayoutedAbstract
									 // Whether paragraphs with no specific layout should be detected as abstracts.
									 // For inner sections, an abstract should only be detected if it has a specific
									 // layout. For others, anything that might look like an abstract should be sought.
									 ) {
	set<pit_type> shouldBeInInfo;
	set<pit_type> mustBeInInfo;
	set<pit_type> abstractWithLayout;
	set<pit_type> abstractNoLayout;

	// Find the first non empty paragraph by mutating bpit.
	while (bpit < epit) {
		Paragraph const &par = paragraphs[bpit];
		if (par.empty() || hasOnlyNotes(par))
			bpit += 1;
		else
			break;
	}

	// Traverse everything that might belong to <info>.
	bool hasAbstractLayout = false;
	static depth_type INVALID_DEPTH = 100000;
	depth_type abstractDepth = INVALID_DEPTH;
	pit_type cpit = bpit;
	for (; cpit < epit; ++cpit) {
		// Skip paragraphs that don't generate anything in DocBook.
		Paragraph const & par = paragraphs[cpit];
		Layout const &style = par.layout();
		if (hasOnlyNotes(par))
			continue;

		// There should never be any section here, except for the first paragraph (a title can be part of <info>).
		// (Just a sanity check: if this fails, this function could end up processing the whole document.)
		if (cpit != bpit && isLayoutSectioningOrSimilar(par.layout())) {
			LYXERR0("Assertion failed: section found in potential <info> paragraphs.");
			break;
		}

		// If this is marked as an abstract by the layout, put it in the right set.
		if (style.docbookabstract()) {
			hasAbstractLayout = true;
			abstractDepth = par.getDepth();
			abstractWithLayout.emplace(cpit);
			continue;
		}

		// Deeper paragraphs following the abstract must still be considered as part of the abstract.
		// For instance, this includes lists. There should not be any other kind of paragraph in between.
		if (abstractDepth != INVALID_DEPTH && style.docbookininfo() == "never") {
			if (par.getDepth() > abstractDepth) {
				abstractWithLayout.emplace(cpit);
				continue;
			}
			if (par.getDepth() == abstractDepth) {
				// This is not an abstract paragraph and it should not either be considered as part
				// of it. It breaks the rule that abstract paragraphs must follow each other.
				abstractDepth = INVALID_DEPTH;
				break;
			}
		}

		// Based on layout information, store this paragraph in one set: should be in <info>, must be,
		// or abstract (either because of layout or of position).
		if (style.docbookininfo() == "always")
			mustBeInInfo.emplace(cpit);
		else if (style.docbookininfo() == "maybe")
			shouldBeInInfo.emplace(cpit);
		else if (documentHasSections && !hasAbstractLayout && detectUnlayoutedAbstract &&
				(style.docbooktag() == "NONE" || style.docbooktag() == "para") &&
				style.docbookwrappertag() == "NONE")
			// In this case, it is very likely that style.docbookininfo() == "never"! Be extra careful
			// about anything that gets caught here. For instance, don't ake into account
			abstractNoLayout.emplace(cpit);
		else // This should definitely not be in <info>.
			break;
	}
	// Now, cpit points to the first paragraph that no more has things that could go in <info>.
	// bpit is the beginning of the <info> part.

	return DocBookInfoTag(shouldBeInInfo, mustBeInInfo,
					      hasAbstractLayout ? abstractWithLayout : abstractNoLayout,
					      hasAbstractLayout, bpit, cpit);
}

} // end anonymous namespace


std::set<const Inset *> gatherInfo(ParagraphList::const_iterator par)
{
	// This function has a structure highly similar to makeAny and its friends. It's only made to be called on what
	// should become the document's <abstract>.
	std::set<const Inset *> values;

	// If this kind of layout should be ignored, already leave.
	if (par->layout().docbooktag() == "IGNORE")
		return values;

	// If this should go in info, mark it as such. Dive deep into the abstract, as it may hide many things that
	// DocBook doesn't want to be inside the abstract.
	for (pos_type i = 0; i < par->size(); ++i) {
		if (par->getInset(i) && par->getInset(i)->asInsetText()) {
			InsetText const *inset = par->getInset(i)->asInsetText();

			if (inset->getLayout().docbookininfo() != "never") {
				values.insert(inset);
			} else {
				auto subpar = inset->paragraphs().begin();
				while (subpar != inset->paragraphs().end()) {
					auto subinfos = gatherInfo(subpar);
					for (auto & subinfo: subinfos)
						values.insert(subinfo);
					++subpar;
				}
			}
		}
	}

	return values;
}


ParagraphList::const_iterator makeAny(Text const &text,
                                      Buffer const &buf,
                                      XMLStream &xs,
                                      OutputParams const &runparams,
                                      ParagraphList::const_iterator par)
{
	bool ignoreParagraph = false;

	// If this kind of layout should be ignored, already leave.
	ignoreParagraph |= par->layout().docbooktag() == "IGNORE";

	// For things that should go into <info>, check the variable rp.docbook_generate_info. This does not apply to the
	// abstract itself.
	bool isAbstract = par->layout().docbookabstract() || par->layout().docbooktag() == "abstract";
	ignoreParagraph |= !isAbstract && par->layout().docbookininfo() != "never" && !runparams.docbook_generate_info;

	// Switch on the type of paragraph to call the right handler.
	if (!ignoreParagraph) {
		switch (par->layout().latextype) {
		case LATEX_COMMAND:
			makeCommand(text, buf, xs, runparams, par);
			break;
		case LATEX_ENVIRONMENT:
			makeEnvironment(text, buf, xs, runparams, par);
			break;
		case LATEX_LIST_ENVIRONMENT:
		case LATEX_ITEM_ENVIRONMENT:
			// Only case when makeAny() might consume more than one paragraph.
			return makeListEnvironment(text, buf, xs, runparams, par);
		case LATEX_PARAGRAPH:
			makeParagraph(text, buf, xs, runparams, par);
			break;
		case LATEX_BIB_ENVIRONMENT:
			makeBibliography(text, buf, xs, runparams, par);
			break;
		}
	}

	// For cases that are not lists, the next paragraph to handle is the next one.
	++par;
	return par;
}


xml::FontTag docbookStartFontTag(xml::FontTypes type)
{
	return xml::FontTag(from_utf8(fontToDocBookTag(type)), from_utf8(fontToAttribute(type)), type);
}


xml::EndFontTag docbookEndFontTag(xml::FontTypes type)
{
	return xml::EndFontTag(from_utf8(fontToDocBookTag(type)), type);
}


void outputDocBookInfo(
		Text const & text,
		Buffer const & buf,
		XMLStream & xs,
		OutputParams const & runparams,
		ParagraphList const & paragraphs,
		DocBookInfoTag const & info)
{
	// Perform an additional check on the abstract. Sometimes, there are many paragraphs that should go
	// into the abstract, but none generates actual content. Thus, first generate to a temporary stream,
	// then only create the <abstract> tag if these paragraphs generate some content.
	// This check must be performed *before* a decision on whether or not to output <info> is made.
	bool hasAbstract = !info.abstract.empty();
	docstring abstract;
	set<const Inset *> infoInsets; // Paragraphs that should go into <info>, but are hidden in an <abstract>
	// paragraph. (This happens for quite a few layouts, unfortunately.)

	if (hasAbstract) {
		// Generate the abstract XML into a string before further checks.
		// Usually, makeAny only generates one paragraph at a time. However, for the specific case of lists, it might
		// generate more than one paragraph, as indicated in the return value.
		odocstringstream os2;
		XMLStream xs2(os2);

		auto rp = runparams;
		rp.docbook_generate_info = false;
		rp.docbook_ignore_wrapper = true;

		set<pit_type> doneParas; // Paragraphs that have already been converted (mostly to deal with lists).
		for (auto const & p : info.abstract) {
			if (doneParas.find(p) == doneParas.end()) {
				auto oldPar = paragraphs.iterator_at(p);
				auto newPar = makeAny(text, buf, xs2, rp, oldPar);

				// Find insets that should go outside the abstract.
				auto subinfos = gatherInfo(oldPar);
				for (auto & subinfo: subinfos)
					infoInsets.insert(subinfo);

				// Insert the indices of all the paragraphs that were just generated (typically, one).
				// **Make the hypothesis that, when an abstract has a list, all its items are consecutive.**
				// Otherwise, makeAny and makeListEnvironment would have to be adapted too.
				pit_type id = p;
				while (oldPar != newPar) {
					doneParas.emplace(id);
					++oldPar;
					++id;
				}
			}
		}

		// Actually output the abstract if there is something to do. Don't count line feeds or spaces in this,
		// even though they must be properly output if there is some abstract.
		abstract = os2.str();
		docstring cleaned = abstract;
		cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), lyx::isSpace), cleaned.end());

		// Nothing? Then there is no abstract!
		if (cleaned.empty())
			hasAbstract = false;
	}

	// The abstract must go in <info>. Otherwise, decide whether to open <info> based on the layouts.
	bool needInfo = !info.mustBeInInfo.empty() || hasAbstract;

	// Start the <info> tag if required.
	if (needInfo) {
		xs.startDivision(false);
		xs << xml::StartTag("info");
		xs << xml::CR();
	}

	// Output the elements that should go in <info>.
	// - First, the title.
	for (auto pit : info.shouldBeInInfo) // Typically, the title: these elements are so important and ubiquitous
		// that mandating a wrapper like <info> would repel users. Thus, generate them first.
		makeAny(text, buf, xs, runparams, paragraphs.iterator_at(pit));
	// If there is no title, generate one (required for the document to be valid).
	// This code is called for the main document, for table cells, etc., so be precise in this condition.
	if (text.isMainText() && info.shouldBeInInfo.empty() && !runparams.inInclude) {
		xs << xml::StartTag("title");
		xs << "Untitled Document";
		xs << xml::EndTag("title");
		xs << xml::CR();
	}

	// - Then, other metadata.
	for (auto pit : info.mustBeInInfo)
		makeAny(text, buf, xs, runparams, paragraphs.iterator_at(pit));
	for (auto const * inset : infoInsets)
		inset->docbook(xs, runparams);

	// - Finally, always output the abstract as the last item of the <info>, as it requires special treatment
	// (especially if it contains several paragraphs that are empty).
	if (hasAbstract) {
		string tag = paragraphs[*info.abstract.begin()].layout().docbookforceabstracttag();
		if (tag == "NONE")
			tag = "abstract";

		if (!xs.isLastTagCR())
			xs << xml::CR();

		xs << xml::StartTag(tag);
		xs << xml::CR();
		xs << XMLStream::ESCAPE_NONE << abstract;
		xs << xml::EndTag(tag);
		xs << xml::CR();
	}

	// End the <info> tag if it was started.
	if (needInfo) {
		if (!xs.isLastTagCR())
			xs << xml::CR();

		xs << xml::EndTag("info");
		xs << xml::CR();
		xs.endDivision();
	}
}


void docbookSimpleAllParagraphs(
		Text const & text,
		Buffer const & buf,
		XMLStream & xs,
		OutputParams const & runparams)
{
	// Handle the given text, supposing it has no sections (i.e. a "simple" text). The input may vary in length
	// between a single paragraph to a whole document.
	pit_type const bpit = runparams.par_begin;
	pit_type const epit = runparams.par_end;
	ParagraphList const &paragraphs = text.paragraphs();

	// First, the <info> tag.
	DocBookInfoTag info = getParagraphsWithInfo(paragraphs, bpit, epit, false, true);
	outputDocBookInfo(text, buf, xs, runparams, paragraphs, info);

	// Then, the content. It starts where the <info> ends.
	auto par = paragraphs.iterator_at(info.epit);
	auto end = paragraphs.iterator_at(epit);
	while (par != end) {
		if (!hasOnlyNotes(*par))
			par = makeAny(text, buf, xs, runparams, par);
		else
			++par;
	}
}


void docbookParagraphs(Text const &text,
					   Buffer const &buf,
					   XMLStream &xs,
					   OutputParams const &runparams) {
	ParagraphList const &paragraphs = text.paragraphs();
	if (runparams.par_begin == runparams.par_end) {
		runparams.par_begin = 0;
		runparams.par_end = paragraphs.size();
	}
	pit_type bpit = runparams.par_begin;
	pit_type const epit = runparams.par_end;
	LASSERT(bpit < epit,
			{
				xs << XMLStream::ESCAPE_NONE << "<!-- DocBook output error! -->\n";
				return;
			});

	// Detect whether the document contains sections. If there are no sections, treatment is largely simplified.
	// In particular, there can't be an abstract, unless it is manually marked.
	bool documentHasSections;
	pit_type eppit;
	tie(documentHasSections, eppit) = hasDocumentSectioning(paragraphs, bpit, epit);

	// Deal with "simple" documents, i.e. those without sections.
	if (!documentHasSections) {
		docbookSimpleAllParagraphs(text, buf, xs, runparams);
		return;
	}

	// Output the first <info> tag (or just the title).
	DocBookInfoTag info = getParagraphsWithInfo(paragraphs, bpit, eppit, true, true);
	outputDocBookInfo(text, buf, xs, runparams, paragraphs, info);
	bpit = info.epit;

	// In the specific case of books, there must be parts or chapters. In some cases, star sections are used at the
	// beginning for many things like acknowledgements or licenses. DocBook has tags for many of these cases, but not
	// the LyX layouts... Gather everything in a <preface>, that's the closest in meaning.
	// This is only useful if the things after the <info> tag are not already parts or chapters!
	if (buf.params().documentClass().docbookroot() == "book") {
	    // Check the condition on the first few elements.
	    bool hasPreface = false;
	    pit_type pref_bpit = bpit;
	    pit_type pref_epit = bpit;

	    static const std::set<std::string> allowedElements = {
	            // List from https://tdg.docbook.org/tdg/5.2/book.html
	            "acknowledgements", "appendix", "article", "bibliography", "chapter", "colophon", "dedication",
	            "glossary", "index", "part", "preface", "reference", "toc"
	    };

	    for (; pref_epit < epit; ++pref_epit) {
            auto par = text.paragraphs().iterator_at(pref_epit);
            if (allowedElements.find(par->layout().docbooktag()) != allowedElements.end() ||
                    allowedElements.find(par->layout().docbooksectiontag()) != allowedElements.end())
                break;

            hasPreface = true;
	    }

	    // Output a preface if required. A title is needed for the document to be valid...
	    if (hasPreface) {
	        xs << xml::StartTag("preface");
	        xs << xml::CR();

	        xs << xml::StartTag("title");
	        xs << "Preface";
	        xs << xml::EndTag("title");
            xs << xml::CR();

            auto pref_par = text.paragraphs().iterator_at(pref_bpit);
            auto pref_end = text.paragraphs().iterator_at(pref_epit);
            while (pref_par != pref_end) {
                // Skip paragraphs not producing any output.
                if (hasOnlyNotes(*pref_par)) {
                    ++pref_par;
                    continue;
                }

                // TODO: must sections be handled here? If so, it might be useful to extract the corresponding loop
                // in the rest of this function to use the same here (and avoid copy-paste mistakes).
                pref_par = makeAny(text, buf, xs, runparams, pref_par);
            }

	        xs << xml::EndTag("preface");
            xs << xml::CR();

            // Skip what has just been generated in the preface.
            bpit = pref_epit;
	    }
	}

	std::stack<std::pair<int, string>> headerLevels; // Used to determine when to open/close sections: store the depth
	// of the section and the tag that was used to open it.

	// Then, iterate through the paragraphs of this document.
	auto par = text.paragraphs().iterator_at(bpit);
	auto end = text.paragraphs().iterator_at(epit);
	while (par != end) {
		// Skip paragraphs not producing any output.
		if (hasOnlyNotes(*par)) {
			++par;
			continue;
		}

		OutputParams ourparams = runparams;
		Layout const &style = par->layout();

		// Think about adding <section> and/or </section>s.
		if (isLayoutSectioning(style) || par->params().startOfAppendix()) {
			int level = style.toclevel;

			// Need to close a previous section if it has the same level or a higher one (close <section> if opening a
			// <h2> after a <h2>, <h3>, <h4>, <h5> or <h6>). More examples:
			//   - current: h2; back: h1; do not close any <section>
			//   - current: h1; back: h2; close two <section> (first the <h2>, then the <h1>, so a new <h1> can come)
			// Some layouts require that Layout::NOT_IN_TOC sections still cause closing of previous sections. This is
			// mostly to ensure that the section is positioned at a DocBook-compatible level (acknowledgements: cannot
			// be under a section!).
			while (!headerLevels.empty() && level <= headerLevels.top().first) {
				// Output the tag only if it corresponds to a legit section.
				int stackLevel = headerLevels.top().first;
				if (stackLevel != Layout::NOT_IN_TOC) {
					xs << xml::EndTag(headerLevels.top().second);
					xs << xml::CR();
				}
				headerLevels.pop();
			}

			// Open the new section: first push it onto the stack, then output it in DocBook.
			string sectionTag = (par->params().startOfAppendix()) ? "appendix" : style.docbooksectiontag();
			headerLevels.push(std::make_pair(level, sectionTag));

			// Some sectioning-like elements should not be output (such as FrontMatter).
			if (level != Layout::NOT_IN_TOC) {
				// Look for a label in the title, i.e. a InsetLabel as a child.
				docstring id = docstring();
				for (pos_type i = 0; i < par->size(); ++i) {
					Inset const *inset = par->getInset(i);
					if (inset) {
						if (auto label = dynamic_cast<InsetLabel const *>(inset)) {
							// Generate the attributes for the section if need be.
							id += "xml:id=\"" + xml::cleanID(label->screenLabel()) + "\"";

							// Don't output the ID as a DocBook <anchor>.
							ourparams.docbook_anchors_to_ignore.emplace(label->screenLabel());

							// Cannot have multiple IDs per tag. If there is another ID inset in the document, it will
							// be output as a DocBook anchor.
							break;
						}
					}
				}

				// Write the open tag for this section.
				docstring attrs;
				if (!id.empty())
					attrs = id;
				xs << xml::StartTag(sectionTag, attrs);
				xs << xml::CR();
			}
		}

		// Close all sections before the bibliography.
		// TODO: Only close all when the bibliography is at the end of the document? Or force to output the bibliography at the end of the document? Or don't care (as allowed by DocBook)?
		if (!par->insetList().empty()) {
			Inset const *firstInset = par->getInset(0);
			if (firstInset && (firstInset->lyxCode() == BIBITEM_CODE || firstInset->lyxCode() == BIBTEX_CODE)) {
				while (!headerLevels.empty()) {
					// Don't close appendices before bibliographies.
					if (headerLevels.top().second == "appendix")
						break;

					// Pop the section from the stack.
					int level = headerLevels.top().first;
					docstring tag = from_utf8("</" + headerLevels.top().second + ">");
					headerLevels.pop();

					// Output the tag only if it corresponds to a legit section, as the rest of the code.
					if (level != Layout::NOT_IN_TOC) {
						xs << XMLStream::ESCAPE_NONE << tag;
						xs << xml::CR();
					}
				}
			}
		}

		// Generate the <info> tag if a section was just opened.
		// Some sections may require abstracts (mostly parts, in books: DocBookForceAbstractTag will not be NONE),
		// others can still have an abstract (it must be detected so that it can be output at the right place).
		// TODO: docbookforceabstracttag is a bit contrived here, but it does the job. Having another field just for this would be cleaner, but that's just for <part> and <partintro>, so it's probably not worth the effort.
		if (isLayoutSectioning(style)) {
			// This abstract may be found between the next paragraph and the next title.
			pit_type cpit = std::distance(text.paragraphs().begin(), par);
			pit_type ppit = std::get<1>(hasDocumentSectioning(paragraphs, cpit + 1L, epit));

			// Generate this abstract (this code corresponds to parts of outputDocBookInfo).
			DocBookInfoTag secInfo = getParagraphsWithInfo(paragraphs, cpit, ppit, true,
												  style.docbookforceabstracttag() != "NONE");

			if (!secInfo.mustBeInInfo.empty() || !secInfo.shouldBeInInfo.empty() || !secInfo.abstract.empty()) {
				// Generate the <info>, if required. If DocBookForceAbstractTag != NONE, this abstract will not be in
				// <info>, unlike other ("standard") abstracts.
				bool hasStandardAbstract = !secInfo.abstract.empty() && style.docbookforceabstracttag() == "NONE";
				bool needInfo = !secInfo.mustBeInInfo.empty() || hasStandardAbstract;

				if (needInfo) {
					xs.startDivision(false);
					xs << xml::StartTag("info");
					xs << xml::CR();
				}

				// Output the elements that should go in <info>, before and after the abstract.
				for (auto pit : secInfo.shouldBeInInfo) // Typically, the title: these elements are so important and ubiquitous
					// that mandating a wrapper like <info> would repel users. Thus, generate them first.
					makeAny(text, buf, xs, ourparams, paragraphs.iterator_at(pit));
				for (auto pit : secInfo.mustBeInInfo)
					makeAny(text, buf, xs, ourparams, paragraphs.iterator_at(pit));

				// Deal with the abstract in <info> if it is standard (i.e. its tag is <abstract>).
				if (!secInfo.abstract.empty() && hasStandardAbstract) {
					if (!secInfo.abstractLayout) {
						xs << xml::StartTag("abstract");
						xs << xml::CR();
					}

					for (auto const &p : secInfo.abstract)
						makeAny(text, buf, xs, ourparams, paragraphs.iterator_at(p));

					if (!secInfo.abstractLayout) {
						xs << xml::EndTag("abstract");
						xs << xml::CR();
					}
				}

				// End the <info> tag if it was started.
				if (needInfo) {
					if (!xs.isLastTagCR())
						xs << xml::CR();

					xs << xml::EndTag("info");
					xs << xml::CR();
					xs.endDivision();
				}

				// Deal with the abstract outside <info> if it is not standard (i.e. its tag is layout-defined).
				if (!secInfo.abstract.empty() && !hasStandardAbstract) {
					// Assert: style.docbookforceabstracttag() != NONE.
					xs << xml::StartTag(style.docbookforceabstracttag());
					xs << xml::CR();
					for (auto const &p : secInfo.abstract)
						makeAny(text, buf, xs, ourparams, paragraphs.iterator_at(p));
					xs << xml::EndTag(style.docbookforceabstracttag());
					xs << xml::CR();
				}

				// Skip all the text that has just been generated.
				par = paragraphs.iterator_at(secInfo.epit);
			} else {
				// No <info> tag to generate, proceed as for normal paragraphs.
				par = makeAny(text, buf, xs, ourparams, par);
			}
		} else {
			// Generate this paragraph, as it has nothing special.
			par = makeAny(text, buf, xs, ourparams, par);
		}
	}

	// If need be, close <section>s, but only at the end of the document (otherwise, dealt with at the beginning
	// of the loop).
	while (!headerLevels.empty() && headerLevels.top().first > Layout::NOT_IN_TOC) {
		docstring tag = from_utf8("</" + headerLevels.top().second + ">");
		headerLevels.pop();
		xs << XMLStream::ESCAPE_NONE << tag;
		xs << xml::CR();
	}
}

} // namespace lyx