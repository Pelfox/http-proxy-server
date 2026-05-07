#include "filter/filter.hpp"

#include <catch2/catch_amalgamated.hpp>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{
    std::string writeConfig(const std::string &fileName, const std::string &content)
    {
        const auto path = "/tmp/" + fileName;
        std::ofstream config(path);
        REQUIRE(config.is_open());
        config << content;
        return path;
    }
}

TEST_CASE("Filter loads blocked base URLs from JSON config", "[filter]")
{
    const auto configPath = writeConfig(
        "http_proxy_filter_valid.json",
        R"([
            "google.com",
            "https://discord.com/"
        ])");

    const Filter filter(configPath);

    CHECK(filter.isBlocked("https://google.com"));
    CHECK(filter.isBlocked("https://google.com/search"));
    CHECK(filter.isBlocked("https://google.com?q=test"));
    CHECK(filter.isBlocked("https://google.com#top"));
    CHECK(filter.isBlocked("https://google.com:443"));
    CHECK(filter.isBlocked("https://discord.com/channels"));

    CHECK_FALSE(filter.isBlocked("https://google.com.evil"));
    CHECK_FALSE(filter.isBlocked("https://example.com"));

    std::remove(configPath.c_str());
}

TEST_CASE("Filter supports manual updates to the blacklist", "[filter]")
{
    const auto configPath = writeConfig("http_proxy_filter_empty.json", "[]");
    Filter filter(configPath);

    filter.addUrl("https://example.com/");
    CHECK(filter.isBlocked("https://example.com/path"));

    filter.removeUrl("https://example.com");
    CHECK_FALSE(filter.isBlocked("https://example.com/path"));

    std::remove(configPath.c_str());
}

TEST_CASE("Filter rejects invalid JSON configs", "[filter]")
{
    const auto objectConfig = writeConfig("http_proxy_filter_object.json", R"({"base_url": "https://google.com"})");
    const auto objectRuleConfig = writeConfig("http_proxy_filter_object_rule.json", R"([{"base_url": "https://google.com"}])");
    const auto emptyRuleConfig = writeConfig("http_proxy_filter_empty_rule.json", R"([""])");

    CHECK_THROWS_AS(Filter(objectConfig), std::runtime_error);
    CHECK_THROWS_AS(Filter(objectRuleConfig), std::runtime_error);
    CHECK_THROWS_AS(Filter(emptyRuleConfig), std::runtime_error);

    std::remove(objectConfig.c_str());
    std::remove(objectRuleConfig.c_str());
    std::remove(emptyRuleConfig.c_str());
}
