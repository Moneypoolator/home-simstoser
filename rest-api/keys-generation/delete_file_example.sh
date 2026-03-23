#!/bin/bash
# delete_file.sh

ACCESS_KEY="AKIAIOSFODNN7EXAMPLE"
SECRET_KEY="wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
URL="http://localhost:9000/test.txt"
METHOD="DELETE"

# Генерируем подпись
HEADERS=$(python3 sign_request.py delete "$URL")

# Выполняем запрос
curl -X DELETE "$URL" $HEADERS