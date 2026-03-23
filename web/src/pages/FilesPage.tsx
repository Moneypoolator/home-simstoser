import React from 'react';
import { FileList } from '../components/files/FileList';
import { FileUpload } from '../components/files/FileUpload';
import { Header } from '../components/layout/Header';

export const FilesPage: React.FC = () => {
  return (
    <div>
      <Header />
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        <div className="mb-6">
          <h2 className="text-2xl font-bold text-gray-900">Файлы</h2>
          <p className="text-gray-500 mt-1">Управление файлами в хранилище</p>
        </div>

        <FileUpload />
        <FileList />
      </div>
    </div>
  );
};