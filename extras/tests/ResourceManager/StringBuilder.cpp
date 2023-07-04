// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2023, Benoit BLANCHON
// MIT License

#include <ArduinoJson/Memory/StringBuilder.hpp>
#include <catch.hpp>

#include "Allocators.hpp"

using namespace ArduinoJson::detail;

TEST_CASE("StringBuilder") {
  ControllableAllocator controllableAllocator;
  SpyingAllocator spyingAllocator(&controllableAllocator);
  ResourceManager resources(0, &spyingAllocator);

  SECTION("Empty string") {
    StringBuilder builder(&resources);

    builder.startString();
    builder.save();

    REQUIRE(resources.size() == sizeofString(0));
    REQUIRE(resources.overflowed() == false);
    REQUIRE(spyingAllocator.log() ==
            AllocatorLog() << AllocatorLog::Allocate(sizeofString(31))
                           << AllocatorLog::Reallocate(sizeofString(31),
                                                       sizeofString(0)));
  }

  SECTION("Short string fits in first allocation") {
    StringBuilder builder(&resources);

    builder.startString();
    builder.append("hello");

    REQUIRE(builder.isValid() == true);
    REQUIRE(builder.str() == "hello");
    REQUIRE(resources.overflowed() == false);
    REQUIRE(spyingAllocator.log() ==
            AllocatorLog() << AllocatorLog::Allocate(sizeofString(31)));
  }

  SECTION("Long string needs reallocation") {
    StringBuilder builder(&resources);

    builder.startString();
    builder.append(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.");

    REQUIRE(builder.isValid() == true);
    REQUIRE(builder.str() ==
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
            "eiusmod tempor incididunt ut labore et dolore magna aliqua.");
    REQUIRE(resources.overflowed() == false);
    REQUIRE(spyingAllocator.log() ==
            AllocatorLog() << AllocatorLog::Allocate(sizeofString(31))
                           << AllocatorLog::Reallocate(sizeofString(31),
                                                       sizeofString(63))
                           << AllocatorLog::Reallocate(sizeofString(63),
                                                       sizeofString(127)));
  }

  SECTION("Realloc fails") {
    StringBuilder builder(&resources);

    builder.startString();
    controllableAllocator.disable();
    builder.append(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua.");

    REQUIRE(spyingAllocator.log() ==
            AllocatorLog() << AllocatorLog::Allocate(sizeofString(31))
                           << AllocatorLog::ReallocateFail(sizeofString(31),
                                                           sizeofString(63))
                           << AllocatorLog::Deallocate(sizeofString(31)));
    REQUIRE(builder.isValid() == false);
    REQUIRE(resources.overflowed() == true);
  }

  SECTION("Initial allocation fails") {
    StringBuilder builder(&resources);

    controllableAllocator.disable();
    builder.startString();

    REQUIRE(builder.isValid() == false);
    REQUIRE(resources.overflowed() == true);
    REQUIRE(spyingAllocator.log() ==
            AllocatorLog() << AllocatorLog::AllocateFail(sizeofString(31)));
  }
}

static StringNode* addStringToPool(ResourceManager& resources, const char* s) {
  StringBuilder builder(&resources);
  builder.startString();
  builder.append(s);
  return builder.save().node();
}

TEST_CASE("StringBuilder::save() deduplicates strings") {
  ResourceManager resources(4096);

  SECTION("Basic") {
    auto s1 = addStringToPool(resources, "hello");
    auto s2 = addStringToPool(resources, "world");
    auto s3 = addStringToPool(resources, "hello");

    REQUIRE(s1 == s3);
    REQUIRE(s2 != s3);
    REQUIRE(s1->references == 2);
    REQUIRE(s2->references == 1);
    REQUIRE(s3->references == 2);
    REQUIRE(resources.size() == 2 * sizeofString(5));
  }

  SECTION("Requires terminator") {
    auto s1 = addStringToPool(resources, "hello world");
    auto s2 = addStringToPool(resources, "hello");

    REQUIRE(s2 != s1);
    REQUIRE(s1->references == 1);
    REQUIRE(s2->references == 1);
    REQUIRE(resources.size() == sizeofString(11) + sizeofString(5));
  }

  SECTION("Don't overrun") {
    auto s1 = addStringToPool(resources, "hello world");
    auto s2 = addStringToPool(resources, "wor");

    REQUIRE(s2 != s1);
    REQUIRE(s1->references == 1);
    REQUIRE(s2->references == 1);
    REQUIRE(resources.size() == sizeofString(11) + sizeofString(3));
  }
}
