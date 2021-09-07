/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes.

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or https://www.github.com/jtfcordes/m2aia for details.

===================================================================*/

#include <queue>

// Blueberry
#include <berryISelectionService.h>
#include <berryIWorkbenchWindow.h>
#include <berryPlatform.h>
#include <berryPlatformUI.h>

// Qmitk
#include "QmitkMultiNodeSelectionWidget.h"
#include "QmitkSingleNodeSelectionWidget.h"
#include "RegistrationView.h"
// Qt
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <qfiledialog.h>

// mitk
#include <mitkIOUtil.h>
#include <mitkImage.h>
#include <mitkImage3DSliceToImage2DFilter.h>
#include <mitkNodePredicateAnd.h>
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateNot.h>
#include <mitkNodePredicateProperty.h>
#include <mitkPointSet.h>
#include <mitkSceneIO.h>

// itk
#include <itksys/SystemTools.hxx>

// m2
#include <m2ElxDefaultParameterFiles.h>
#include <m2ElxRegistrationHelper.h>
#include <m2ElxUtil.h>
#include <ui_ParameterFileEditorDialog.h>

const std::string RegistrationView::VIEW_ID = "org.mitk.views.registration";

void RegistrationView::SetFocus() {}

void RegistrationView::CreateQtPartControl(QWidget *parent)
{
  // create GUI widgets from the Qt Designer's .ui file
  m_Controls.setupUi(parent);

  m_ParameterFileEditor = new QDialog(parent);
  m_ParameterFileEditorControls.setupUi(m_ParameterFileEditor);

  // Initialize defaults
  {
    m_DefaultParameterFiles[0] = {m2::Elx::Rigid(), m2::Elx::Deformable()};

    auto rigid = m2::Elx::Rigid();
    m2::ElxUtil::ReplaceParameter(rigid, "FixedImageDimension", "3");
    m2::ElxUtil::ReplaceParameter(rigid, "MovingImageDimension", "3");
    auto deformable = m2::Elx::Deformable();
    m2::ElxUtil::ReplaceParameter(deformable, "FixedImageDimension", "3");
    m2::ElxUtil::ReplaceParameter(deformable, "MovingImageDimension", "3");
    m2::ElxUtil::RemoveParameter(deformable, "FinalGridSpacingInPhysicalUnits");
    m2::ElxUtil::RemoveParameter(deformable, "GridSpacingSchedule");
    m_DefaultParameterFiles[1] = {rigid, deformable};
  }

  m_ParameterFiles = m_DefaultParameterFiles;

  connect(m_Controls.registrationStrategy,
          qOverload<int>(&QComboBox::currentIndexChanged),
          this,
          [this](auto currentIndex)
          {
            m_ParameterFileEditorControls.rigidText->setText(m_ParameterFiles[currentIndex][0].c_str());
            m_ParameterFileEditorControls.deformableText->setText(m_ParameterFiles[currentIndex][1].c_str());

            switch (currentIndex)
            {
              case 0:
                m_Controls.label->setText("MSI or histology image data are typical examples for slice data.");
                break;
              case 1:
                m_Controls.label->setText("Any volume image data such as CT or MRI images.");
                break;
            }
          });

  m_Controls.registrationStrategy->setCurrentIndex(1);

  connect(m_ParameterFileEditorControls.buttonBox->button(QDialogButtonBox::StandardButton::RestoreDefaults),
          &QAbstractButton::clicked,
          this,
          [this]()
          {
            auto currentIndex = this->m_Controls.registrationStrategy->currentIndex();
            m_ParameterFileEditorControls.rigidText->setText(m_DefaultParameterFiles[currentIndex][0].c_str());
            m_ParameterFileEditorControls.deformableText->setText(m_DefaultParameterFiles[currentIndex][1].c_str());
          });

  connect(m_ParameterFileEditorControls.buttonBox->button(QDialogButtonBox::StandardButton::Close),
          &QAbstractButton::clicked,
          this,
          [this]()
          {
            auto currentIndex = this->m_Controls.registrationStrategy->currentIndex();
            m_ParameterFiles[currentIndex] = {
              m_ParameterFileEditorControls.rigidText->toPlainText().toStdString(),
              m_ParameterFileEditorControls.deformableText->toPlainText().toStdString()};
          });

  connect(m_Controls.btnStartRegistration, &QPushButton::clicked, this, &RegistrationView::StartRegistration);

  m_FixedImageSingleNodeSelection = m_Controls.fixedImageSelection;
  m_FixedImageSingleNodeSelection->SetDataStorage(GetDataStorage());
  m_FixedImageSingleNodeSelection->SetNodePredicate(
    mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<mitk::Image>::New(),
                                mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
  m_FixedImageSingleNodeSelection->SetSelectionIsOptional(true);
  m_FixedImageSingleNodeSelection->SetEmptyInfo(QString("Select fixed image"));
  m_FixedImageSingleNodeSelection->SetPopUpTitel(QString("Select fixed image node"));

  m_FixedPointSetSingleNodeSelection = m_Controls.fixedPointSelection;
  m_FixedPointSetSingleNodeSelection->SetDataStorage(GetDataStorage());
  m_FixedPointSetSingleNodeSelection->SetNodePredicate(
    mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<mitk::PointSet>::New(),
                                mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
  m_FixedPointSetSingleNodeSelection->SetSelectionIsOptional(true);
  m_FixedPointSetSingleNodeSelection->SetEmptyInfo(QString("Select fixed point set"));
  m_FixedPointSetSingleNodeSelection->SetPopUpTitel(QString("Select fixed point set node"));

  // m_Controls.movingPointSetData->insertWidget(1, m_MovingPointSetSingleNodeSelection);

  m_Controls.addFixedPointSet->setToolTip("Add point set for the fixed image reference points");
  m_Controls.addFixedPointSet->setToolTip("Add point set for the moving image reference points");

  connect(m_Controls.btnAddModality, &QPushButton::clicked, this, [this]() { AddNewModalityTab(); });

  connect(m_Controls.btnOpenPontSetInteractionView,
          &QPushButton::clicked,
          this,
          []()
          {
            try
            {
              if (auto platform = berry::PlatformUI::GetWorkbench())
                if (auto workbench = platform->GetActiveWorkbenchWindow())
                  if (auto page = workbench->GetActivePage())
                    if (page.IsNotNull())
                      page->ShowView("org.mitk.views.pointsetinteraction", "", 1);
            }
            catch (berry::PartInitException &e)
            {
              BERRY_ERROR << "Error: " << e.what() << std::endl;
            }
          });

  connect(m_Controls.addFixedPointSet,
          &QPushButton::clicked,
          this,
          [&]()
          {
            auto node = mitk::DataNode::New();
            node->SetData(mitk::PointSet::New());
            node->SetName("FixedPointSet");
            auto fixedNode = m_FixedImageSingleNodeSelection->GetSelectedNode();
            if (fixedNode)
              this->GetDataStorage()->Add(node, fixedNode);
            else
              this->GetDataStorage()->Add(node);
            node->SetFloatProperty("point 2D size", 0.5);
            node->SetProperty("color", mitk::ColorProperty::New(1.0f, 0.0f, 0.0f));
            this->m_FixedPointSetSingleNodeSelection->SetCurrentSelection({node});
          });

  connect(m_Controls.btnEditParameterFiles,
          &QPushButton::clicked,
          this,
          [this]()
          {
            m_ParameterFileEditor->exec();
            auto currentIndex = this->m_Controls.registrationStrategy->currentIndex();
            m_ParameterFiles[currentIndex] = {
              m_ParameterFileEditorControls.rigidText->toPlainText().toStdString(),
              m_ParameterFileEditorControls.deformableText->toPlainText().toStdString()};
          });
}

void RegistrationView::AddNewModalityTab()
{
  auto widget = new QWidget();
  Ui::MovingModalityWidgetControls controls;
  controls.setupUi(widget);
  const auto modalityId = m_ModalityId;
  const auto tabId = m_Controls.tabWidget->addTab(widget, QString(m_ModalityId));
  auto page = m_Controls.tabWidget->widget(tabId);
  page->setObjectName(QString(m_ModalityId));
  m_Controls.tabWidget->setCurrentIndex(tabId);

  QWidget::setTabOrder(m_Controls.btnAddModality, controls.lineEditName);

  controls.lineEditName->setPlaceholderText(QString(modalityId) + ": new name for this modality.");
  connect(controls.lineEditName,
          &QLineEdit::textChanged,
          this,
          [tabId, this](const QString text) { m_Controls.tabWidget->setTabText(tabId, text); });

  auto imageSelection = new QmitkSingleNodeSelectionWidget();
  imageSelection->SetDataStorage(GetDataStorage());
  imageSelection->SetNodePredicate(
    mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<mitk::Image>::New(),
                                mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
  imageSelection->SetSelectionIsOptional(true);
  imageSelection->SetEmptyInfo(QString("Select image"));
  imageSelection->SetPopUpTitel(QString("Select image node"));
  // m_Controls.movingImageData->insertWidget(0, m_MovingImageSingleNodeSelection);
  controls.widgetList->insertWidget(1, imageSelection);
  m_MovingModalities[modalityId].push_back(imageSelection);

  auto pointSetSelection = new QmitkSingleNodeSelectionWidget();
  pointSetSelection->SetDataStorage(GetDataStorage());
  pointSetSelection->SetNodePredicate(
    mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<mitk::PointSet>::New(),
                                mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
  pointSetSelection->SetSelectionIsOptional(true);
  pointSetSelection->SetEmptyInfo(QString("Select point set"));
  pointSetSelection->SetPopUpTitel(QString("Select point set node"));
  controls.movingPointSetData->addWidget(pointSetSelection);
  m_MovingModalities[modalityId].push_back(pointSetSelection);

  connect(controls.addMovingPointSet,
          &QPushButton::clicked,
          this,
          [imageSelection, pointSetSelection, this]()
          {
            auto node = mitk::DataNode::New();
            node->SetData(mitk::PointSet::New());
            node->SetName("MovingPointSet");

            auto movingNode = imageSelection->GetSelectedNode();
            if (movingNode)
            {
              this->GetDataStorage()->Add(node, movingNode);
              node->SetFloatProperty("point 2D size", 0.5);
              node->SetProperty("color", mitk::ColorProperty::New(0.0f, 1.0f, 1.0f));
              pointSetSelection->SetCurrentSelection({node});
            }
          });

  connect(controls.btnRemove,
          &QPushButton::clicked,
          this,
          [this, modalityId]()
          {
            auto widget = this->m_Controls.tabWidget->findChild<QWidget *>(QString(modalityId));
            auto id = m_Controls.tabWidget->indexOf(widget);
            this->m_Controls.tabWidget->removeTab(id);
          });

  ++m_ModalityId;
}

void RegistrationView::OnSelectionChanged(berry::IWorkbenchPart::Pointer /*part*/,
                                          const QList<mitk::DataNode::Pointer> &nodes)
{
  if (m_Controls.chkBxReinitOnSelectionChanged->isChecked())
  {
    if (!nodes.empty())
    {
      auto set = mitk::DataStorage::SetOfObjects::New();
      set->push_back(nodes.back());
      auto data = nodes.back();
      if (auto image = dynamic_cast<mitk::Image *>(data->GetData()))
      {
        mitk::RenderingManager::GetInstance()->InitializeViews(
          image->GetTimeGeometry(), mitk::RenderingManager::REQUEST_UPDATE_ALL, true);
        // auto boundingGeometry = GetDataStorage()->ComputeBoundingGeometry3D(set, "visible", nullptr);

        // mitk::RenderingManager::GetInstance()->InitializeViews(boundingGeometry);
      }
    }
  }
}

QString RegistrationView::GetElastixPath() const
{
  berry::IPreferences::Pointer preferences =
    berry::Platform::GetPreferencesService()->GetSystemPreferences()->Node("/org.mitk.gui.qt.ext.externalprograms");

  return preferences.IsNotNull() ? preferences->Get("elastix", "") : "";
}

void RegistrationView::StartRegistration()
{
  // check if
  auto elastix = m2::ElxUtil::Executable("elastix");
  if (elastix.empty())
  {
    elastix = GetElastixPath().toStdString();
    if (elastix.empty())
    {
      QMessageBox::information(nullptr, "Error", "No elastix executable specified in the MITK properties!");
      return;
    }
  }

  mitk::PointSet::Pointer fixedPointSet;
  mitk::Image::Pointer fixedImage;

  if (auto node = m_FixedImageSingleNodeSelection->GetSelectedNode())
    fixedImage = dynamic_cast<mitk::Image *>(node->GetData());

  if (auto node = m_FixedPointSetSingleNodeSelection->GetSelectedNode())
    fixedPointSet = dynamic_cast<mitk::PointSet *>(node->GetData());

  // const auto rigid = m_Controls.rigidText->toPlainText().toStdString();
  // const auto deformable = m_Controls.deformableText->toPlainText().toStdString();

  for (const auto kv : m_MovingModalities)
  {
    MITK_INFO << "Process modality with ID [" << kv.first << "] ";
    mitk::Image::Pointer movingImage;
    mitk::PointSet::Pointer movingPointSet;

    if (auto movingPointSetNode = kv.second.at(1)->GetSelectedNode())
    {
      movingPointSet = dynamic_cast<mitk::PointSet *>(movingPointSetNode->GetData());
    }

    auto movingImageNode = kv.second.at(0)->GetSelectedNode();
    if (movingImageNode)
    {
      movingImage = dynamic_cast<mitk::Image *>(movingImageNode->GetData());
    }

    try
    {
      auto currentIndex = m_Controls.registrationStrategy->currentIndex();
      auto f = std::make_shared<QFutureWatcher<mitk::Image::Pointer>>();
      std::list<std::string> queue;

      std::function<void(std::string)> statusCallback = [=](std::string v) mutable
      {
        if (queue.size() > 15)
          queue.pop_front();
        queue.push_back(v);
        QString s;
        for (auto line : queue)
          s = s + line.c_str() + "\n";

        m_Controls.labelStatus->setText(s);
      };

      f->setFuture(QtConcurrent::run(
        [=]() -> mitk::Image::Pointer
        {
          m2::ElxRegistrationHelper helper;
          std::vector<std::string> parameterFiles;

          // copy valid and discard empty parameterfiles
          for (auto &p : m_ParameterFiles[currentIndex])
            if (!p.empty())
              parameterFiles.push_back(p);

          // setup and run
          helper.SetAdditionalBinarySearchPath(itksys::SystemTools::GetParentDirectory(elastix));
          helper.SetImageData(fixedImage, movingImage);
          helper.SetPointData(fixedPointSet, movingPointSet);
          helper.SetRegistrationParameters(parameterFiles);
          helper.SetRemoveWorkingDirectory(true);
          helper.UseMovingImageSpacing(m_Controls.keepSpacings->isChecked());
          helper.SetStatusCallback(statusCallback);
          helper.GetRegistration();
          return helper.WarpImage(movingImage);
        }));

      // started
      connect(f.get(),
              &QFutureWatcher<mitk::Image::Pointer>::started,
              [=]()
              {
                statusCallback("Running ...");
                m_Controls.btnStartRegistration->setDisabled(true);
              });

      connect(f.get(),
              &QFutureWatcher<mitk::Image::Pointer>::finished,
              [=]() mutable
              {
                auto node = mitk::DataNode::New();
                node->SetData(f->result());
                node->SetName("Warped Image");
                this->GetDataStorage()->Add(node, movingImageNode);

                m_Controls.btnStartRegistration->setEnabled(true);
                statusCallback("Completed");

                f.reset();
              });
    }
    catch (std::exception &e)
    {
      MITK_ERROR << e.what();
    }
  }

  // auto resultPath = SystemTools::ConvertToOutputPath(SystemTools::JoinPath({path, "/", "result.1.nrrd"}));
  // MITK_INFO << "Result image path: " << resultPath;
  // if (SystemTools::FileExists(resultPath))
  // {
  //   auto vData = mitk::IOUtil::Load(resultPath);
  //   mitk::Image::Pointer image = dynamic_cast<mitk::Image *>(vData.back().GetPointer());
  //   auto node = mitk::DataNode::New();
  //   node->SetName(movingNode->GetName() + "_result");
  //   node->SetData(image);
  //   this->GetDataStorage()->Add(node, movingNode);
  // }

  // filesystem::remove(path);
}
