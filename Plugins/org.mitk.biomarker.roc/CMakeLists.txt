project(org_mitk_biomarker_roc)

mitk_create_plugin(
  EXPORT_DIRECTIVE ROC_EXPORT
  EXPORTED_INCLUDE_SUFFIXES src
  MODULE_DEPENDS MitkQtWidgetsExt M2aiaCore M2aiaBiomarkerIdentificationAlgorithms 
  PACKAGE_DEPENDS Qt5|Charts
)
