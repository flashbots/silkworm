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
#ifndef SILKWORM_STAGES_SYNCMANAGER_HPP_
#define SILKWORM_STAGES_SYNCMANAGER_HPP_

#include <memory>
#include <vector>

#include <silkworm/common/directories.hpp>

#include "common.hpp"
#include "stage_blockhashes.hpp"

namespace silkworm::stages {

//! \brief Handles the stage cycle loop
class SyncManager {
  public:
    //! \brief Crates instance of StageManager
    //! \param [in] db : A reference to MDBX opened database
    explicit SyncManager(mdbx::env& db, DataDirectory& data_directory);

    //! \brief Runs sync cycles and returns result
    stages::StageResult void run();

    //! \brief Returns the number of stages this instance manages
    [[nodiscard]] size_t size() const { return stages_.size(); }

  private:
    mdbx::env& db_;                                          // Database
    SyncContext context_;                                    // Initializes context
    std::vector<std::unique_ptr<stages::IStage>> stages_{};  // Collection of stages
    uint32_t current_stage_index_{0};                        // Actual stage index wrt stages_

    db::PruneMode load_prune_mode(mdbx::env& db);  // Used to load prune mode in context ctor
    void load_stages();                            // Used to prime stages
    stages::StageResult run_cycle();               // Runs a single sync cycle
};
}  // namespace silkworm::stages
#endif  // SILKWORM_STAGES_SYNCMANAGER_HPP_
