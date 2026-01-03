#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <fcntl.h>

class VPNServer {
private:
    std::atomic<bool> running{true};
    int server_fd;
    int port;
    std::vector<std::thread> client_threads;
    
public:
    VPNServer(int port = 8080) : port(port), server_fd(-1) {}
    
    ~VPNServer() {
        stop();
    }
    
    bool initialize() {
        // Create socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }
        
        // Set socket options for Render compatibility
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Setsockopt failed" << std::endl;
            close(server_fd);
            return false;
        }
        
        // Bind to all interfaces (Render provides external IP)
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed on port " << port << std::endl;
            close(server_fd);
            return false;
        }
        
        // Listen for connections
        if (listen(server_fd, 10) < 0) {
            std::cerr << "Listen failed" << std::endl;
            close(server_fd);
            return false;
        }
        
        std::cout << "VPN Server started on port " << port << std::endl;
        std::cout << "Waiting for connections..." << std::endl;
        
        return true;
    }
    
    void handle_client(int client_fd, const std::string& client_ip) {
        std::cout << "New connection from: " << client_ip << std::endl;
        
        char buffer[4096];
        ssize_t bytes_read;
        
        // Simple echo server for demonstration
        while (running) {
            bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            
            if (bytes_read <= 0) {
                break;
            }
            
            buffer[bytes_read] = '\0';
            std::cout << "Received from " << client_ip << ": " 
                      << std::string(buffer, bytes_read) << std::endl;
            
            // Echo back (in real VPN, you would route traffic)
            write(client_fd, buffer, bytes_read);
        }
        
        close(client_fd);
        std::cout << "Connection closed: " << client_ip << std::endl;
    }
    
    void start() {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (running) {
            int client_fd = accept(server_fd, 
                                  (struct sockaddr*)&client_addr, 
                                  &client_len);
            
            if (client_fd < 0) {
                if (running) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }
            
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            
            // Create thread for each client
            client_threads.emplace_back([this, client_fd, client_ip]() {
                handle_client(client_fd, client_ip);
            });
        }
    }
    
    void stop() {
        running = false;
        
        // Close server socket to break accept()
        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
        
        // Join all client threads
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        client_threads.clear();
        std::cout << "VPN Server stopped" << std::endl;
    }
};

// Render-совместимый HTTP endpoint для health check
void start_health_check_endpoint() {
    std::thread health_thread([]() {
        int health_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (health_fd < 0) return;
        
        struct sockaddr_in health_addr;
        memset(&health_addr, 0, sizeof(health_addr));
        health_addr.sin_family = AF_INET;
        health_addr.sin_addr.s_addr = INADDR_ANY;
        health_addr.sin_port = htons(8081);  // Health check порт
        
        if (bind(health_fd, (struct sockaddr*)&health_addr, sizeof(health_addr)) >= 0) {
            listen(health_fd, 5);
            
            while (true) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client = accept(health_fd, (struct sockaddr*)&client_addr, &client_len);
                
                if (client >= 0) {
                    const char* response = 
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: 21\r\n\r\n"
                        "{\"status\":\"ok\"}";
                    write(client, response, strlen(response));
                    close(client);
                }
            }
        }
    });
    
    health_thread.detach();
}

int main() {
    std::cout << "Starting VPN Server for Render.com" << std::endl;
    
    // Стартуем health check endpoint (обязательно для Render)
    start_health_check_endpoint();
    
    // Получаем порт из переменной окружения (Render автоматически устанавливает PORT)
    const char* env_port = std::getenv("PORT");
    int port = env_port ? std::stoi(env_port) : 10000;
    
    VPNServer server(port);
    
    if (!server.initialize()) {
        std::cerr << "Failed to initialize VPN server" << std::endl;
        return 1;
    }
    
    // Обработка сигналов для graceful shutdown
    signal(SIGINT, [](int sig) {
        std::cout << "\nShutting down..." << std::endl;
        exit(0);
    });
    
    signal(SIGTERM, [](int sig) {
        std::cout << "\nReceived SIGTERM, shutting down..." << std::endl;
        exit(0);
    });
    
    std::cout << "VPN Server is running. Press Ctrl+C to stop." << std::endl;
    server.start();
    
    return 0;
}
