#ifndef __qMRMLSliceAstroWidget_h
#define __qMRMLSliceAstroWidget_h


// qMRMLWidget includes
#include "qMRMLSliceWidget.h"

// Volumes includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qMRMLSliceAstroWidgetPrivate;
class qMRMLSliceAstroControllerWidget;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qMRMLSliceAstroWidget
    : public qMRMLSliceWidget
{
  Q_OBJECT

public:
  /// Superclass typedef
  typedef qMRMLSliceWidget Superclass;

  /// Constructors
  explicit qMRMLSliceAstroWidget(QWidget* parent = 0);
  virtual ~qMRMLSliceAstroWidget();

  /// Get slice controller
  Q_INVOKABLE qMRMLSliceAstroControllerWidget* sliceController()const;

  /// \sa qMRMLSliceControllerWidget::mrmlSliceNode()
  /// \sa setMRMLSliceNode()
  Q_INVOKABLE vtkMRMLSliceNode* mrmlSliceNode()const;

  // \sa qMRMLSliceControllerWidget::sliceLogic()
  Q_INVOKABLE vtkMRMLSliceLogic* sliceLogic()const;

  /// \sa qMRMLSliceControllerWidget::sliceOrientation()
  /// \sa setSliceOrientation()
  QString sliceOrientation()const;

  /// \sa qMRMLSliceControllerWidget::imageData()
  /// \sa setImageData();
#if (VTK_MAJOR_VERSION <= 5)
  Q_INVOKABLE vtkImageData* imageData()const;
#else
  Q_INVOKABLE vtkAlgorithmOutput* imageDataConnection()const;
#endif

  /// \sa qMRMLSliceControllerWidget::mrmlSliceCompositeNode()
  Q_INVOKABLE vtkMRMLSliceCompositeNode* mrmlSliceCompositeNode()const;

  /// \sa qMRMLSliceControllerWidget::sliceViewName()
  /// \sa setSliceViewName()
  QString sliceViewName()const;

  /// \sa qMRMLSliceControllerWidget::sliceViewName()
  /// \sa sliceViewName()
  void setSliceViewName(const QString& newSliceViewName);

  /// \sa qMRMLSliceControllerWidget::sliceViewLabel()
  /// \sa setSliceViewLabel()
  QString sliceViewLabel()const;

  /// \sa qMRMLSliceControllerWidget::sliceViewLabel()
  /// \sa sliceViewLabel()
  void setSliceViewLabel(const QString& newSliceViewLabel);

  /// \sa qMRMLSliceControllerWidget::sliceViewColor()
  /// \sa setSliceViewColor()
  QColor sliceViewColor()const;

  /// \sa qMRMLSliceControllerWidget::sliceViewColor()
  /// \sa sliceViewColor()
  void setSliceViewColor(const QColor& newSliceViewColor);

  /// propagates the logics to the qMRMLSliceControllerWidget
  void setSliceLogics(vtkCollection* logics);

  //virtual bool eventFilter(QObject* object, QEvent* event);
public slots:

  /// \sa qMRMLSliceControllerWidget::setMRMLSliceNode()
  /// \sa mrmlSliceNode()
  void setMRMLSliceNode(vtkMRMLSliceNode* newSliceNode);

  /// \sa qMRMLSliceControllerWidget::setImageData()
  /// \sa imageData()
#if (VTK_MAJOR_VERSION <= 5)
  void setImageData(vtkImageData* newImageData);
#else
  void setImageDataConnection(vtkAlgorithmOutput* newImageDataConnection);
#endif

  /// \sa qMRMLSliceAstroControllerWidget::setSliceOrientation()
  /// \sa sliceOrientation()
  void setSliceOrientation(const QString& orienation);

  /// Fit slices to background
  void fitSliceToBackground();

protected:
  qMRMLSliceAstroWidget(qMRMLSliceAstroWidgetPrivate* pimpl, QWidget* parent = 0);

private:
  Q_DECLARE_PRIVATE(qMRMLSliceAstroWidget);
  Q_DISABLE_COPY(qMRMLSliceAstroWidget);
};

#endif
