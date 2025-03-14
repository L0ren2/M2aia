/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/
#include <signal/m2PeakDetection.h>
#include <m2SpectrumImageBase.h>
#include <mitkDataNode.h>
#include <mitkLevelWindowProperty.h>
#include <mitkLookupTableProperty.h>
#include <mitkOperation.h>


namespace m2{
  itkEventMacroDefinition(PeakListModifiedEvent, itk::AnyEvent);
}

void m2::SpectrumImageBase::PeakListModified(){
  InvokeEvent(m2::PeakListModifiedEvent());
}

double m2::SpectrumImageBase::ApplyTolerance(double xValue) const
{
  if (this->GetUseToleranceInPPM())
    return m2::PartPerMillionToFactor(this->GetTolerance()) * xValue;
  else
    return this->GetTolerance();
}

void m2::SpectrumImageBase::InsertImageArtifact(const std::string &key, mitk::Image *img)
{
  m_ImageArtifacts[key] = img;
  const unsigned int DIMS = 3;

  if (img->GetDimension() != DIMS)
  {
    mitkThrow() << "SpectrumBaseImage related image artifacts require 3 dimensions.";
  }
  auto aD = this->GetDimensions();
  auto bD = img->GetDimensions();

  if (!std::equal(aD, aD + DIMS, bD))
  {
    mitkThrow() << "SpectrumBaseImage related image artifacts require identical image dimensions.";
  }

  auto aS = this->GetGeometry()->GetSpacing();
  auto bS = img->GetGeometry()->GetSpacing();

  if (!std::equal(aS.GetDataPointer(),
                  aS.GetDataPointer() + DIMS,
                  bS.GetDataPointer(),
                  [](const auto &a, const auto &b) { return itk::Math::FloatAlmostEqual(a, b); }))
  {
    mitkThrow() << "SpectrumBaseImage related image artifacts require identical spacings.";
  }

  

  // if spacing and dimensions are equal, copy origin and vtkMatrix to the new image artifact.
  img->SetClonedTimeGeometry(this->GetTimeGeometry());

}

void m2::SpectrumImageBase::ApplyMoveOriginOperation(const mitk::Vector3D &v)
{
  auto geometry = this->GetGeometry();
  geometry->Translate(v);

  for (auto kv : m_ImageArtifacts)
  {
    geometry = kv.second->GetGeometry();
    geometry->Translate(v);
  }
}

void m2::SpectrumImageBase::ApplyGeometryOperation(mitk::Operation *op)
{
  auto manipulatedGeometry = this->GetGeometry()->Clone();
  manipulatedGeometry->ExecuteOperation(op);
  this->GetGeometry()->SetIdentity();
  this->GetGeometry()->Compose(manipulatedGeometry->GetIndexToWorldTransform());

  for (auto kv : m_ImageArtifacts)
  {
    auto manipulatedGeometry = kv.second->GetGeometry()->Clone();
    manipulatedGeometry->ExecuteOperation(op);
    kv.second->GetGeometry()->SetIdentity();
    kv.second->GetGeometry()->Compose(manipulatedGeometry->GetIndexToWorldTransform());
  }
}

m2::SpectrumImageBase::SpectrumArtifactVectorType &m2::SpectrumImageBase::SkylineSpectrum()
{
  return m_SpectraArtifacts[(SpectrumType::Maximum)];
}

m2::SpectrumImageBase::SpectrumArtifactVectorType &m2::SpectrumImageBase::MeanSpectrum()
{
  return m_SpectraArtifacts[(SpectrumType::Mean)];
}

m2::SpectrumImageBase::SpectrumArtifactVectorType &m2::SpectrumImageBase::SumSpectrum()
{
  return m_SpectraArtifacts[(SpectrumType::Sum)];
}

m2::SpectrumImageBase::SpectrumArtifactVectorType &m2::SpectrumImageBase::GetXAxis()
{
  return m_XAxis;
}

const m2::SpectrumImageBase::SpectrumArtifactVectorType &m2::SpectrumImageBase::GetXAxis() const
{
  return m_XAxis;
}

mitk::Image::Pointer m2::SpectrumImageBase::GetNormalizationImage()
{
  if (m_ImageArtifacts.find("NormalizationImage") != m_ImageArtifacts.end())
    return dynamic_cast<mitk::Image *>(m_ImageArtifacts.at("NormalizationImage").GetPointer());
  return nullptr;
}

mitk::Image::Pointer m2::SpectrumImageBase::GetMaskImage()
{
  if (m_ImageArtifacts.find("mask") != m_ImageArtifacts.end())
    return dynamic_cast<mitk::Image *>(m_ImageArtifacts.at("mask").GetPointer());
  return nullptr;
}

mitk::Image::Pointer m2::SpectrumImageBase::GetIndexImage()
{
  if (m_ImageArtifacts.find("index") != m_ImageArtifacts.end())
    return dynamic_cast<mitk::Image *>(m_ImageArtifacts.at("index").GetPointer());
  return nullptr;
}

void m2::SpectrumImageBase::GetImage(double, double, const mitk::Image *, mitk::Image *) const
{
  MITK_WARN("SpectrumImageBase") << "Get image is not implemented in derived class!";
}

m2::SpectrumImageBase::~SpectrumImageBase() {}
m2::SpectrumImageBase::SpectrumImageBase() : mitk::Image() {}
