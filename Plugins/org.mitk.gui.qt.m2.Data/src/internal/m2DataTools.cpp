/*===================================================================

Mass Spectrometry Imaging applications for interactive
analysis in MITK (M2aia)

Copyright (c) Jonas Cordes, Hochschule Mannheim.
Division of Medical Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/

#include "m2DataTools.h"

#include <QmitkRenderWindow.h>
#include <m2UIUtils.h>
#include <m2ImzMLSpectrumImage.h>
#include <m2SpectrumImageBase.h>
#include <mitkLayoutAnnotationRenderer.h>
#include <mitkLookupTableProperty.h>
#include <mitkNodePredicateAnd.h>
#include <mitkNodePredicateDataType.h>
#include <mitkNodePredicateNot.h>
#include <mitkNodePredicateProperty.h>

const std::string m2DataTools::VIEW_ID = "org.mitk.views.m2.DataTools";



void m2DataTools::CreateQtPartControl(QWidget *parent)
{
  // create GUI widgets from the Qt Designer's .ui file
  m_Controls.setupUi(parent);
  m_Parent = parent;

  {
    m_Controls.ReferenceLevelWindowSelection->SetDataStorage(GetDataStorage());
    m_Controls.ReferenceLevelWindowSelection->SetNodePredicate(
      mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<m2::SpectrumImageBase>::New(),
                                  mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
    m_Controls.ReferenceLevelWindowSelection->SetSelectionIsOptional(true);
    m_Controls.ReferenceLevelWindowSelection->SetEmptyInfo(QString("Reference image selection"));
    m_Controls.ReferenceLevelWindowSelection->SetPopUpTitel(QString("Image"));

    m_Controls.ReferenceSelectionForScaleBar->SetDataStorage(GetDataStorage());
    m_Controls.ReferenceSelectionForScaleBar->SetNodePredicate(
      mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<m2::SpectrumImageBase>::New(),
                                  mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
    m_Controls.ReferenceSelectionForScaleBar->SetSelectionIsOptional(true);
    m_Controls.ReferenceSelectionForScaleBar->SetEmptyInfo(QString("Reference image selection"));
    m_Controls.ReferenceSelectionForScaleBar->SetPopUpTitel(QString("Image"));

    m_Controls.ReferenceSelectionForDataInteraction->SetDataStorage(GetDataStorage());
    m_Controls.ReferenceSelectionForDataInteraction->SetNodePredicate(
      mitk::NodePredicateAnd::New(mitk::TNodePredicateDataType<m2::SpectrumImageBase>::New(),
                                  mitk::NodePredicateNot::New(mitk::NodePredicateProperty::New("helper object"))));
    m_Controls.ReferenceSelectionForDataInteraction->SetSelectionIsOptional(true);
    m_Controls.ReferenceSelectionForDataInteraction->SetEmptyInfo(QString("Reference image selection"));
    m_Controls.ReferenceSelectionForDataInteraction->SetPopUpTitel(QString("Image"));
  }

  
  // disable reference point set
  m_Controls.refPointSetGroup->setVisible(false);

  connect(m_Controls.btnEqualizeLW, &QAbstractButton::clicked, this ,&m2DataTools::OnEqualizeLW);
  connect(m_Controls.resetTiling, &QAbstractButton::clicked, this, &m2DataTools::OnResetTiling);
  connect(m_Controls.applyTiling, &QAbstractButton::clicked, this, &m2DataTools::OnApplyTiling);
  connect(m_Controls.toggleDataInteractor, &QAbstractButton::toggled, this, &m2DataTools::OnToggleDataInteraction);
  

  // scale bar
  
  // m_Controls.sclaeBarLabel->setEnabled(true);
  m_Controls.ReferenceSelectionForScaleBar->setEnabled(true);

  connect(m_Controls.ScaleBar,
          &QGroupBox::toggled,
          this,
          [this](bool state)
          {
            if (state && !m_Controls.ReferenceSelectionForScaleBar->GetSelectedNode())
            {
              m_Controls.ScaleBar->setChecked(false);
              return;
            }
            // m_Controls.sclaeBarLabel->setEnabled(true);
            m_Controls.ReferenceSelectionForScaleBar->setEnabled(true);
            m_ColorBarAnnotations[0]->SetVisibility(state);
            UpdateColorBarAndRenderWindows();
            RequestRenderWindowUpdate();
          });

  this->m_ColorBarAnnotations.clear();
  for (int i = 0; i < 2; ++i)
  {
    auto cbAnnotation = mitk::ColorBarAnnotation::New();
    this->m_ColorBarAnnotations.push_back(cbAnnotation);
    cbAnnotation->SetFontSize(20);
    cbAnnotation->SetOrientation(1);
    cbAnnotation->SetVisibility(0);

    m_Controls.scaleBarFontSize->setValue(cbAnnotation->GetFontSize());
    m_Controls.scaleBarOrientation->setCurrentIndex(cbAnnotation->GetOrientation());
    m_Controls.scaleBarLenght->setValue(cbAnnotation->GetLenght());
    m_Controls.scaleBarWidth->setValue(cbAnnotation->GetWidth());

    connect(m_Controls.scaleBarFontSize,
            &QSlider::sliderMoved,
            this,
            [this, i](int pos)
            {
              m_ColorBarAnnotations[i]->SetFontSize(pos);
              UpdateColorBarAndRenderWindows();
              RequestRenderWindowUpdate();
            });

    connect(m_Controls.scaleBarLenght,
            &QSlider::sliderMoved,
            this,
            [this, i](int pos)
            {
              m_ColorBarAnnotations[i]->SetLength(pos);
              UpdateColorBarAndRenderWindows();
              RequestRenderWindowUpdate();
            });

    connect(m_Controls.scaleBarWidth,
            &QSlider::sliderMoved,
            this,
            [this, i](int pos)
            {
              m_ColorBarAnnotations[i]->SetWidth(pos);
              UpdateColorBarAndRenderWindows();
              RequestRenderWindowUpdate();
            });

    connect(m_Controls.scaleBarOrientation,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this, i](int pos)
            {
              m_ColorBarAnnotations[i]->SetOrientation(pos);
              UpdateColorBarAndRenderWindows();
              RequestRenderWindowUpdate();
            });
  }
}

void m2DataTools::OnResetTiling()
{

  auto allNodes = m2::UIUtils::AllNodes(GetDataStorage());
  
  //	unsigned int maxWidth = 0, maxHeight = 0;
  if (allNodes->Size() == 0)
    return;

  for (auto &e : *allNodes)
  {
    double initP[] = {0, 0, 0};
    mitk::Point3D origin(initP);
    mitk::Point3D prevOrigin(initP);
    if (auto *image = dynamic_cast<m2::SpectrumImageBase *>(e->GetData()))
    {
      prevOrigin = image->GetGeometry()->GetOrigin();
      origin = image->GetGeometry()->GetOrigin();

      origin[0] = image->GetPropertyValue<double>("origin x");
      origin[1] = image->GetPropertyValue<double>("origin y");
      origin[2] = 0;
      image->GetGeometry()->SetOrigin(origin);
      for (auto &&kv : image->GetImageArtifacts())
        kv.second->GetGeometry()->SetOrigin(origin);
    }

    double dx = origin[0] - prevOrigin[0];
    double dy = origin[1] - prevOrigin[1];

    auto der = this->GetDataStorage()->GetDerivations(e, mitk::TNodePredicateDataType<mitk::PointSet>::New());
    for (auto &&e : *der)
    {
      auto pts = dynamic_cast<mitk::PointSet *>(e->GetData());
      for (auto p = pts->Begin(); p != pts->End(); ++p)
      {
        auto &pp = p->Value();
        pp[0] += dx;
        pp[1] += dy;
      }
    }
  }
  mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(this->GetDataStorage());
}

void m2DataTools::OnEqualizeLW()
{
  auto allNodes = m2::UIUtils::AllNodes(GetDataStorage());

  if (auto node = this->m_Controls.ReferenceLevelWindowSelection->GetSelectedNode())
  {
    mitk::LevelWindow lw_ref;
    node->GetLevelWindow(lw_ref);

    for (auto &n : *allNodes)
      n->SetLevelWindow(lw_ref);
  }

  RequestRenderWindowUpdate();
}

void m2DataTools::OnToggleDataInteraction(bool checked)
{
  if (auto node = m_Controls.ReferenceSelectionForDataInteraction->GetSelectedNode())
  {
    if (checked)
    {
      m_Controls.toggleDataInteractor->setText("Deactivate");
      m_DataInteractor = node->GetDataInteractor();
      // If no data Interactor is present create a new one
      if (m_DataInteractor.IsNull())
      {
        // Create PointSetData Interactor
        m_DataInteractor = m2::SpectrumImageDataInteractor::New();
        // Load the according state machine for regular point set interaction
        m_DataInteractor->LoadStateMachine("PointSet.xml");
        // Set the configuration file that defines the triggers for the transitions
        m_DataInteractor->SetEventConfig("PointSetConfig.xml");
        // set the DataNode (which already is added to the DataStorage
        m_DataInteractor->SetDataNode(node);
      }
    }
    else
    {
      m_Controls.toggleDataInteractor->setText("Activate");
      node->SetDataInteractor(nullptr);
      m_DataInteractor = nullptr;
    }
    // emit EditPointSets(checked);
  }
}

void m2DataTools::OnApplyTiling()
{

  auto allNodes = m2::UIUtils::AllNodes(GetDataStorage());

  auto v = m_Controls.mosaicRows->value();
  if (v < 1)
    return;
  
  std::vector<mitk::DataNode::Pointer> nodes;
  unsigned int maxWidth = 0, maxHeight = 0;
  if (allNodes->Size() == 0)
    return;

  for (auto &&e : *allNodes)
  {
    if (auto *image = dynamic_cast<m2::SpectrumImageBase *>(e->GetData()))
    {
      maxWidth = std::max(maxWidth, image->GetDimensions()[0]);
      maxHeight = std::max(maxHeight, image->GetDimensions()[1]);
      nodes.push_back(e);
    }
  }

  int R = v;
  int N = (nodes.size() + 1) / R;
  int i = 0;
  if (N < 1)
    return;

  std::sort(nodes.begin(),
            nodes.end(),
            [](mitk::DataNode::Pointer &a, mitk::DataNode::Pointer &b) -> bool
            { return a->GetName().compare(b->GetName()) < 0; });

  for (auto &&e : nodes)
  {
    mitk::Point3D origin, prevOrigin;
    prevOrigin.Fill(0);
    if (auto *image = dynamic_cast<m2::SpectrumImageBase *>(e->GetData()))
    {
      prevOrigin = image->GetGeometry()->GetOrigin();
      origin[0] = maxWidth * int(i % N) * image->GetGeometry()->GetSpacing()[0];
      origin[1] = maxHeight * int(i / N) * image->GetGeometry()->GetSpacing()[1];
      origin[2] = double(0.0);
      ++i;
      image->GetGeometry()->SetOrigin(origin);
      for (auto &&kv : image->GetImageArtifacts())
        kv.second->GetGeometry()->SetOrigin(origin);
    }

    double dx = origin[0] - prevOrigin[0];
    double dy = origin[1] - prevOrigin[1];

    auto der = this->GetDataStorage()->GetDerivations(e, mitk::TNodePredicateDataType<mitk::PointSet>::New());
    for (auto &&e : *der)
    {
      auto pts = dynamic_cast<mitk::PointSet *>(e->GetData());
      for (auto p = pts->Begin(); p != pts->End(); ++p)
      {
        auto &pp = p->Value();
        pp[0] += dx;
        pp[1] += dy;
      }
    }
  }
  mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(this->GetDataStorage());
  // this->RequestRenderWindowUpdate();
}

void m2DataTools::UpdateColorBarAndRenderWindows()
{
  mitk::ColorBarAnnotation::Pointer cbAnnotation;
  auto lookuptabel = mitk::LookupTableProperty::New();

  cbAnnotation = m_ColorBarAnnotations[0];

  auto renderer = GetRenderWindowPart()->GetQmitkRenderWindow("axial")->GetRenderer();
  mitk::LayoutAnnotationRenderer::AddAnnotation(cbAnnotation, renderer);

  auto node = this->m_Controls.ReferenceSelectionForScaleBar->GetSelectedNode();
  if (node)
  {
    if (node->GetProperty(lookuptabel, "LookupTable"))
      cbAnnotation->SetLookupTable(lookuptabel->GetValue()->GetVtkLookupTable());
  }
}

void m2DataTools::UpdateLevelWindow(const mitk::DataNode *node)
{
  if (auto msImageBase = dynamic_cast<m2::SpectrumImageBase *>(node->GetData()))
  {
    mitk::LevelWindow lw;
    node->GetLevelWindow(lw);
    lw.SetAuto(msImageBase);
    const_cast<mitk::DataNode *>(node)->SetLevelWindow(lw);
  }
}
