set(SRC_CPP_FILES
  m2ChartView.cpp
)

set(INTERNAL_CPP_FILES
  org_mitk_gui_qt_m2_Spectrum_Activator.cpp 
  m2Spectrum.cpp
  m2Crosshair.cpp
  Qm2SpectrumChartDataProvider.cpp
)

set(UI_FILES
  src/internal/m2Spectrum.ui
)

set(MOC_H_FILES
  src/internal/org_mitk_gui_qt_m2_Spectrum_Activator.h
  src/internal/m2Spectrum.h
  src/internal/m2Crosshair.h
  src/internal/Qm2SpectrumChartDataProvider.h
  src/m2ChartView.h
)

# list of resource files which can be used by the plug-in
# system without loading the plug-ins shared library,
# for example the icon used in the menu and tabs for the
# plug-in views in the workbench
set(CACHED_RESOURCE_FILES
  resources/noun_full_spectrum_2852801.svg
  plugin.xml
)

# list of Qt .qrc files which contain additional resources
# specific to this plugin
set(QRC_FILES

)

set(CPP_FILES )

foreach(file ${SRC_CPP_FILES})
  set(CPP_FILES ${CPP_FILES} src/${file})
endforeach(file ${SRC_CPP_FILES})

foreach(file ${INTERNAL_CPP_FILES})
  set(CPP_FILES ${CPP_FILES} src/internal/${file})
endforeach(file ${INTERNAL_CPP_FILES})
