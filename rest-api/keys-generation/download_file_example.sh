#!/bin/bash
# download_file.sh

ACCESS_KEY="AKIAIOSFODNN7EXAMPLE"
SECRET_KEY="wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
URL="http://localhost:9000/test.txt"
METHOD="GET"

# Генерируем подпись
HEADERS=$(python3 sign_request.py get "$URL")

# Выполняем запрос
curl -X GET "$URL" $HEADERS -o downloaded.txt