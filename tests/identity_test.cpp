#include "application_config.h"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

TEST_CASE("native desktop identity is stable")
{
    REQUIRE(std::string_view(ADRENALIN_APP_NAME) == "AMD Software");
    REQUIRE(std::string_view(ADRENALIN_APP_ID) == "org.adrenalinlinux.App");
    REQUIRE(!std::string_view(ADRENALIN_APP_VERSION).empty());
}
