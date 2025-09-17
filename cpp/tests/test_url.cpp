/*
 * Copyright (c) 2025, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <kvikio/detail/url.hpp>
#include <kvikio/shim/libcurl.hpp>
#include <stdexcept>

using ::testing::HasSubstr;
using ::testing::ThrowsMessage;

TEST(UrlTest, parse_scheme)
{
  {
    std::vector<std::string> invalid_scheme_urls{
      "invalid_scheme://host",
      // The S3 scheme is not supported by libcurl. Without the CURLU_NON_SUPPORT_SCHEME flag, an
      // exception is expected.
      "s3://host"};

    for (auto const& invalid_scheme_url : invalid_scheme_urls) {
      EXPECT_THAT([&] { kvikio::detail::UrlParser::parse(invalid_scheme_url); },
                  ThrowsMessage<std::runtime_error>(HasSubstr("KvikIO detects an URL error")));
    }
  }

  // With the CURLU_NON_SUPPORT_SCHEME flag, the S3 scheme is now accepted.
  {
    std::vector<std::string> schemes{"s3", "S3"};
    for (auto const& scheme : schemes) {
      auto parsed_url =
        kvikio::detail::UrlParser::parse(scheme + "://host", CURLU_NON_SUPPORT_SCHEME);
      EXPECT_EQ(parsed_url.scheme.value(), "s3");  // Lowercase due to CURL's normalization
    }
  }
}

TEST(UrlTest, parse_host)
{
  std::vector<std::string> invalid_host_urls{"http://host with spaces.com",
                                             "http://host[brackets].com",
                                             "http://host{braces}.com",
                                             "http://host<angle>.com",
                                             R"(http://host\backslash.com)",
                                             "http://host^caret.com",
                                             "http://host`backtick.com"};
  for (auto const& invalid_host_url : invalid_host_urls) {
    EXPECT_THROW({ kvikio::detail::UrlParser::parse(invalid_host_url); }, std::runtime_error);
  }
}

TEST(UrlTest, url_encode)
{
  // Test encoding of special characters that require URL encoding for S3 object keys
  // according to AWS S3 guidelines

  // Test basic alphanumeric characters (should remain unchanged)
  EXPECT_EQ(kvikio::url_encode("test123"), "test123");
  EXPECT_EQ(kvikio::url_encode("Test_file-name.txt"), "Test_file-name.txt");

  // Test that forward slashes are preserved (important for S3 object paths)
  EXPECT_EQ(kvikio::url_encode("path/to/file"), "path/to/file");

  // Test characters that require special handling according to AWS docs
  EXPECT_EQ(kvikio::url_encode("file=name"), "file%3Dname");  // Equal sign
  EXPECT_EQ(kvikio::url_encode("file&name"), "file%26name");  // Ampersand
  EXPECT_EQ(kvikio::url_encode("file$name"), "file%24name");  // Dollar sign
  EXPECT_EQ(kvikio::url_encode("file@name"), "file%40name");  // At symbol
  EXPECT_EQ(kvikio::url_encode("file+name"), "file%2Bname");  // Plus sign
  EXPECT_EQ(kvikio::url_encode("file name"), "file%20name");  // Space
  EXPECT_EQ(kvikio::url_encode("file,name"), "file%2Cname");  // Comma
  EXPECT_EQ(kvikio::url_encode("file?name"), "file%3Fname");  // Question mark
  EXPECT_EQ(kvikio::url_encode("file;name"), "file%3Bname");  // Semicolon
  EXPECT_EQ(kvikio::url_encode("file:name"), "file%3Aname");  // Colon

  // Test complex object names with multiple special characters
  EXPECT_EQ(kvikio::url_encode("my-data=2024&type=csv"), "my-data%3D2024%26type%3Dcsv");
  // Forward slashes should NOT be encoded as they are path separators
  EXPECT_EQ(kvikio::url_encode("folder/file name+data.csv"), "folder/file%20name%2Bdata.csv");

  // Test empty string
  EXPECT_EQ(kvikio::url_encode(""), "");
}
