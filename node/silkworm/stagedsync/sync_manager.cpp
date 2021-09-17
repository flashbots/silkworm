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

#include "sync_manager.hpp"

namespace silkworm::stages {

SyncManager::SyncManager(mdbx::env& db, DataDirectory& data_directory)
    : db_{db}, context_(data_directory, load_prune_mode(db)) {
    context_.clear();  // Ensure everything is clean
    load_stages();     // Loads all stages instances
}

void SyncManager::load_stages() {
    uint32_t ordinal{1};
    stages_.push_back(std::make_unique<StageBlockHashes>(ordinal++));

    // Prime stages
    auto roTx{db_.start_read()};
    for (const auto& stage : stages_) {
        (void)context_.get_progress(roTx, stage->key());
        (void)context_.get_prune_progress(roTx, stage->key());
    }
    roTx.commit();
}

db::PruneMode SyncManager::load_prune_mode(mdbx::env& db) {
    auto roTx{db_.start_read()};
    return db::read_prune_mode(roTx);
}

stages::StageResult SyncManager::run() {
    StageResult res{StageResult::kSuccess};
    while (!context_.is_done()) {
        res = run_cycle();
        if (res != StageResult::kSuccess) {
            return res;
        }
        context_.clear_first_cycle();
    }
    return res;
}

stages::StageResult SyncManager::run_cycle() {
    while (!context_.is_done()) {
        // Process unwindings
        if (context_.unwind_height().has_value()) {
            for (const auto& stage_key : db::stages::kReverseStages) {

                auto item =
                    std::find_if(stages_.begin(), stages_.end(),
                                 [&stage_key](std::unique_ptr<IStage>& stage) -> bool { return stage_key == stage->key(); });
                if(item == stages_.end() || item->get()->is_disabled()) {
                    continue;
                }

            }
        }
    }
}

}  // namespace silkworm::stages

// namespace silkworm::stages