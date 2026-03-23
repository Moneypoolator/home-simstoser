#!/bin/bash
# upload_file.sh

ACCESS_KEY="AKIAIOSFODNN7EXAMPLE"
SECRET_KEY="wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
URL="http://localhost:9000/test.txt"
METHOD="PUT"
BODY="Hello, World!"

# Генерируем подпись (используем Python скрипт)
python3 << EOF
import sys
sys.path.insert(0, '.')
from sign_request import AWSSignatureV4

signer = AWSSignatureV4('$ACCESS_KEY', '$SECRET_KEY')
headers = signer.sign_request('$METHOD', '$URL', {'Content-Type': 'text/plain'}, '$BODY')

for key, value in headers.items():
    print(f"-H '{key}: {value}'")
EOF

# Или используем готовый скрипт:
python3 sign_request.py put "http://localhost:9000/test.txt" "Hello, World!"