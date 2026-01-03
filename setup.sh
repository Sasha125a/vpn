#!/bin/bash
echo "Setting up VPN Server for Render.com"

# Установка зависимостей
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    sudo apt-get update
    sudo apt-get install -y libssl-dev g++ cmake make
elif [[ "$OSTYPE" == "darwin"* ]]; then
    brew install openssl
    export LDFLAGS="-L/usr/local/opt/openssl@3/lib"
    export CPPFLAGS="-I/usr/local/opt/openssl@3/include"
fi

# Компиляция
echo "Compiling VPN server..."
g++ -std=c++11 -o vpn-server server.cpp -lssl -lcrypto -lpthread

echo "Setup complete!"
echo "Run: ./vpn-server"
