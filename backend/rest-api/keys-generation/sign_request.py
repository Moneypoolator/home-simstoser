#!/usr/bin/env python3
"""
Подписание запросов AWS Signature v4
"""
import hashlib
import hmac
import datetime
from urllib.parse import urlparse, quote

class AWSSignatureV4:
    """Реализация подписи запросов AWS Signature v4"""
    
    def __init__(self, access_key, secret_key, region='us-east-1', service='s3'):
        self.access_key = access_key
        self.secret_key = secret_key
        self.region = region
        self.service = service
    
    def sign_request(self, method, url, headers=None, body=None):
        """Подписывает запрос и возвращает заголовки"""
        if headers is None:
            headers = {}
        
        # Текущая дата
        now = datetime.datetime.utcnow()
        amz_date = now.strftime('%Y%m%dT%H%M%SZ')
        date_stamp = now.strftime('%Y%m%d')
        
        # Парсим URL
        parsed_url = urlparse(url)
        host = parsed_url.netloc
        path = parsed_url.path or '/'
        query = parsed_url.query
        
        # Подготовка заголовков
        headers_to_sign = {
            'host': host,
            'x-amz-date': amz_date,
            **{k.lower(): v for k, v in headers.items()}
        }
        
        # Сортируем заголовки
        signed_headers = ';'.join(sorted(headers_to_sign.keys()))
        
        # Canonical request
        canonical_headers = ''.join(
            f"{k}:{headers_to_sign[k]}\n" 
            for k in sorted(headers_to_sign.keys())
        )
        
        # Hash тела запроса
        payload_hash = hashlib.sha256((body or '').encode('utf-8')).hexdigest()
        
        canonical_request = f"{method}\n{path}\n{query}\n{canonical_headers}\n{signed_headers}\n{payload_hash}"
        
        # String to sign
        algorithm = 'AWS4-HMAC-SHA256'
        credential_scope = f"{date_stamp}/{self.region}/{self.service}/aws4_request"
        string_to_sign = f"{algorithm}\n{amz_date}\n{credential_scope}\n{hashlib.sha256(canonical_request.encode()).hexdigest()}"
        
        # Подпись
        signing_key = self._get_signature_key(
            self.secret_key, date_stamp, self.region, self.service
        )
        signature = hmac.new(
            signing_key, string_to_sign.encode(), hashlib.sha256
        ).hexdigest()
        
        # Authorization header
        authorization_header = (
            f"{algorithm} "
            f"Credential={self.access_key}/{credential_scope}, "
            f"SignedHeaders={signed_headers}, "
            f"Signature={signature}"
        )
        
        # Обновляем заголовки
        headers.update({
            'x-amz-date': amz_date,
            'Authorization': authorization_header,
            'host': host
        })
        
        return headers
    
    def _sign(self, key, msg):
        return hmac.new(key, msg.encode('utf-8'), hashlib.sha256).digest()
    
    def _get_signature_key(self, key, date_stamp, region_name, service_name):
        k_date = self._sign(('AWS4' + key).encode('utf-8'), date_stamp)
        k_region = self._sign(k_date, region_name)
        k_service = self._sign(k_region, service_name)
        k_signing = self._sign(k_service, 'aws4_request')
        return k_signing


# Пример использования
if __name__ == '__main__':
    # Ваши ключи из keys.json
    ACCESS_KEY = 'AKIAIOSFODNN7EXAMPLE'
    SECRET_KEY = 'wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY'
    
    signer = AWSSignatureV4(ACCESS_KEY, SECRET_KEY)
    
    # Подписываем запрос
    method = 'PUT'
    url = 'http://localhost:9000/test.txt'
    body = 'Hello, World!'
    
    headers = signer.sign_request(method, url, {'Content-Type': 'text/plain'}, body)
    
    print("Заголовки для curl:")
    print()
    for key, value in headers.items():
        print(f"  -H '{key}: {value}'")
    print()
    print(f"Полный curl команд:")
    print(f"curl -X {method} {url} \\")
    for key, value in headers.items():
        print(f"  -H '{key}: {value}' \\")
    print(f"  --data-binary '{body}'")