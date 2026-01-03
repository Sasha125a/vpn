# VPN Server for Render.com

Простой VPN-сервер на C++, готовый к развертыванию на Render.com.

## Особенности

- Многопоточная обработка клиентов
- Health check endpoint для Render
- Обработка SIGTERM для graceful shutdown
- Поддержка переменной окружения PORT

## Локальная разработка

### Требования
- g++ с поддержкой C++11
- OpenSSL development libraries

### Установка и запуск

```bash
# Дать права на выполнение скрипта
chmod +x setup.sh

# Установить зависимости и скомпилировать
./setup.sh

# Запустить сервер
./vpn-server
