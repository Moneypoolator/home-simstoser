#!/bin/bash
# list_files.sh

ACCESS_KEY="AKIAIOSFODNN7EXAMPLE"
SECRET_KEY="wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
URL="http://localhost:9000/list"
METHOD="GET"

# Генерируем подпись
HEADERS=$(python3 sign_request.py get "$URL")

# Выполняем запрос
curl "$URL" $HEADERS | jq '.'