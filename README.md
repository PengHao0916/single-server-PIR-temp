# Dual-Side Optimization for HintlessPIR

This repository contains the implementation of the paper **"Dual-Side Optimization for HintlessPIR: Achieving Practical Bidirectional Communication Efficiency"**.

Built upon the [HintlessPIR](https://eprint.iacr.org/2023/1733) framework, this project addresses the critical bidirectional communication bottlenecks in single-server Private Information Retrieval (PIR). By introducing a dual-side optimization strategy, we achieve significant bandwidth reductions and throughput improvements while maintaining the "hintless" property (i.e., no database-dependent client storage).

## 🚀 Key Contributions

While the original HintlessPIR eliminates client-side storage, it fundamentally shifts the burden to the network, causing a "bandwidth explosion." Our work resolves this via two core optimizations:

1.  **Downlink Optimization (Asymmetric Response Decomposition):**
    * **Mechanism:** Structurally isolates static public components from the response ciphertext and offloads them to an offline phase.
    * [cite_start]**Impact:** Mathematically guarantees a **50% reduction** in the online response payload[cite: 28, 1167].

2.  **Uplink Optimization (Session-Based Key Caching):**
    * **Mechanism:** Implements server-side caching for homomorphic evaluation keys, treating them as session-invariant.
    * [cite_start]**Impact:** Reduces uplink query sizes by up to **74%** for subsequent requests[cite: 30, 1287].

## 📊 Performance

Our approach establishes a new practical equilibrium between throughput and communication efficiency.

### Bandwidth Reduction (1 GB Database)
| Metric | Baseline (HintlessPIR) | **Ours (Dual-Side Optimized)** | Improvement |
| :--- | :--- | :--- | :--- |
| **Response Size (Download)** | ~1.45 MB | **~0.76 MB** | 📉 **-50%** |
| **Query Size (Upload)** | ~365 KB | **~95 KB** | 📉 **-74%** |

### Throughput
[cite_start]By eliminating redundant computations of static artifacts, our scheme achieves a **1.17× improvement** in server throughput compared to the baseline[cite: 31, 230].

## 🧩 Background: HintlessPIR

Private Information Retrieval (PIR) allows a client to retrieve a record from a database without revealing which record was selected.

* **Single-Server PIR:** Uses Homomorphic Encryption (HE) to process encrypted queries over the database.
* **Hintless Setting:** The client stores no database-dependent state ("hint"), and the server stores no client-specific state. This simplifies database updates and client deployment.

This library is based on the original [HintlessPIR implementation](https://github.com/google/hintless_pir) by Li et al., which utilizes LWE-based SimplePIR and outsources hint computation to the server via LinPIR.

## 🛠️ Installation & Build

This project uses [Bazel](https://bazel.build/) for building and testing.

### Prerequisites
* Bazel
* C++17 compatible compiler

### Building
Clone the repository and run the tests to verify the installation:

```bash
# Run all tests
bazel test //...

# Run benchmarks
bazel run -c opt //hintless_simplepir:hintless_simplepir_benchmarks