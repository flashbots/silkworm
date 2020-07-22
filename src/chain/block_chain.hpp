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

#ifndef SILKWORM_CHAIN_BLOCK_CHAIN_H_
#define SILKWORM_CHAIN_BLOCK_CHAIN_H_

#include "config.hpp"
#include "db/database.hpp"

namespace silkworm {

class BlockChain {
 public:
  BlockChain(const BlockChain&) = delete;
  BlockChain& operator=(const BlockChain&) = delete;

  explicit BlockChain(db::Database* db);

  const ChainConfig& config() const { return config_; }

  std::optional<BlockHeader> get_header(uint64_t block_number,
                                        const evmc::bytes32& block_hash) const;

 private:
  ChainConfig config_{kEthMainnetChainConfig};
  db::Database* db_{nullptr};
};
}  // namespace silkworm

#endif  // SILKWORM_CHAIN_BLOCK_CHAIN_H_