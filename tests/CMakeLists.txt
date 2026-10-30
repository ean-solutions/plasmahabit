add_executable(tst_streakcalculator
    tst_streakcalculator.cpp
)

target_link_libraries(tst_streakcalculator PRIVATE
    plasmahabit_core
    Qt6::Core
    Qt6::Test
)

add_test(NAME tst_streakcalculator COMMAND tst_streakcalculator)

add_executable(tst_databasemanager
    tst_databasemanager.cpp
)

target_link_libraries(tst_databasemanager PRIVATE
    plasmahabit_core
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_databasemanager COMMAND tst_databasemanager)

add_executable(tst_habitmodel
    tst_habitmodel.cpp
)

target_link_libraries(tst_habitmodel PRIVATE
    plasmahabit_core
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_habitmodel COMMAND tst_habitmodel)

add_executable(tst_dbvalidation
    tst_dbvalidation.cpp
)

target_link_libraries(tst_dbvalidation PRIVATE
    plasmahabit_core
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_dbvalidation COMMAND tst_dbvalidation)

add_executable(tst_startup
    tst_startup.cpp
    ../src/resources.qrc
)

target_link_libraries(tst_startup PRIVATE
    plasmahabit_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
    KF6::Kirigami
)

add_test(NAME tst_startup COMMAND tst_startup)
