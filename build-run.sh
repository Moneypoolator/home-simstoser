# Установка зависимостей (Ubuntu/Debian)
sudo apt-get install libboost-all-dev libssl-dev nlohmann-json3-dev

# Сборка
mkdir build && cd build
cmake ..
make -j$(nproc)

# Запуск
./s3_server