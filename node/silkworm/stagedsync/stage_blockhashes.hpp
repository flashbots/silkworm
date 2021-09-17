/*
    Copyright 2021 The Silkworm Authors

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

#pragma once
#ifndef SILKWORM_STAGEDSYNC_BLOCKHASHES_HPP_
#define SILKWORM_STAGEDSYNC_BLOCKHASHES_HPP_

#include "common.hpp"

namespace silkworm::stages {

//! \brief BlockHashes creates the mapping from CanonicalHashes bucket (BlockNumber -> HeaderHash) to HeaderNumber table
//! (HeaderHash -> BlockNumber).
class StageBlockHashes : public IStage {
  public:
    explicit StageBlockHashes(uint32_t ordinal) : IStage(db::stages::kBlockHashesKey, ordinal, false, false){};

    [[nodiscard]] StageResult forward(db::TransactionManager& tx_mgr, SyncContext& context) final;

    [[nodiscard]] StageResult unwind(db::TransactionManager& tx_mgr, SyncContext& context) final;
};

}  // namespace silkworm::stages
#endif  // SILKWORM_STAGEDSYNC_BLOCKHASHES_HPP_
