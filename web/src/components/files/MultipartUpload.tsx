import React, { useState, useRef } from 'react';
import { Upload, X, Check, AlertCircle, Play, StopCircle } from 'lucide-react';
import { multipartService } from '../../services/multipart';
import { useToast } from '../common/Toast';

interface UploadPart {
  partNumber: number;
  size: number;
  status: 'pending' | 'uploading' | 'completed' | 'error';
  progress: number;
}

interface MultipartUploadState {
  filename: string;
  uploadId: string | null;
  parts: UploadPart[];
  totalSize: number;
  uploadedSize: number;
  status: 'idle' | 'initiating' | 'uploading' | 'completing' | 'completed' | 'error' | 'aborted';
}

const CHUNK_SIZE = 5 * 1024 * 1024; // 5MB chunks

export const MultipartUpload: React.FC = () => {
  const [state, setState] = useState<MultipartUploadState>({
    filename: '',
    uploadId: null,
    parts: [],
    totalSize: 0,
    uploadedSize: 0,
    status: 'idle',
  });
  const [file, setFile] = useState<File | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const { showToast } = useToast();

  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    const selectedFile = event.target.files?.[0];
    if (!selectedFile) return;

    setFile(selectedFile);
    setState(prev => ({
      ...prev,
      filename: selectedFile.name,
      totalSize: selectedFile.size,
    }));

    // Calculate number of parts
    const numParts = Math.ceil(selectedFile.size / CHUNK_SIZE);
    const parts: UploadPart[] = Array.from({ length: numParts }, (_, i) => ({
      partNumber: i + 1,
      size: i === numParts - 1 ? selectedFile.size % CHUNK_SIZE || CHUNK_SIZE : CHUNK_SIZE,
      status: 'pending',
      progress: 0,
    }));

    setState(prev => ({ ...prev, parts }));
  };

  const initiateUpload = async () => {
    if (!file) {
      showToast({ title: 'Ошибка', message: 'Пожалуйста, выберите файл', type: 'error' });
      return;
    }

    try {
      setState(prev => ({ ...prev, status: 'initiating' }));
      const response = await multipartService.initiateUpload(state.filename);
      
      setState(prev => ({
        ...prev,
        uploadId: response.upload_id,
        status: 'uploading',
      }));

      showToast({ title: 'Успех', message: 'Загрузка инициирована', type: 'success' });
      await uploadParts(response.upload_id);
    } catch (error) {
      console.error('Error initiating upload:', error);
      showToast({ title: 'Ошибка', message: 'Ошибка инициации загрузки', type: 'error' });
      setState(prev => ({ ...prev, status: 'error' }));
    }
  };

  const uploadParts = async (uploadId: string) => {
    if (!file) return;

    const reader = new FileReader();
    let currentPart = 0;

    const readNextPart = () => {
      if (currentPart >= state.parts.length) {
        completeUpload(uploadId);
        return;
      }

      const part = state.parts[currentPart];
      const start = (part.partNumber - 1) * CHUNK_SIZE;
      const end = Math.min(start + CHUNK_SIZE, file.size);
      const chunk = file.slice(start, end);

      reader.readAsArrayBuffer(chunk);
    };

    reader.onload = async (e) => {
      const part = state.parts[currentPart];
      try {
        setState(prev => ({
          ...prev,
          parts: prev.parts.map(p => 
            p.partNumber === part.partNumber 
              ? { ...p, status: 'uploading', progress: 50 }
              : p
          ),
        }));

        const arrayBuffer = e.target?.result as ArrayBuffer;
        await multipartService.uploadPart(uploadId, part.partNumber, arrayBuffer);

        setState(prev => ({
          ...prev,
          parts: prev.parts.map(p => 
            p.partNumber === part.partNumber 
              ? { ...p, status: 'completed', progress: 100 }
              : p
          ),
          uploadedSize: prev.uploadedSize + part.size,
        }));

        showToast({ title: 'Успех', message: `Часть ${part.partNumber} загружена`, type: 'success' });
        currentPart++;
        readNextPart();
      } catch (error) {
        console.error(`Error uploading part ${part.partNumber}:`, error);
        setState(prev => ({
          ...prev,
          parts: prev.parts.map(p =>
            p.partNumber === part.partNumber
              ? { ...p, status: 'error', progress: 0 }
              : p
          ),
          status: 'error',
        }));
        showToast({ title: 'Ошибка', message: `Ошибка загрузки части ${part.partNumber}`, type: 'error' });
      }
    };

    readNextPart();
  };

  const completeUpload = async (uploadId: string) => {
    try {
      setState(prev => ({ ...prev, status: 'completing' }));
      const parts = state.parts.map(p => p.partNumber);
      await multipartService.completeUpload(uploadId, parts);
      
      setState(prev => ({ ...prev, status: 'completed' }));
      showToast({ title: 'Успех', message: 'Файл успешно загружен', type: 'success' });
    } catch (error) {
      console.error('Error completing upload:', error);
      showToast({ title: 'Ошибка', message: 'Ошибка завершения загрузки', type: 'error' });
      setState(prev => ({ ...prev, status: 'error' }));
    }
  };

  const abortUpload = async () => {
    if (!state.uploadId) return;

    try {
      await multipartService.abortUpload(state.uploadId);
      setState(prev => ({ ...prev, status: 'aborted' }));
      showToast({ title: 'Предупреждение', message: 'Загрузка отменена', type: 'warning' });
    } catch (error) {
      console.error('Error aborting upload:', error);
      showToast({ title: 'Ошибка', message: 'Ошибка отмены загрузки', type: 'error' });
    }
  };

  const resetUpload = () => {
    setFile(null);
    setState({
      filename: '',
      uploadId: null,
      parts: [],
      totalSize: 0,
      uploadedSize: 0,
      status: 'idle',
    });
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  const getStatusColor = (status: MultipartUploadState['status']) => {
    switch (status) {
      case 'completed': return 'bg-green-100 text-green-800';
      case 'uploading': return 'bg-blue-100 text-blue-800';
      case 'error': return 'bg-red-100 text-red-800';
      case 'aborted': return 'bg-yellow-100 text-yellow-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const getStatusText = (status: MultipartUploadState['status']) => {
    switch (status) {
      case 'idle': return 'Ожидание';
      case 'initiating': return 'Инициация';
      case 'uploading': return 'Загрузка';
      case 'completing': return 'Завершение';
      case 'completed': return 'Завершено';
      case 'error': return 'Ошибка';
      case 'aborted': return 'Отменено';
      default: return 'Неизвестно';
    }
  };

  const progressPercentage = state.totalSize > 0 
    ? Math.round((state.uploadedSize / state.totalSize) * 100) 
    : 0;

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <div className="flex items-center justify-between mb-6">
        <div>
          <h3 className="text-lg font-semibold text-gray-900">Многокомпонентная загрузка</h3>
          <p className="text-sm text-gray-500">Для больших файлов (более 5 МБ)</p>
        </div>
        <div className={`px-3 py-1 rounded-full text-sm font-medium ${getStatusColor(state.status)}`}>
          {getStatusText(state.status)}
        </div>
      </div>

      <div className="space-y-6">
        {/* File Selection */}
        <div>
          <label className="block text-sm font-medium text-gray-700 mb-2">
            Выберите файл
          </label>
          <div className="flex items-center space-x-4">
            <input
              type="file"
              ref={fileInputRef}
              onChange={handleFileSelect}
              className="block w-full text-sm text-gray-500 file:mr-4 file:py-2 file:px-4 file:rounded-full file:border-0 file:text-sm file:font-semibold file:bg-primary-50 file:text-primary-700 hover:file:bg-primary-100"
              disabled={state.status === 'uploading' || state.status === 'completing'}
            />
            {file && (
              <div className="flex items-center text-sm text-gray-600">
                <Check className="w-4 h-4 text-green-500 mr-1" />
                {file.name} ({Math.round(file.size / 1024 / 1024)} МБ)
              </div>
            )}
          </div>
        </div>

        {/* Progress Bar */}
        {state.totalSize > 0 && (
          <div>
            <div className="flex justify-between text-sm text-gray-600 mb-1">
              <span>Прогресс</span>
              <span>{progressPercentage}% ({Math.round(state.uploadedSize / 1024 / 1024)} / {Math.round(state.totalSize / 1024 / 1024)} МБ)</span>
            </div>
            <div className="w-full bg-gray-200 rounded-full h-2.5">
              <div 
                className="bg-primary-600 h-2.5 rounded-full transition-all duration-300" 
                style={{ width: `${progressPercentage}%` }}
              />
            </div>
          </div>
        )}

        {/* Parts Grid */}
        {state.parts.length > 0 && (
          <div>
            <h4 className="text-sm font-medium text-gray-700 mb-3">Части файла</h4>
            <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-6 gap-3">
              {state.parts.map(part => (
                <div 
                  key={part.partNumber}
                  className={`p-3 rounded-lg border ${
                    part.status === 'completed' ? 'border-green-200 bg-green-50' :
                    part.status === 'uploading' ? 'border-blue-200 bg-blue-50' :
                    part.status === 'error' ? 'border-red-200 bg-red-50' :
                    'border-gray-200 bg-gray-50'
                  }`}
                >
                  <div className="flex items-center justify-between mb-1">
                    <span className="text-sm font-medium">Часть {part.partNumber}</span>
                    {part.status === 'completed' && <Check className="w-4 h-4 text-green-500" />}
                    {part.status === 'uploading' && <div className="w-4 h-4 border-2 border-blue-500 border-t-transparent rounded-full animate-spin" />}
                    {part.status === 'error' && <AlertCircle className="w-4 h-4 text-red-500" />}
                  </div>
                  <div className="text-xs text-gray-500">
                    {Math.round(part.size / 1024)} КБ
                  </div>
                  {part.progress > 0 && part.progress < 100 && (
                    <div className="mt-2 w-full bg-gray-200 rounded-full h-1">
                      <div 
                        className="bg-blue-600 h-1 rounded-full" 
                        style={{ width: `${part.progress}%` }}
                      />
                    </div>
                  )}
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Actions */}
        <div className="flex space-x-3 pt-4 border-t">
          <button
            onClick={initiateUpload}
            disabled={!file || state.status === 'uploading' || state.status === 'completing'}
            className="inline-flex items-center px-4 py-2 border border-transparent rounded-md shadow-sm text-sm font-medium text-white bg-primary-600 hover:bg-primary-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary-500 disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <Play className="w-4 h-4 mr-2" />
            {state.status === 'idle' ? 'Начать загрузку' : 'Продолжить'}
          </button>

          {(state.status === 'uploading' || state.status === 'initiating') && (
            <button
              onClick={abortUpload}
              className="inline-flex items-center px-4 py-2 border border-gray-300 rounded-md shadow-sm text-sm font-medium text-gray-700 bg-white hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary-500"
            >
              <StopCircle className="w-4 h-4 mr-2" />
              Отменить
            </button>
          )}

          {(state.status === 'completed' || state.status === 'error' || state.status === 'aborted') && (
            <button
              onClick={resetUpload}
              className="inline-flex items-center px-4 py-2 border border-gray-300 rounded-md shadow-sm text-sm font-medium text-gray-700 bg-white hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary-500"
            >
              <X className="w-4 h-4 mr-2" />
              Сбросить
            </button>
          )}
        </div>

        {/* Instructions */}
        <div className="text-sm text-gray-500 pt-4 border-t">
          <p className="font-medium mb-1">Как это работает:</p>
          <ul className="list-disc pl-5 space-y-1">
            <li>Файл разбивается на части по 5 МБ каждая</li>
            <li>Каждая часть загружается отдельно</li>
            <li>После загрузки всех частей файл собирается на сервере</li>
            <li>Подходит для файлов любого размера</li>
            <li>Можно возобновить загрузку при обрыве соединения</li>
          </ul>
        </div>
      </div>
    </div>
  );
};