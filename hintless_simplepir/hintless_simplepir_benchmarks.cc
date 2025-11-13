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
#include <iostream>
#include <iomanip>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "benchmark/benchmark.h"
#include "hintless_simplepir/client.h"
#include "hintless_simplepir/database_hwy.h"
#include "hintless_simplepir/parameters.h"
#include "hintless_simplepir/server.h"
#include "linpir/parameters.h"

// 定义命令行参数
ABSL_FLAG(int, num_rows, 1024, "Number of rows");
ABSL_FLAG(int, num_cols, 1024, "Number of cols");

namespace hintless_pir {
namespace hintless_simplepir {
namespace {

using RlweInteger = Parameters::RlweInteger;

// 默认参数配置
const Parameters kParameters{
    .db_rows = 1024,
    .db_cols = 1024,
    .db_record_bit_size = 64,
    .lwe_secret_dim = 1024,
    .lwe_modulus_bit_size = 32,
    .lwe_plaintext_bit_size = 8,
    .lwe_error_variance = 8,
    .linpir_params =
        linpir::RlweParameters<RlweInteger>{
            .log_n = 12,
            .qs = {35184371884033ULL, 35184371703809ULL},
            .ts = {2056193, 1990657},
            .gadget_log_bs = {16, 16},
            .error_variance = 8,
            .prng_type = rlwe::PRNG_TYPE_HKDF,
            .rows_per_block = 1024,
        },
    .prng_type = rlwe::PRNG_TYPE_HKDF,
};

// 测试环境封装类，用于初始化 Server 和 Client
struct BenchmarkEnv {
  std::unique_ptr<Server> server;
  std::unique_ptr<Client> client;
  HintlessPirServerPublicParams public_params;

  BenchmarkEnv(const Parameters& params) {
    server = Server::CreateWithRandomDatabaseRecords(params).value();
    server->Preprocess().IgnoreError();
    public_params = server->GetPublicParams();
    client = Client::Create(params, public_params).value();
  }
};

void BM_SessionInit_FirstQuery(benchmark::State& state) {
  int64_t num_rows = absl::GetFlag(FLAGS_num_rows);
  int64_t num_cols = absl::GetFlag(FLAGS_num_cols);
  
  Parameters params = kParameters;
  params.db_rows = num_rows;
  params.db_cols = num_cols;

  BenchmarkEnv env(params);

  auto request = env.client->GenerateRequest(1).value();
  auto temp_response = env.server->HandleRequest(request).value();
  state.counters["Up (KB)"] = request.ByteSizeLong() / 1024.0;
  state.counters["Down (KB)"] = temp_response.ByteSizeLong() / 1024.0;
  state.counters["Hint (KB)"] = env.public_params.ByteSizeLong() / 1024.0;

  for (auto _ : state) {
    auto response = env.server->HandleRequest(request);
    benchmark::DoNotOptimize(response);
  }
}

// 注册测试：指定名称和时间单位
BENCHMARK(BM_SessionInit_FirstQuery)
    ->Name("1. First Query (Send Key)")
    ->Unit(benchmark::kMillisecond);

void BM_SessionReuse_SubsequentQuery(benchmark::State& state) {
  int64_t num_rows = absl::GetFlag(FLAGS_num_rows);
  int64_t num_cols = absl::GetFlag(FLAGS_num_cols);
  
  Parameters params = kParameters;
  params.db_rows = num_rows;
  params.db_cols = num_cols;

  BenchmarkEnv env(params);

  auto request_1 = env.client->GenerateRequest(1).value();
  env.server->HandleRequest(request_1).IgnoreError();
  auto request_2 = env.client->GenerateRequest(2).value();

  auto temp_response = env.server->HandleRequest(request_2).value();
  state.counters["Up (KB)"] = request_2.ByteSizeLong() / 1024.0;
  state.counters["Down (KB)"] = temp_response.ByteSizeLong() / 1024.0;
  state.counters["Hint (KB)"] = env.public_params.ByteSizeLong() / 1024.0;

  for (auto _ : state) {
    auto response = env.server->HandleRequest(request_2);
    benchmark::DoNotOptimize(response);
  }
}
BENCHMARK(BM_SessionReuse_SubsequentQuery)
    ->Name("2. Subsequent (Cached Key)")
    ->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace hintless_simplepir
}  // namespace hintless_pir

int main(int argc, char* argv[]) {
  // 先解析 Benchmark 参数（如 --benchmark_filter）
  benchmark::Initialize(&argc, argv);
  // 再解析 Abseil 参数（如 --num_rows）
  absl::ParseCommandLine(argc, argv);
  int rows = absl::GetFlag(FLAGS_num_rows);
  int cols = absl::GetFlag(FLAGS_num_cols);
  std::cout << "\n";
  std::cout << "============================================================================\n";
  std::cout << "                    HintlessPIR Performance Benchmark                     \n";
  std::cout << "============================================================================\n";
  std::cout << "  Database Config : " << rows << " rows x " << cols << " cols\n";
  std::cout << "  Block Size      : 1024 rows/block\n";
  std::cout << "  Optimization    : Upload and Download Cost Reduction (Session Resumption)\n";
  std::cout << "=====================================================================\n";
  benchmark::RunSpecifiedBenchmarks();
  std::cout << "=====================================================================\n";
  return 0;
}