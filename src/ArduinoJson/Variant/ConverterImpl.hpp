// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2023, Benoit BLANCHON
// MIT License

#pragma once

#include <ArduinoJson/Json/JsonSerializer.hpp>
#include <ArduinoJson/Memory/StringBuilder.hpp>
#include <ArduinoJson/Variant/JsonVariantConst.hpp>

ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE

template <typename T, typename Enable>
struct Converter {
  static void toJson(const T& src, JsonVariant dst) {
    // clang-format off
    convertToJson(src, dst); // Error here? See https://arduinojson.org/v6/unsupported-set/
    // clang-format on
  }

  static T fromJson(JsonVariantConst src) {
    // clang-format off
    T result; // Error here? See https://arduinojson.org/v6/non-default-constructible/
    convertFromJson(src, result);  // Error here? See https://arduinojson.org/v6/unsupported-as/
    // clang-format on
    return result;
  }

  static bool checkJson(JsonVariantConst src) {
    T dummy = T();
    // clang-format off
    return canConvertFromJson(src, dummy);  // Error here? See https://arduinojson.org/v6/unsupported-is/
    // clang-format on
  }
};

template <typename T>
struct Converter<
    T, typename detail::enable_if<detail::is_integral<T>::value &&
                                  !detail::is_same<bool, T>::value &&
                                  !detail::is_same<char, T>::value>::type>
    : private detail::VariantAttorney {
  static void toJson(T src, JsonVariant dst) {
    ARDUINOJSON_ASSERT_INTEGER_TYPE_IS_SUPPORTED(T);
    auto data = getData(dst);
    if (data)
      data->setInteger(src, getResourceManager(dst));
  }

  static T fromJson(JsonVariantConst src) {
    ARDUINOJSON_ASSERT_INTEGER_TYPE_IS_SUPPORTED(T);
    auto data = getData(src);
    return data ? data->template asIntegral<T>() : T();
  }

  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->template isInteger<T>();
  }
};

template <typename T>
struct Converter<T, typename detail::enable_if<detail::is_enum<T>::value>::type>
    : private detail::VariantAttorney {
  static void toJson(T src, JsonVariant dst) {
    dst.set(static_cast<JsonInteger>(src));
  }

  static T fromJson(JsonVariantConst src) {
    auto data = getData(src);
    return data ? static_cast<T>(data->template asIntegral<int>()) : T();
  }

  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->template isInteger<int>();
  }
};

template <>
struct Converter<bool> : private detail::VariantAttorney {
  static void toJson(bool src, JsonVariant dst) {
    auto data = getData(dst);
    if (data)
      data->setBoolean(src, getResourceManager(dst));
  }

  static bool fromJson(JsonVariantConst src) {
    auto data = getData(src);
    return data ? data->asBoolean() : false;
  }

  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isBoolean();
  }
};

template <typename T>
struct Converter<
    T, typename detail::enable_if<detail::is_floating_point<T>::value>::type>
    : private detail::VariantAttorney {
  static void toJson(T src, JsonVariant dst) {
    auto data = getData(dst);
    if (data)
      data->setFloat(static_cast<JsonFloat>(src), getResourceManager(dst));
  }

  static T fromJson(JsonVariantConst src) {
    auto data = getData(src);
    return data ? data->template asFloat<T>() : 0;
  }

  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isFloat();
  }
};

template <>
struct Converter<const char*> : private detail::VariantAttorney {
  static void toJson(const char* src, JsonVariant dst) {
    detail::VariantData::setString(getData(dst), detail::adaptString(src),
                                   getResourceManager(dst));
  }

  static const char* fromJson(JsonVariantConst src) {
    auto data = getData(src);
    return data ? data->asString().c_str() : 0;
  }

  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isString();
  }
};

template <>
struct Converter<JsonString> : private detail::VariantAttorney {
  static void toJson(JsonString src, JsonVariant dst) {
    detail::VariantData::setString(getData(dst), detail::adaptString(src),
                                   getResourceManager(dst));
  }

  static JsonString fromJson(JsonVariantConst src) {
    auto data = getData(src);
    return data ? data->asString() : 0;
  }

  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data && data->isString();
  }
};

template <typename T>
inline typename detail::enable_if<detail::IsString<T>::value>::type
convertToJson(const T& src, JsonVariant dst) {
  using namespace detail;
  auto data = VariantAttorney::getData(dst);
  auto resources = VariantAttorney::getResourceManager(dst);
  detail::VariantData::setString(data, adaptString(src), resources);
}

// SerializedValue<std::string>
// SerializedValue<String>
// SerializedValue<const __FlashStringHelper*>
template <typename T>
struct Converter<SerializedValue<T>> : private detail::VariantAttorney {
  static void toJson(SerializedValue<T> src, JsonVariant dst) {
    detail::VariantData::setRawString(getData(dst), src,
                                      getResourceManager(dst));
  }
};

template <>
struct Converter<decltype(nullptr)> : private detail::VariantAttorney {
  static void toJson(decltype(nullptr), JsonVariant dst) {
    detail::VariantData::setNull(getData(dst), getResourceManager(dst));
  }
  static decltype(nullptr) fromJson(JsonVariantConst) {
    return nullptr;
  }
  static bool checkJson(JsonVariantConst src) {
    auto data = getData(src);
    return data == 0 || data->isNull();
  }
};

#if ARDUINOJSON_ENABLE_ARDUINO_STREAM

namespace detail {
class StringBuilderPrint : public Print {
 public:
  StringBuilderPrint(ResourceManager* resources) : builder_(resources) {
    builder_.startString();
  }

  PoolString save() {
    ARDUINOJSON_ASSERT(!overflowed());
    return builder_.save();
  }

  size_t write(uint8_t c) {
    builder_.append(char(c));
    return builder_.isValid() ? 1 : 0;
  }

  size_t write(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
      builder_.append(char(buffer[i]));
      if (!builder_.isValid())
        return i;
    }
    return size;
  }

  bool overflowed() const {
    return !builder_.isValid();
  }

 private:
  StringBuilder builder_;
};
}  // namespace detail

inline void convertToJson(const ::Printable& src, JsonVariant dst) {
  auto resources = detail::VariantAttorney::getResourceManager(dst);
  auto data = detail::VariantAttorney::getData(dst);
  if (!resources || !data)
    return;
  detail::StringBuilderPrint print(resources);
  src.printTo(print);
  if (print.overflowed()) {
    data->setNull();
    return;
  }
  data->setOwnedString(print.save().node());
}

#endif

#if ARDUINOJSON_ENABLE_ARDUINO_STRING

inline void convertFromJson(JsonVariantConst src, ::String& dst) {
  JsonString str = src.as<JsonString>();
  if (str)
    dst = str.c_str();
  else
    serializeJson(src, dst);
}

inline bool canConvertFromJson(JsonVariantConst src, const ::String&) {
  return src.is<JsonString>();
}

#endif

#if ARDUINOJSON_ENABLE_STD_STRING

inline void convertFromJson(JsonVariantConst src, std::string& dst) {
  JsonString str = src.as<JsonString>();
  if (str)
    dst.assign(str.c_str(), str.size());
  else
    serializeJson(src, dst);
}

inline bool canConvertFromJson(JsonVariantConst src, const std::string&) {
  return src.is<JsonString>();
}

#endif

#if ARDUINOJSON_ENABLE_STRING_VIEW

inline void convertFromJson(JsonVariantConst src, std::string_view& dst) {
  JsonString str = src.as<JsonString>();
  if (str)  // the standard doesn't allow passing null to the constructor
    dst = std::string_view(str.c_str(), str.size());
}

inline bool canConvertFromJson(JsonVariantConst src, const std::string_view&) {
  return src.is<JsonString>();
}

#endif

namespace detail {
template <typename T>
struct ConverterNeedsWriteableRef {
 protected:  // <- to avoid GCC's "all member functions in class are private"
  static int probe(T (*f)(ArduinoJson::JsonVariant));
  static char probe(T (*f)(ArduinoJson::JsonVariantConst));

 public:
  static const bool value =
      sizeof(probe(Converter<T>::fromJson)) == sizeof(int);
};
}  // namespace detail

ARDUINOJSON_END_PUBLIC_NAMESPACE
