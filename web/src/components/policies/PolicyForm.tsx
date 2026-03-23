import React, { useState } from 'react';
import { AccessPolicy, Permission, PermissionType } from '../../types/api';
import { Button } from '../common/Button';

interface PolicyFormProps {
  policy?: AccessPolicy;
  onSubmit: (name: string, description: string, permissions: Permission[]) => void;
  onCancel: () => void;
}

const PERMISSION_TYPES: PermissionType[] = ['READ', 'WRITE', 'DELETE', 'LIST', 'MANAGE_ACL'];

export const PolicyForm: React.FC<PolicyFormProps> = ({ policy, onSubmit, onCancel }) => {
  const [name, setName] = useState(policy?.name || '');
  const [description, setDescription] = useState(policy?.description || '');
  const [permissions, setPermissions] = useState<Permission[]>(
    policy?.permissions || [{ type: 'READ', resource_pattern: '*', allow: true }]
  );
  const [errors, setErrors] = useState<{ name?: string }>({});

  const addPermission = () => {
    setPermissions([...permissions, { type: 'READ', resource_pattern: '*', allow: true }]);
  };

  const removePermission = (index: number) => {
    setPermissions(permissions.filter((_, i) => i !== index));
  };

  const updatePermission = (index: number, field: keyof Permission, value: any) => {
    const newPermissions = [...permissions];
    newPermissions[index] = { ...newPermissions[index], [field]: value };
    setPermissions(newPermissions);
  };

  const validate = (): boolean => {
    const newErrors: { name?: string } = {};
    if (!name.trim()) {
      newErrors.name = 'Название политики обязательно';
    }
    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (validate()) {
      onSubmit(name.trim(), description.trim(), permissions);
    }
  };

  return (
    <form onSubmit={handleSubmit} className="space-y-4">
      <div>
        <label htmlFor="name" className="block text-sm font-medium text-gray-700">
          Название *
        </label>
        <input
          type="text"
          id="name"
          value={name}
          onChange={(e) => setName(e.target.value)}
          className={`mt-1 block w-full rounded-md border ${
            errors.name ? 'border-red-500' : 'border-gray-300'
          } shadow-sm focus:border-primary-500 focus:ring-primary-500`}
        />
        {errors.name && <p className="mt-1 text-sm text-red-600">{errors.name}</p>}
      </div>

      <div>
        <label htmlFor="description" className="block text-sm font-medium text-gray-700">
          Описание
        </label>
        <textarea
          id="description"
          value={description}
          onChange={(e) => setDescription(e.target.value)}
          rows={3}
          className="mt-1 block w-full rounded-md border border-gray-300 shadow-sm focus:border-primary-500 focus:ring-primary-500"
        />
      </div>

      <div>
        <div className="flex justify-between items-center mb-2">
          <label className="block text-sm font-medium text-gray-700">
            Разрешения
          </label>
          <Button variant="secondary" size="sm" type="button" onClick={addPermission}>
            + Добавить
          </Button>
        </div>

        <div className="space-y-3">
          {permissions.map((perm, index) => (
            <div key={index} className="flex items-center space-x-3 p-3 bg-gray-50 rounded-md">
              <select
                value={perm.type}
                onChange={(e) => updatePermission(index, 'type', e.target.value as PermissionType)}
                className="rounded-md border border-gray-300 px-2 py-1 text-sm"
              >
                {PERMISSION_TYPES.map((type) => (
                  <option key={type} value={type}>
                    {type}
                  </option>
                ))}
              </select>

              <input
                type="text"
                value={perm.resource_pattern}
                onChange={(e) => updatePermission(index, 'resource_pattern', e.target.value)}
                placeholder="Паттерн ресурса (например: *, /bucket/*, /private/*)"
                className="flex-1 rounded-md border border-gray-300 px-2 py-1 text-sm"
              />

              <label className="flex items-center space-x-2">
                <input
                  type="checkbox"
                  checked={perm.allow}
                  onChange={(e) => updatePermission(index, 'allow', e.target.checked)}
                  className="rounded border-gray-300 text-primary-600"
                />
                <span className="text-sm text-gray-700">Разрешить</span>
              </label>

              {permissions.length > 1 && (
                <button
                  type="button"
                  onClick={() => removePermission(index)}
                  className="text-red-600 hover:text-red-800"
                  title="Удалить"
                >
                  ✕
                </button>
              )}
            </div>
          ))}
        </div>

        <p className="mt-2 text-xs text-gray-500">
          Паттерны: <code>*</code> - все ресурсы, <code>/bucket/*</code> - все в директории, 
          <code>/file.txt</code> - конкретный файл
        </p>
      </div>

      <div className="flex justify-end space-x-3 pt-4">
        <Button variant="secondary" onClick={onCancel} type="button">
          Отмена
        </Button>
        <Button type="submit">
          {policy ? 'Сохранить изменения' : 'Создать политику'}
        </Button>
      </div>
    </form>
  );
};