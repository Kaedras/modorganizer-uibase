/*
Mod Organizer shared UI functionality

Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "questionboxmemory.h"
#include "log.h"
#include "ui_questionboxmemory.h"

#include <QApplication>
#include <QIcon>
#include <QMutex>
#include <QMutexLocker>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <utility>

using namespace Qt::StringLiterals;

namespace MOBase
{

static QMutex g_mutex;
static QuestionBoxMemory::GetButton g_get;
static QuestionBoxMemory::SetWindowButton g_setWindow;
static QuestionBoxMemory::SetFileButton g_setFile;

QuestionBoxMemory::QuestionBoxMemory(QWidget* parent, const QString& title,
                                     const QString& text, QString const* filename,
                                     const QDialogButtonBox::StandardButtons buttons,
                                     QDialogButtonBox::StandardButton defaultButton)
    : QDialog(parent), ui(new Ui::QuestionBoxMemory), m_Button(QDialogButtonBox::Cancel)
{
  ui->setupUi(this);

  setWindowFlag(Qt::WindowType::WindowContextHelpButtonHint, false);
  setWindowTitle(title);

  QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion);
  ui->iconLabel->setPixmap(icon.pixmap(128));
  ui->messageLabel->setText(text);

  if (filename == nullptr) {
    // delete the 2nd check box
    QCheckBox* box = ui->rememberForCheckBox;
    box->parentWidget()->layout()->removeWidget(box);
    delete box;
  } else {
    ui->rememberForCheckBox->setText(ui->rememberForCheckBox->text().arg(*filename));
  }

  ui->buttonBox->setStandardButtons(buttons);

  if (defaultButton != QDialogButtonBox::NoButton) {
    ui->buttonBox->button(defaultButton)->setDefault(true);
  }

  connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this,
          SLOT(buttonClicked(QAbstractButton*)));
}

QuestionBoxMemory::~QuestionBoxMemory() = default;

void QuestionBoxMemory::setCallbacks(GetButton get, SetWindowButton setWindow,
                                     SetFileButton setFile)
{
  QMutexLocker locker(&g_mutex);

  g_get       = std::move(get);
  g_setWindow = std::move(setWindow);
  g_setFile   = std::move(setFile);
}

void QuestionBoxMemory::buttonClicked(QAbstractButton* button)
{
  m_Button = ui->buttonBox->standardButton(button);
}

QDialogButtonBox::StandardButton
QuestionBoxMemory::query(QWidget* parent, const QString& windowName,
                         const QString& title, const QString& text,
                         QDialogButtonBox::StandardButtons buttons,
                         QDialogButtonBox::StandardButton defaultButton)
{
  return queryImpl(parent, windowName, nullptr, title, text, buttons, defaultButton);
}

QDialogButtonBox::StandardButton
QuestionBoxMemory::query(QWidget* parent, const QString& windowName,
                         const QString& fileName, const QString& title,
                         const QString& text, QDialogButtonBox::StandardButtons buttons,
                         QDialogButtonBox::StandardButton defaultButton)
{
  return queryImpl(parent, windowName, &fileName, title, text, buttons, defaultButton);
}

QDialogButtonBox::StandardButton
QuestionBoxMemory::queryImpl(QWidget* parent, const QString& windowName,
                             const QString* fileName, const QString& title,
                             const QString& text,
                             QDialogButtonBox::StandardButtons buttons,
                             QDialogButtonBox::StandardButton defaultButton)
{
  QMutexLocker locker(&g_mutex);

  const auto button = getMemory(windowName, (fileName ? *fileName : ""));
  if (button != NoButton) {
    log::debug("{}: not asking because user always wants response {}",
               QString(windowName % (fileName ? '/' % *fileName : u""_s)),
               buttonToString(button));

    return button;
  }

  QuestionBoxMemory dialog(parent, title, text, fileName, buttons, defaultButton);
  dialog.exec();

  if (dialog.m_Button != QDialogButtonBox::Cancel) {
    if (dialog.ui->rememberCheckBox->isChecked()) {
      setWindowMemory(windowName, dialog.m_Button);
    }

    if (fileName != nullptr && dialog.ui->rememberForCheckBox->isChecked()) {
      setFileMemory(windowName, *fileName, dialog.m_Button);
    }
  }

  return dialog.m_Button;
}

void QuestionBoxMemory::setWindowMemory(const QString& windowName, Button b)
{
  log::debug("remembering choice {} for window {}", buttonToString(b), windowName);

  g_setWindow(windowName, b);
}

void QuestionBoxMemory::setFileMemory(const QString& windowName,
                                      const QString& filename, Button b)
{
  log::debug("remembering choice {} for file {}", buttonToString(b),
             QString(windowName % '/' % filename));

  g_setFile(windowName, filename, b);
}

QuestionBoxMemory::Button QuestionBoxMemory::getMemory(const QString& windowName,
                                                       const QString& filename)
{
  return g_get(windowName, filename);
}

QString QuestionBoxMemory::buttonToString(Button b)
{
  using BB = QDialogButtonBox;

  static const std::map<Button, QString> map = {
      {BB::NoButton, u"none"_s},
      {BB::Ok, u"ok"_s},
      {BB::Save, u"save"_s},
      {BB::SaveAll, u"saveall"_s},
      {BB::Open, u"open"_s},
      {BB::Yes, u"yes"_s},
      {BB::YesToAll, u"yestoall"_s},
      {BB::No, u"no"_s},
      {BB::NoToAll, u"notoall"_s},
      {BB::Abort, u"abort"_s},
      {BB::Retry, u"retry"_s},
      {BB::Ignore, u"ignore"_s},
      {BB::Close, u"close"_s},
      {BB::Cancel, u"cancel"_s},
      {BB::Discard, u"discard"_s},
      {BB::Help, u"help"_s},
      {BB::Apply, u"apply"_s},
      {BB::Reset, u"reset"_s},
      {BB::RestoreDefaults, u"restoredefaults"_s}};

  auto itor = map.find(b);

  if (itor == map.end()) {
    return QStringLiteral("0x%1").arg(static_cast<int>(b), 0, 16);
  } else {
    return QStringLiteral("'%1' (0x%2)")
        .arg(itor->second)
        .arg(static_cast<int>(b), 0, 16);
  }
}

}  // namespace MOBase
