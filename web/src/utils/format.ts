export function formatBytes(bytes: number, decimals = 2): string {
  if (bytes === 0) return '0 Bytes';

  const k = 1024;
  const dm = decimals < 0 ? 0 : decimals;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];

  const i = Math.floor(Math.log(bytes) / Math.log(k));

  return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

export function formatDate(dateString: string): string {
  const date = new Date(dateString);
  return date.toLocaleString('ru-RU', {
    year: 'numeric',
    month: 'long',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  });
}

export function getRoleBadgeColor(role: string): string {
  const colors: Record<string, string> = {
    ADMIN: 'bg-red-100 text-red-800',
    MANAGER: 'bg-orange-100 text-orange-800',
    CONTRIBUTOR: 'bg-blue-100 text-blue-800',
    VIEWER: 'bg-green-100 text-green-800',
    GUEST: 'bg-gray-100 text-gray-800',
  };
  return colors[role] || colors.GUEST;
}

export function getPermissionBadgeColor(permission: string): string {
  const colors: Record<string, string> = {
    READ: 'bg-blue-100 text-blue-800',
    WRITE: 'bg-green-100 text-green-800',
    DELETE: 'bg-red-100 text-red-800',
    LIST: 'bg-purple-100 text-purple-800',
    MANAGE_ACL: 'bg-yellow-100 text-yellow-800',
  };
  return colors[permission] || 'bg-gray-100 text-gray-800';
}