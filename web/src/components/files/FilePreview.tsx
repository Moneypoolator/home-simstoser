import React, { useEffect, useState } from 'react';
import { FileMetadata } from '../../types/api';
import { filesService } from '../../services/files';
import { formatBytes } from '../../utils/format';
import { X, Download, FileText } from 'lucide-react'; // Убраны неиспользуемые импорты
import { Button } from '../common/Button';

interface FilePreviewProps {
  file: FileMetadata;
  onClose: () => void;
}

export const FilePreview: React.FC<FilePreviewProps> = ({ file, onClose }) => {
  const [previewData, setPreviewData] = useState<string | null>(null);
  const [previewType, setPreviewType] = useState<'image' | 'video' | 'audio' | 'pdf' | 'text' | 'code' | 'other'>('other');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    loadPreview();
  }, [file.name]);

  const loadPreview = async () => {
    try {
      setLoading(true);
      setError(null);

      // Определяем тип файла
      const extension = file.name.split('.').pop()?.toLowerCase() || '';
      const mimeType = getMimeType(extension);

      if (isPreviewableImage(mimeType)) {
        setPreviewType('image');
        const blob = await filesService.downloadFile(file.name);
        const url = URL.createObjectURL(blob);
        setPreviewData(url);
      } else if (isPreviewableVideo(mimeType)) {
        setPreviewType('video');
        const blob = await filesService.downloadFile(file.name);
        const url = URL.createObjectURL(blob);
        setPreviewData(url);
      } else if (isPreviewableAudio(mimeType)) {
        setPreviewType('audio');
        const blob = await filesService.downloadFile(file.name);
        const url = URL.createObjectURL(blob);
        setPreviewData(url);
      } else if (extension === 'pdf') {
        setPreviewType('pdf');
        const blob = await filesService.downloadFile(file.name);
        const url = URL.createObjectURL(blob);
        setPreviewData(url);
      } else if (isTextFile(extension)) {
        setPreviewType('text');
        const blob = await filesService.downloadFile(file.name);
        const text = await blob.text();
        setPreviewData(text);
      } else if (isCodeFile(extension)) {
        setPreviewType('code');
        const blob = await filesService.downloadFile(file.name);
        const text = await blob.text();
        setPreviewData(text);
      } else {
        setPreviewType('other');
      }
    } catch (err) {
      setError('Не удалось загрузить файл для предпросмотра');
      console.error('Preview error:', err);
    } finally {
      setLoading(false);
    }
  };

  const getMimeType = (ext: string): string => {
    const mimeTypes: Record<string, string> = {
      'jpg': 'image/jpeg',
      'jpeg': 'image/jpeg',
      'png': 'image/png',
      'gif': 'image/gif',
      'bmp': 'image/bmp',
      'webp': 'image/webp',
      'svg': 'image/svg+xml',
      'mp4': 'video/mp4',
      'webm': 'video/webm',
      'mov': 'video/quicktime',
      'mp3': 'audio/mpeg',
      'wav': 'audio/wav',
      'ogg': 'audio/ogg', // Теперь только один раз
      'flac': 'audio/flac',
      'pdf': 'application/pdf',
    };
    return mimeTypes[ext] || 'application/octet-stream';
  };

  const isPreviewableImage = (mimeType: string): boolean => {
    return mimeType.startsWith('image/') && !mimeType.includes('svg+xml');
  };

  const isPreviewableVideo = (mimeType: string): boolean => {
    return mimeType.startsWith('video/');
  };

  const isPreviewableAudio = (mimeType: string): boolean => {
    return mimeType.startsWith('audio/');
  };

  const isTextFile = (ext: string): boolean => {
    const textExtensions = ['txt', 'md', 'csv', 'log', 'json', 'xml', 'html', 'css'];
    return textExtensions.includes(ext);
  };

  const isCodeFile = (ext: string): boolean => {
    const codeExtensions = ['js', 'ts', 'jsx', 'tsx', 'py', 'java', 'cpp', 'c', 'h', 'cs', 'go', 'rs', 'rb', 'php'];
    return codeExtensions.includes(ext);
  };

  const handleDownload = async () => {
    try {
      const blob = await filesService.downloadFile(file.name);
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = file.name;
      document.body.appendChild(a);
      a.click();
      URL.revokeObjectURL(url);
      document.body.removeChild(a);
    } catch (err) {
      console.error('Download error:', err);
    }
  };

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 p-4">
      <div className="bg-white rounded-lg shadow-2xl max-w-4xl w-full max-h-[90vh] flex flex-col">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b">
          <div>
            <h3 className="text-lg font-semibold text-gray-900">{file.name}</h3>
            <p className="text-sm text-gray-500 mt-1">{formatBytes(file.size)}</p>
          </div>
          <div className="flex space-x-2">
            <Button variant="ghost" size="sm" onClick={handleDownload}>
              <Download className="w-4 h-4 mr-1" />
              Скачать
            </Button>
            <button
              onClick={onClose}
              className="text-gray-400 hover:text-gray-600"
            >
              <X className="w-6 h-6" />
            </button>
          </div>
        </div>

        {/* Preview Content */}
        <div className="flex-1 overflow-auto p-4">
          {loading && (
            <div className="flex justify-center items-center h-64">
              <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary-600"></div>
            </div>
          )}

          {error && (
            <div className="text-center py-12">
              <FileText className="w-16 h-16 mx-auto text-gray-400" />
              <p className="text-gray-600 mt-2">{error}</p>
            </div>
          )}

          {!loading && !error && previewData && (
            <>
              {previewType === 'image' && (
                <img
                  src={previewData}
                  alt={file.name}
                  className="max-w-full max-h-[60vh] mx-auto"
                  onLoad={() => URL.revokeObjectURL(previewData)}
                />
              )}

              {previewType === 'video' && (
                <video
                  controls
                  className="max-w-full max-h-[60vh]"
                  onLoadedMetadata={() => URL.revokeObjectURL(previewData)}
                >
                  <source src={previewData} />
                  Ваш браузер не поддерживает видео
                </video>
              )}

              {previewType === 'audio' && (
                <audio controls className="w-full" onLoadedMetadata={() => URL.revokeObjectURL(previewData)}>
                  <source src={previewData} />
                  Ваш браузер не поддерживает аудио
                </audio>
              )}

              {previewType === 'pdf' && (
                <iframe
                  src={previewData}
                  className="w-full h-[60vh]"
                  title={file.name}
                />
              )}

              {(previewType === 'text' || previewType === 'code') && (
                <pre
                  className={`p-4 bg-gray-50 rounded-lg overflow-auto max-h-[60vh] ${
                    previewType === 'code' ? 'font-mono text-sm' : ''
                  }`}
                >
                  {previewData}
                </pre>
              )}
            </>
          )}

          {!loading && !error && !previewData && (
            <div className="text-center py-12">
              <FileText className="w-16 h-16 mx-auto text-gray-400" />
              <p className="text-gray-600 mt-2">Предпросмотр недоступен для этого типа файла</p>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};