/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/


// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qt
#include <QMessageBox>

// mitk image
#include <mitkImage.h>

#include "CommandLineInterface.h"

const std::string CommandLineInterface::VIEW_ID = "org.mitk.views.commandlineinterface";

void CommandLineInterface::SetFocus()
{
  m_Controls.chooseFileTextEdit->setFocus();
}

void CommandLineInterface::CreateQtPartControl(QWidget *parent)
{
  // create GUI widgets from the Qt Designer's .ui file
  m_Controls.setupUi(parent);
  connect(m_Controls.executeButton, &QPushButton::clicked, this, &CommandLineInterface::DoExecute);
}

void CommandLineInterface::DoExecute()
{
  std::string filename = m_Controls.chooseFileTextEdit->getText();

}
