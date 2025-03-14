/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes.

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or https://www.github.com/jtfcordes/m2aia for details.

===================================================================*/

#ifndef _MITK_M2_ABSTRACT_VIEW_H
#define _MITK_M2_ABSTRACT_VIEW_H

#include <m2SpectrumImageBase.h>
#include <mitkDataNode.h>
#include <org_mitk_gui_qt_m2_common_Export.h>
#include <qobject.h>

namespace mitk{
  class DataStorage;
}

namespace m2
{
  class MITK_M2_CORE_HELPER_EXPORT UIUtils : public QObject
  {
    Q_OBJECT
  public:
    UIUtils(QObject *parent = nullptr) : QObject(parent) {}
    static UIUtils *Instance()
    {
      static UIUtils *_service = nullptr;
      if (!_service)
        _service = new UIUtils();
      return _service;
    }

  public:
    using NodesVectorType = itk::VectorContainer<unsigned, mitk::DataNode::Pointer>;
    static NodesVectorType::Pointer AllNodes(const mitk::DataStorage*  dataStoarage);

  signals:
    void SpectrumImageNodeAdded(const mitk::DataNode *);

    /**
     * @brief This signal can be emitted to send a range indicator update in the spectrum view.
     *
     * @param x
     * @param tol
     */
    void RangeChanged(qreal x, qreal tol);

    /**
     * @brief This signal can be emitted to send a image update request to the data-view.
     *
     * @param x
     * @param tol
     */
    void UpdateImage(qreal x, qreal tol);

    void NextImage();
    void PreviousImage();
    void IncreaseTolerance();
    void DecreaseTolerance();

    void OverviewSpectrumChanged(const mitk::DataNode *, m2::SpectrumType);
    
  
  private:
  };

} // namespace m2

#endif /* BERRYQTSELECTIONPROVIDER_H_ */
