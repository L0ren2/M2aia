/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/

#include <array>
#include <cstdlib>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itksys/SystemTools.hxx>
#include <m2ImzMLSpectrumImage.h>
#include <m2SpectrumImageProcessor.h>
#include <m2SpectrumImageStack.h>
#include <mitkExtractSliceFilter.h>
#include <mitkIOUtil.h>
#include <mitkITKImageImport.h>
#include <mitkImageAccessByItk.h>
#include <mitkImageReadAccessor.h>
#include <mitkImageWriteAccessor.h>
#include <signal/m2PeakDetection.h>

namespace m2
{
  void SpectrumImageStack::Insert(unsigned int sliceId, std::shared_ptr<m2::ElxRegistrationHelper> transformer)
  {
    m_SliceTransformers[sliceId] = transformer;

    if (auto spectrumImage = dynamic_cast<m2::SpectrumImageBase *>(transformer->GetMovingImage().GetPointer()))
    {
      auto newMin = spectrumImage->GetPropertyValue<double>("x_min");
      auto newMax = spectrumImage->GetPropertyValue<double>("x_max");
      auto xLabel = spectrumImage->GetSpectrumType().XAxisLabel;
      
      auto currentMax = GetPropertyValue<double>("x_max");
      auto currentMin = GetPropertyValue<double>("x_min");
      if (newMin < currentMin)
        SetPropertyValue<double>("x_min", newMin);
      if (newMax > currentMax)
        SetPropertyValue<double>("x_max", newMax);

      this->GetSpectrumType().XAxisLabel = xLabel;
    }
    else
    {
      mitkThrow() << "Spectrum image base object expected!";
    }
  }

  SpectrumImageStack::SpectrumImageStack(double spacingZ) : m_SpacingZ(spacingZ)
  {
    SetPropertyValue<double>("x_min", std::numeric_limits<double>::max());
    SetPropertyValue<double>("x_max", std::numeric_limits<double>::min());
  }

  void SpectrumImageStack::InitializeGeometry()
  {
    unsigned int dims[3];
    mitk::Vector3D spacing;

    auto &transformer = m_SliceTransformers.begin()->second;
    auto image = transformer->GetMovingImage();
    if (!transformer->GetTransformation().empty())
    {
      image = transformer->WarpImage(image);
    }
    std::copy(image->GetDimensions(), image->GetDimensions() + 3, dims);
    spacing = image->GetGeometry()->GetSpacing();

    dims[2] = m_SliceTransformers.size();
    this->Initialize(mitk::MakeScalarPixelType<m2::DisplayImagePixelType>(), 3, dims);

    spacing[2] = m_SpacingZ;
    this->GetGeometry()->SetSpacing(spacing);

    // fill with data
    for (auto &kv : m_SliceTransformers)
    {
      auto sliceId = kv.first;
      const auto &transformer = kv.second;
      if (auto movingImage = transformer->GetMovingImage())
      {
        // create temp image and copy requested image range to the stack
        if (!transformer->GetTransformation().empty())
        {
          movingImage = transformer->WarpImage(movingImage);
          CopyWarpedImageToStackImage(movingImage, this, sliceId);
        }
        else
        {
          CopyWarpedImageToStackImage(movingImage, this, sliceId);
        }
      }
    }
  }

  void SpectrumImageStack::InitializeProcessor()
  {
    if(m_SliceTransformers.empty()){
      MITK_ERROR("SpectrumImageStack::InitializeProcessor") << "No transformer found!";
      return;
    }

    // assign import spectrum type based on first transformer
    // this must be equally for all slices/images
    auto movingImage = m_SliceTransformers.begin()->second->GetMovingImage().GetPointer();
    if(auto specImage =dynamic_cast<m2::SpectrumImageBase *>(movingImage))
      m_SpectrumType.Format = specImage->GetSpectrumType().Format;
    
    
    for (auto &kv : m_SliceTransformers)
    {
      const auto &transformer = kv.second;
      auto specImage = dynamic_cast<m2::SpectrumImageBase *>(transformer->GetMovingImage().GetPointer());
      
      if (m_SpectrumType.Format != specImage->GetSpectrumType().Format)
      {
        MITK_ERROR("SpectrumImageStack::InitializeProcessor") << "Different import modes detected";
      }
    }
  }

  void SpectrumImageStack::InitializeImageAccess()
  {
    std::list<m2::Peak> peaks;
    const auto &bins = GetNumberOfBins();
    //    const auto normalizationStrategy = GetNormalizationStrategy();

    double max = 0;
    double min = std::numeric_limits<double>::max();
    double binSize = 1;

    for (auto &kv : m_SliceTransformers)
    {
      const auto &transformer = kv.second;
      auto specImage = dynamic_cast<m2::SpectrumImageBase *>(transformer->GetMovingImage().GetPointer());
      auto xAxis = specImage->GetXAxis();
      min = std::min(min, xAxis.front());
      max = std::max(max, xAxis.back());
    }

    binSize = (max - min) / double(bins);
    using SpectrumVector = m2::SpectrumImageBase::SpectrumArtifactVectorType;
    SpectrumVector xSumVec(bins);
    SpectrumVector hits(bins);

    SpectrumVector ySumVec = SumSpectrum();
    ySumVec.resize(bins, 0);
    SpectrumVector yMeanVec = MeanSpectrum();
    yMeanVec.resize(bins, 0);
    SpectrumVector yMaxVec = SkylineSpectrum();
    yMaxVec.resize(bins, 0);

    for (auto &kv : m_SliceTransformers)
    {
      const auto &transformer = kv.second;
      auto specImage = dynamic_cast<m2::SpectrumImageBase *>(transformer->GetMovingImage().GetPointer());
      auto sliceXAxis = specImage->GetXAxis();
      auto sliceSumVec = specImage->SumSpectrum();
      auto sliceMaxVec = specImage->SkylineSpectrum();
      auto sliceMeanVec = specImage->MeanSpectrum();

      for (unsigned int k = 0; k < sliceXAxis.size(); ++k)
      {
        auto j = (long)((sliceXAxis[k] - min) / binSize);

        if (j >= bins)
          j = bins - 1;
        else if (j < 0)
          j = 0;

        xSumVec[j] += sliceXAxis[k];
        ySumVec[j] += sliceSumVec[k];
        yMeanVec[j] += sliceMeanVec[k];
        yMaxVec[j] = std::max(yMaxVec[j], double(sliceMaxVec[k]));
        hits[j]++;
      }
    }

    SpectrumVector &xVecFinal = GetXAxis();
    xVecFinal.clear();
    SpectrumVector &ySumVecFinal = SumSpectrum();
    ySumVecFinal.clear();
    SpectrumVector &yMeanVecFinal = MeanSpectrum();
    yMeanVecFinal.clear();
    SpectrumVector &yMaxVecFinal = SkylineSpectrum();
    yMaxVecFinal.clear();

    for (int k = 0; k < bins; ++k)
    {
      if (hits[k] > 0)
      {
        xVecFinal.push_back(xSumVec[k] / (double)hits[k]);      // mean uof sum of x values within bin range
        ySumVecFinal.push_back(ySumVec[k] / (double)hits[k]);   // mean of sums
        yMeanVecFinal.push_back(yMeanVec[k] / (double)hits[k]); // mean of means
        yMaxVecFinal.push_back(yMaxVec[k]);                     // max
      }
    }

    SetPropertyValue<double>("x_min", xVecFinal.front());
    SetPropertyValue<double>("x_max", xVecFinal.back());
  }

  void SpectrumImageStack::SpectrumImageStack::CopyWarpedImageToStackImage(mitk::Image *warped,
                                                                           mitk::Image *stack,
                                                                           unsigned i) const
  {
    auto warpN = warped->GetDimensions()[0] * warped->GetDimensions()[1];
    auto stackN = stack->GetDimensions()[0] * stack->GetDimensions()[1];
    if (warpN != stackN)
      mitkThrow() << "Slice dimensions are not equal for target slice with index !" << i;
    if (i >= stack->GetDimensions()[2])
      mitkThrow() << "Stack index is invalid! Z dim is " << stack->GetDimensions()[2];

    mitk::ImageWriteAccessor stackAccess(stack);
    auto stackData = static_cast<m2::DisplayImagePixelType *>(stackAccess.GetData());

    AccessByItk(warped,
                (
                  [&](auto itkImg)
                  {
                    auto warpedData = itkImg->GetBufferPointer();
                    std::copy(warpedData, warpedData + stackN, stackData + (i * stackN));
                  }));
  }

  void SpectrumImageStack::GetImage(double center, double tol, const mitk::Image * /*mask*/, mitk::Image *img) const
  {
    for (auto &kv : m_SliceTransformers)
    {
      auto sliceId = kv.first;
      const auto &transformer = kv.second;
      if (auto spectrumImage = dynamic_cast<m2::SpectrumImageBase *>(transformer->GetMovingImage().GetPointer()))
      {
        // create temp image and copy requested image range to the stack
        auto imageTemp = mitk::Image::New();
        imageTemp->Initialize(spectrumImage);
        spectrumImage->GetImage(center, tol, spectrumImage->GetMaskImage(), imageTemp);
        if (!transformer->GetTransformation().empty())
        {
          imageTemp = transformer->WarpImage(imageTemp);
          CopyWarpedImageToStackImage(imageTemp, img, sliceId);
        }
        else
        {
          CopyWarpedImageToStackImage(imageTemp, img, sliceId);
        }
      }
    }

    // current->GetImage(mz, tol, current->GetMaskImage(), current);

    // auto warped = Transform(
    //   current, transformations, [this](auto trafo) { ReplaceParameter(trafo, "ResultImagePixelType", "\"double\"");
    //   });

    // auto warpedIndexImage = TransformImageUsingNNInterpolation(i);
    // CopyWarpedImageToStack<IndexImagePixelType>(warpedIndexImage, GetIndexImage(), i);
  }

} // namespace m2
