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
