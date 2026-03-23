/**
 * Утилиты для работы с локальным хранилищем
 */

import type { AppSettings } from '../types';

export class LocalStorage {
  /**
   * Сохранить значение в localStorage
   */
  static set<T>(key: string, value: T): void {
    try {
      const serialized = JSON.stringify(value);
      localStorage.setItem(key, serialized);
    } catch (error) {
      console.error(`Error saving to localStorage key "${key}":`, error);
    }
  }

  /**
   * Получить значение из localStorage
   */
  static get<T>(key: string, defaultValue?: T): T | undefined {
    try {
      const item = localStorage.getItem(key);
      if (item === null) {
        return defaultValue;
      }
      return JSON.parse(item) as T;
    } catch (error) {
      console.error(`Error reading from localStorage key "${key}":`, error);
      return defaultValue;
    }
  }

  /**
   * Удалить значение из localStorage
   */
  static remove(key: string): void {
    try {
      localStorage.removeItem(key);
    } catch (error) {
      console.error(`Error removing from localStorage key "${key}":`, error);
    }
  }

  /**
   * Очистить localStorage
   */
  static clear(): void {
    try {
      localStorage.clear();
    } catch (error) {
      console.error('Error clearing localStorage:', error);
    }
  }

  /**
   * Проверить, поддерживается ли localStorage
   */
  static isSupported(): boolean {
    try {
      const testKey = '__test__';
      localStorage.setItem(testKey, testKey);
      localStorage.removeItem(testKey);
      return true;
    } catch (error) {
      return false;
    }
  }
}

export class SessionStorage {
  /**
   * Сохранить значение в sessionStorage
   */
  static set<T>(key: string, value: T): void {
    try {
      const serialized = JSON.stringify(value);
      sessionStorage.setItem(key, serialized);
    } catch (error) {
      console.error(`Error saving to sessionStorage key "${key}":`, error);
    }
  }

  /**
   * Получить значение из sessionStorage
   */
  static get<T>(key: string, defaultValue?: T): T | undefined {
    try {
      const item = sessionStorage.getItem(key);
      if (item === null) {
        return defaultValue;
      }
      return JSON.parse(item) as T;
    } catch (error) {
      console.error(`Error reading from sessionStorage key "${key}":`, error);
      return defaultValue;
    }
  }

  /**
   * Удалить значение из sessionStorage
   */
  static remove(key: string): void {
    try {
      sessionStorage.removeItem(key);
    } catch (error) {
      console.error(`Error removing from sessionStorage key "${key}":`, error);
    }
  }

  /**
   * Очистить sessionStorage
   */
  static clear(): void {
    try {
      sessionStorage.clear();
    } catch (error) {
      console.error('Error clearing sessionStorage:', error);
    }
  }

  /**
   * Проверить, поддерживается ли sessionStorage
   */
  static isSupported(): boolean {
    try {
      const testKey = '__test__';
      sessionStorage.setItem(testKey, testKey);
      sessionStorage.removeItem(testKey);
      return true;
    } catch (error) {
      return false;
    }
  }
}

/**
 * Кэширование с временем жизни (TTL)
 */
export class CacheStorage {
  private static readonly CACHE_PREFIX = 'cache_';
  private static readonly TTL_PREFIX = 'ttl_';

  /**
   * Сохранить значение в кэш с временем жизни
   */
  static set<T>(key: string, value: T, ttlMinutes: number = 60): void {
    const cacheKey = `${this.CACHE_PREFIX}${key}`;
    const ttlKey = `${this.TTL_PREFIX}${key}`;

    LocalStorage.set(cacheKey, value);
    LocalStorage.set(ttlKey, Date.now() + ttlMinutes * 60 * 1000);
  }

  /**
   * Получить значение из кэша
   */
  static get<T>(key: string, defaultValue?: T): T | undefined {
    const cacheKey = `${this.CACHE_PREFIX}${key}`;
    const ttlKey = `${this.TTL_PREFIX}${key}`;

    const ttl = LocalStorage.get<number>(ttlKey);
    if (ttl && Date.now() > ttl) {
      this.remove(key);
      return defaultValue;
    }

    return LocalStorage.get<T>(cacheKey, defaultValue);
  }

  /**
   * Удалить значение из кэша
   */
  static remove(key: string): void {
    const cacheKey = `${this.CACHE_PREFIX}${key}`;
    const ttlKey = `${this.TTL_PREFIX}${key}`;

    LocalStorage.remove(cacheKey);
    LocalStorage.remove(ttlKey);
  }

  /**
   * Очистить весь кэш
   */
  static clear(): void {
    const keysToRemove: string[] = [];

    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i);
      if (key && (key.startsWith(this.CACHE_PREFIX) || key.startsWith(this.TTL_PREFIX))) {
        keysToRemove.push(key);
      }
    }

    keysToRemove.forEach(key => LocalStorage.remove(key.replace(this.CACHE_PREFIX, '').replace(this.TTL_PREFIX, '')));
  }

  /**
   * Проверить, есть ли значение в кэше и не истекло ли оно
   */
  static has(key: string): boolean {
    const ttlKey = `${this.TTL_PREFIX}${key}`;
    const ttl = LocalStorage.get<number>(ttlKey);
    return !!ttl && Date.now() <= ttl;
  }
}

/**
 * Хранилище настроек приложения
 */
export class SettingsStorage {
  private static readonly SETTINGS_KEY = 'app_settings';

  static getDefaultSettings(): AppSettings {
    return {
      theme: 'light',
      language: 'ru',
      notifications: true,
      autoRefresh: true,
      storagePath: './storage',
      maxFileSize: 100,
    };
  }

  static load(): AppSettings {
    const saved = LocalStorage.get<AppSettings>(this.SETTINGS_KEY);
    return saved || this.getDefaultSettings();
  }

  static save(settings: Partial<AppSettings>): void {
    const current = this.load();
    const updated = { ...current, ...settings };
    LocalStorage.set(this.SETTINGS_KEY, updated);
  }

  static reset(): void {
    LocalStorage.set(this.SETTINGS_KEY, this.getDefaultSettings());
  }

  static get<K extends keyof AppSettings>(key: K): AppSettings[K] {
    const settings = this.load();
    return settings[key];
  }

  static set<K extends keyof AppSettings>(key: K, value: AppSettings[K]): void {
    const settings = this.load();
    settings[key] = value;
    this.save(settings);
  }
}

// Вспомогательные функции
export const storageUtils = {
  /**
   * Сохранить токен аутентификации
   */
  saveAuthToken(accessKeyId: string, secretAccessKey: string): void {
    LocalStorage.set('accessKeyId', accessKeyId);
    LocalStorage.set('secretAccessKey', secretAccessKey);
  },

  /**
   * Получить токен аутентификации
   */
  getAuthToken(): { accessKeyId: string; secretAccessKey: string } | null {
    const accessKeyId = LocalStorage.get<string>('accessKeyId');
    const secretAccessKey = LocalStorage.get<string>('secretAccessKey');
    
    if (!accessKeyId || !secretAccessKey) {
      return null;
    }
    
    return { accessKeyId, secretAccessKey };
  },

  /**
   * Удалить токен аутентификации
   */
  clearAuthToken(): void {
    LocalStorage.remove('accessKeyId');
    LocalStorage.remove('secretAccessKey');
    LocalStorage.remove('userId');
    LocalStorage.remove('username');
  },

  /**
   * Сохранить информацию о пользователе
   */
  saveUserInfo(userId: string, username: string): void {
    LocalStorage.set('userId', userId);
    LocalStorage.set('username', username);
  },

  /**
   * Получить информацию о пользователе
   */
  getUserInfo(): { userId: string; username: string } | null {
    const userId = LocalStorage.get<string>('userId');
    const username = LocalStorage.get<string>('username');
    
    if (!userId || !username) {
      return null;
    }
    
    return { userId, username };
  },
};