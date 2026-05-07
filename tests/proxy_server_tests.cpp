#include "core/proxy_server.hpp"

#include <catch2/catch_amalgamated.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    using namespace std::chrono_literals;

    uint16_t findFreePort()
    {
        const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(sock >= 0);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        REQUIRE(::bind(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);

        socklen_t length = sizeof(address);
        REQUIRE(::getsockname(sock, reinterpret_cast<sockaddr *>(&address), &length) == 0);
        const auto port = ntohs(address.sin_port);

        ::close(sock);
        return port;
    }

    int connectLoopback(uint16_t port)
    {
        const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(sock >= 0);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        REQUIRE(::connect(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        return sock;
    }

    bool waitForListener(uint16_t port, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(port);
            const bool connected = ::connect(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
            ::close(sock);
            if (connected)
            {
                return true;
            }
            std::this_thread::sleep_for(10ms);
        }
        return false;
    }

    void sendAll(int sock, std::string_view data)
    {
        while (!data.empty())
        {
            const auto sent = ::send(sock, data.data(), data.size(), 0);
            REQUIRE(sent > 0);
            data.remove_prefix(static_cast<std::size_t>(sent));
        }
    }

    std::string recvAll(int sock)
    {
        std::string out;
        char buffer[4096];
        while (true)
        {
            const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
            if (received <= 0)
            {
                break;
            }
            out.append(buffer, static_cast<std::size_t>(received));
        }
        return out;
    }

    std::string recvN(int sock, std::size_t count)
    {
        std::string out;
        out.reserve(count);
        char buffer[4096];
        while (out.size() < count)
        {
            const auto received = ::recv(sock, buffer, std::min(sizeof(buffer), count - out.size()), 0);
            if (received <= 0)
            {
                break;
            }
            out.append(buffer, static_cast<std::size_t>(received));
        }
        return out;
    }

    /**
     * @brief Читает с сокета HTTP-запрос (заголовки + тело по Content-Length).
     */
    std::string readHttpRequest(int sock)
    {
        constexpr std::string_view HeaderSeparator = "\r\n\r\n";
        std::string data;
        char buffer[4096];

        while (data.find(HeaderSeparator) == std::string::npos)
        {
            const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
            if (received <= 0)
            {
                return data;
            }
            data.append(buffer, static_cast<std::size_t>(received));
        }

        const auto headerEnd = data.find(HeaderSeparator) + HeaderSeparator.size();
        const auto lowerData = [&]
        {
            std::string copy = data;
            std::ranges::transform(copy, copy.begin(), [](unsigned char c)
                                   { return static_cast<char>(std::tolower(c)); });
            return copy;
        }();
        const auto cl = lowerData.find("content-length:");
        std::size_t bodySize = 0;
        if (cl != std::string::npos && cl < headerEnd)
        {
            bodySize = std::stoul(data.substr(cl + std::string_view("content-length:").size()));
        }

        while (data.size() < headerEnd + bodySize)
        {
            const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
            if (received <= 0)
            {
                break;
            }
            data.append(buffer, static_cast<std::size_t>(received));
        }
        return data;
    }

    /**
     * @brief TCP-сервер для тестов: принимает соединения и передаёт сокет хендлеру.
     */
    class MockServer
    {
    public:
        using Handler = std::function<void(int clientSocket)>;

        explicit MockServer(Handler handler)
            : handler(std::move(handler))
        {
            listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
            REQUIRE(listenFd >= 0);

            constexpr int enabled = 1;
            ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = 0;
            REQUIRE(::bind(listenFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);

            socklen_t length = sizeof(address);
            REQUIRE(::getsockname(listenFd, reinterpret_cast<sockaddr *>(&address), &length) == 0);
            port_ = ntohs(address.sin_port);

            REQUIRE(::listen(listenFd, 16) == 0);

            acceptThread = std::thread([this]
                                       { run(); });
        }

        ~MockServer()
        {
            stop.store(true);
            ::shutdown(listenFd, SHUT_RDWR);
            ::close(listenFd);
            if (acceptThread.joinable())
            {
                acceptThread.join();
            }
        }

        uint16_t port() const { return port_; }
        int connectionCount() const { return connections.load(); }

    private:
        void run()
        {
            while (!stop.load())
            {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(listenFd, &fds);
                timeval timeout{0, 50000};
                const auto ready = ::select(listenFd + 1, &fds, nullptr, nullptr, &timeout);
                if (ready <= 0)
                {
                    continue;
                }

                const int client = ::accept(listenFd, nullptr, nullptr);
                if (client < 0)
                {
                    continue;
                }

                connections.fetch_add(1);
                std::thread([handler = handler, client]
                            {
                    handler(client);
                    ::close(client); })
                    .detach();
            }
        }

        Handler handler;
        int listenFd = -1;
        uint16_t port_ = 0;
        std::thread acceptThread;
        std::atomic<bool> stop{false};
        std::atomic<int> connections{0};
    };

    std::string writeFilterFile(const std::string &json)
    {
        static std::atomic<int> sequence{0};
        const auto path = "/tmp/http_proxy_test_filter_" + std::to_string(::getpid()) + "_" + std::to_string(++sequence) + ".json";
        std::ofstream config(path);
        REQUIRE(config.is_open());
        config << json;
        return path;
    }

    /**
     * @brief Поднимает ProxyServer на свободном порту и ждёт готовности.
     *
     * Прокси не имеет shutdown-механики, поэтому поток отпускается в detach -
     * он закроется вместе с тестовым процессом.
     */
    class ProxyHarness
    {
    public:
        explicit ProxyHarness(const std::string &filterJson = "[]")
            : filterPath(writeFilterFile(filterJson)),
              port_(findFreePort())
        {
            std::thread([port = port_, path = filterPath]
                        {
                try
                {
                    ProxyServer proxy(port, path);
                    proxy.start();
                }
                catch (...) {} })
                .detach();

            REQUIRE(waitForListener(port_, 2s));
        }

        ~ProxyHarness()
        {
            std::remove(filterPath.c_str());
        }

        ProxyHarness(const ProxyHarness &) = delete;
        ProxyHarness &operator=(const ProxyHarness &) = delete;

        uint16_t port() const { return port_; }

    private:
        std::string filterPath;
        uint16_t port_;
    };
}

TEST_CASE("ProxyServer relays plain HTTP request and rewrites absolute URI", "[proxy]")
{
    std::mutex captureMutex;
    std::string captured;

    MockServer target([&](int sock)
                      {
        const auto request = readHttpRequest(sock);
        {
            std::scoped_lock lock(captureMutex);
            captured = request;
        }
        const std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello";
        sendAll(sock, response); });

    ProxyHarness proxy;

    const int client = connectLoopback(proxy.port());
    const auto targetPort = std::to_string(target.port());
    sendAll(client,
            "GET http://127.0.0.1:" + targetPort + "/path?x=1 HTTP/1.1\r\n"
                                                   "Host: 127.0.0.1:" +
                targetPort + "\r\n"
                             "Proxy-Connection: keep-alive\r\n"
                             "X-Custom: yes\r\n"
                             "\r\n");
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.starts_with("HTTP/1.1 200 OK"));
    CHECK(response.find("hello") != std::string::npos);

    std::scoped_lock lock(captureMutex);
    CHECK(captured.starts_with("GET /path?x=1 HTTP/1.1"));
    CHECK(captured.find("Host: 127.0.0.1:" + targetPort) != std::string::npos);
    CHECK(captured.find("X-Custom: yes") != std::string::npos);
    CHECK(captured.find("Proxy-Connection") == std::string::npos);
    CHECK(captured.find("Connection: close") != std::string::npos);
}

TEST_CASE("ProxyServer forwards POST body and Content-Length", "[proxy]")
{
    std::mutex captureMutex;
    std::string captured;

    MockServer target([&](int sock)
                      {
        const auto request = readHttpRequest(sock);
        {
            std::scoped_lock lock(captureMutex);
            captured = request;
        }
        sendAll(sock,
            "HTTP/1.1 201 Created\r\n"
            "Content-Length: 0\r\n"
            "\r\n"); });

    ProxyHarness proxy;

    const int client = connectLoopback(proxy.port());
    const std::string body = "name=hello&value=world";
    const auto targetPort = std::to_string(target.port());
    sendAll(client,
            "POST http://127.0.0.1:" + targetPort + "/submit HTTP/1.1\r\n"
                                                    "Host: 127.0.0.1:" +
                targetPort + "\r\n"
                             "Content-Type: application/x-www-form-urlencoded\r\n"
                             "Content-Length: " +
                std::to_string(body.size()) + "\r\n"
                                              "\r\n" +
                body);
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.starts_with("HTTP/1.1 201 Created"));

    std::scoped_lock lock(captureMutex);
    CHECK(captured.starts_with("POST /submit HTTP/1.1"));
    CHECK(captured.find("Content-Length: " + std::to_string(body.size())) != std::string::npos);
    CHECK(captured.ends_with(body));
}

TEST_CASE("ProxyServer routes relative-URI requests via Host header", "[proxy]")
{
    std::mutex captureMutex;
    std::string captured;

    MockServer target([&](int sock)
                      {
        const auto request = readHttpRequest(sock);
        {
            std::scoped_lock lock(captureMutex);
            captured = request;
        }
        sendAll(sock, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"); });

    ProxyHarness proxy;

    const int client = connectLoopback(proxy.port());
    const auto targetPort = std::to_string(target.port());
    sendAll(client,
            "GET /index HTTP/1.1\r\n"
            "Host: 127.0.0.1:" +
                targetPort + "\r\n\r\n");
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.find("ok") != std::string::npos);

    std::scoped_lock lock(captureMutex);
    CHECK(captured.starts_with("GET /index HTTP/1.1"));
}

TEST_CASE("ProxyServer blocks filtered host with 403", "[proxy]")
{
    MockServer target([](int) {});
    const auto targetPort = std::to_string(target.port());
    ProxyHarness proxy(R"(["127.0.0.1"])");

    const int client = connectLoopback(proxy.port());
    sendAll(client,
            "GET http://127.0.0.1:" + targetPort + "/secret HTTP/1.1\r\n"
                                                   "Host: 127.0.0.1:" +
                targetPort + "\r\n\r\n");
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.starts_with("HTTP/1.1 403 Forbidden"));
    CHECK(response.find("blocked") != std::string::npos);
    CHECK(target.connectionCount() == 0);
}

TEST_CASE("ProxyServer returns 502 when target is unreachable", "[proxy]")
{
    ProxyHarness proxy;

    const int client = connectLoopback(proxy.port());
    // Port 1 на loopback гарантированно не слушается обычными процессами.
    sendAll(client,
            "GET http://127.0.0.1:1/ HTTP/1.1\r\n"
            "Host: 127.0.0.1:1\r\n\r\n");
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.starts_with("HTTP/1.1 502 Bad Gateway"));
}

TEST_CASE("ProxyServer returns 502 when request is missing Host header", "[proxy]")
{
    ProxyHarness proxy;

    const int client = connectLoopback(proxy.port());
    sendAll(client, "GET /relative HTTP/1.1\r\n\r\n");
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.starts_with("HTTP/1.1 502 Bad Gateway"));
}

TEST_CASE("ProxyServer establishes CONNECT tunnel and relays bytes both ways", "[proxy]")
{
    MockServer target([](int sock)
                      {
        // Эхо-сервер: возвращает каждый принятый байт.
        char buffer[1024];
        while (true)
        {
            const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
            if (received <= 0)
            {
                break;
            }
            sendAll(sock, std::string_view(buffer, static_cast<std::size_t>(received)));
        } });

    ProxyHarness proxy;

    const int client = connectLoopback(proxy.port());
    const auto targetPort = std::to_string(target.port());
    sendAll(client,
            "CONNECT 127.0.0.1:" + targetPort + " HTTP/1.1\r\n"
                                                "Host: 127.0.0.1:" +
                targetPort + "\r\n\r\n");

    constexpr std::string_view ConnectOk = "HTTP/1.1 200 Connection Established\r\n\r\n";
    const auto handshake = recvN(client, ConnectOk.size());
    REQUIRE(handshake == ConnectOk);

    sendAll(client, "ping");
    const auto echoed = recvN(client, 4);
    CHECK(echoed == "ping");

    sendAll(client, "second");
    const auto echoed2 = recvN(client, 6);
    CHECK(echoed2 == "second");

    ::close(client);
}

TEST_CASE("ProxyServer blocks CONNECT to filtered host with 403", "[proxy]")
{
    MockServer target([](int) {});
    const auto targetPort = std::to_string(target.port());
    ProxyHarness proxy(R"(["127.0.0.1"])");

    const int client = connectLoopback(proxy.port());
    sendAll(client,
            "CONNECT 127.0.0.1:" + targetPort + " HTTP/1.1\r\n"
                                                "Host: 127.0.0.1:" +
                targetPort + "\r\n\r\n");
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.starts_with("HTTP/1.1 403 Forbidden"));
    CHECK(target.connectionCount() == 0);
}

TEST_CASE("ProxyServer handles concurrent requests in parallel", "[proxy]")
{
    std::atomic<int> processed{0};

    MockServer target([&](int sock)
                      {
        readHttpRequest(sock);
        processed.fetch_add(1);
        // Небольшая задержка, чтобы соединения действительно перекрылись.
        std::this_thread::sleep_for(50ms);
        sendAll(sock, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"); });

    ProxyHarness proxy;
    const auto targetPort = std::to_string(target.port());

    constexpr int RequestCount = 5;
    std::vector<std::future<std::string>> futures;
    futures.reserve(RequestCount);

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < RequestCount; ++i)
    {
        futures.push_back(std::async(std::launch::async, [&proxy, &targetPort]
                                     {
            const int client = connectLoopback(proxy.port());
            sendAll(client,
                "GET http://127.0.0.1:" + targetPort + "/x HTTP/1.1\r\n"
                "Host: 127.0.0.1:" + targetPort + "\r\n\r\n");
            const auto response = recvAll(client);
            ::close(client);
            return response; }));
    }

    for (auto &future : futures)
    {
        const auto response = future.get();
        CHECK(response.find("HTTP/1.1 200 OK") != std::string::npos);
        CHECK(response.find("ok") != std::string::npos);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(target.connectionCount() == RequestCount);
    CHECK(processed.load() == RequestCount);
    CHECK(elapsed < 200ms);
}

TEST_CASE("ProxyServer returns 502 on malformed client request", "[proxy]")
{
    ProxyHarness proxy;

    const int client = connectLoopback(proxy.port());
    sendAll(client, "this is not a valid request line\r\n\r\n");
    const auto response = recvAll(client);
    ::close(client);

    CHECK(response.starts_with("HTTP/1.1 502 Bad Gateway"));
}
