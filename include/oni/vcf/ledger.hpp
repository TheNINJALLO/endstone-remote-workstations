#pragma once
#include "core.hpp"
#include <filesystem>
namespace oni::vcf {
enum class Boundary:uint32_t { prepared=1, debit=2, escrow=3, output=4, delivery=5, acknowledgement=6 };
struct Record {uint64_t sequence,transaction;Boundary boundary;std::vector<uint8_t> payload;};
// Durable append-only evidence. All surviving transactions are uncertain until
// BDS and plugin state are reconciled. This class deliberately never grants items.
class Ledger {
 std::filesystem::path path_;intptr_t file_=-1;uint64_t sequence_=0;
 std::vector<Record> records_;bool poisoned_=false;
 void write(std::span<const uint8_t>);
public:
 explicit Ledger(const std::filesystem::path&);
 ~Ledger();
 Ledger(const Ledger&)=delete;Ledger&operator=(const Ledger&)=delete;
 void append(uint64_t transaction,Boundary,std::span<const uint8_t> payload);
 std::span<const Record> records()const{return records_;}
 std::vector<uint64_t> quarantined()const;
};
}
