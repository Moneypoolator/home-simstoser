# 1. Пересоберите проект
cd build
cmake ..
make -j$(nproc)

# Копируем веб-файлы в директорию сборки
cp -r ../web ./web

# Проверьте права на запись в текущей директории
ls -la | grep storage
# Проверьте, что директория создаётся
ls -la ./storage/

# 2. Запустите сервер в отдельном терминале
./s3_server --port 9000 --storage ./my_storage
#./s3_server
#./s3_server --storage /tmp/s3_storage

# 3. В другом терминале выполните тесты:

echo "Hello, World!" > test.txt
curl -X PUT http://localhost:9000/test.txt \
  -H "Content-Type: text/plain" \
  --data-binary "@test.txt"

#curl -X PUT http://localhost:9000/myfile.txt -H "Content-Type: text/plain" --data-binary "@local_file.txt"
#curl -X PUT http://localhost:8080/test.txt -H "Content-Type: text/plain" -d "Hello, S3!"
# Должно вернуть: {"filename":"test.txt","size":12,"success":true}

curl -X GET http://localhost:9000/myfile.txt -o downloaded.txt
#curl http://localhost:8080/test.txt
# Должно вернуть: Hello, S3!

curl http://localhost:9000/list
#curl http://localhost:8080/
# Должен вернуть список файлов в JSON

#Удаление файла
curl -X DELETE http://localhost:9000/myfile.txt


# 1. Инициировать загрузку
curl -X POST "http://localhost:9000/upload/initiate?filename=large_file.zip"

# Ответ: {"upload_id": "a1b2c3d4e5f6...", "filename": "large_file.zip"}

# 2. Загрузить часть 1
curl -X PUT "http://localhost:9000/upload/part?upload_id=a1b2c3d4e5f6&part_number=1" \
  --data-binary "@part1.bin"

# 3. Загрузить часть 2
curl -X PUT "http://localhost:9000/upload/part?upload_id=a1b2c3d4e5f6&part_number=2" \
  --data-binary "@part2.bin"

# 4. Проверить прогресс
curl "http://localhost:9000/upload/progress?upload_id=a1b2c3d4e5f6"

# 5. Завершить загрузку
curl -X POST "http://localhost:9000/upload/complete?upload_id=a1b2c3d4e5f6" \
  -H "Content-Type: application/json" \
  -d '{"parts": [1, 2]}'

# 6. Отменить загрузку (если нужно)
curl -X DELETE "http://localhost:9000/upload/abort?upload_id=a1b2c3d4e5f6"


# Тестирование HTTPS:

# Автоматическая генерация сертификата и запуск
./s3_server --ssl --port 9443

# Использование собственных сертификатов
./s3_server --ssl --cert /path/to/cert.pem --key /path/to/key.pem --port 9443

# С использованием curl
curl -k https://localhost:9443/list

# С указанием сертификата
curl --cacert ./certs/server.crt https://localhost:9443/list


# Запуск сервера с аутентификацией:
# Запуск с аутентификацией
./s3_server --keys access_keys.csv --port 9000

# Запуск с аутентификацией и HTTPS
./s3_server --keys access_keys.csv --ssl --port 9443


# Запуск с полной защитой:

# Создаем файл ключей
echo "AKIA1234567890ABCDEF,secret_key_12345,user1,1" > access_keys.csv

# Создаем файл пользователей (опционально)
echo "user1,admin@example.com,ADMIN" > users.csv

# Запускаем сервер с аутентификацией, авторизацией и HTTPS
./s3_server --keys access_keys.csv --users users.csv --ssl --port 9443

# Без аутентификации и авторизации - все файлы публичные
./s3_server --port 9000

# Аутентификация включена, но некоторые файлы публичные
./s3_server --keys access_keys.csv --port 9000



# Примеры использования

# Загрузка файла
curl -X PUT http://localhost:9000/myfile.txt \
  -H "Authorization: AWS4-HMAC-SHA256 Credential=YOUR_KEY/20230101/region/s3/aws4_request, SignedHeaders=host;content-type, Signature=YOUR_SIGNATURE" \
  -H "Content-Type: text/plain" \
  --data-binary "@local_file.txt"

# Скачивание файла
curl -X GET http://localhost:9000/myfile.txt \
  -H "Authorization: AWS4-HMAC-SHA256 Credential=YOUR_KEY/20230101/region/s3/aws4_request, SignedHeaders=host, Signature=YOUR_SIGNATURE" \
  -o downloaded.txt

# Multipart upload
# 1. Инициировать загрузку
UPLOAD_ID=$(curl -X POST "http://localhost:9000/upload/initiate?filename=large_video.mp4" | jq -r '.upload_id')

# 2. Загрузить части
curl -X PUT "http://localhost:9000/upload/part?upload_id=$UPLOAD_ID&part_number=1" --data-binary "@part1.bin"
curl -X PUT "http://localhost:9000/upload/part?upload_id=$UPLOAD_ID&part_number=2" --data-binary "@part2.bin"

# 3. Завершить загрузку
curl -X POST "http://localhost:9000/upload/complete?upload_id=$UPLOAD_ID" \
  -H "Content-Type: application/json" \
  -d '{"parts": [1, 2]}'


