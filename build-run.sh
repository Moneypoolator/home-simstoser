# Установка зависимостей (Ubuntu/Debian)
# sudo apt-get install libboost-all-dev libssl-dev nlohmann-json3-dev

cd web && npm run build
cd ..

# Сборка
mkdir build && cd build
cmake ..
make -j$(nproc)

cp -r ../web/dist/* ./web/dist/

# Запуск
./s3_server --port 9000 --storage ./storage --no-auth