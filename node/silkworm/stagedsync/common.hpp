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
#ifndef SILKWORM_STAGES_COMMON_HPP_
#define SILKWORM_STAGES_COMMON_HPP_

#include <cstdint>
#include <exception>

#include <magic_enum.hpp>

#include <silkworm/common/directories.hpp>
#include <silkworm/common/log.hpp>
#include <silkworm/db/access_layer.hpp>
#include <silkworm/db/stages.hpp>
#include <silkworm/db/storage.hpp>

namespace silkworm::stages {

enum class [[nodiscard]] StageResult{
    kSuccess,                 //
    kUnknownChainId,          //
    kUnknownConsensusEngine,  //
    kBadBlockHash,            //
    kBadChainSequence,        //
    kInvalidRange,            //
    kInvalidProgress,         //
    kInvalidBlock,            //
    kInvalidTransaction,      //
    kMissingSenders,          //
    kDecodingError,           //
    kUnexpectedError,         //
    kUnknownError,            //
    kDbError,                 //
    kAborted,                 //
    kNotImplemented           //
};

//! \brief Stage execution exception
class StageError : public std::exception {
  public:
    explicit StageError(StageResult err)
        : err_{magic_enum::enum_integer<StageResult>(err)},
          message_{"Stage error : " + std::string(magic_enum::enum_name<StageResult>(err))} {};
    [[maybe_unused]] explicit StageError(StageResult err, std::string message)
        : err_{magic_enum::enum_integer<StageResult>(err)}, message_{std::move(message)} {};
    ~StageError() noexcept override = default;
    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }
    [[nodiscard]] int err() const noexcept { return err_; }

  protected:
    int err_;
    std::string message_;
};

//! \brief Throws StageError exception when code =! StageResult::kSuccess
//! \param [in] code : The result of a stage operation
inline void success_or_throw(StageResult code) {
    if (code != StageResult::kSuccess) {
        throw StageError(code);
    }
}

//! \brief SyncContext holds the info common to stages during sync cycles
class SyncContext {
  public:
    //! \brief Creates an instance of SyncContext
    //! \param [in] data_dir : A reference to DataDirectory
    explicit SyncContext(DataDirectory& data_dir, db::PruneMode prune_mode)
        : data_dir_{data_dir}, prune_mode_{std::move(prune_mode)} {};

    //! \brief Clears all context data
    //! \remarks Also clears etl temporary directory
    void clear() {
        progresses_.clear();
        prune_progresses_.clear();
        data_dir_.etl().clear();
        first_cycle_ = true;
    }

    //! \brief Resets `first_cycle_` flag
    void clear_first_cycle() { first_cycle_ = false; }

    //! \brief Returns current data directory for this context
    DataDirectory& data_dir() { return data_dir_; }

    //! \brief Return current progress for stage
    //! \param [in] txn : A db transaction reference
    //! \param [in] stage_key : The key (name) of the stage
    //! \return BlockNum
    //! \remarks If current progress is not cached it then gets pulled from db
    BlockNum get_progress(mdbx::txn& txn, const char* stage_key) {
        if (progresses_.find(stage_key) != progresses_.end()) {
            return progresses_[stage_key];
        }
        auto db_progress{db::stages::read_stage_progress(txn, stage_key)};
        progresses_[stage_key] = db_progress;
        return db_progress;
    }

    //! \brief Return current prune progress for stage
    //! \param [in] txn : A db transaction reference
    //! \param [in] stage_key : The key (name) of the stage
    //! \return BlockNum
    //! \remarks If current progress is not cached it then gets pulled from db
    BlockNum get_prune_progress(mdbx::txn& txn, const char* stage_key) {
        if (prune_progresses_.find(stage_key) != prune_progresses_.end()) {
            return prune_progresses_[stage_key];
        }
        auto db_progress{db::stages::read_stage_prune_progress(txn, stage_key)};
        prune_progresses_[stage_key] = db_progress;
        return db_progress;
    }

    //! \brief Returns whether we've done syncing
    [[nodiscard]] bool is_done() const { return is_done_; }

    //! \brief Whether or not the provided stage needs unwind
    //! \param [in] progress : The actual height progress of the stage
    //! \param [out] unwind_progress : The new height the stage should unwind to
    //! \return True/False. In case of True progress is valued with actual unwind point
    bool needs_unwind(BlockNum progress, BlockNum& unwind_progress) {
        if (!unwind_height_.has_value() || unwind_height_.value() >= progress) {
            return false;
        }
        unwind_progress = unwind_height_.value();
        return true;
    }

    //! \brief Returns actual prune mode
    [[nodiscard]] db::PruneMode prune_mode() const { return prune_mode_; }

    //! \brief No more work has to be done
    void set_done() { is_done_ = true; }

    //! \brief Returns the required unwind height (if any)
    //! \remarks If no unwind point is set it returns std::nullopt
    std::optional<BlockNum> unwind_height() { return unwind_height_; }

    //! \brief Saves current progress for stage both in stage cache and db
    //! \param [in] txn : A db transaction reference
    //! \param [in] stage_key : The key (name) of the stage
    //! \param [in] progress : Actual progress to store
    void update_progress(mdbx::txn& txn, const char* stage_key, BlockNum progress) {
        progresses_[stage_key] = progress;
        db::stages::write_stage_progress(txn, stage_key, progress);
    }

    //! \brief Saves current prune progress for stage both in stage cache and db
    //! \param [in] txn : A db transaction reference
    //! \param [in] stage_key : The key (name) of the stage
    //! \param [in] progress : Actual progress to store
    void update_prune_progress(mdbx::txn& txn, const char* stage_key, BlockNum progress) {
        prune_progresses_[stage_key] = progress;
        db::stages::write_stage_prune_progress(txn, stage_key, progress);
    }

  private:
    DataDirectory& data_dir_;                                          // Where silkworm stores all data;
    const db::PruneMode prune_mode_;                                   // Actual prune mode
    bool first_cycle_{true};                                           // Whether this is first sync cycle.
    bool is_done_{false};                                              // Whether we've finished syncing
    std::map<const char*, BlockNum, str_compare> progresses_{};        // Track forward progress for each stage
    std::map<const char*, BlockNum, str_compare> prune_progresses_{};  // Track prune progress for each stage
    std::optional<BlockNum> unwind_height_{std::nullopt};              // The new height all stages should unwind to
};

//! Base Stage interface. All stages MUST inherit from this class and MUST override forward / unwind /
//! prune
class IStage {
  public:
    explicit IStage(const char* stage_key, uint32_t ordinal, bool has_pruning, bool is_disabled)
        : stage_key_{stage_key}, ordinal_{ordinal}, has_pruning_{has_pruning}, disabled_{is_disabled} {};

    //! \brief Forward is called when the stage is executed. The main logic of the stage should be here.
    //! \param [in] txn_mgr : TransactionManager holding db transaction
    //! \param [in] context : Actual SyncContext
    //! \return StageResult
    //! \remarks Must be overridden
    [[nodiscard]] virtual StageResult forward(db::TransactionManager& txn_mgr, SyncContext& context) = 0;

    //! \brief Whether this instance is disabled
    //! \return True/False
    [[nodiscard]] bool is_disabled() const { return disabled_; }

    //! \brief Unwind is called when the stage should be unwound. The unwind logic should be there.
    //! \param [in] txn_mgr : TransactionManager holding db transaction
    //! \param [in] context : Actual SyncContext
    //! \return StageResult
    //! \remarks Must be overridden
    [[nodiscard]] virtual StageResult unwind(db::TransactionManager& txn_mgr, SyncContext& context) = 0;

    //! \brief Prune is called when (part of) stage previously persisted data should be deleted. The pruning logic
    //! should be there.
    //! \param [in] txn_mgr : TransactionManager holding db transaction
    //! \param [in] context : Actual SyncContext
    //! \return StageResult
    [[nodiscard]] virtual StageResult prune(db::TransactionManager& txn_mgr, SyncContext& context) {
        (void)txn_mgr;
        (void)context;
        SILKWORM_LOG(LogLevel::Warn) << "Prune called for stage " << stage_key_
                                     << " but is not implemented in its class" << std::endl;
        return StageResult::kSuccess;
    };

    //! \return The unique ordinal identifier of this instance
    [[nodiscard]] uint32_t get_ordinal() const { return ordinal_; }

    //! \return Whether this stage instance implements pruning
    [[nodiscard]] bool has_pruning() const { return has_pruning_; }

    //! \return The key of instance
    [[nodiscard]] const char* key() const { return stage_key_; }

  protected:
    const char* stage_key_;   // Unique key of the stage
    const uint32_t ordinal_;  // Unique ordinal identifier of this stage instance. Must be > 0;
    const bool has_pruning_;  // Whether this stage implements pruning
    const bool disabled_;     // Whether this stage is disabled
};

}  // namespace silkworm::stages
#endif  // SILKWORM_STAGES_COMMON_HPP_
