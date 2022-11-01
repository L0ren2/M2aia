project(org_mitk_cli)

mitk_create_plugin(
  EXPORT_DIRECTIVE CLI_EXPORT
  EXPORTED_INCLUDE_SUFFIXES src
  MODULE_DEPENDS MitkQtWidgetsExt
)
