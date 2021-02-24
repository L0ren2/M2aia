/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes.

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or https://www.github.com/jtfcordes/m2aia for details.

===================================================================*/

#include <itkOpenSlideImageIO.h>
#include <m2OpenSlideIO.h>

namespace m2
{
  OpenSlideIO::OpenSlideIO()
    : AbstractFileIO(mitk::Image::GetStaticNameOfClass(), OPENSLIDE_MIMETYPE(), "OpenSlide Image")
  {
    AbstractFileWriter::SetRanking(10);
    AbstractFileReader::SetRanking(10);
    this->RegisterService();
  }

  mitk::IFileIO::ConfidenceLevel OpenSlideIO::GetWriterConfidenceLevel() const
  {
    if (AbstractFileIO::GetWriterConfidenceLevel() == Unsupported)
      return Unsupported;
    const auto *input = static_cast<const m2::ImzMLMassSpecImage *>(this->GetInput());
    if (input)
      return Supported;
    else
      return Unsupported;
  }

  void OpenSlideIO::Write() { ValidateOutputLocation(); }

  mitk::IFileIO::ConfidenceLevel OpenSlideIO::GetReaderConfidenceLevel() const
  {
    if (AbstractFileIO::GetReaderConfidenceLevel() == Unsupported)
      return Unsupported;
    return Supported;
  }

  std::vector<mitk::BaseData::Pointer> OpenSlideIO::DoRead()
  {
    /*std::string mzGroupId, intGroupId;
    mitk::Image *image;
    */

    auto osIO = itk::OpenSlideImageIO::New();
    osIO->SetFileName(this->GetInputLocation());
    osIO->ReadImageInformation();
    osIO->SetLevel(osIO->GetLevelCount() - 1);
    

    mitk::ItkImageIO reader(osIO.GetPointer());
    reader.AbstractFileReader::SetInput(this->GetInputLocation());

    return reader.Read();
  }

  OpenSlideIO *OpenSlideIO::IOClone() const { return new OpenSlideIO(*this); }
} // namespace m2
