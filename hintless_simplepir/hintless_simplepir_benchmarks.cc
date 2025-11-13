// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hintless_simplepir/client.h"
#include "hintless_simplepir/database_hwy.h"
#include "hintless_simplepir/parameters.h"
#include "hintless_simplepir/server.h"
#include "linpir/parameters.h"
#include "shell_encryption/testing/status_testing.h"

ABSL_FLAG(int, num_rows, 1024, "Number of rows");
ABSL_FLAG(int, num_cols, 1024, "Number of cols");

namespace hintless_pir {
namespace hintless_simplepir {
namespace {

using RlweInteger = Parameters::RlweInteger;

const Parameters kParameters{
    .db_rows = 1024,
    .db_cols = 1024,
    .db_record_bit_size =64,
    .lwe_secret_dim = 1024,
    .lwe_modulus_bit_size = 32,
    .lwe_plaintext_bit_size = 8,
    .lwe_error_variance = 8,
    .linpir_params =
        linpir::RlweParameters<RlweInteger>{
            .log_n = 12,
            .qs = {35184371884033ULL, 35184371703809ULL},  // 90 bits
            .ts = {2056193, 1990657},                      // 42 bits
            .gadget_log_bs = {16, 16},
            .error_variance = 8,
            .prng_type = rlwe::PRNG_TYPE_HKDF,
            .rows_per_block = 1024,
        },
    .prng_type = rlwe::PRNG_TYPE_HKDF,
};

void BM_HintlessPirRlwe64(benchmark::State& state) {
  int64_t num_rows = absl::GetFlag(FLAGS_num_rows);
  int64_t num_cols = absl::GetFlag(FLAGS_num_cols);
  Parameters params = kParameters;
  params.db_rows = num_rows;
  params.db_cols = num_cols;

  // Create server and fill in random database records.
  auto server = Server::CreateWithRandomDatabaseRecords(params).value();
  const Database* database = server->GetDatabase();
  ASSERT_EQ(database->NumRecords(), num_rows * num_cols);

  // Preprocess the server and get public parameters.
  ASSERT_OK(server->Preprocess());
  auto public_params = server->GetPublicParams();

  // Create a client and issue request.
  auto client = Client::Create(params, public_params).value();
  
 auto request_1 = client->GenerateRequest(1).value();

  auto request_2 = client->GenerateRequest(2).value();

  if (state.thread_index() == 0) {
    auto response_for_size_measure = server->HandleRequest(request_1).value();
    size_t lwe_response_size = 0;
    for (const auto& record : response_for_size_measure.ct_records()) {
      lwe_response_size += record.ByteSizeLong();
    }

    size_t rlwe_response_size = 0;
    for (const auto& resp : response_for_size_measure.linpir_responses()) {
      rlwe_response_size += resp.ByteSizeLong();
    }
    std::cout << "----------------------------------------\n";
    std::cout << "Communication Sizes (KB)\n";
    std::cout << "Database Dimensions: " << num_rows << " x " << num_cols << "\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Hint Size (Public Params): "
              << public_params.ByteSizeLong() / 1024.0 << " KB\n";
    std::cout << "Query Size (Online 1st time):       "
              << request_1.ByteSizeLong() / 1024.0 << " KB(Includes Key)\n";

    std::cout << "Query Size (Online 2nd time):     "
              << request_2.ByteSizeLong() / 1024.0 << " KB (Optimized!)\n";

    std::cout << "Response Size (LWE part):  "
              << lwe_response_size / 1024.0 << " KB\n"; 
    std::cout << "Response Size (RLWE part): "
              << rlwe_response_size / 1024.0 << " KB\n"; 
    std::cout << "Response Size (Total):     "
              << response_for_size_measure.ByteSizeLong() / 1024.0 << " KB\n"; //           
    std::cout << "----------------------------------------\n";
  }

  for (auto _ : state) {
    auto response = server->HandleRequest(request_2);
    benchmark::DoNotOptimize(response);
  }

  // Sanity check on the correctness of the instantiation.
  auto response = server->HandleRequest(request_2).value();
  std::string record = client->RecoverRecord(response).value();
  std::string expected = database->Record(2).value();
  ASSERT_EQ(record, expected);
}
BENCHMARK(BM_HintlessPirRlwe64);

}  // namespace
}  // namespace hintless_simplepir
}  // namespace hintless_pir

// Declare benchmark_filter flag, which will be defined by benchmark library.
// Use it to check if any benchmarks were specified explicitly.
//
namespace benchmark {
extern std::string FLAGS_benchmark_filter;
}
using benchmark::FLAGS_benchmark_filter;

int main(int argc, char* argv[]) {
  FLAGS_benchmark_filter = "";
  benchmark::Initialize(&argc, argv);
  absl::ParseCommandLine(argc, argv);
  if (!FLAGS_benchmark_filter.empty()) {
    benchmark::RunSpecifiedBenchmarks();
  }
  benchmark::Shutdown();
  return 0;
}
