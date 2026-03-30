# Генерация Python клиента
openapi-generator generate -i openapi.yaml -g python -o ./python-client

# Генерация JavaScript/TypeScript клиента
openapi-generator generate -i openapi.yaml -g typescript-fetch -o ./ts-client

# Генерация C++ клиента
openapi-generator generate -i openapi.yaml -g cpp-restsdk -o ./cpp-client