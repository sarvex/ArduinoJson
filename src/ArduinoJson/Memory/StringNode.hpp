// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2023, Benoit BLANCHON
// MIT License

#pragma once

#include <ArduinoJson/Memory/Allocator.hpp>
#include <ArduinoJson/Namespace.hpp>
#include <ArduinoJson/Polyfills/assert.hpp>

#include <stdint.h>  // uint16_t

ARDUINOJSON_BEGIN_PRIVATE_NAMESPACE

struct StringNode {
  struct StringNode* next;
  uint16_t length;
  uint16_t references;
  char data[1];

  static constexpr size_t sizeForLength(size_t n) {
    return n + 1 + offsetof(StringNode, data);
  }

  static StringNode* create(size_t length, Allocator* allocator) {
    auto node = reinterpret_cast<StringNode*>(
        allocator->allocate(sizeForLength(length)));
    if (node) {
      node->length = uint16_t(length);
      node->references = 1;
    }
    return node;
  }

  static StringNode* resize(StringNode* node, size_t length,
                            Allocator* allocator) {
    ARDUINOJSON_ASSERT(node != nullptr);
    auto newNode = reinterpret_cast<StringNode*>(
        allocator->reallocate(node, sizeForLength(length)));
    if (newNode)
      newNode->length = uint16_t(length);
    else
      allocator->deallocate(node);
    return newNode;
  }

  static void destroy(StringNode* node, Allocator* allocator) {
    allocator->deallocate(node);
  }
};

// Returns the size (in bytes) of an string with n characters.
constexpr size_t sizeofString(size_t n) {
  return StringNode::sizeForLength(n);
}

// A string adapter for StringNode
class PoolString {
 public:
  static const size_t typeSortKey = 4;

  PoolString(StringNode* node) : node_(node) {}

  bool isNull() const {
    return !node_;
  }

  bool isLinked() const {
    return false;
  }

  size_t size() const {
    ARDUINOJSON_ASSERT(node_);
    return node_->length;
  }

  char operator[](size_t i) const {
    ARDUINOJSON_ASSERT(node_);
    return node_->data[i];
  }

  const char* data() const {
    ARDUINOJSON_ASSERT(node_);
    return node_->data;
  }

  StringNode* node() {
    return node_;
  }

 private:
  StringNode* node_;
};

ARDUINOJSON_END_PRIVATE_NAMESPACE
