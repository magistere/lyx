// -*- C++ -*-
/**
 * \file InsetCommand.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author Lars Gullik Bjønnes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef INSETCOMMAND_H
#define INSETCOMMAND_H

#include "Inset.h"
#include "InsetCommandParams.h"
#include "RenderButton.h"


namespace lyx {

class Cursor;

/////////////////////////////////////////////////////////////////////////
//
// InsetCommand
//
/////////////////////////////////////////////////////////////////////////

// Created by Alejandro 970222
// Used to insert a LaTeX command automatically.

class InsetCommand : public Inset
{
public:
	///
	InsetCommand(Buffer *, InsetCommandParams const &);
	///
	InsetCommand(InsetCommand const & rhs);
	///
	InsetCommand & operator=(InsetCommand const & rhs);
	///
	virtual ~InsetCommand();
	///
	InsetCommand * asInsetCommand() override { return this; }
	///
	InsetCommand const * asInsetCommand() const override { return this; }

	/// \return true if params are successfully read
	static bool string2params(std::string const & data,
				  InsetCommandParams &);
	///
	static std::string params2string(InsetCommandParams const &);
	///
	InsetCommandParams const & params() const { return p_; }
	///
	void setParams(InsetCommandParams const &);
	///
	docstring const & getParam(std::string const & name) const;
	///
	void setParam(std::string const & name, docstring const & value);
	/// FIXME Remove
	docstring const getFirstNonOptParam() const { return p_.getFirstNonOptParam(); }
	///
	void setBroken(bool const b) const { broken_ = b; }
	///
	bool isBroken() const { return broken_; }

	/// \name Public functions inherited from Inset class
	//@{
	///
	void write(std::ostream & os) const override { p_.write(os); }
	///
	void read(Lexer & lex) override { p_.Read(lex, &buffer()); }
	///
	void doDispatch(Cursor & cur, FuncRequest & cmd) override;
	///
	bool getStatus(Cursor & cur, FuncRequest const & cmd, FuncStatus &) const override;
	///
	void metrics(MetricsInfo &, Dimension &) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void drawBackground(PainterInfo &, int, int) const override {}
	///
	void latex(otexstream &, OutputParams const &) const override;
	///
	int plaintext(odocstringstream & ods, OutputParams const & op,
	              size_t max_length = INT_MAX) const override;
	///
	void docbook(XMLStream &, OutputParams const &) const override;
	///
	void validate(LaTeXFeatures & features) const override;
	///
	bool setMouseHover(BufferView const * bv, bool mouse_hover) const override;
	///
	bool clickable(BufferView const &, int, int) const override { return hasSettings(); }
	//@}

protected:
	/// \name Methods relaying to the InsetCommandParams p_
	//@{
	///
	std::string contextMenuName() const override;
	///
	bool showInsetDialog(BufferView * bv) const override;
	//@}

protected:
	/// \name Functions relaying to the InsetCommandParams
	//@{
	/// Build the complete LaTeX command
	/// \see InsetCommandParams::getCommand
	docstring const getCommand(OutputParams const & rp) const
		{ return p_.getCommand(rp); }
	/// Return the command name
	/// \see InsetCommandParams::getCmdName
	std::string const & getCmdName() const { return p_.getCmdName(); }
	/// Set the name to \p n. This must be a known name. All parameters
	/// are cleared except those that exist also in the new command.
	/// What matters here is the parameter name, not position.
	/// \see InsetCommandParams::setCmdName
	void setCmdName(std::string const & n) { p_.setCmdName(n); }
	///
	void changeCmdName(std::string const & new_name);
	//@}

private:
	/// This should provide the text for the button
	virtual docstring screenLabel() const = 0;

	/// \name Static public methods obligated for InsetCommand derived classes
	//@{
	/// Return parameter information for command cmdName.
	/// Not implemented here. Must be implemented in derived class.
	static ParamInfo const & findInfo(std::string const & cmdName);
	/// Return default command for this inset.
	/// Not implemented here. Must be implemented in derived class.
	static std::string defaultCommand();
	/// Whether this is a command this inset can represent.
	/// Not implemented here. Must be implemented in derived class.
	static bool isCompatibleCommand(std::string const & cmd);
	//@}

	///
	InsetCommandParams p_;
	/// changes color when mouse enters/leaves this inset
	mutable std::map<BufferView const *, bool> mouse_hover_;
	///
	mutable RenderButton button_;
	///
	mutable bool broken_;
};

/// Decode InsetCommand considering Inset name and data.
bool decodeInsetParam(std::string const & name, std::string & data,
	Buffer const & buffer);

} // namespace lyx

#endif
