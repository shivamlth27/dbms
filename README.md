## B+ Tree Index (C++ Implementation)

This project implements a disk-backed B+ tree index using C++.

### Features

- Integer keys
- Fixed-size value/tuple of 100 bytes
- Page size: 4096 bytes
- File-backed index that persists across runs
- APIs:
  - `writeData(key, data)` – insert key and 100-byte tuple
  - `deleteData(key)` – delete key
  - `readData(key)` – lookup single key
  - `readRangeData(lowerKey, upperKey, n)` – range query

### Requirements

- **Standard Ubuntu (or macOS)** with:
  - `g++` supporting C++17
  - `make`

No external libraries are required, so **no `requirements.txt` is needed** for this C++ implementation.

### Build (Linux-style instructions)

Open a terminal and run:

```bash
cd /path/to/assgDBMS
make
```

This produces an executable named `bpt_driver` in the project directory.

### Run

From the same directory:

```bash
./bpt_driver index.dat
```

If `index.dat` does not exist, it will be created. If it exists, the existing index is reused.

You will get a simple interactive driver with the following commands:

- `insert <key> <string>` – inserts key and string (string is truncated/padded to 100 bytes)
- `delete <key>` – deletes key
- `get <key>` – reads a single key
- `range <low> <high>` – reads all keys in `[low, high]`
- `quit` – exit the program

Example session:

```bash
./bpt_driver index.dat
insert 10 hello
insert 20 world
get 10
range 5 25
delete 10
get 10
quit
```

### API Documentation (C-style / Linux-style)

- **`bool writeData(int32_t key, const uint8_t data[100])`**
  - **Description**: Inserts or updates the tuple associated with `key` in the B+ tree index stored on disk.
  - **Return**: `true` (1) on success, `false` (0) on failure.

- **`bool deleteData(int32_t key)`**
  - **Description**: Deletes the tuple associated with `key` from the index, if it exists.
  - **Return**: `true` (1) if the key was found and deleted, `false` (0) otherwise.

- **`bool readData(int32_t key, uint8_t outData[100])`**
  - **Description**: Searches for `key` in the index. If found, the corresponding 100-byte tuple is written into `outData`.
  - **Return**: `true` (1) if the key exists, `false` (0) if the key is not present.

- **`std::vector<std::array<uint8_t, 100>> readRangeData(int32_t lowerKey, int32_t upperKey, int &n)`**
  - **Description**: Searches for all keys in the range `[lowerKey, upperKey]` (inclusive) and returns the corresponding tuples as a vector.
  - **Output parameter**: `n` is set to the number of tuples in the returned vector.
  - **Return**: An empty vector if no key in the range exists in the index.

### Notes

- The index file (`index.dat`) is created if it does not exist.
- The tree is stored fully on disk; only individual pages are loaded into memory as needed.


