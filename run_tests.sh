# Сборка
mkdir build && cd build
cmake .. 
#-DGTEST_ROOT=/path/to/googletest
make

# Запуск всех тестов

# Или запуск конкретных тестов
./tests/unit_tests
./tests/integration_tests

# Запуск с подробным выводом
./tests/unit_tests --gtest_output=xml:unit_tests.xml
./tests/integration_tests --gtest_filter="ServerIntegrationTest.*"