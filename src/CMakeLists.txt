add_executable(plasmahabit
    main.cpp
    resources.qrc
)

target_link_libraries(plasmahabit PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::QuickControls2
    KF6::CoreAddons
    KF6::I18n
    KF6::I18nQml
    KF6::Kirigami
)

install(TARGETS plasmahabit ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
