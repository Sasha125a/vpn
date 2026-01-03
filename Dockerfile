# Используем образ с C++ компилятором
FROM ubuntu:22.04

# Устанавливаем зависимости
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Создаем рабочую директорию
WORKDIR /app

# Копируем исходный код
COPY server.cpp .

# Компилируем сервер
RUN g++ -std=c++11 -o vpn-server server.cpp -lssl -lcrypto -lpthread

# Открываем порты
EXPOSE $PORT 8081

# Запускаем сервер
CMD ["./vpn-server"]
