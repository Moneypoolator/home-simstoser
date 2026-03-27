import boto3
from botocore.config import Config

# Создаем клиент с вашими ключами
s3 = boto3.client(
    's3',
    endpoint_url='http://localhost:9000',
    aws_access_key_id='AKIA1234567890ABCDEF',
    aws_secret_access_key='abcdefghijklmnopqrstuvwxyz1234567890',
    config=Config(signature_version='s3v4')
)

# Загрузка файла
s3.upload_file('local_file.txt', 'bucket', 'remote_file.txt')

# Скачивание файла
s3.download_file('bucket', 'remote_file.txt', 'downloaded.txt')