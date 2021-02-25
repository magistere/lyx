/**
 * \file GuiChanges.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Michael Gerz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiChanges.h"

#include "qt_helpers.h"

#include "Author.h"
#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Changes.h"
#include "Cursor.h"
#include "FuncRequest.h"
#include "LyXRC.h"

#include <QDateTime>
#include <QTextBrowser>


namespace lyx {
namespace frontend {


GuiChanges::GuiChanges(GuiView & lv)
	: GuiDialog(lv, "changes", qt_("Merge Changes"))
{
	setupUi(this);

	connect(buttonBox, SIGNAL(clicked(QAbstractButton *)),
		this, SLOT(slotButtonBox(QAbstractButton *)));
	connect(nextPB, SIGNAL(clicked()), this, SLOT(nextChange()));
	connect(previousPB, SIGNAL(clicked()), this, SLOT(previousChange()));
	connect(rejectPB, SIGNAL(clicked()), this, SLOT(rejectChange()));
	connect(acceptPB, SIGNAL(clicked()), this, SLOT(acceptChange()));

	bc().setPolicy(ButtonPolicy::NoRepeatedApplyReadOnlyPolicy);
	bc().setCancel(buttonBox->button(QDialogButtonBox::Cancel));
}


void GuiChanges::updateContents()
{
	bool const changesPresent = buffer().areChangesPresent();
	nextPB->setEnabled(changesPresent);
	previousPB->setEnabled(changesPresent);
	changeTB->setEnabled(changesPresent);

	Change const & c = bufferview()->getCurrentChange();
	bool const changePresent = c.type != Change::UNCHANGED;
	rejectPB->setEnabled(changePresent && !isBufferReadonly());
	acceptPB->setEnabled(changePresent && !isBufferReadonly());
	bool const inserted = c.type == Change::INSERTED;

	QString text;
	if (changePresent) {
		QString const author =
			toqstr(buffer().params().authors().get(c.author).nameAndEmail());
		if (!author.isEmpty())
			text += inserted ? qt_("Inserted by %1").arg(author)
					 : qt_("Deleted by %1").arg(author);

		QString const date =
			QLocale().toString(QDateTime::fromTime_t(c.changetime),
					QLocale::LongFormat);
		if (!date.isEmpty()) {
			if (!author.isEmpty())
				text += qt_(" on[[date]] %1").arg(date);
			else
				text += inserted ? qt_("Inserted on %1").arg(date)
						 : qt_("Deleted on %1").arg(date);
		}
		QString changedcontent = toqstr(bufferview()->cursor().selectionAsString(false));
		if (!changedcontent.isEmpty()) {
			text += ":<br><br><b>";
			if (inserted)
				text += "<u><span style=\"color:blue\">";
			else
				text += "<s><span style=\"color:red\">";
			text += changedcontent;
			if (inserted)
				text += "</u></span></b>";
			else
				text += "</s></span></b>";
		}
	}
	changeTB->setHtml(text);
}


void GuiChanges::nextChange()
{
	dispatch(FuncRequest(LFUN_CHANGE_NEXT));
}


void GuiChanges::previousChange()
{
	dispatch(FuncRequest(LFUN_CHANGE_PREVIOUS));
}


void GuiChanges::acceptChange()
{
	dispatch(FuncRequest(LFUN_CHANGE_ACCEPT));
	nextChange();
}


void GuiChanges::rejectChange()
{
	dispatch(FuncRequest(LFUN_CHANGE_REJECT));
	nextChange();
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiChanges.cpp"
