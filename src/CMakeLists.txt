cmake_minimum_required(VERSION 3.16)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Network Qml Quick QuickWidgets Widgets)
find_package(spdlog CONFIG REQUIRED)
find_package(vdf-parser CONFIG REQUIRED)

if (UNIX)
    set(os_name linux)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBFLATPAK REQUIRED flatpak)
    find_package(KF6KIO CONFIG REQUIRED)

    set(os_specific_private_deps Qt::DBus flatpak KF6::KIOCore KF6::KIOFileWidgets KF6::KIOWidgets ${LIBFLATPAK_LIBRARIES})
    set(os_specific_private_include_dirs ${LIBFLATPAK_INCLUDE_DIRS})

	set(os_specific_headers
			../include/uibase/linux/compatibility.h
			../include/uibase/linux/fdcloser.h
	)
	set(os_specific_sources
			linux/fdcloser.cpp
	)
else()
    set(os_name win32)

    set(os_specific_private_deps Version)
    set(os_specific_private_include_dirs)

	set(os_specific_headers)
	set(os_specific_sources)
endif()

set(root_headers
        ../include/uibase/delayedfilewriter.h
        ../include/uibase/diagnosisreport.h
        ../include/uibase/dllimport.h
        ../include/uibase/errorcodes.h
        ../include/uibase/${os_name}/errorcodes.h
        ../include/uibase/eventfilter.h
        ../include/uibase/exceptions.h
        ../include/uibase/executableinfo.h
        ../include/uibase/filemapping.h
        ../include/uibase/filesystemutilities.h
        ../include/uibase/guessedvalue.h
        ../include/uibase/idownloadmanager.h
        ../include/uibase/json.h
        ../include/uibase/log.h
        ../include/uibase/memoizedlock.h
        ../include/uibase/moassert.h
        ../include/uibase/modrepositoryfileinfo.h
        ../include/uibase/nxmurl.h
        ../include/uibase/peextractor.h
        ../include/uibase/petypes.h
        ../include/uibase/pluginrequirements.h
        ../include/uibase/pluginsetting.h
        ../include/uibase/registry.h
        ../include/uibase/report.h
        ../include/uibase/safewritefile.h
        ../include/uibase/scopeguard.h
        ../include/uibase/steamutility.h
        ../include/uibase/stringutility.h
        ../include/uibase/utility.h
        ../include/uibase/${os_name}/utility_${os_name}.h
        ../include/uibase/versioning.h
        ../include/uibase/versioninfo.h
)
set(interface_headers
        ../include/uibase/ifiletree.h
        ../include/uibase/iinstallationmanager.h
        ../include/uibase/imodinterface.h
        ../include/uibase/imodlist.h
        ../include/uibase/imodrepositorybridge.h
        ../include/uibase/imoinfo.h
        ../include/uibase/iplugin.h
        ../include/uibase/iplugindiagnose.h
        ../include/uibase/ipluginfilemapper.h
        ../include/uibase/iplugingame.h
        ../include/uibase/iplugingamefeatures.h
        ../include/uibase/iplugininstaller.h
        ../include/uibase/iplugininstallercustom.h
        ../include/uibase/iplugininstallersimple.h
        ../include/uibase/ipluginlist.h
        ../include/uibase/ipluginmodpage.h
        ../include/uibase/ipluginpreview.h
        ../include/uibase/ipluginproxy.h
        ../include/uibase/iplugintool.h
        ../include/uibase/iprofile.h
        ../include/uibase/isavegame.h
        ../include/uibase/isavegameinfowidget.h
)
set(tutorial_headers
        ../include/uibase/tutorabledialog.h
        ../include/uibase/tutorialcontrol.h
        ../include/uibase/tutorialmanager.h
)
set(widget_headers
        ../include/uibase/expanderwidget.h
        ../include/uibase/filterwidget.h
        ../include/uibase/finddialog.h
        ../include/uibase/lineeditclear.h
        ../include/uibase/linklabel.h
        ../include/uibase/questionboxmemory.h
        ../include/uibase/sortabletreewidget.h
        ../include/uibase/taskprogressmanager.h
        ../include/uibase/${os_name}/taskprogressmanager.h
        ../include/uibase/textviewer.h
        ../include/uibase/widgetutility.h
)
set(game_features_header
        ../include/uibase/game_features/bsainvalidation.h
        ../include/uibase/game_features/dataarchives.h
        ../include/uibase/game_features/game_feature.h
        ../include/uibase/game_features/gameplugins.h
        ../include/uibase/game_features/igamefeatures.h
        ../include/uibase/game_features/localsavegames.h
        ../include/uibase/game_features/moddatachecker.h
        ../include/uibase/game_features/moddatacontent.h
        ../include/uibase/game_features/savegameinfo.h
        ../include/uibase/game_features/scriptextender.h
        ../include/uibase/game_features/unmanagedmods.h
)
set(formatters_header
        ../include/uibase/formatters/enums.h
        ../include/uibase/formatters/qt.h
        ../include/uibase/formatters/random_access_containers.h
        ../include/uibase/formatters/strings.h
        ../include/uibase/formatters.h
)

add_library(uibase SHARED)
mo2_configure_target(uibase NO_SOURCES WARNINGS ON EXTERNAL_WARNINGS ON)
mo2_default_source_group()

set(root_sources
        ${root_headers}
        ${os_specific_headers}
        ${os_specific_sources}
        delayedfilewriter.cpp
        diagnosisreport.cpp
        ${os_name}/errorcodes.cpp
        eventfilter.cpp
        executableinfo.cpp
        filesystemutilities.cpp
        guessedvalue.cpp
        json.cpp
        log.cpp
        ${os_name}/log_${os_name}.cpp
        modrepositoryfileinfo.cpp
        nxmurl.cpp
        peextractor.cpp
        pch.cpp
        pluginrequirements.cpp
        ${os_name}/registry.cpp
        report.cpp
        ${os_name}/report_${os_name}.cpp
        safewritefile.cpp
        scopeguard.cpp
        steamutility.cpp
        ${os_name}/steamutility_${os_name}.cpp
        stringutility.cpp
        utility.cpp
        ${os_name}/utility_${os_name}.cpp
        versioning.cpp
        versioninfo.cpp
)

mo2_target_sources(uibase
        FOLDER src
        PRIVATE
        ${root_sources}
)

mo2_target_sources(uibase
        FOLDER src/interfaces
        PRIVATE
        ${interface_headers}
        ifiletree.cpp
        imodrepositorybridge.cpp
        imoinfo.cpp
)

mo2_target_sources(uibase
        FOLDER src/tutorials
        PRIVATE
        ${tutorial_headers}
        tutorabledialog.cpp
        tutorialcontrol.cpp
        tutorialmanager.cpp
)

mo2_target_sources(uibase
        FOLDER src/widgets
        PRIVATE
        ${widget_headers}
        expanderwidget.cpp
        finddialog.cpp
        lineeditclear.cpp
        linklabel.cpp
        questionboxmemory.cpp
        sortabletreewidget.cpp
        ${os_name}/taskprogressmanager.cpp
        textviewer.cpp
        widgetutility.cpp
        filterwidget.cpp
)

mo2_target_sources(uibase FOLDER src/formatters PRIVATE ${formatters_header})
mo2_target_sources(uibase FOLDER src/game_features PRIVATE ${game_features_header})

target_sources(uibase
        PRIVATE
        finddialog.ui
        questionboxmemory.ui
        taskdialog.ui
        textviewer.ui

        version.rc

        ${root_headers}
        pch.h

        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../include
        FILES
        ${root_headers}
        ${interface_headers}
        ${tutorial_headers}
        ${widget_headers}
        ${game_features_header}
        ${formatters_header}
        ${os_specific_headers}
)

set_target_properties(uibase PROPERTIES DEBUG_POSTFIX d)

# TODO: remove this after fixing UIBase #include<> directives
target_include_directories(uibase PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/uibase>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/uibase/game_features>
)

add_library(mo2::uibase ALIAS uibase)

target_compile_definitions(uibase PRIVATE -DUIBASE_EXPORT SPDLOG_USE_STD_FORMAT)

target_link_libraries(uibase
    PUBLIC Qt6::Widgets Qt6::Network Qt6::QuickWidgets
    PRIVATE spdlog::spdlog_header_only Qt6::Qml Qt6::Quick vdf-parser ${os_specific_private_deps})

target_include_directories(uibase PRIVATE ${os_specific_private_include_dirs})

# installation
install(TARGETS uibase EXPORT uibaseTargets FILE_SET HEADERS)
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:uibase> DESTINATION pdb)
endif ()
install(EXPORT uibaseTargets
        FILE mo2-uibase-targets.cmake
        NAMESPACE mo2::
        DESTINATION lib/cmake/mo2-uibase
)
