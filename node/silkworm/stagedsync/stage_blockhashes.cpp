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

#include "stage_blockhashes.hpp"

#include <silkworm/common/endian.hpp>
#include <silkworm/etl/collector.hpp>

namespace silkworm::stages {

StageResult StageBlockHashes::forward(db::TransactionManager& tx_mgr, SyncContext& context) {
    etl::Collector collector(context.data_dir().etl().path(), /* flush size */ 512_Mebi);

    try {
        BlockNum progress_bodies{context.get_progress(*tx_mgr, db::stages::kBlockBodiesKey)};
        BlockNum progress{context.get_progress(*tx_mgr, stage_key_)};
        BlockNum expected_block_number{progress ? progress + 1 : progress};
        BlockNum block_number{0};

        auto source_table{db::open_cursor(*tx_mgr, db::table::kCanonicalHashes)};
        auto source_key{db::block_key(expected_block_number)};
        auto source_data{source_table.find(db::to_slice(source_key), /*throw_notfound=*/false)};
        while (source_data.done) {
            block_number = endian::load_big_u64(static_cast<uint8_t*>(source_data.key.iov_base));
            if (block_number != expected_block_number) {
                // Something wrong with db. Blocks are out of sequence for any reason
                // Should not happen, but you never know
                SILKWORM_LOG(LogLevel::Error) << "Bad headers sequence. Expected " << expected_block_number << " got "
                                              << block_number << std::endl;
                return StageResult::kBadChainSequence;
            }
            if (source_data.value.length() != kHashLength) {
                SILKWORM_LOG(LogLevel::Error) << "Bad header hash for block " << block_number << std::endl;
                return StageResult::kBadBlockHash;
            }

            // Collect mapping swapping value with key
            collector.collect(
                etl::Entry{Bytes(static_cast<uint8_t*>(source_data.value.iov_base), source_data.value.iov_len),
                           Bytes(static_cast<uint8_t*>(source_data.key.iov_base), source_data.key.iov_len)});
            ++expected_block_number;
            source_data = source_table.to_next(/*throw_notfound=*/false);
        }
        source_table.close();
        if (block_number != progress_bodies) {
            // We did not reach the same progress.
            // Should not happen
            SILKWORM_LOG(LogLevel::Error)
                << "Bad header sequence. Expected " << progress_bodies << " got " << block_number << std::endl;
            return StageResult::kBadChainSequence;
        }
        SILKWORM_LOG(LogLevel::Debug) << "Entries Collected << " << collector.size() << std::endl;
        if (collector.empty()) {
            return StageResult::kSuccess;
        }

        auto target_table{db::open_cursor(*tx_mgr, db::table::kHeaderNumbers)};
        auto target_table_empty{tx_mgr->get_map_stat(target_table.map()).ms_entries == 0};
        MDBX_put_flags_t db_flags{target_table_empty ? MDBX_put_flags_t::MDBX_APPEND : MDBX_put_flags_t::MDBX_UPSERT};
        collector.load(target_table, nullptr, db_flags, /* log_every_percent = */ 10);
        context.update_progress(*tx_mgr, stage_key_, block_number);
        tx_mgr.commit();
        return StageResult::kSuccess;

    } catch (const mdbx::exception& ex) {
        SILKWORM_LOG(LogLevel::Error) << "Unexpected database error in " << std::string(__FUNCTION__) << " : "
                                      << ex.what() << std::endl;
        return StageResult::kDbError;
    } catch (const std::exception& ex) {
        SILKWORM_LOG(LogLevel::Error) << "Unexpected error in " << std::string(__FUNCTION__) << " : " << ex.what()
                                      << std::endl;
        return StageResult::kUnexpectedError;
    } catch (...) {
        SILKWORM_LOG(LogLevel::Error) << "Undefined error in " << std::string(__FUNCTION__) << std::endl;
        return StageResult::kUnknownError;
    }
}

StageResult StageBlockHashes::unwind(db::TransactionManager& tx_mgr, SyncContext& context) {
    try {
        BlockNum progress{context.get_progress(*tx_mgr, stage_key_)};
        BlockNum unwind_progress{0};
        if (!context.needs_unwind(progress, unwind_progress)) {
            return StageResult::kSuccess;
        }

        BlockNum expected_block_number{unwind_progress ? unwind_progress + 1 : unwind_progress};
        BlockNum block_number{0};

        auto source_table{db::open_cursor(*tx_mgr, db::table::kCanonicalHashes)};
        auto target_table{db::open_cursor(*tx_mgr, db::table::kHeaderNumbers)};

        auto source_key{db::block_key(expected_block_number)};
        auto source_data{source_table.find(db::to_slice(source_key), /*throw_notfound=*/false)};
        while (source_data.done) {
            block_number = endian::load_big_u64(static_cast<uint8_t*>(source_data.key.iov_base));
            if (block_number != expected_block_number) {
                // Something wrong with db. Blocks are out of sequence for any reason
                // Should not happen, but you never know
                SILKWORM_LOG(LogLevel::Error) << "Bad headers sequence. Expected " << expected_block_number << " got "
                                              << block_number << std::endl;
                return StageResult::kBadChainSequence;
            }
            if (source_data.value.length() != kHashLength) {
                SILKWORM_LOG(LogLevel::Error) << "Bad header hash for block " << block_number << std::endl;
                return StageResult::kBadBlockHash;
            }

            // Delete mapping
            if (target_table.seek(source_data.value)) {
                target_table.erase();
            } else {
                // This should not happen
                SILKWORM_LOG(LogLevel::Warn)
                    << "Could not locate hash for block #" << block_number << " in " << stage_key_;
            }

            ++expected_block_number;
            source_data = source_table.to_next(/*throw_notfound=*/false);
        }

        source_table.close();
        target_table.close();
        context.update_progress(*tx_mgr, stage_key_, unwind_progress);
        tx_mgr.commit();
        return StageResult::kSuccess;

    } catch (const mdbx::exception& ex) {
        SILKWORM_LOG(LogLevel::Error) << "Unexpected database error in " << std::string(__FUNCTION__) << " : "
                                      << ex.what() << std::endl;
        return StageResult::kDbError;
    } catch (const std::exception& ex) {
        SILKWORM_LOG(LogLevel::Error) << "Unexpected error in " << std::string(__FUNCTION__) << " : " << ex.what()
                                      << std::endl;
        return StageResult::kUnexpectedError;
    } catch (...) {
        SILKWORM_LOG(LogLevel::Error) << "Undefined error in " << std::string(__FUNCTION__) << std::endl;
        return StageResult::kUnknownError;
    }
}

}  // namespace silkworm::stages
