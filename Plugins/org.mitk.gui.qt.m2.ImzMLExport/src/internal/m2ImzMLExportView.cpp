/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes.

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or https://www.github.com/jtfcordes/m2aia for details.

===================================================================*/

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>

// Qmitk
#include "m2ImzMLExportView.h"

// Qt
#include <QMessageBox>
#include <QPushButton>

// mitk image
#include <m2ImzMLSpectrumImage.h>
#include <m2SpectrumImageBase.h>
#include <mitkImage.h>

const std::string m2ImzMLExportView::VIEW_ID = "org.mitk.views.m2.imzmlexport";

void m2ImzMLExportView::SetFocus() {}

void m2ImzMLExportView::NodeAdded(const mitk::DataNode *node)
{
  UpdateExportSettings(node);
}

void m2ImzMLExportView::UpdateExportSettings(const mitk::DataNode *node)
{
  if (auto image = dynamic_cast<m2::ImzMLSpectrumImage *>(node->GetData()))
  {
    auto format = static_cast<m2::SpectrumFormat>(m_Controls.cmbBxOutputMode->currentData(Qt::UserRole).toUInt());
    image->GetExportSpectrumType().Format = format;
    
    auto yType = static_cast<m2::NumericType>(m_Controls.cmbBxOutputDatatypeInt->currentData(Qt::UserRole).toUInt());
    image->GetExportSpectrumType().YAxisType = yType;

    auto xType = static_cast<m2::NumericType>(m_Controls.cmbBxOutputDatatypeMz->currentData(Qt::UserRole).toUInt());
    image->GetExportSpectrumType().XAxisType = xType;


    image->SetPropertyValue<bool>("imzML.bounds", m_Controls.ckBxBounds->isChecked());
    if (m_Controls.ckBxBounds->isChecked())
    {
      image->SetPropertyValue<double>("imzML.bounds.lower", m_Controls.spBxLower->value());
      image->SetPropertyValue<double>("imzML.bounds.upper", m_Controls.spBxUpper->value());
    }
    else
    {
      image->RemoveProperty("imzML.bounds.lower");
      image->RemoveProperty("imzML.bounds.upper");
    }

    switch (format)
    {
      case m2::SpectrumFormat::ContinuousProfile:
        m_Controls.labelInfo->setText(
          "Modified intensity values are stored , according to M2aia's Data view settings.");
        break;
      case m2::SpectrumFormat::ContinuousCentroid:
        m_Controls.labelInfo->setText(
          "Centroid spectra are stored based on peak picking result of the Peak Picking view.");
        break;
      case m2::SpectrumFormat::ProcessedCentroid:
        m_Controls.labelInfo->setText(
          "Export centroid spectra in processed fashion. Spectral preprocessing is applied\n"
          "as specified in m2Data view. Peak picking is applied for each pixel spectrum\n"
          "individually. (optional) Monoisotopic peaks are selected.");
        break;
      default:
        break;
    }
  }
}

void m2ImzMLExportView::UpdateExportSettingsAllNodes()
{
  auto all = this->GetDataStorage()->GetAll();
  for (auto &elem : *all)
  {
    UpdateExportSettings(elem);
  }
}

void m2ImzMLExportView::CreateQtPartControl(QWidget *parent)
{
  // create GUI widgets from the Qt Designer's .ui file
  m_Controls.setupUi(parent);

  m_Controls.cmbBxOutputMode->addItem("Continuous Profile",
                                      static_cast<unsigned>(m2::SpectrumFormat::ContinuousProfile));
  m_Controls.cmbBxOutputMode->addItem("Continuous Centroid",
                                      static_cast<unsigned>(m2::SpectrumFormat::ContinuousCentroid));
  // m_Controls.cmbBxOutputMode->addItem("Continuous Centroid (filter monoisotopic peaks)",
  // static_cast<unsigned>(m2::ImzMLFormatType::ContinuousMonoisotopicCentroid));
  // m_Controls.cmbBxOutputMode->addItem("Processed Centroid",
  // static_cast<unsigned>(m2::ImzMLFormatType::ProcessedCentroid));
  // m_Controls.cmbBxOutputMode->addItem("Processed Centroid (filter monoisotopic peaks)",
  // static_cast<unsigned>(m2::ImzMLFormatType::ProcessedMonoisotopicCentroid));
  // m_Controls.cmbBxOutputMode->addItem("Processed Profile",
  //                                    static_cast<unsigned>(m2::ImzMLFormatType::ProcessedProfile));

  m_Controls.cmbBxOutputDatatypeInt->addItem("Float", static_cast<unsigned>(m2::NumericType::Float));
  m_Controls.cmbBxOutputDatatypeInt->addItem("Double", static_cast<unsigned>(m2::NumericType::Double));

  m_Controls.cmbBxOutputDatatypeMz->addItem("Float", static_cast<unsigned>(m2::NumericType::Float));
  m_Controls.cmbBxOutputDatatypeMz->addItem("Double", static_cast<unsigned>(m2::NumericType::Double));

  connect(m_Controls.cmbBxOutputDatatypeInt,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          &m2ImzMLExportView::UpdateExportSettingsAllNodes,
          Qt::UniqueConnection);

  connect(m_Controls.cmbBxOutputDatatypeMz,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          &m2ImzMLExportView::UpdateExportSettingsAllNodes,
          Qt::UniqueConnection);

  connect(m_Controls.cmbBxOutputMode,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          &m2ImzMLExportView::UpdateExportSettingsAllNodes,
          Qt::UniqueConnection);

  // connect(m_Controls.spnBxPeakPickingHWS,
  //        qOverload<int>(&QSpinBox::valueChanged),
  //        this,
  //        &m2ImzMLExportView::UpdateExportSettingsAllNodes,
  //        Qt::UniqueConnection);

  // connect(m_Controls.spnBxSNR,
  //        qOverload<int>(&QSpinBox::valueChanged),
  //        this,
  //        &m2ImzMLExportView::UpdateExportSettingsAllNodes,
  //        Qt::UniqueConnection);
}
