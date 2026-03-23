import React from 'react';
import { Header } from '../components/layout/Header';
import { Sidebar } from '../components/layout/Sidebar';
import { AccessKeyList } from '../components/keys/AccessKeyList';

export const KeysPage: React.FC = () => {
  return (
    <div className="flex h-screen overflow-hidden bg-gray-50">
      <Sidebar />
      <div className="flex-1 flex flex-col overflow-hidden">
        <Header />
        <main className="flex-1 overflow-y-auto p-6">
          <div className="max-w-7xl mx-auto">
            <AccessKeyList />
          </div>
        </main>
      </div>
    </div>
  );
};