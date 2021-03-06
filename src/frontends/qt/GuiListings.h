// -*- C++ -*-
/**
 * \file GuiListings.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Bo Peng
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUILISTINGS_H
#define GUILISTINGS_H

#include "GuiDialog.h"
#include "ui_ListingsUi.h"
#include "insets/InsetListingsParams.h"

namespace lyx {
namespace frontend {

class GuiListings : public GuiDialog, public Ui::ListingsUi
{
	Q_OBJECT
public:
	GuiListings(GuiView & lv);
	/// get values from all the widgets and form a string
	std::string construct_params();
	/// validate listings parameters and return an error message, if any
	docstring validate_listings_params();
private Q_SLOTS:
	void change_adaptor();
	/// AFAIK, QValidator only works for QLineEdit so
	/// I have to validate listingsED (QTextEdit) manually.
	/// This function displays a hint or error message returned by
	/// validate_listings_params
	void setListingsMsg();
	/// turn off inline when float is clicked
	void on_floatCB_stateChanged(int state);
	/// turn off float when inline is clicked
	void on_inlineCB_stateChanged(int state);
	/// turn off numbering options when none is selected
	void on_numberSideCO_currentIndexChanged(int);
	/// show dialect when language is chosen
	void on_languageCO_currentIndexChanged(int);
private:
	/// return false if validate_listings_params returns error
	bool isValid() override;
	/// Apply changes
	void applyView() override;
	/// update
	void updateContents() override;
	///
	bool initialiseParams(std::string const & data) override;
	/// clean-up on hide.
	void clearParams() override;
	/// clean-up on hide.
	void dispatchParams() override;
	///
	bool isBufferDependent() const override { return true; }
	///
	void setParams(InsetListingsParams const &);

	///
	InsetListingsParams params_;
};

} // namespace frontend
} // namespace lyx

#endif // GUILISTINGS_H
