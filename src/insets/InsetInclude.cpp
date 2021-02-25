/**
 * \file InsetInclude.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author Richard Kimberly Heck (conversion to InsetCommand)
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetInclude.h"

#include "Buffer.h"
#include "buffer_funcs.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Converter.h"
#include "Cursor.h"
#include "Encoding.h"
#include "ErrorList.h"
#include "Exporter.h"
#include "Format.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "LaTeXFeatures.h"
#include "LayoutModuleList.h"
#include "LyX.h"
#include "MetricsInfo.h"
#include "output_plaintext.h"
#include "output_xhtml.h"
#include "texstream.h"
#include "TextClass.h"
#include "TocBackend.h"

#include "frontends/alert.h"
#include "frontends/Painter.h"

#include "graphics/PreviewImage.h"
#include "graphics/PreviewLoader.h"

#include "insets/InsetLabel.h"
#include "insets/InsetListingsParams.h"
#include "insets/RenderPreview.h"

#include "mathed/MacroTable.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/FileNameList.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h" // contains
#include "support/mutex.h"
#include "support/ExceptionMessage.h"

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace Alert = frontend::Alert;


namespace {

docstring const uniqueID()
{
	static unsigned int seed = 1000;
	static Mutex mutex;
	Mutex::Locker lock(&mutex);
	return "file" + convert<docstring>(++seed);
}


/// the type of inclusion
enum Types {
	INCLUDE, VERB, INPUT, VERBAST, LISTINGS, NONE
};


Types type(string const & s)
{
	if (s == "input")
		return INPUT;
	if (s == "verbatiminput")
		return VERB;
	if (s == "verbatiminput*")
		return VERBAST;
	if (s == "lstinputlisting" || s == "inputminted")
		return LISTINGS;
	if (s == "include")
		return INCLUDE;
	return NONE;
}


Types type(InsetCommandParams const & params)
{
	return type(params.getCmdName());
}


bool isListings(InsetCommandParams const & params)
{
	return type(params) == LISTINGS;
}


bool isVerbatim(InsetCommandParams const & params)
{
	Types const t = type(params);
	return t == VERB || t == VERBAST;
}


bool isInputOrInclude(InsetCommandParams const & params)
{
	Types const t = type(params);
	return t == INPUT || t == INCLUDE;
}


FileName const masterFileName(Buffer const & buffer)
{
	return buffer.masterBuffer()->fileName();
}


void add_preview(RenderMonitoredPreview &, InsetInclude const &, Buffer const &);


string const parentFileName(Buffer const & buffer)
{
	return buffer.absFileName();
}


FileName const includedFileName(Buffer const & buffer,
			      InsetCommandParams const & params)
{
	return makeAbsPath(ltrim(to_utf8(params["filename"])),
			onlyPath(parentFileName(buffer)));
}


InsetLabel * createLabel(Buffer * buf, docstring const & label_str)
{
	if (label_str.empty())
		return nullptr;
	InsetCommandParams icp(LABEL_CODE);
	icp["name"] = label_str;
	return new InsetLabel(buf, icp);
}


char_type replaceCommaInBraces(docstring & params)
{
	// Code point from private use area
	char_type private_char = 0xE000;
	int count = 0;
	for (char_type & c : params) {
		if (c == '{')
			++count;
		else if (c == '}')
			--count;
		else if (c == ',' && count)
			c = private_char;
	}
	return private_char;
}

} // namespace


InsetInclude::InsetInclude(Buffer * buf, InsetCommandParams const & p)
	: InsetCommand(buf, p), include_label(uniqueID()),
	  preview_(make_unique<RenderMonitoredPreview>(this)), failedtoload_(false),
	  label_(nullptr), child_buffer_(nullptr), file_exist_(false),
	  recursion_error_(false)
{
	preview_->connect([this](){ fileChanged(); });

	if (isListings(params())) {
		InsetListingsParams listing_params(to_utf8(p["lstparams"]));
		label_ = createLabel(buffer_, from_utf8(listing_params.getParamValue("label")));
	} else if (isInputOrInclude(params()) && buf)
		loadIfNeeded();
}


InsetInclude::InsetInclude(InsetInclude const & other)
	: InsetCommand(other), include_label(other.include_label),
	  preview_(make_unique<RenderMonitoredPreview>(this)), failedtoload_(false),
	  label_(nullptr), child_buffer_(nullptr),
	  file_exist_(other.file_exist_),recursion_error_(other.recursion_error_)
{
	preview_->connect([this](){ fileChanged(); });

	if (other.label_)
		label_ = new InsetLabel(*other.label_);
}


InsetInclude::~InsetInclude()
{
	delete label_;
}


void InsetInclude::setBuffer(Buffer & buffer)
{
	InsetCommand::setBuffer(buffer);
	if (label_)
		label_->setBuffer(buffer);
}


void InsetInclude::setChildBuffer(Buffer * buffer)
{
	child_buffer_ = buffer;
}


ParamInfo const & InsetInclude::findInfo(string const & /* cmdName */)
{
	// FIXME
	// This is only correct for the case of listings, but it'll do for now.
	// In the other cases, this second parameter should just be empty.
	static ParamInfo param_info_;
	if (param_info_.empty()) {
		param_info_.add("filename", ParamInfo::LATEX_REQUIRED);
		param_info_.add("lstparams", ParamInfo::LATEX_OPTIONAL);
		param_info_.add("literal", ParamInfo::LYX_INTERNAL);
	}
	return param_info_;
}


bool InsetInclude::isCompatibleCommand(string const & s)
{
	return type(s) != NONE;
}


bool InsetInclude::needsCProtection(bool const /*maintext*/, bool const fragile) const
{
	// We need to \cprotect all types in fragile context
	return fragile;
}


void InsetInclude::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_EDIT: {
		editIncluded(ltrim(to_utf8(params()["filename"])));
		break;
	}

	case LFUN_INSET_MODIFY: {
		// It should be OK just to invalidate the cache in setParams()
		// If not....
		// child_buffer_ = 0;
		InsetCommandParams p(INCLUDE_CODE);
		if (cmd.getArg(0) == "changetype") {
			cur.recordUndo();
			InsetCommand::doDispatch(cur, cmd);
			p = params();
		} else
			InsetCommand::string2params(to_utf8(cmd.argument()), p);
		if (!p.getCmdName().empty()) {
			if (isListings(p)){
				InsetListingsParams new_params(to_utf8(p["lstparams"]));
				docstring const new_label =
					from_utf8(new_params.getParamValue("label"));

				if (new_label.empty()) {
					delete label_;
					label_ = nullptr;
				} else {
					docstring old_label;
					if (label_)
						old_label = label_->getParam("name");
					else {
						label_ = createLabel(buffer_, new_label);
						label_->setBuffer(buffer());
					}

					if (new_label != old_label) {
						label_->updateLabelAndRefs(new_label, &cur);
						// the label might have been adapted (duplicate)
						if (new_label != label_->getParam("name")) {
							new_params.addParam("label", "{" +
								to_utf8(label_->getParam("name")) + "}", true);
							p["lstparams"] = from_utf8(new_params.params());
						}
					}
				}
			}
			cur.recordUndo();
			setParams(p);
			cur.forceBufferUpdate();
		} else
			cur.noScreenUpdate();
		break;
	}

	case LFUN_MOUSE_RELEASE: {
		if (cmd.modifier() == ControlModifier) {
			FileName const incfile = includedFileName(buffer(), params());
			string const & incname = incfile.absFileName();
			editIncluded(incname);
			break;
		}
	}
	// fall through

	//pass everything else up the chain
	default:
		InsetCommand::doDispatch(cur, cmd);
		break;
	}
}


void InsetInclude::editIncluded(string const & f)
{
	if (isLyXFileName(f)) {
		FuncRequest fr(LFUN_BUFFER_CHILD_OPEN, f);
		lyx::dispatch(fr);
	} else
		// tex file or other text file in verbatim mode
		theFormats().edit(buffer(),
			support::makeAbsPath(f, support::onlyPath(buffer().absFileName())),
			"text");
}


bool InsetInclude::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {

	case LFUN_INSET_EDIT:
		flag.setEnabled(true);
		return true;

	case LFUN_INSET_MODIFY:
		if (cmd.getArg(0) == "changetype")
			return InsetCommand::getStatus(cur, cmd, flag);
		else
			flag.setEnabled(true);
		return true;

	default:
		return InsetCommand::getStatus(cur, cmd, flag);
	}
}


void InsetInclude::setParams(InsetCommandParams const & p)
{
	// invalidate the cache
	child_buffer_ = nullptr;

	// reset in order to allow loading new file
	failedtoload_ = false;
	recursion_error_ = false;

	InsetCommand::setParams(p);

	if (preview_->monitoring())
		preview_->stopMonitoring();

	if (type(params()) == INPUT)
		add_preview(*preview_, *this, buffer());
}


bool InsetInclude::isChildIncluded() const
{
	std::list<std::string> includeonlys =
		buffer().params().getIncludedChildren();
	if (includeonlys.empty())
		return true;
	return (std::find(includeonlys.begin(),
			  includeonlys.end(),
			  ltrim(to_utf8(params()["filename"]))) != includeonlys.end());
}


docstring InsetInclude::screenLabel() const
{
	docstring pre = file_exist_ ? docstring() : _("MISSING:");

	docstring temp;

	switch (type(params())) {
		case INPUT:
			temp = buffer().B_("Input");
			break;
		case VERB:
			temp = buffer().B_("Verbatim");
			break;
		case VERBAST:
			temp = buffer().B_("Verbatim*");
			break;
		case INCLUDE:
			if (isChildIncluded())
				temp = buffer().B_("Include");
			else
				temp += buffer().B_("Include (excluded)");
			break;
		case LISTINGS:
			temp = listings_label_;
			break;
		case NONE:
			LASSERT(false, temp = buffer().B_("Unknown"));
			break;
	}

	temp += ": ";

	if (ltrim(params()["filename"]).empty())
		temp += "???";
	else
		temp += from_utf8(onlyFileName(ltrim(to_utf8(params()["filename"]))));

	return pre.empty() ? temp : pre + from_ascii(" ") + temp;
}


Buffer * InsetInclude::loadIfNeeded() const
{
	// This is for background export and preview. We don't even want to
	// try to load the cloned child document again.
	if (buffer().isClone())
		return child_buffer_;

	// Don't try to load it again if we failed before.
	if (failedtoload_ || isVerbatim(params()) || isListings(params()))
		return nullptr;

	FileName const included_file = includedFileName(buffer(), params());
	// Use cached Buffer if possible.
	if (child_buffer_ != nullptr) {
		if (theBufferList().isLoaded(child_buffer_)
		    // additional sanity check: make sure the Buffer really is
		    // associated with the file we want.
		    && child_buffer_ == theBufferList().getBuffer(included_file))
			return child_buffer_;
		// Buffer vanished, so invalidate cache and try to reload.
		child_buffer_ = nullptr;
	}

	if (!isLyXFileName(included_file.absFileName()))
		return nullptr;

	Buffer * child = theBufferList().getBuffer(included_file);
	if (checkForRecursiveInclude(child))
		return child;

	if (!child) {
		// the readonly flag can/will be wrong, not anymore I think.
		if (!included_file.exists()) {
			failedtoload_ = true;
			return nullptr;
		}

		child = theBufferList().newBuffer(included_file.absFileName());
		if (!child)
			// Buffer creation is not possible.
			return nullptr;

		buffer().pushIncludedBuffer(child);
		// Set parent before loading, such that macros can be tracked
		child->setParent(&buffer());

		if (child->loadLyXFile() != Buffer::ReadSuccess) {
			failedtoload_ = true;
			child->setParent(nullptr);
			//close the buffer we just opened
			theBufferList().release(child);
			buffer().popIncludedBuffer();
			return nullptr;
		}

		buffer().popIncludedBuffer();
		if (!child->errorList("Parse").empty()) {
			// FIXME: Do something.
		}
	} else {
		// The file was already loaded, so, simply
		// inform parent buffer about local macros.
		Buffer const * parent = &buffer();
		child->setParent(parent);
		MacroNameSet macros;
		child->listMacroNames(macros);
		MacroNameSet::const_iterator cit = macros.begin();
		MacroNameSet::const_iterator end = macros.end();
		for (; cit != end; ++cit)
			parent->usermacros.insert(*cit);
	}

	// Cache the child buffer.
	child_buffer_ = child;
	return child;
}


bool InsetInclude::checkForRecursiveInclude(
	Buffer const * cbuf, bool silent) const
{
	if (recursion_error_)
		return true;

	if (!buffer().isBufferIncluded(cbuf))
		return false;

	if (!silent) {
		docstring const msg = _("The file\n%1$s\n has attempted to include itself.\n"
			"The document set will not work properly until this is fixed!");
		frontend::Alert::warning(_("Recursive Include"),
			bformat(msg, from_utf8(cbuf->fileName().absFileName())));
	}
	recursion_error_ = true;
	return true;
}


void InsetInclude::latex(otexstream & os, OutputParams const & runparams) const
{
	string incfile = ltrim(to_utf8(params()["filename"]));

	// Warn if no file name has been specified
	if (incfile.empty()) {
		frontend::Alert::warning(_("No file name specified"),
			_("An included file name is empty.\n"
			   "Ignoring Inclusion"),
			true);
		return;
	}
	// Warn if file doesn't exist
	if (!includedFileExist()) {
		frontend::Alert::warning(_("Included file not found"),
			bformat(_("The included file\n"
				  "'%1$s'\n"
				  "has not been found. LyX will ignore the inclusion."),
				from_utf8(incfile)),
			true);
		 return;
	}

	FileName const included_file = includedFileName(buffer(), params());
	Buffer const * const masterBuffer = buffer().masterBuffer();

	if (runparams.inDeletedInset) {
		// We cannot strike-out whole children,
		// so we just output a note.
		os << "\\textbf{"
		   << bformat(buffer().B_("[INCLUDED FILE %1$s DELETED!]"),
			      from_utf8(included_file.onlyFileName()))
		   << "}";
		return;
	}

	// if incfile is relative, make it relative to the master
	// buffer directory.
	if (!FileName::isAbsolute(incfile)) {
		// FIXME UNICODE
		incfile = to_utf8(makeRelPath(from_utf8(included_file.absFileName()),
					      from_utf8(masterBuffer->filePath())));
	}

	string exppath = incfile;
	if (!runparams.export_folder.empty()) {
		exppath = makeAbsPath(exppath, runparams.export_folder).realPath();
	}

	// write it to a file (so far the complete file)
	string exportfile;
	string mangled;
	// bug 5681
	if (type(params()) == LISTINGS) {
		exportfile = exppath;
		mangled = DocFileName(included_file).mangledFileName();
	} else {
		exportfile = changeExtension(exppath, ".tex");
		mangled = DocFileName(changeExtension(included_file.absFileName(), ".tex")).
			mangledFileName();
	}

	if (!runparams.nice)
		incfile = mangled;
	else if (!runparams.silent)
		; // no warning wanted
	else if (!isValidLaTeXFileName(incfile)) {
		frontend::Alert::warning(_("Invalid filename"),
			_("The following filename will cause troubles "
				"when running the exported file through LaTeX: ") +
			from_utf8(incfile));
	} else if (!isValidDVIFileName(incfile)) {
		frontend::Alert::warning(_("Problematic filename for DVI"),
			_("The following filename can cause troubles "
				"when running the exported file through LaTeX "
				"and opening the resulting DVI: ") +
			from_utf8(incfile), true);
	}

	FileName const writefile(makeAbsPath(mangled, runparams.for_preview ?
						 buffer().temppath() : masterBuffer->temppath()));

	LYXERR(Debug::LATEX, "incfile:" << incfile);
	LYXERR(Debug::LATEX, "exportfile:" << exportfile);
	LYXERR(Debug::LATEX, "writefile:" << writefile);

	string const tex_format = flavor2format(runparams.flavor);

	switch (type(params())) {
	case VERB:
	case VERBAST: {
		incfile = latex_path(incfile);
		// FIXME UNICODE
		os << '\\' << from_ascii(params().getCmdName()) << '{'
		   << from_utf8(incfile) << '}';
		break;
	}
	case INPUT: {
		runparams.exportdata->addExternalFile(tex_format, writefile,
						      exportfile);

		// \input wants file with extension (default is .tex)
		if (!isLyXFileName(included_file.absFileName())) {
			incfile = latex_path(incfile);
			// FIXME UNICODE
			os << '\\' << from_ascii(params().getCmdName())
			   << '{' << from_utf8(incfile) << '}';
		} else {
			incfile = changeExtension(incfile, ".tex");
			incfile = latex_path(incfile);
			// FIXME UNICODE
			os << '\\' << from_ascii(params().getCmdName())
			   << '{' << from_utf8(incfile) <<  '}';
		}
		break;
	}
	case LISTINGS: {
		// Here, listings and minted have slightly different behaviors.
		// Using listings, it is always possible to have a caption,
		// even for non-floats. Using minted, only floats can have a
		// caption. So, with minted we use the following strategy.
		// If a caption was specified but the float parameter was not,
		// we ourselves add a caption above the listing (because the
		// listing comes from a file and might span several pages).
		// Otherwise, if float was specified, the floating listing
		// environment provided by minted is used. In either case, the
		// label parameter is taken as the label by which the float
		// can be referenced, otherwise it will have the meaning
		// intended by minted. In this last case, the label will
		// serve as a sort of caption that, however, will be shown
		// by minted only if the frame parameter is also specified.
		bool const use_minted = buffer().params().use_minted;
		runparams.exportdata->addExternalFile(tex_format, writefile,
						      exportfile);
		string const opt = to_utf8(params()["lstparams"]);
		// opt is set in QInclude dialog and should have passed validation.
		InsetListingsParams lstparams(opt);
		docstring parameters = from_utf8(lstparams.params());
		docstring language;
		docstring caption;
		docstring label;
		docstring placement;
		bool isfloat = lstparams.isFloat();
		// We are going to split parameters at commas, so
		// replace commas that are not parameter separators
		// with a code point from the private use area
		char_type comma = replaceCommaInBraces(parameters);
		// Get float placement, language, caption, and
		// label, then remove the relative options if minted.
		vector<docstring> opts =
			getVectorFromString(parameters, from_ascii(","), false);
		vector<docstring> latexed_opts;
		for (size_t i = 0; i < opts.size(); ++i) {
			// Restore replaced commas
			opts[i] = subst(opts[i], comma, ',');
			if (use_minted && prefixIs(opts[i], from_ascii("float"))) {
				if (prefixIs(opts[i], from_ascii("float=")))
					placement = opts[i].substr(6);
				opts.erase(opts.begin() + i--);
			} else if (use_minted && prefixIs(opts[i], from_ascii("language="))) {
				language = opts[i].substr(9);
				opts.erase(opts.begin() + i--);
			} else if (prefixIs(opts[i], from_ascii("caption="))) {
				caption = params().prepareCommand(runparams, trim(opts[i].substr(8), "{}"),
								  ParamInfo::HANDLING_LATEXIFY);
				opts.erase(opts.begin() + i--);
				if (!use_minted)
					latexed_opts.push_back(from_ascii("caption={") + caption + "}");
			} else if (prefixIs(opts[i], from_ascii("label="))) {
				label = params().prepareCommand(runparams, trim(opts[i].substr(6), "{}"),
								ParamInfo::HANDLING_ESCAPE);
				opts.erase(opts.begin() + i--);
				if (!use_minted)
					latexed_opts.push_back(from_ascii("label={") + label + "}");
			}
			if (use_minted && !label.empty()) {
				if (isfloat || !caption.empty())
					label = trim(label, "{}");
				else
					opts.push_back(from_ascii("label=") + label);
			}
		}
		if (!latexed_opts.empty())
			opts.insert(opts.end(), latexed_opts.begin(), latexed_opts.end());
		parameters = getStringFromVector(opts, from_ascii(","));
		if (language.empty())
			language = from_ascii("TeX");
		if (use_minted && isfloat) {
			os << breakln << "\\begin{listing}";
			if (!placement.empty())
				os << '[' << placement << "]";
			os << breakln;
		} else if (use_minted && !caption.empty()) {
			os << breakln << "\\lyxmintcaption[t]{" << caption;
			if (!label.empty())
				os << "\\label{" << label << "}";
			os << "}\n";
		}
		os << (use_minted ? "\\inputminted" : "\\lstinputlisting");
		if (!parameters.empty())
			os << "[" << parameters << "]";
		if (use_minted)
			os << '{'  << ascii_lowercase(language) << '}';
		os << '{'  << incfile << '}';
		if (use_minted && isfloat) {
			if (!caption.empty())
				os << breakln << "\\caption{" << caption << "}";
			if (!label.empty())
				os << breakln << "\\label{" << label << "}";
			os << breakln << "\\end{listing}\n";
		}
		break;
	}
	case INCLUDE: {
		runparams.exportdata->addExternalFile(tex_format, writefile,
						      exportfile);

		// \include don't want extension and demands that the
		// file really have .tex
		incfile = changeExtension(incfile, string());
		incfile = latex_path(incfile);
		// FIXME UNICODE
		os << '\\' << from_ascii(params().getCmdName()) << '{'
		   << from_utf8(incfile) << '}';
		break;
	}
	case NONE:
		break;
	}

	if (runparams.inComment || runparams.dryrun)
		// Don't try to load or copy the file if we're
		// in a comment or doing a dryrun
		return;

	if (!isInputOrInclude(params()) ||
		 !isLyXFileName(included_file.absFileName())) {
		// In this case, it's not a LyX file, so we copy the file
		// to the temp dir, so that .aux files etc. are not created
		// in the original dir. Files included by this file will be
		// found via either the environment variable TEXINPUTS, or
		// input@path, see ../Buffer.cpp.
		unsigned long const checksum_in  = included_file.checksum();
		unsigned long const checksum_out = writefile.checksum();
		if (checksum_in != checksum_out) {
			if (!included_file.copyTo(writefile)) {
				// FIXME UNICODE
				LYXERR(Debug::LATEX,
					to_utf8(bformat(_("Could not copy the file\n%1$s\n"
									"into the temporary directory."),
							 from_utf8(included_file.absFileName()))));
			}
		}
		return;
	}

		
	// it's a LyX file and we're inputting or including, so
	// try to load it so we can write the associated latex
	Buffer * tmp = loadIfNeeded();
	if (!tmp) {
		if (!runparams.silent) {
			docstring text = bformat(_("Could not load included "
				"file\n`%1$s'\n"
				"Please, check whether it actually exists."),
				included_file.displayName());
			throw ExceptionMessage(ErrorException, _("Error: "),
						   text);
		}
		return;
	}

	if (recursion_error_)
		return;

	if (!runparams.silent) {
		if (tmp->params().baseClass() != masterBuffer->params().baseClass()) {
			// FIXME UNICODE
			docstring text = bformat(_("Included file `%1$s'\n"
				"has textclass `%2$s'\n"
				"while parent file has textclass `%3$s'."),
				included_file.displayName(),
				from_utf8(tmp->params().documentClass().name()),
				from_utf8(masterBuffer->params().documentClass().name()));
			Alert::warning(_("Different textclasses"), text, true);
		}

		string const child_tf = tmp->params().useNonTeXFonts ? "true" : "false";
		string const master_tf = masterBuffer->params().useNonTeXFonts ? "true" : "false";
		if (tmp->params().useNonTeXFonts != masterBuffer->params().useNonTeXFonts) {
			docstring text = bformat(_("Included file `%1$s'\n"
				"has use-non-TeX-fonts set to `%2$s'\n"
				"while parent file has use-non-TeX-fonts set to `%3$s'."),
				included_file.displayName(),
				from_utf8(child_tf),
				from_utf8(master_tf));
			Alert::warning(_("Different use-non-TeX-fonts settings"), text, true);
		} 
		else if (tmp->params().inputenc != masterBuffer->params().inputenc) {
			docstring text = bformat(_("Included file `%1$s'\n"
				"uses input encoding \"%2$s\" [%3$s]\n"
				"while parent file uses input encoding \"%4$s\" [%5$s]."),
				included_file.displayName(),
				_(tmp->params().inputenc),
				from_utf8(tmp->params().encoding().guiName()),
				_(masterBuffer->params().inputenc),
				from_utf8(masterBuffer->params().encoding().guiName()));
			Alert::warning(_("Different LaTeX input encodings"), text, true);
		}

		// Make sure modules used in child are all included in master
		// FIXME It might be worth loading the children's modules into the master
		// over in BufferParams rather than doing this check.
		LayoutModuleList const masterModules = masterBuffer->params().getModules();
		LayoutModuleList const childModules = tmp->params().getModules();
		LayoutModuleList::const_iterator it = childModules.begin();
		LayoutModuleList::const_iterator end = childModules.end();
		for (; it != end; ++it) {
			string const module = *it;
			LayoutModuleList::const_iterator found =
				find(masterModules.begin(), masterModules.end(), module);
			if (found == masterModules.end()) {
				docstring text = bformat(_("Included file `%1$s'\n"
					"uses module `%2$s'\n"
					"which is not used in parent file."),
					included_file.displayName(), from_utf8(module));
				Alert::warning(_("Module not found"), text, true);
			}
		}
	}

	tmp->markDepClean(masterBuffer->temppath());

	// Don't assume the child's format is latex
	string const inc_format = tmp->params().bufferFormat();
	FileName const tmpwritefile(changeExtension(writefile.absFileName(),
		theFormats().extension(inc_format)));

	// FIXME: handle non existing files
	// The included file might be written in a different encoding
	// and language.
	Encoding const * const oldEnc = runparams.encoding;
	Language const * const oldLang = runparams.master_language;
	// If the master uses non-TeX fonts (XeTeX, LuaTeX),
	// the children must be encoded in plain utf8!
	if (masterBuffer->params().useNonTeXFonts)
		runparams.encoding = encodings.fromLyXName("utf8-plain");
	else if (oldEnc)
		runparams.encoding = oldEnc;
	else runparams.encoding = &tmp->params().encoding();
	runparams.master_language = buffer().params().language;
	runparams.par_begin = 0;
	runparams.par_end = tmp->paragraphs().size();
	runparams.is_child = true;
	Buffer::ExportStatus retval =
		tmp->makeLaTeXFile(tmpwritefile, masterFileName(buffer()).
			onlyPath().absFileName(), runparams, Buffer::OnlyBody);
	if (retval == Buffer::ExportKilled && buffer().isClone() &&
		  buffer().isExporting()) {
	  // We really shouldn't get here, I don't think.
	  LYXERR0("No conversion exception?");
		throw ConversionException();
	}
	else if (retval != Buffer::ExportSuccess) {
		if (!runparams.silent) {
			docstring msg = bformat(_("Included file `%1$s' "
				"was not exported correctly.\n "
				"LaTeX export is probably incomplete."),
				included_file.displayName());
			ErrorList const & el = tmp->errorList("Export");
			if (!el.empty())
				msg = bformat(from_ascii("%1$s\n\n%2$s\n\n%3$s"),
						msg, el.begin()->error, el.begin()->description);
			throw ExceptionMessage(ErrorException, _("Error: "), msg);
		}
	}
	runparams.encoding = oldEnc;
	runparams.master_language = oldLang;
	runparams.is_child = false;

	// If needed, use converters to produce a latex file from the child
	if (tmpwritefile != writefile) {
		ErrorList el;
		Converters::RetVal const conv_retval =
			theConverters().convert(tmp, tmpwritefile, writefile,
				included_file, inc_format, tex_format, el);
		if (conv_retval == Converters::KILLED && buffer().isClone() &&
			buffer().isExporting()) {
			// We really shouldn't get here, I don't think.
			LYXERR0("No conversion exception?");
			throw ConversionException();
		} else if (conv_retval != Converters::SUCCESS && !runparams.silent) {
			docstring msg = bformat(_("Included file `%1$s' "
					"was not exported correctly.\n "
					"LaTeX export is probably incomplete."),
					included_file.displayName());
			if (!el.empty())
				msg = bformat(from_ascii("%1$s\n\n%2$s\n\n%3$s"),
						msg, el.begin()->error, el.begin()->description);
			throw ExceptionMessage(ErrorException, _("Error: "), msg);
		}
	}
}


docstring InsetInclude::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	if (rp.inComment)
		 return docstring();

	// For verbatim and listings, we just include the contents of the file as-is.
	// In the case of listings, we wrap it in <pre>.
	bool const listing = isListings(params());
	if (listing || isVerbatim(params())) {
		if (listing)
			xs << xml::StartTag("pre");
		// FIXME: We don't know the encoding of the file, default to UTF-8.
		xs << includedFileName(buffer(), params()).fileContents("UTF-8");
		if (listing)
			xs << xml::EndTag("pre");
		return docstring();
	}

	// We don't (yet) know how to Input or Include non-LyX files.
	// (If we wanted to get really arcane, we could run some tex2html
	// converter on the included file. But that's just masochistic.)
	FileName const included_file = includedFileName(buffer(), params());
	if (!isLyXFileName(included_file.absFileName())) {
		if (!rp.silent)
			frontend::Alert::warning(_("Unsupported Inclusion"),
					 bformat(_("LyX does not know how to include non-LyX files when "
					           "generating HTML output. Offending file:\n%1$s"),
					            ltrim(params()["filename"])));
		return docstring();
	}

	// In the other cases, we will generate the HTML and include it.

	Buffer const * const ibuf = loadIfNeeded();
	if (!ibuf)
		return docstring();

	if (recursion_error_)
		return docstring();

	// are we generating only some paragraphs, or all of them?
	bool const all_pars = !rp.dryrun ||
			(rp.par_begin == 0 &&
			 rp.par_end == (int)buffer().text().paragraphs().size());

	OutputParams op = rp;
	if (all_pars) {
		op.par_begin = 0;
		op.par_end = 0;
		ibuf->writeLyXHTMLSource(xs.os(), op, Buffer::IncludedFile);
	} else {
		xs << XMLStream::ESCAPE_NONE << "<!-- Included file: ";
		xs << from_utf8(included_file.absFileName());
		xs << XMLStream::ESCAPE_NONE << " -->";
	}
	
	return docstring();
}


int InsetInclude::plaintext(odocstringstream & os,
        OutputParams const & op, size_t) const
{
	// just write the filename if we're making a tooltip or toc entry,
	// or are generating this for advanced search
	if (op.for_tooltip || op.for_toc || op.for_searchAdv != OutputParams::NoSearch) {
		os << '[' << screenLabel() << '\n'
		   << ltrim(getParam("filename")) << "\n]";
		return PLAINTEXT_NEWLINE + 1; // one char on a separate line
	}

	if (isVerbatim(params()) || isListings(params())) {
		if (op.for_searchAdv != OutputParams::NoSearch) {
			os << '[' << screenLabel() << ']';
		}
		else {
			os << '[' << screenLabel() << '\n'
			   // FIXME: We don't know the encoding of the file, default to UTF-8.
			   << includedFileName(buffer(), params()).fileContents("UTF-8")
			   << "\n]";
		}
		return PLAINTEXT_NEWLINE + 1; // one char on a separate line
	}

	Buffer const * const ibuf = loadIfNeeded();
	if (!ibuf) {
		docstring const str = '[' + screenLabel() + ']';
		os << str;
		return str.size();
	}

	if (recursion_error_)
		return 0;

	writePlaintextFile(*ibuf, os, op);
	return 0;
}


void InsetInclude::docbook(XMLStream & xs, OutputParams const & rp) const
{
	if (rp.inComment)
		return;

	// For verbatim and listings, we just include the contents of the file as-is.
	bool const verbatim = isVerbatim(params());
	bool const listing = isListings(params());
	if (listing || verbatim) {
		if (listing)
			xs << xml::StartTag("programlisting");
		else if (verbatim)
			xs << xml::StartTag("literallayout");

		// FIXME: We don't know the encoding of the file, default to UTF-8.
		xs << includedFileName(buffer(), params()).fileContents("UTF-8");

		if (listing)
			xs << xml::EndTag("programlisting");
		else if (verbatim)
			xs << xml::EndTag("literallayout");

		return;
	}

	// We don't know how to input or include non-LyX files. Input it as a comment.
	FileName const included_file = includedFileName(buffer(), params());
	if (!isLyXFileName(included_file.absFileName())) {
		if (!rp.silent)
			frontend::Alert::warning(_("Unsupported Inclusion"),
			                         bformat(_("LyX does not know how to process included non-LyX files when "
			                                   "generating DocBook output. The content of the file will be output as a "
									           "comment. Offending file:\n%1$s"),
			                                 ltrim(params()["filename"])));

		// Read the file, output it wrapped into comments.
		xs << XMLStream::ESCAPE_NONE << "<!-- Included file: ";
		xs << from_utf8(included_file.absFileName());
		xs << XMLStream::ESCAPE_NONE << " -->";

		xs << XMLStream::ESCAPE_NONE << "<!-- ";
		xs << included_file.fileContents("UTF-8");
		xs << XMLStream::ESCAPE_NONE << " -->";

		xs << XMLStream::ESCAPE_NONE << "<!-- End of included file: ";
		xs << from_utf8(included_file.absFileName());
		xs << XMLStream::ESCAPE_NONE << " -->";
	}

	// In the other cases, we generate the DocBook version and include it.
	Buffer const * const ibuf = loadIfNeeded();
	if (!ibuf)
		return;

	if (recursion_error_)
		return;

	// are we generating only some paragraphs, or all of them?
	bool const all_pars = !rp.dryrun ||
	                      (rp.par_begin == 0 &&
	                       rp.par_end == (int) buffer().text().paragraphs().size());

	OutputParams op = rp;
	if (all_pars) {
		op.par_begin = 0;
		op.par_end = 0;
		op.inInclude = true;
		op.docbook_in_par = false;
		ibuf->writeDocBookSource(xs.os(), op, Buffer::IncludedFile);
	} else {
		xs << XMLStream::ESCAPE_NONE << "<!-- Included file: ";
		xs << from_utf8(included_file.absFileName());
		xs << XMLStream::ESCAPE_NONE << " -->";
	}
}


void InsetInclude::validate(LaTeXFeatures & features) const
{
	LATTEST(&buffer() == &features.buffer());

	string incfile = ltrim(to_utf8(params()["filename"]));
	string const included_file =
		includedFileName(buffer(), params()).absFileName();

	string writefile;
	if (isLyXFileName(included_file))
		writefile = changeExtension(included_file, ".sgml");
	else
		writefile = included_file;

	if (!features.runparams().nice && !isVerbatim(params()) && !isListings(params())) {
		incfile = DocFileName(writefile).mangledFileName();
		writefile = makeAbsPath(incfile,
					buffer().masterBuffer()->temppath()).absFileName();
	}

	features.includeFile(include_label, writefile);

	features.useInsetLayout(getLayout());
	if (isVerbatim(params()))
		features.require("verbatim");
	else if (isListings(params())) {
		if (buffer().params().use_minted) {
			features.require("minted");
			string const opts = to_utf8(params()["lstparams"]);
			InsetListingsParams lstpars(opts);
			if (!lstpars.isFloat() && contains(opts, "caption="))
				features.require("lyxmintcaption");
		} else
			features.require("listings");
	}

	// Here we must do the fun stuff...
	// Load the file in the include if it needs
	// to be loaded:
	Buffer * const tmp = loadIfNeeded();
	if (!tmp) 
		return;

	// the file is loaded
	if (checkForRecursiveInclude(tmp))
		return;
	buffer().pushIncludedBuffer(tmp);

	// We must temporarily change features.buffer,
	// otherwise it would always be the master buffer,
	// and nested includes would not work.
	features.setBuffer(*tmp);
	// Maybe this is already a child
	bool const is_child =
		features.runparams().is_child;
	features.runparams().is_child = true;
	tmp->validate(features);
	features.runparams().is_child = is_child;
	features.setBuffer(buffer());
	
	buffer().popIncludedBuffer();
}


void InsetInclude::collectBibKeys(InsetIterator const & /*di*/, FileNameList & checkedFiles) const
{
	Buffer * ibuf = loadIfNeeded();
	if (!ibuf)
		return;

	if (checkForRecursiveInclude(ibuf))
		return;
	buffer().pushIncludedBuffer(ibuf);
	ibuf->collectBibKeys(checkedFiles);
	buffer().popIncludedBuffer();	
}


bool InsetInclude::inheritFont() const
{
	return !isVerbatim(params());
}


void InsetInclude::metrics(MetricsInfo & mi, Dimension & dim) const
{
	LBUFERR(mi.base.bv);

	bool use_preview = false;
	if (RenderPreview::previewText()) {
		graphics::PreviewImage const * pimage =
			preview_->getPreviewImage(mi.base.bv->buffer());
		use_preview = pimage && pimage->image();
	}

	if (use_preview) {
		preview_->metrics(mi, dim);
	} else {
		setBroken(!file_exist_ || recursion_error_);
		InsetCommand::metrics(mi, dim);
	}
}


void InsetInclude::draw(PainterInfo & pi, int x, int y) const
{
	LBUFERR(pi.base.bv);

	bool use_preview = false;
	if (RenderPreview::previewText()) {
		graphics::PreviewImage const * pimage =
			preview_->getPreviewImage(pi.base.bv->buffer());
		use_preview = pimage && pimage->image();
	}

	if (use_preview)
		preview_->draw(pi, x, y);
	else
		InsetCommand::draw(pi, x, y);
}


void InsetInclude::write(ostream & os) const
{
	params().Write(os, &buffer());
}


string InsetInclude::contextMenuName() const
{
	return "context-include";
}


Inset::RowFlags InsetInclude::rowFlags() const
{
	return type(params()) == INPUT ? Inline : Display;
}


docstring InsetInclude::layoutName() const
{
	if (isListings(params()))
		return from_ascii("IncludeListings");
	return InsetCommand::layoutName();
}


//
// preview stuff
//

void InsetInclude::fileChanged() const
{
	Buffer const * const buffer = updateFrontend();
	if (!buffer)
		return;

	preview_->removePreview(*buffer);
	add_preview(*preview_, *this, *buffer);
	preview_->startLoading(*buffer);
}


namespace {

bool preview_wanted(InsetCommandParams const & params, Buffer const & buffer)
{
	FileName const included_file = includedFileName(buffer, params);

	return type(params) == INPUT && params.preview() &&
		included_file.isReadableFile();
}


docstring latexString(InsetInclude const & inset)
{
	odocstringstream ods;
	otexstream os(ods);
	// We don't need to set runparams.encoding since this will be done
	// by latex() anyway.
	OutputParams runparams(nullptr);
	runparams.flavor = Flavor::LaTeX;
	runparams.for_preview = true;
	inset.latex(os, runparams);

	return ods.str();
}


void add_preview(RenderMonitoredPreview & renderer, InsetInclude const & inset,
		 Buffer const & buffer)
{
	InsetCommandParams const & params = inset.params();
	if (RenderPreview::previewText() && preview_wanted(params, buffer)) {
		renderer.setAbsFile(includedFileName(buffer, params));
		docstring snippet;
		try {
			// InsetInclude::latex() throws if generation of LaTeX
			// fails, e.g. if lyx2lyx fails because file is too
			// new, or knitr fails.
			snippet = latexString(inset);
		} catch (...) {
			// remove current preview because it is likely
			// associated with the previous included file name
			renderer.removePreview(buffer);
			LYXERR0("Preview of include failed.");
			return;
		}
		renderer.addPreview(snippet, buffer);
	}
}

} // namespace


void InsetInclude::addPreview(DocIterator const & /*inset_pos*/,
	graphics::PreviewLoader & ploader) const
{
	Buffer const & buffer = ploader.buffer();
	if (!preview_wanted(params(), buffer))
		return;
	preview_->setAbsFile(includedFileName(buffer, params()));
	docstring const snippet = latexString(*this);
	preview_->addPreview(snippet, ploader);
}


void InsetInclude::addToToc(DocIterator const & cpit, bool output_active,
                            UpdateType utype, TocBackend & backend) const
{
	if (isListings(params())) {
		if (label_)
			label_->addToToc(cpit, output_active, utype, backend);
		TocBuilder & b = backend.builder("listing");
		b.pushItem(cpit, screenLabel(), output_active);
		InsetListingsParams p(to_utf8(params()["lstparams"]));
		b.argumentItem(from_utf8(p.getParamValue("caption")));
		b.pop();
		return;
	}
	if (isVerbatim(params())) {
		TocBuilder & b = backend.builder("child");
		b.pushItem(cpit, screenLabel(), output_active);
		b.pop();
		return;
	}
	// the common case
	Buffer const * const childbuffer = loadIfNeeded();

	TocBuilder & b = backend.builder("child");
	string const fname = ltrim(to_utf8(params()["filename"]));
	// mark non-existent file with MISSING
	docstring const str = (file_exist_ ? from_ascii("") : _("MISSING: "))
		+ from_utf8(onlyFileName(fname)) + " (" + from_utf8(fname) + ")";
	b.pushItem(cpit, str, output_active);
	b.pop();

	if (!childbuffer)
		return;

	if (checkForRecursiveInclude(childbuffer))
		return;
	buffer().pushIncludedBuffer(childbuffer);
	// Update the child's tocBackend. The outliner uses the master's, but
	// the navigation menu uses the child's.
	childbuffer->tocBackend().update(output_active, utype);
	// Include Tocs from children
	childbuffer->inset().addToToc(DocIterator(), output_active, utype,
								  backend);
	buffer().popIncludedBuffer();
	// Copy missing outliner names (though the user has been warned against
	// having different document class and module selection between master
	// and child).
	for (auto const & name
			 : childbuffer->params().documentClass().outlinerNames())
		backend.addName(name.first, translateIfPossible(name.second));
}


void InsetInclude::updateCommand()
{
	if (!label_)
		return;

	docstring old_label = label_->getParam("name");
	label_->updateLabel(old_label);
	// the label might have been adapted (duplicate)
	docstring new_label = label_->getParam("name");
	if (old_label == new_label)
		return;

	// update listings parameters...
	InsetCommandParams p(INCLUDE_CODE);
	p = params();
	InsetListingsParams par(to_utf8(params()["lstparams"]));
	par.addParam("label", "{" + to_utf8(new_label) + "}", true);
	p["lstparams"] = from_utf8(par.params());
	setParams(p);
}


void InsetInclude::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	file_exist_ = includedFileExist();
	Buffer const * const childbuffer = loadIfNeeded();
	if (childbuffer) {
		if (!checkForRecursiveInclude(childbuffer))
			childbuffer->updateBuffer(Buffer::UpdateChildOnly, utype);
		return;
	}

	if (!isListings(params()))
		return;

	if (label_)
		label_->updateBuffer(it, utype, deleted);

	InsetListingsParams const par(to_utf8(params()["lstparams"]));
	if (par.getParamValue("caption").empty()) {
		listings_label_ = buffer().B_("Program Listing");
		return;
	}
	Buffer const & master = *buffer().masterBuffer();
	Counters & counters = master.params().documentClass().counters();
	docstring const cnt = from_ascii("listing");
	listings_label_ = master.B_("Program Listing");
	if (counters.hasCounter(cnt)) {
		counters.step(cnt, utype);
		listings_label_ += " " + convert<docstring>(counters.value(cnt));
	}
}


bool InsetInclude::includedFileExist() const
{
	// check whether the included file exist
	string incFileName = ltrim(to_utf8(params()["filename"]));
	FileName fn =
		support::makeAbsPath(incFileName,
				     support::onlyPath(buffer().absFileName()));
	return fn.exists();
}

} // namespace lyx
