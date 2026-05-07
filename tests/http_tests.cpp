#include "http/http_request.hpp"
#include "http/http_response.hpp"

#include <catch2/catch_amalgamated.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    std::vector<uint8_t> bytes(const std::string &value)
    {
        return {value.begin(), value.end()};
    }

    std::string text(const std::vector<uint8_t> &value)
    {
        return {value.begin(), value.end()};
    }
}

TEST_CASE("HttpRequest parses request line, headers, and body", "[http]")
{
    HttpRequest request;
    request.parse(bytes(
        "POST http://example.com/api HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 5\r\n"
        "X-Test: yes\r\n"
        "\r\n"
        "helloignored"));

    CHECK(request.method == "POST");
    CHECK(request.url == "http://example.com/api");
    CHECK(request.headers.at("Host") == "example.com");
    CHECK(request.headers.at("Content-Length") == "5");
    CHECK(request.headers.at("X-Test") == "yes");
    CHECK(text(request.body) == "hello");
}

TEST_CASE("HttpRequest rejects malformed input", "[http]")
{
    HttpRequest request;

    CHECK_THROWS_AS(
        request.parse(bytes("GET / HTTP/1.1\r\nHost: example.com\r\n")),
        std::runtime_error);

    CHECK_THROWS_AS(
        request.parse(bytes("GET / HTTP/1.1\r\nBrokenHeader\r\n\r\n")),
        std::runtime_error);

    CHECK_THROWS_AS(
        request.parse(bytes("POST / HTTP/1.1\r\nContent-Length: x\r\n\r\nbody")),
        std::runtime_error);
}

TEST_CASE("HttpRequest validates request line shape", "[http]")
{
    HttpRequest request;

    CHECK_THROWS_AS(
        request.parse(bytes("GET / HTTP/1.1 extra\r\nHost: example.com\r\n\r\n")),
        std::runtime_error);

    CHECK_THROWS_AS(
        request.parse(bytes("GET /\r\nHost: example.com\r\n\r\n")),
        std::runtime_error);

    REQUIRE_NOTHROW(
        request.parse(bytes("GET   /index.html   HTTP/1.1\r\nHost: example.com\r\n\r\n")));
    CHECK(request.method == "GET");
    CHECK(request.url == "/index.html");
}

TEST_CASE("HttpResponse serializes status line, headers, and body", "[http]")
{
    HttpResponse response;
    response.statusCode = 200;
    response.headers["Content-Type"] = "text/plain";
    response.body = bytes("hello");

    CHECK(text(response.serialize()) ==
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/plain\r\n"
          "Content-Length: 5\r\n"
          "\r\n"
          "hello");
}

TEST_CASE("HttpResponse keeps explicit Content-Length header", "[http]")
{
    HttpResponse response;
    response.statusCode = 404;
    response.headers["Content-Length"] = "0";

    CHECK(text(response.serialize()) ==
          "HTTP/1.1 404 Not Found\r\n"
          "Content-Length: 0\r\n"
          "\r\n");
}
