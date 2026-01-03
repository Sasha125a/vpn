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
#include <signal.h>  // Добавлен для signal() и SIG*
#include <cstdlib>   // Для std::getenv()

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
                if (bytes_read < 0) {
                    std::cerr << "Read error from " << client_ip << std::endl;
                }
                break;
            }
            
            buffer[bytes_read] = '\0';
            std::cout << "Received from " << client_ip << ": " 
                      << std::string(buffer, bytes_read) << std::endl;
            
            // Echo back (in real VPN, you would route traffic)
            ssize_t bytes_written = write(client_fd, buffer, bytes_read);
            if (bytes_written < 0) {
                std::cerr << "Write error to " << client_ip << std::endl;
                break;
            }
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
            shutdown(server_fd, SHUT_RDWR);
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
        
        int opt = 1;
        setsockopt(health_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        if (bind(health_fd, (struct sockaddr*)&health_addr, sizeof(health_addr)) >= 0) {
            listen(health_fd, 5);
            
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            while (true) {
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
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        close(health_fd);
    });
    
    health_thread.detach();
}

// Глобальный указатель для обработки сигналов
VPNServer* global_server = nullptr;

void signal_handler(int sig) {
    std::cout << "\nReceived signal " << sig << ", shutting down..." << std::endl;
    if (global_server) {
        global_server->stop();
    }
    exit(0);
}

int main() {
    std::cout << "Starting VPN Server for Render.com" << std::endl;
    
    // Стартуем health check endpoint (обязательно для Render)
    start_health_check_endpoint();
    
    // Получаем порт из переменной окружения (Render автоматически устанавливает PORT)
    const char* env_port = std::getenv("PORT");
    int port = 10000; // порт по умолчанию
    if (env_port != nullptr) {
        try {
            port = std::stoi(env_port);
        } catch (const std::exception& e) {
            std::cerr << "Invalid PORT environment variable, using default: " << port << std::endl;
        }
    }
    
    VPNServer server(port);
    global_server = &server;
    
    if (!server.initialize()) {
        std::cerr << "Failed to initialize VPN server" << std::endl;
        return 1;
    }
    
    // Обработка сигналов для graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "VPN Server is running on port " << port << std::endl;
    std::cout << "Health check available on port 8081" << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;
    
    server.start();
    
    return 0;
}
