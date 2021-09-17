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

#ifndef SILKWORM_STAGEDSYNC_STAGEDSYNC_HPP_
#define SILKWORM_STAGEDSYNC_STAGEDSYNC_HPP_

// See https://github.com/ledgerwatch/erigon/blob/devel/eth/stagedsync/README.md

#include <filesystem>
#include <vector>

#include <silkworm/db/stages.hpp>
#include <silkworm/db/storage.hpp>
#include <silkworm/db/tables.hpp>
#include <silkworm/stagedsync/util.hpp>

#include "common.hpp"

namespace silkworm::stagedsync {

using namespace silkworm::stages;  // TODO(Andrea) Remove when rename stagedsync namespace to stages

constexpr size_t kDefaultBatchSize = 512_Mebi;
constexpr size_t kDefaultRecoverySenderBatch = 50'000;  // This a number of transactions not number of bytes

typedef StageResult (*ForwardFunc)(db::TransactionManager&, const std::filesystem::path& etl_path, uint64_t prune_from);
typedef StageResult (*UnwindFunc)(db::TransactionManager&, const std::filesystem::path& etl_path, uint64_t unwind_to);
typedef StageResult (*PruneFunc)(db::TransactionManager&, const std::filesystem::path& etl_path, uint64_t prune_from);


/* Refactor below */

struct Stage {
    std::string description;           // A string shown in the logs
    bool disabled;                     // Whether it should be executed
    std::string disabled_description;  // Reason for the stage being disabled

    //! \brief Forward is called when the stage is executed. The main logic of the stage should be here.
    ForwardFunc forward;
    //! \brief Unwind is called when the stage should be unwound. The unwind logic should be there.
    UnwindFunc unwind;
    //! \brief Prune is called when the stage data should be deleted. The pruning logic should be there.
    PruneFunc prune;
    //! \brief Unique identifier of the stage. Should be > 0
    uint32_t id;
};

// Stage functions
StageResult stage_headers(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t prune_from = 0);
StageResult stage_bodies(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t prune_from = 0);
StageResult stage_senders(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t prune_from = 0);
StageResult stage_execution(db::TransactionManager& txn, const std::filesystem::path& etl_path, size_t batch_size,
                            uint64_t prune_from);
inline StageResult stage_execution(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                                   uint64_t prune_from = 0) {
    return stage_execution(txn, etl_path, kDefaultBatchSize, prune_from);
}

/* HashState Promotion Functions*/

/*
    * Operation is used to distinguish what bucket we want to generated
    * HashAccount is for genenerating HashedAccountBucket
    * HashStorage is for genenerating HashedStorageBucket
    * Code generates hashed key => code_hash mapping

*/
enum class HashstateOperation {
    HashAccount,
    HashStorage,
    Code,
};

void hashstate_promote(mdbx::txn&, HashstateOperation);
void hashstate_promote_clean_code(mdbx::txn& txn, const std::filesystem::path& etl_path);
void hashstate_promote_clean_state(mdbx::txn& txn, const std::filesystem::path& etl_path);

/* **************************** */
StageResult stage_hashstate(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                            uint64_t prune_from = 0);
StageResult stage_interhashes(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                              uint64_t prune_from = 0);
StageResult stage_account_history(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                                  uint64_t prune_from = 0);
StageResult stage_storage_history(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                                  uint64_t prune_from = 0);
StageResult stage_log_index(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                            uint64_t prune_from = 0);
StageResult stage_tx_lookup(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                            uint64_t prune_from = 0);

// Unwind functions
StageResult unwind_blockhashes(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t unwind_to);
StageResult unwind_senders(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t unwind_to);
StageResult unwind_execution(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t unwind_to);
StageResult unwind_hashstate(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t unwind_to);
StageResult unwind_interhashes(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t unwind_to);
StageResult unwind_account_history(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                                   uint64_t unwind_to);
StageResult unwind_storage_history(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                                   uint64_t unwind_to);
StageResult unwind_log_index(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t unwind_to);
StageResult unwind_tx_lookup(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t unwind_to);
// Prune functions
StageResult prune_senders(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t prune_from);
StageResult prune_execution(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t prune_from);
StageResult prune_account_history(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                                  uint64_t prune_from);
StageResult prune_storage_history(db::TransactionManager& txn, const std::filesystem::path& etl_path,
                                  uint64_t prune_from);
StageResult prune_log_index(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t prune_from);
StageResult prune_tx_lookup(db::TransactionManager& txn, const std::filesystem::path& etl_path, uint64_t prune_from);

}  // namespace silkworm::stagedsync

#endif  // SILKWORM_STAGEDSYNC_STAGEDSYNC_HPP_
