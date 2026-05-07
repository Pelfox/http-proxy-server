#include "proxy_server.hpp"

#include "http/http_response.hpp"
#include "utils/string_utils.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    constexpr int BacklogSize = 128;
    constexpr std::size_t BufferSize = 16 * 1024;
    constexpr std::string_view HeaderSeparator = "\r\n\r\n";

    struct TargetEndpoint
    {
        std::string host;
        std::string port;
        std::string path;
        std::string filterUrl;
    };

    /**
     * @brief RAII-обёртка над файловым дескриптором сокета.
     *
     * Закрывает сокет при выходе из области видимости, включая пути с
     * исключениями. Это защищает сервер от утечек дескрипторов при ошибках
     * подключения, парсинга или пересылки данных.
     */
    class FileDescriptor
    {
    public:
        explicit FileDescriptor(int value = -1)
            : value(value) {}

        FileDescriptor(FileDescriptor &&other) noexcept
            : value(std::exchange(other.value, -1)) {}

        ~FileDescriptor()
        {
            if (value >= 0)
            {
                close(value);
            }
        }

        int get() const
        {
            return value;
        }

    private:
        int value;
    };

    /**
     * @brief Отключает SIGPIPE на macOS при записи в закрытый сокет.
     *
     * Без этого сервер может завершиться сигналом, если клиент закрыл
     * соединение раньше, чем прокси успел отправить ответ.
     */
    void disableSigPipe(int socket)
    {
#ifdef SO_NOSIGPIPE
        constexpr int enabled = 1;
        setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#else
        (void)socket;
#endif
    }

    /**
     * @brief Отправляет все байты в сокет.
     *
     * send() не обязан записывать весь буфер за один вызов, поэтому отправка
     * выполняется в цикле до конца данных или до ошибки.
     */
    bool sendAll(int socket, std::string_view data)
    {
        while (!data.empty())
        {
            const auto sent = send(socket, data.data(), data.size(), 0);
            if (sent <= 0)
            {
                return false;
            }

            data.remove_prefix(static_cast<std::size_t>(sent));
        }

        return true;
    }

    bool sendAll(int socket, const std::vector<uint8_t> &data)
    {
        return sendAll(socket, std::string_view(reinterpret_cast<const char *>(data.data()), data.size()));
    }

    /**
     * @brief Читает из клиентского сокета один полный HTTP-запрос.
     *
     * Сначала читается блок заголовков до разделителя, затем, если есть
     * Content-Length, дочитывается тело запроса указанной длины.
     */
    std::vector<uint8_t> readHttpMessage(int socket)
    {
        std::string data;
        std::array<char, BufferSize> buffer{};

        const auto readChunk = [&](std::string_view onCloseError)
        {
            const auto received = recv(socket, buffer.data(), buffer.size(), 0);
            if (received <= 0)
            {
                throw std::runtime_error(std::string(onCloseError));
            }
            data.append(buffer.data(), static_cast<std::size_t>(received));
        };

        while (data.find(HeaderSeparator) == std::string::npos)
        {
            readChunk("Client closed connection before sending HTTP headers.");
        }

        const auto headerBlockEnd = data.find(HeaderSeparator);
        const auto bodyOffset = headerBlockEnd + HeaderSeparator.size();
        const auto expectedBodySize = HttpRequest::contentLengthFromHeaders(
            std::string_view(data).substr(0, headerBlockEnd));

        while (data.size() < bodyOffset + expectedBodySize)
        {
            readChunk("Client closed connection before sending complete HTTP body.");
        }

        return {data.begin(), data.end()};
    }

    /**
     * @brief Разделяет URL на host и port.
     *
     * Поддерживает обычный host:port и IPv6-форму [::1]:port. Если порт не
     * указан, возвращается defaultPort.
     */
    std::pair<std::string, std::string> splitHostPort(std::string_view authority, std::string_view defaultPort)
    {
        if (authority.starts_with('['))
        {
            const auto closingBracket = authority.find(']');
            if (closingBracket == std::string_view::npos)
            {
                throw std::runtime_error("Invalid IPv6 authority.");
            }

            const auto host = authority.substr(1, closingBracket - 1);
            if (closingBracket + 1 < authority.size() && authority[closingBracket + 1] == ':')
            {
                return {std::string(host), std::string(authority.substr(closingBracket + 2))};
            }

            return {std::string(host), std::string(defaultPort)};
        }

        const auto delimiter = authority.rfind(':');
        if (delimiter != std::string_view::npos)
        {
            return {std::string(authority.substr(0, delimiter)), std::string(authority.substr(delimiter + 1))};
        }

        return {std::string(authority), std::string(defaultPort)};
    }

    /**
     * @brief Получает адрес назначения для HTTPS-туннеля CONNECT.
     */
    TargetEndpoint targetForConnect(const HttpRequest &request)
    {
        const auto [host, port] = splitHostPort(request.url, "443");
        return {host, port, {}, host + ":" + port};
    }

    /**
     * @brief Получает адрес назначения для обычного HTTP-запроса.
     *
     * Браузер может прислать абсолютный URI, а может прислать форму запроса с
     * Host-заголовком. Для пересылки на target-сервер прокси должен знать host,
     * port и path отдельно.
     */
    TargetEndpoint targetForHttp(const HttpRequest &request)
    {
        constexpr std::string_view HttpScheme = "http://";

        if (std::string_view(request.url).starts_with(HttpScheme))
        {
            const std::string_view absoluteTarget(request.url);
            const auto authorityStart = HttpScheme.size();
            const auto pathStart = absoluteTarget.find('/', authorityStart);
            const auto authority = pathStart == std::string_view::npos
                                       ? absoluteTarget.substr(authorityStart)
                                       : absoluteTarget.substr(authorityStart, pathStart - authorityStart);
            const auto [host, port] = splitHostPort(authority, "80");
            const auto path = pathStart == std::string_view::npos ? "/" : std::string(absoluteTarget.substr(pathStart));
            return {host, port, path, request.url};
        }

        const auto hostHeader = request.headerValue("Host");
        if (!hostHeader.has_value())
        {
            throw std::runtime_error("HTTP request without absolute URI must contain Host header.");
        }

        const auto [host, port] = splitHostPort(*hostHeader, "80");
        return {host, port, request.url.empty() ? "/" : request.url, "http://" + host + request.url};
    }

    /**
     * @brief Открывает TCP-соединение до целевого сервера.
     */
    FileDescriptor connectToTarget(const std::string &host, const std::string &port)
    {
        addrinfo hints{};
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;

        addrinfo *result = nullptr;
        const int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
        if (status != 0)
        {
            throw std::runtime_error("Unable to resolve target host: " + host);
        }

        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(result, freeaddrinfo);
        for (auto *address = addresses.get(); address != nullptr; address = address->ai_next)
        {
            FileDescriptor socketDescriptor(socket(address->ai_family, address->ai_socktype, address->ai_protocol));
            if (socketDescriptor.get() < 0)
            {
                continue;
            }

            disableSigPipe(socketDescriptor.get());
            if (connect(socketDescriptor.get(), address->ai_addr, address->ai_addrlen) == 0)
            {
                return socketDescriptor;
            }
        }

        throw std::runtime_error("Unable to connect to target: " + host + ":" + port);
    }

    /**
     * @brief Собирает запрос, который будет отправлен целевому HTTP-серверу.
     */
    std::string buildForwardRequest(const HttpRequest &request, const TargetEndpoint &target)
    {
        std::string result = request.method + " " + target.path + " " + request.version + "\r\n";
        bool hasHostHeader = false;

        for (const auto &[name, value] : request.headers)
        {
            const auto lowerName = string_utils::toLower(name);
            if (lowerName == "proxy-connection" || lowerName == "connection")
            {
                continue;
            }
            if (lowerName == "host")
            {
                hasHostHeader = true;
            }
            result += name + ": " + value + "\r\n";
        }

        if (!hasHostHeader)
        {
            result += "Host: " + target.host + "\r\n";
        }

        result += "Connection: close\r\n\r\n";
        result.append(reinterpret_cast<const char *>(request.body.data()), request.body.size());
        return result;
    }

    void sendResponse(int clientSocket, const HttpResponse &response)
    {
        sendAll(clientSocket, response.serialize());
    }

    /**
     * @brief Пересылает HTTP-ответ target-сервера обратно клиенту.
     */
    void relayResponse(int sourceSocket, int destinationSocket)
    {
        std::array<char, BufferSize> buffer{};

        while (true)
        {
            const auto received = recv(sourceSocket, buffer.data(), buffer.size(), 0);
            if (received <= 0)
            {
                break;
            }

            if (!sendAll(destinationSocket, std::string_view(buffer.data(), static_cast<std::size_t>(received))))
            {
                break;
            }
        }
    }

    /**
     * @brief Читает ответ target-сервера до закрытия соединения.
     *
     * Прокси принудительно устанавливает "Connection: close" в переадресуемом
     * запросе, поэтому конечный сервер закроет соединение по окончании ответа.
     * Эта функция собирает все байты ответа в буфер, чтобы их можно было
     * одновременно отправить клиенту и сохранить в кэш.
     */
    std::vector<uint8_t> readResponseUntilClose(int socket)
    {
        std::vector<uint8_t> data;
        std::array<char, BufferSize> buffer{};

        while (true)
        {
            const auto received = recv(socket, buffer.data(), buffer.size(), 0);
            if (received <= 0)
            {
                break;
            }
            data.insert(data.end(),
                        reinterpret_cast<const uint8_t *>(buffer.data()),
                        reinterpret_cast<const uint8_t *>(buffer.data()) + received);
        }

        return data;
    }

    /**
     * @brief Прокидывает байты в обе стороны для HTTPS CONNECT.
     *
     * После ответа "200 Connection Established" прокси больше не понимает
     * содержимое соединения: TLS идёт сквозным потоком между браузером и
     * целевым сервером.
     */
    void tunnel(int firstSocket, int secondSocket)
    {
        std::array<char, BufferSize> buffer{};

        while (true)
        {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(firstSocket, &readSet);
            FD_SET(secondSocket, &readSet);

            const int maxSocket = std::max(firstSocket, secondSocket);
            const int ready = select(maxSocket + 1, &readSet, nullptr, nullptr, nullptr);
            if (ready <= 0)
            {
                break;
            }

            const auto relayOneDirection = [&](int source, int destination)
            {
                const auto received = recv(source, buffer.data(), buffer.size(), 0);
                return received > 0 && sendAll(destination, std::string_view(buffer.data(), static_cast<std::size_t>(received)));
            };

            if (FD_ISSET(firstSocket, &readSet) && !relayOneDirection(firstSocket, secondSocket))
            {
                break;
            }

            if (FD_ISSET(secondSocket, &readSet) && !relayOneDirection(secondSocket, firstSocket))
            {
                break;
            }
        }
    }
}

ProxyServer::ProxyServer(uint16_t port, const std::string &filterConfigPath)
    : port(port), cache(100, std::chrono::seconds(300)), filter(filterConfigPath), logger(std::cout) {}

void ProxyServer::start()
{
    // Главный поток только принимает соединения. Каждый клиент обрабатывается
    // в отдельном потоке.
    FileDescriptor serverSocket(socket(AF_INET, SOCK_STREAM, 0));
    if (serverSocket.get() < 0)
    {
        throw std::runtime_error("Unable to create server socket.");
    }

    disableSigPipe(serverSocket.get());
    constexpr int enabled = 1;
    setsockopt(serverSocket.get(), SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(serverSocket.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        throw std::runtime_error("Unable to bind server socket on port " + std::to_string(port) + ".");
    }

    if (listen(serverSocket.get(), BacklogSize) < 0)
    {
        throw std::runtime_error("Unable to listen on server socket.");
    }

    logger.write(LogLevel::Info, "HTTP proxy server started on port " + std::to_string(port));
    while (true)
    {
        FileDescriptor clientSocket(accept(serverSocket.get(), nullptr, nullptr));
        if (clientSocket.get() < 0)
        {
            logger.write(LogLevel::Warning, "Failed to accept client connection.");
            continue;
        }

        disableSigPipe(clientSocket.get());
        std::thread([this, clientSocket = std::move(clientSocket)]() mutable
                    {
            try
            {
                HttpRequest request;
                request.parse(readHttpMessage(clientSocket.get()));
                processRequest(clientSocket.get(), std::move(request));
            }
            catch (const std::exception &error)
            {
                logger.write(LogLevel::Error, error.what());
                sendResponse(clientSocket.get(), HttpResponse::text(502, "Proxy request failed.\n"));
            } })
            .detach();
    }
}

void ProxyServer::processRequest(int clientSocket, HttpRequest request)
{
    logger.write(LogLevel::Info, request.method + " " + request.url);

    if (request.method == "CONNECT")
    {
        // HTTPS в HTTP-прокси начинается с CONNECT. Если хост не заблокирован,
        // открываем TCP-соединение и дальше просто прокидываем зашифрованные
        // байты в обе стороны.
        const auto target = targetForConnect(request);
        if (filter.isBlocked(target.host) || filter.isBlocked(target.filterUrl))
        {
            sendResponse(clientSocket, HttpResponse::text(403, "Request blocked by proxy filter.\n"));
            return;
        }

        FileDescriptor targetSocket = connectToTarget(target.host, target.port);
        sendAll(clientSocket, "HTTP/1.1 200 Connection Established\r\n\r\n");
        tunnel(clientSocket, targetSocket.get());
        return;
    }

    // Обычный HTTP-запрос пересобирается из прокси формы в форму, которую
    // понимает target-сервер, затем ответ target-сервера копируется клиенту.
    const auto target = targetForHttp(request);
    if (filter.isBlocked(target.host) || filter.isBlocked(target.filterUrl))
    {
        sendResponse(clientSocket, HttpResponse::text(403, "Request blocked by proxy filter.\n"));
        return;
    }

    const bool cacheable = request.method == "GET";
    if (cacheable)
    {
        if (auto cached = cache.get(target.filterUrl))
        {
            logger.write(LogLevel::Info, "Cache hit: " + target.filterUrl);
            sendResponse(clientSocket, cached->response);
            return;
        }
    }

    FileDescriptor targetSocket = connectToTarget(target.host, target.port);
    const std::string forwardRequest = buildForwardRequest(request, target);
    if (!sendAll(targetSocket.get(), forwardRequest))
    {
        throw std::runtime_error("Unable to forward HTTP request to target.");
    }

    if (!cacheable)
    {
        relayResponse(targetSocket.get(), clientSocket);
        return;
    }

    // Кэшируем только успешные GET-ответы. Если парсинг не удался или код
    // не 200, просто отдаём ответ клиенту без сохранения.
    const auto rawResponse = readResponseUntilClose(targetSocket.get());
    sendAll(clientSocket, rawResponse);

    try
    {
        HttpResponse response;
        response.parse(rawResponse);
        if (response.statusCode == 200)
        {
            cache.put(target.filterUrl, response);
            logger.write(LogLevel::Info, "Cache stored: " + target.filterUrl);
        }
    }
    catch (const std::exception &error)
    {
        logger.write(LogLevel::Debug, std::string("Skip caching: ") + error.what());
    }
}
