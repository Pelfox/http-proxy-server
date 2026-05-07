#include "cache/cache.hpp"
#include "http/http_response.hpp"

#include <catch2/catch_amalgamated.hpp>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace
{
    HttpResponse makeResponse(uint16_t code, std::string_view body)
    {
        return HttpResponse::text(code, body);
    }
}

TEST_CASE("Cache returns nullopt for missing url", "[cache]")
{
    Cache cache(10, 60s);
    REQUIRE_FALSE(cache.get("http://example.com/").has_value());
}

TEST_CASE("Cache stores and returns a response", "[cache]")
{
    Cache cache(10, 60s);
    cache.put("http://example.com/", makeResponse(200, "hello"));

    const auto entry = cache.get("http://example.com/");
    REQUIRE(entry.has_value());
    REQUIRE(entry->response.statusCode == 200);
    REQUIRE(std::string(entry->response.body.begin(), entry->response.body.end()) == "hello");
}

TEST_CASE("Cache evicts expired entries on get", "[cache]")
{
    Cache cache(10, std::chrono::seconds(0));
    cache.put("http://example.com/", makeResponse(200, "stale"));
    std::this_thread::sleep_for(10ms);

    REQUIRE_FALSE(cache.get("http://example.com/").has_value());
}

TEST_CASE("Cache evicts least recently used when over capacity", "[cache]")
{
    Cache cache(2, 60s);
    cache.put("a", makeResponse(200, "a"));
    cache.put("b", makeResponse(200, "b"));
    REQUIRE(cache.get("a").has_value());
    cache.put("c", makeResponse(200, "c"));

    REQUIRE(cache.get("a").has_value());
    REQUIRE(cache.get("c").has_value());
    REQUIRE_FALSE(cache.get("b").has_value());
}

TEST_CASE("Cache put replaces existing entry for the same url", "[cache]")
{
    Cache cache(10, 60s);
    cache.put("http://example.com/", makeResponse(200, "first"));
    cache.put("http://example.com/", makeResponse(200, "second"));

    const auto entry = cache.get("http://example.com/");
    REQUIRE(entry.has_value());
    REQUIRE(std::string(entry->response.body.begin(), entry->response.body.end()) == "second");
}

TEST_CASE("Cache remove drops the entry", "[cache]")
{
    Cache cache(10, 60s);
    cache.put("http://example.com/", makeResponse(200, "x"));
    cache.remove("http://example.com/");
    REQUIRE_FALSE(cache.get("http://example.com/").has_value());
}

TEST_CASE("Cache clear empties all entries", "[cache]")
{
    Cache cache(10, 60s);
    cache.put("a", makeResponse(200, "a"));
    cache.put("b", makeResponse(200, "b"));
    cache.clear();
    REQUIRE_FALSE(cache.get("a").has_value());
    REQUIRE_FALSE(cache.get("b").has_value());
}

TEST_CASE("HttpResponse::parse extracts status, headers, and body", "[http]")
{
    const std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";

    HttpResponse response;
    response.parse(std::vector<uint8_t>(raw.begin(), raw.end()));

    REQUIRE(response.statusCode == 200);
    REQUIRE(response.headers["Content-Type"] == "text/plain");
    REQUIRE(response.headers["Content-Length"] == "5");
    REQUIRE(std::string(response.body.begin(), response.body.end()) == "hello");
}
