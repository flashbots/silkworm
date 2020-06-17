/*
   Copyright 2020 The Silkworm Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "decode.hpp"

#include <boost/algorithm/hex.hpp>
#include <sstream>
#include <string_view>

// TODO(Andrew) Hunter's Catch
#include "../tests/catch.hpp"

namespace {

std::string unhex(std::string_view in) {
  std::string res;
  res.reserve(in.size() / 2);
  boost::algorithm::unhex(in.begin(), in.end(), std::back_inserter(res));
  return res;
}

std::string decoded_string(const std::string& s) {
  std::istringstream stream{s};
  return silkworm::rlp::decode_string(stream);
}

uint64_t decoded_uint64(const std::string& s) {
  std::istringstream stream{s};
  return silkworm::rlp::decode_uint64(stream);
}

}  // namespace

namespace silkworm::rlp {

TEST_CASE("decode", "[rlp]") {
  using namespace Catch;
  using namespace std::string_literals;

  SECTION("strings") {
    CHECK(decoded_string(unhex("00")) == "\x00"s);
    CHECK(decoded_string(unhex("8D6162636465666768696A6B6C6D")) == "abcdefghijklm");

    CHECK(decoded_string("\xB8\x38Lorem ipsum dolor sit amet, consectetur adipisicing elit") ==
          "Lorem ipsum dolor sit amet, consectetur adipisicing elit");

    CHECK_THROWS_MATCHES(decoded_string(unhex("C0")), DecodingError, Message("unexpected list"));
  }

  SECTION("uint64") {
    CHECK(decoded_uint64(unhex("09")) == 9);
    CHECK(decoded_uint64(unhex("80")) == 0);
    CHECK(decoded_uint64(unhex("820505")) == 0x0505);
    CHECK(decoded_uint64(unhex("850505050505")) == 0x0505050505);

    CHECK_THROWS_MATCHES(decoded_uint64(unhex("C0")), DecodingError, Message("unexpected list"));
    CHECK_THROWS_MATCHES(decoded_uint64(unhex("00")), DecodingError, Message("leading zero(s)"));
    CHECK_THROWS_MATCHES(decoded_uint64(unhex("8105")), DecodingError, Message("non-canonical single byte"));
    CHECK_THROWS_MATCHES(decoded_uint64(unhex("820004")), DecodingError, Message("leading zero(s)"));
    CHECK_THROWS_MATCHES(decoded_uint64(unhex("B8020004")), DecodingError, Message("non-canonical size"));
    CHECK_THROWS_MATCHES(decoded_uint64(unhex("89FFFFFFFFFFFFFFFFFF7C")), DecodingError, Message("uint64 overflow"));
  }
}

}  // namespace silkworm::rlp