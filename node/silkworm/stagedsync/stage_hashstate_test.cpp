/*
   Copyright 2020-2021 The Silkworm Authors

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

#include <catch2/catch.hpp>
#include <ethash/keccak.hpp>

#include <silkworm/chain/config.hpp>
#include <silkworm/chain/protocol_param.hpp>
#include <silkworm/common/test_context.hpp>
#include <silkworm/db/buffer.hpp>
#include <silkworm/db/stages.hpp>
#include <silkworm/execution/address.hpp>
#include <silkworm/execution/execution.hpp>

#include "stagedsync.hpp"

TEST_CASE("Stage Hashstate") {
    using namespace evmc::literals;
    using namespace silkworm;

    test::Context context;
    db::RWTxn txn{context.txn()};

    // ---------------------------------------
    // Prepare
    // ---------------------------------------

    uint64_t block_number{1};
    auto miner{0x5a0b54d5dc17e0aadc383d2db43b0a0d3e029c4c_address};

    Block block{};
    block.header.number = block_number;
    block.header.beneficiary = miner;
    block.header.gas_limit = 100'000;
    block.header.gas_used = 63'820;

    // This contract initially sets its 0th storage to 0x2a
    // and its 1st storage to 0x01c9.
    // When called, it updates its 0th storage to the input provided.
    Bytes contract_code{*from_hex("600035600055")};
    Bytes deployment_code{*from_hex("602a6000556101c960015560068060166000396000f3") + contract_code};

    block.transactions.resize(1);
    block.transactions[0].data = deployment_code;
    block.transactions[0].gas_limit = block.header.gas_limit;
    block.transactions[0].max_priority_fee_per_gas = 20 * kGiga;
    block.transactions[0].max_fee_per_gas = block.transactions[0].max_priority_fee_per_gas;

    auto sender{0xb685342b8c54347aad148e1f22eff3eb3eb29391_address};
    block.transactions[0].r = 1;  // dummy
    block.transactions[0].s = 1;  // dummy
    block.transactions[0].from = sender;

    db::Buffer buffer{*txn, 0};
    Account sender_account{};
    sender_account.balance = kEther;
    buffer.update_account(sender, std::nullopt, sender_account);

    // ---------------------------------------
    // Execute first block
    // ---------------------------------------
    REQUIRE(execute_block(block, buffer, kMainnetConfig) == ValidationResult::kOk);

    auto contract_address{create_address(sender, /*nonce=*/0)};

    // ---------------------------------------
    // Execute second block
    // ---------------------------------------

    evmc::bytes32 new_val{0x000000000000000000000000000000000000000000000000000000000000003e_bytes32};

    block_number = 2;
    block.header.number = block_number;
    block.header.gas_used = 26'201;

    block.transactions[0].nonce = 1;
    block.transactions[0].value = 1000;

    block.transactions[0].to = contract_address;
    block.transactions[0].data = ByteView{new_val};

    REQUIRE(execute_block(block, buffer, kMainnetConfig) == ValidationResult::kOk);

    // ---------------------------------------
    // Execute third block
    // ---------------------------------------

    new_val = 0x000000000000000000000000000000000000000000000000000000000000003b_bytes32;

    block_number = 3;
    block.header.number = block_number;
    block.header.gas_used = 26'201;

    block.transactions[0].nonce = 2;
    block.transactions[0].value = 1000;

    block.transactions[0].to = contract_address;
    block.transactions[0].data = ByteView{new_val};

    REQUIRE(execute_block(block, buffer, kMainnetConfig) == ValidationResult::kOk);
    REQUIRE_NOTHROW(buffer.write_to_db());
    REQUIRE_NOTHROW(db::stages::write_stage_progress(*txn, db::stages::kExecutionKey, 3));

    // ---------------------------------------
    // Execute stage forward
    // ---------------------------------------
    REQUIRE(stagedsync::stage_hashstate(txn, context.dir().etl().path()) == stagedsync::StageResult::kSuccess);
    REQUIRE(db::stages::read_stage_progress(*txn, db::stages::kHashStateKey) == 3);

    // ---------------------------------------
    // Check hashed account
    // ---------------------------------------

    auto hashed_address_table{db::open_cursor(*txn, db::table::kHashedAccounts)};
    auto sender_keccak{keccak256(sender)};
    REQUIRE(hashed_address_table.seek(db::to_slice(sender_keccak.bytes)));
    {
        auto account_encoded{db::from_slice(hashed_address_table.current().value)};
        auto [acc, _]{decode_account_from_storage(account_encoded)};
        CHECK(acc.nonce == 3);
        CHECK(acc.balance < kEther);
    }

    // ---------------------------------------
    // Check hashed storage
    // ---------------------------------------

    auto hashed_storage_cursor{db::open_cursor(*txn, db::table::kHashedStorage)};
    auto contract_keccak{keccak256(contract_address)};
    Bytes storage_key{db::storage_prefix(contract_keccak.bytes, kDefaultIncarnation)};

    hashed_storage_cursor.find(db::to_slice(storage_key));

    // we expect two and only two values
    CHECK(hashed_storage_cursor.count_multivalue() == 2);

    // location 0
    auto hashed_loc0{keccak256(0x0000000000000000000000000000000000000000000000000000000000000000_bytes32)};
    hashed_storage_cursor.to_current_first_multi();
    mdbx::slice db_val{hashed_storage_cursor.current().value};
    CHECK(db_val.starts_with(db::to_slice(hashed_loc0.bytes)));
    ByteView value{db::from_slice(db_val).substr(kHashLength)};
    CHECK(to_hex(value) == to_hex(zeroless_view(new_val)));

    // location 1
    auto hashed_loc1{keccak256(0x0000000000000000000000000000000000000000000000000000000000000001_bytes32)};
    hashed_storage_cursor.to_current_next_multi();
    db_val = hashed_storage_cursor.current().value;
    CHECK(db_val.starts_with(db::to_slice(hashed_loc1.bytes)));
    value = db::from_slice(db_val).substr(kHashLength);
    CHECK(to_hex(value) == "01c9");

    // Unwind the stage
    REQUIRE(stagedsync::unwind_hashstate(txn, context.dir().etl().path(), 1) == stagedsync::StageResult::kSuccess);

    hashed_address_table = db::open_cursor(*txn, db::table::kHashedAccounts);
    auto address_keccak{keccak256(sender)};

    REQUIRE(hashed_address_table.seek(db::to_slice(address_keccak.bytes)));
    {
        auto account_encoded{db::from_slice(hashed_address_table.current().value)};
        auto [acc, _]{decode_account_from_storage(account_encoded)};
        CHECK(acc.nonce == 2);
        CHECK(acc.balance < kEther);  // Slightly less due to fees
        CHECK(db::stages::read_stage_progress(*txn, db::stages::kHashStateKey) == 1);
    }
}
