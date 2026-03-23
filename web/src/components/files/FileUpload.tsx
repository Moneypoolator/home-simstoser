import React, { useState } from 'react';
import { Upload, FileUp } from 'lucide-react';
import { filesService } from '../../services/files';

export const FileUpload: React.FC = () => {
  const [dragActive, setDragActive] = useState(false);
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);

  const handleDrag = (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === 'dragenter' || e.type === 'dragover') {
      setDragActive(true);
    } else if (e.type === 'dragleave') {
      setDragActive(false);
    }
  };

  const handleDrop = async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setDragActive(false);

    const files = Array.from(e.dataTransfer.files);
    if (files.length) {
      await uploadFiles(files);
    }
  };

  const handleFileInput = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || []);
    if (files.length) {
      await uploadFiles(files);
    }
  };

  const uploadFiles = async (files: File[]) => {
    setUploading(true);
    setProgress(0);

    try {
      for (let i = 0; i < files.length; i++) {
        const file = files[i];
        await filesService.uploadFile(file.name, file);
        
        // Обновляем прогресс
        setProgress(Math.round(((i + 1) / files.length) * 100));
      }

      alert('Файлы успешно загружены!');
      // Перезагружаем список файлов через родительский компонент
      window.location.reload();
    } catch (error) {
      console.error('Error uploading files:', error);
      alert('Ошибка при загрузке файлов');
    } finally {
      setUploading(false);
      setProgress(0);
    }
  };

  return (
    <div className="mb-6">
      <div
        className={`border-2 border-dashed rounded-lg p-8 text-center transition-colors ${
          dragActive
            ? 'border-primary-500 bg-primary-50'
            : 'border-gray-300 hover:border-primary-400'
        }`}
        onDragEnter={handleDrag}
        onDragOver={handleDrag}
        onDragLeave={handleDrag}
        onDrop={handleDrop}
      >
        <input
          type="file"
          id="file-upload"
          className="hidden"
          multiple
          onChange={handleFileInput}
        />
        
        <Upload className="w-12 h-12 mx-auto text-gray-400 mb-4" />
        
        <p className="text-sm text-gray-600 mb-2">
          {dragActive
            ? 'Отпустите файлы для загрузки'
            : 'Перетащите файлы сюда или'}
        </p>
        
        <label
          htmlFor="file-upload"
          className="inline-flex items-center px-4 py-2 border border-transparent text-sm font-medium rounded-md shadow-sm text-white bg-primary-600 hover:bg-primary-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-primary-500 cursor-pointer"
        >
          <FileUp className="w-4 h-4 mr-2" />
          Выберите файлы
        </label>
      </div>

      {uploading && (
        <div className="mt-4">
          <div className="w-full bg-gray-200 rounded-full h-2.5">
            <div
              className="bg-primary-600 h-2.5 rounded-full transition-all"
              style={{ width: `${progress}%` }}
            ></div>
          </div>
          <p className="text-sm text-gray-500 text-center mt-1">
            Загрузка: {progress}%
          </p>
        </div>
      )}
    </div>
  );
};