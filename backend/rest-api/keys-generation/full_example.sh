# 1. Загрузка файла
curl -X PUT http://localhost:9000/hello.txt \
  -H "Content-Type: text/plain" \
  -H "Authorization: AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20240319/us-east-1/s3/aws4_request, SignedHeaders=host;content-type;x-amz-date, Signature=fe5f80f77d5fa3beca038a2a822895d3e1f5e5d8c5b5e5d8c5b5e5d8c5b5e5d8" \
  -H "x-amz-date: 20240319T120000Z" \
  -H "host: localhost:9000" \
  --data-binary "Hello, S3!"

# 2. Скачивание файла
curl -X GET http://localhost:9000/hello.txt \
  -H "Authorization: AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20240319/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=fe5f80f77d5fa3beca038a2a822895d3e1f5e5d8c5b5e5d8c5b5e5d8c5b5e5d8" \
  -H "x-amz-date: 20240319T120000Z" \
  -H "host: localhost:9000"

# 3. Список файлов
curl http://localhost:9000/list \
  -H "Authorization: AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20240319/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=fe5f80f77d5fa3beca038a2a822895d3e1f5e5d8c5b5e5d8c5b5e5d8c5b5e5d8" \
  -H "x-amz-date: 20240319T120000Z" \
  -H "host: localhost:9000" | jq '.'

# 4. Удаление файла
curl -X DELETE http://localhost:9000/hello.txt \
  -H "Authorization: AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20240319/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=fe5f80f77d5fa3beca038a2a822895d3e1f5e5d8c5b5e5d8c5b5e5d8c5b5e5d8" \
  -H "x-amz-date: 20240319T120000Z" \
  -H "host: localhost:9000"