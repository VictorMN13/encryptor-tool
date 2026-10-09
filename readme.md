# Concurrent File Encryptor

A high-throughput concurrent file encryption engine written in POSIX C. It optimizes processing for large datasets (e.g., 1GB+ files) through dynamic parallel chunking, zero-copy file I/O operations (`mmap`), and semaphore-based concurrency control.

## Features
* **Zero-copy I/O:** Uses memory mapping (`mmap`) to reduce system call overhead.
* **Concurrency Throttling:** Strictly manages detached POSIX threads execution state via semaphores to prevent memory exhaustion.
* **Boundary-safe Chunking:** Dynamically calculates memory chunk limits to avoid data corruption across thread boundaries during encryption.

## How it works
The encryption algorithm operates strictly on a word-by-word basis, ensuring that the original file's structure, formatting, and pagination remain completely intact. 

* **Text Parsing:** The engine scans the mapped memory chunks and identifies individual words. Spaces, tabs, newlines, and punctuation marks are treated as structural boundaries and are copied directly to the output file without modification.
* **Encryption (Permutation):** The characters within each identified word are randomly permuted (shuffled) using a custom seed generated per thread. The precise random mapping array for each word is simultaneously saved in a parallel binary key file (`*_key.bin`).
* **Decryption:** The decryption mode reads the encrypted text alongside the binary key file, utilizing the saved indices to perfectly reconstruct the original characters within each word while skipping over the untouched whitespace and punctuation.

## Build Instructions
To compile the project with maximum performance optimizations, run the following command in your terminal. Note that some systems might require the `-pthread` flag at compilation:

```bash
gcc encryptor.c -o encryptor -O3
```

## Usage

**1. Encrypt a file**
```bash
./encryptor my_file.txt
```

**2. Decrypt a file**
```bash
./encryptor my_file_criptat.txt my_file_key.bin
```