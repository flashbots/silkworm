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

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#include <CLI/CLI.hpp>

#include <silkworm/common/directories.hpp>
#include <silkworm/common/log.hpp>
#include <silkworm/db/stages.hpp>
#include <silkworm/stagedsync/stagedsync.hpp>

using namespace silkworm;

int main(int argc, char* argv[]) {
    namespace fs = std::filesystem;

    CLI::App app{"Generates History Indexes"};

    std::string chaindata{DataDirectory{}.chaindata().path().string()};
    bool full{false}, storage{false};
    app.add_option("--chaindata", chaindata, "Path to a database populated by Erigon", true)
        ->check(CLI::ExistingDirectory);

    app.add_flag("--full", full, "Start making history indexes from block 0");
    app.add_flag("--storage", storage, "Do history of storages");

    CLI11_PARSE(app, argc, argv);

    auto data_dir{DataDirectory::from_chaindata(chaindata)};
    data_dir.deploy();
    db::EnvConfig db_config{data_dir.chaindata().path().string()};
    db::MapConfig index_config = storage ? db::table::kStorageHistory : db::table::kAccountHistory;
    const char* stage_key = storage ? db::stages::kStorageHistoryIndexKey : db::stages::kAccountHistoryIndexKey;

    try {
        auto env{db::open_env(db_config)};

        if (full) {
            auto txn{env.start_write()};
            txn.clear_map(db::open_map(txn, index_config));
            db::stages::write_stage_progress(txn, stage_key, 0);
            txn.commit();
        }

        db::RWTxn tm{env};
        if (storage) {
            stagedsync::success_or_throw(stagedsync::stage_storage_history(tm, data_dir.etl().path()));
        } else {
            stagedsync::success_or_throw(stagedsync::stage_account_history(tm, data_dir.etl().path()));
        }

    } catch (const std::exception& ex) {
        log::Error() << ex.what();
        return -5;
    }
    return 0;
}
