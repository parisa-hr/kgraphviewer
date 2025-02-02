/* This file is part of KGraphViewer.
   Copyright (C) 2005-2007 Gael de Chalendar <kleag@free.fr>

   KGraphViewer is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA
*/

/* This file was part of the KDE project
   Copyright (C) 2005 Jarosław Staniek <staniek@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
 */

#include "simpleprintingpagesetup.h"
#include "dotgraphview.h"
#include "kgraphviewerlib_debug.h"
#include "simpleprintingcommand.h"
#include "simpleprintingsettings.h"
#include "simpleprintpreviewwindow.h"
#include "ui_simpleprintingpagesetupbase.h"

#include <QApplication>
#include <QDebug>
#include <QFontDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QUrl>

#include <QRadioButton>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qslider.h>
#include <qtimer.h>
#include <qtooltip.h>
// Added by qt3to4:
#include <QPixmap>
#include <QVBoxLayout>
#include <iostream>
#include <klocalizedstring.h>

namespace KGraphViewer
{
KGVSimplePrintingPageSetup::KGVSimplePrintingPageSetup(KGVSimplePrintingCommand *command, DotGraphView *mainWin, QWidget *parent, QMap<QString, QString> *args)
    : QWidget(parent)
    , m_settings(command->engine()->settings())
    , m_command(command)
{
    setObjectName("KGVSimplePrintingPageSetup");
    // object to print
    bool ok = args;
    m_graphView = mainWin;
    ok = m_graphView;

    bool printPreview = false;
    bool print = false;
    bool pageSetup = false;
    if (ok) {
        printPreview = (*args)["action"] == "printPreview";
        print = (*args)["action"] == "print";
        pageSetup = (*args)["action"] == "pageSetup";
        ok = printPreview || print || pageSetup;
    }

    // settings
    //! @todo default?
    m_unit = QLocale().measurementSystem() == QLocale::MetricSystem ? KgvUnit::U_CM : KgvUnit::U_INCH;

    // GUI
    m_contents = new Ui::KGVSimplePrintingPageSetupBase();
    m_contents->setupUi(this);
    layout()->setMargin(0);

    setFocusPolicy(Qt::WheelFocus);
    // 	m_contents->setFocusProxy(m_contents->headerTitleLineEdit);

    m_contents->printButton->setIcon(QIcon::fromTheme("printer"));
    connect(m_contents->printButton, &QPushButton::clicked, this, &KGVSimplePrintingPageSetup::slotPrint);

    m_contents->printPreviewButton->setIcon(QIcon::fromTheme("document-print-preview"));
    m_contents->printPreviewButton->setText(i18n("Print Preview..."));
    connect(m_contents->printPreviewButton, &QPushButton::clicked, this, &KGVSimplePrintingPageSetup::slotPrintPreview);

    m_contents->headerTitleFontButton->setText(i18n("Font..."));
    m_contents->headerTitleFontButton->setWhatsThis(i18n("Changes font for title text."));
    connect(m_contents->headerTitleFontButton, &QPushButton::clicked, this, &KGVSimplePrintingPageSetup::slotChangeTitleFont);

    if (m_graphView) {
        m_contents->headerTitleLineEdit->setText((*args)["title"]);
        QString origCaptionLabelText = m_contents->captionLabel->text();
        m_contents->captionLabel->setText(i18n("<qt><h2>Page Setup for Printing Graph \"%1\"</h2></qt>", QUrl::fromLocalFile(m_graphView->dotFileName()).fileName()));
    }
    connect(m_contents->headerTitleLineEdit, &QLineEdit::textChanged, this, &KGVSimplePrintingPageSetup::slotTitleTextChanged);
    m_contents->headerTitleLineEdit->setFont(m_settings->pageTitleFont);

    m_contents->saveSetupLink->setWhatsThis(i18n("Saves settings for this setup as default."));
    connect(m_contents->saveSetupLink, static_cast<void (KUrlLabel::*)()>(&KUrlLabel::leftClickedUrl), this, &KGVSimplePrintingPageSetup::slotSaveSetup);

    m_contents->addDateTimeCheckbox->setWhatsThis(i18n("Adds date and time to the header."));
    m_contents->addPageNumbersCheckbox->setWhatsThis(i18n("Adds page numbers to the footer."));
    m_contents->addTableBordersCheckbox->setWhatsThis(i18n("Adds table borders."));

#ifdef KGV_NO_UNFINISHED
    m_contents->addDateTimeCheckbox->hide();
    m_contents->addPageNumbersCheckbox->hide();
#endif

    updatePageLayoutAndUnitInfo();
    m_contents->changePageSizeAndMarginsButton->setText(i18n("Change Page Size and Margins..."));
    m_contents->changePageSizeAndMarginsButton->setWhatsThis(i18n("Changes page size and margins."));
    connect(m_contents->changePageSizeAndMarginsButton, &QPushButton::clicked, this, &KGVSimplePrintingPageSetup::slotChangePageSizeAndMargins);

    connect(m_contents->addPageNumbersCheckbox, &QCheckBox::toggled, this, &KGVSimplePrintingPageSetup::slotAddPageNumbersCheckboxToggled);
    connect(m_contents->addDateTimeCheckbox, &QCheckBox::toggled, this, &KGVSimplePrintingPageSetup::slotAddDateTimeCheckboxToggled);
    connect(m_contents->addTableBordersCheckbox, &QCheckBox::toggled, this, &KGVSimplePrintingPageSetup::slotAddTableBordersCheckboxToggled);

    //
    m_contents->addPageNumbersCheckbox->setChecked(m_settings->addPageNumbers);
    m_contents->addDateTimeCheckbox->setChecked(m_settings->addDateAndTime);
    m_contents->addTableBordersCheckbox->setChecked(m_settings->addTableBorders);
    setDirty(false);

    // 	clear it back to false after widgets initialization
    m_printPreviewNeedsReloading = false;

    if (printPreview)
        QTimer::singleShot(50, this, SLOT(printPreview()));
    else if (print)
        QTimer::singleShot(50, this, SLOT(print()));
    connect(this, static_cast<void (KGVSimplePrintingPageSetup::*)()>(&KGVSimplePrintingPageSetup::print), m_graphView, &DotGraphView::print);
    connect(this, static_cast<void (KGVSimplePrintingPageSetup::*)()>(&KGVSimplePrintingPageSetup::printPreview), m_graphView, &DotGraphView::printPreview);

    connect(m_contents->buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &KGVSimplePrintingPageSetup::slotClose);

    m_contents->horizFitNumInput->setRange(1, m_command->engine()->maxHorizFit());
    m_contents->vertFitNumInput->setRange(1, m_command->engine()->maxVertFit());
    m_settings->horizFitting = m_command->engine()->maxHorizFit();
    m_settings->vertFitting = m_command->engine()->maxVertFit();
    m_contents->horizFitNumInput->setValue(m_settings->horizFitting);
    m_contents->vertFitNumInput->setValue(m_settings->vertFitting);
    connect(m_contents->maintainAspectButton, &QPushButton::clicked, this, &KGVSimplePrintingPageSetup::slotMaintainAspectButtonToggled);
    connect(m_contents->horizFitNumInput, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KGVSimplePrintingPageSetup::slotHorizFitChanged);
    connect(m_contents->vertFitNumInput, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KGVSimplePrintingPageSetup::slotVertFitChanged);

    m_fittingModeButtons.addButton(m_contents->naturalSizeRadioButton, NaturalSize);
    m_fittingModeButtons.addButton(m_contents->fitToOnePageRadioButton, FitToOnePage);
    m_fittingModeButtons.addButton(m_contents->fitToSeveralPagesRadioButton, FitToSeveralPages);
    connect(&m_fittingModeButtons, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &KGVSimplePrintingPageSetup::slotFittingButtonClicked);

    m_fittingModeButtons.button(m_settings->fittingMode)->setChecked(true);
    if (m_settings->fittingMode != FitToSeveralPages) {
        m_contents->horizFitNumInput->setEnabled(false);
        m_contents->vertFitNumInput->setEnabled(false);
        m_contents->maintainAspectButton->setEnabled(false);
    }

    QString chainStatePixString = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kgraphviewerpart/pics/chain.png");
    if (!m_settings->chainedFittings) {
        chainStatePixString = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kgraphviewerpart/pics/chain-broken.png");
    }
    if (chainStatePixString.isNull()) {
        std::cerr << "chain state pixmap not found !" << std::endl;
    }
    m_contents->maintainAspectButton->setIcon(QPixmap(chainStatePixString));

    // hides currently unused title label
    m_contents->headerTitleLineEdit->setText(i18n("Chosen font looks like this"));
    m_contents->headerTitleLineEdit->setReadOnly(true);
}

KGVSimplePrintingPageSetup::~KGVSimplePrintingPageSetup()
{
    delete m_contents;
}

void KGVSimplePrintingPageSetup::updatePageLayoutAndUnitInfo()
{
    QString s;
    if (m_settings->pageLayout.format == PG_CUSTOM) {
        s += QString(" (%1 %2 x %3 %4)").arg(m_settings->pageLayout.ptWidth).arg(KgvUnit::unitName(m_unit)).arg(m_settings->pageLayout.ptHeight).arg(KgvUnit::unitName(m_unit));
    } else
        s += KgvPageFormat::name(m_settings->pageLayout.format);
    s += QString(", ") + (m_settings->pageLayout.orientation == PG_PORTRAIT ? i18n("Portrait") : i18n("Landscape")) + ", " + i18n("margins:") + ' ' + KgvUnit::toUserStringValue(m_settings->pageLayout.ptLeft, m_unit) + '/' +
        KgvUnit::toUserStringValue(m_settings->pageLayout.ptRight, m_unit) + '/' + KgvUnit::toUserStringValue(m_settings->pageLayout.ptTop, m_unit) + '/' + KgvUnit::toUserStringValue(m_settings->pageLayout.ptBottom, m_unit) + ' ' +
        KgvUnit::unitName(m_unit);
    m_contents->pageSizeAndMarginsLabel->setText(s);
    m_contents->horizFitNumInput->setRange(1, m_command->engine()->maxHorizFit());
    m_contents->vertFitNumInput->setRange(1, m_command->engine()->maxVertFit());
}

void KGVSimplePrintingPageSetup::setDirty(bool set)
{
    m_contents->saveSetupLink->setEnabled(set);
    if (set) {
        m_printPreviewNeedsReloading = true;
        m_command->engine()->clear();
        emit needsRedraw();
    }
}

void KGVSimplePrintingPageSetup::slotSaveSetup()
{
    m_settings->save();
    setDirty(false);
}

void KGVSimplePrintingPageSetup::slotPrint()
{
    emit print();
}

void KGVSimplePrintingPageSetup::slotPrintPreview()
{
    emit printPreview();
    m_printPreviewNeedsReloading = false;
}

void KGVSimplePrintingPageSetup::slotTitleTextChanged(const QString &)
{
    if (m_contents->headerTitleLineEdit->isModified()) {
        m_printPreviewNeedsReloading = true;
    }

    m_contents->headerTitleLineEdit->setModified(false);
    setDirty(true);
}

void KGVSimplePrintingPageSetup::slotChangeTitleFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, m_settings->pageTitleFont, this);
    if (!ok)
        return;
    m_settings->pageTitleFont = font;
    m_contents->headerTitleLineEdit->setFont(m_settings->pageTitleFont);
    setDirty(true);
}

void KGVSimplePrintingPageSetup::slotChangePageSizeAndMargins()
{
    KgvHeadFoot headfoot; // dummy
    //   std::cerr << "PageLayout before: " << m_settings->pageLayout.orientation << std::endl;
    if (int(QDialog::Accepted) != KgvPageLayoutDia::pageLayout(m_settings->pageLayout, headfoot, FORMAT_AND_BORDERS | DISABLE_UNIT, m_unit, this))
        return;

    //   std::cerr << "PageLayout after: " << m_settings->pageLayout.orientation << std::endl;
    // update
    updatePageLayoutAndUnitInfo();
    setDirty(true);
}

void KGVSimplePrintingPageSetup::slotAddPageNumbersCheckboxToggled(bool set)
{
    m_settings->addPageNumbers = set;
    setDirty(true);
}

void KGVSimplePrintingPageSetup::slotAddDateTimeCheckboxToggled(bool set)
{
    m_settings->addDateAndTime = set;
    setDirty(true);
}

void KGVSimplePrintingPageSetup::slotAddTableBordersCheckboxToggled(bool set)
{
    m_settings->addTableBorders = set;
    setDirty(true);
}

void KGVSimplePrintingPageSetup::slotFittingButtonClicked(int id)
{
    qCDebug(KGRAPHVIEWERLIB_LOG) << "KGVSimplePrintingPageSetup::slotFittingButtonClicked " << id;
    if (id == NaturalSize) {
        m_settings->fitToOnePage = false;
        m_contents->horizFitNumInput->setEnabled(false);
        m_contents->vertFitNumInput->setEnabled(false);
        m_contents->maintainAspectButton->setEnabled(false);
    } else if (id == FitToOnePage) {
        m_settings->fitToOnePage = true;
        m_contents->horizFitNumInput->setEnabled(false);
        m_contents->vertFitNumInput->setEnabled(false);
        m_contents->maintainAspectButton->setEnabled(false);
    } else if (id == FitToSeveralPages) {
        m_settings->fitToOnePage = false;
        m_contents->horizFitNumInput->setEnabled(true);
        m_contents->vertFitNumInput->setEnabled(true);
        m_contents->maintainAspectButton->setEnabled(true);
    }
    setDirty(true);
}

void KGVSimplePrintingPageSetup::slotMaintainAspectButtonToggled()
{
    if (m_settings->chainedFittings) {
        QString chainBreakPixString = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kgraphviewerpart/pics/chain-broken.png");
        if (chainBreakPixString.isNull()) {
            std::cerr << "chain break pixmap not found !" << std::endl;
        }
        m_contents->maintainAspectButton->setIcon(QPixmap(chainBreakPixString));
        m_settings->chainedFittings = false;
    } else {
        QString chainPixString = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kgraphviewerpart/pics/chain.png");
        if (chainPixString.isNull()) {
            std::cerr << "chain pixmap not found !" << std::endl;
        }
        m_contents->maintainAspectButton->setIcon(QPixmap(chainPixString));
        m_settings->chainedFittings = true;
    }
    emit needsRedraw();
}

void KGVSimplePrintingPageSetup::slotClose()
{
    parentWidget()->hide();
}

void KGVSimplePrintingPageSetup::slotHorizFitChanged(int newValue)
{
    m_settings->horizFitting = newValue;
    m_printPreviewNeedsReloading = true;
    emit needsRedraw();
}

void KGVSimplePrintingPageSetup::slotVertFitChanged(int newValue)
{
    m_settings->vertFitting = newValue;
    m_printPreviewNeedsReloading = true;
    emit needsRedraw();
}

}
