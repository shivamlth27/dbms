#include "bplustree.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static void printValue(const uint8_t data[VALUE_SIZE]) {
    // Print as a C-style string (stop at first zero) for convenience
    std::string s(reinterpret_cast<const char *>(data),
                  reinterpret_cast<const char *>(data) + VALUE_SIZE);
    // trim at first '\0'
    auto pos = s.find('\0');
    if (pos != std::string::npos) s.resize(pos);
    std::cout << s;
}

static void fillValueFromString(uint8_t data[VALUE_SIZE], const std::string &s) {
    std::memset(data, 0, VALUE_SIZE);
    std::size_t len = s.size();
    if (len > VALUE_SIZE) len = VALUE_SIZE;
    std::memcpy(data, s.data(), len);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <index_file>\n";
        return 1;
    }

    std::string filename = argv[1];
    BPlusTree tree(filename);

    std::cout << "B+ Tree driver. Index file: " << filename << "\n";
    std::cout << "Commands:\n";
    std::cout << "  insert <key> <string>\n";
    std::cout << "  delete <key>\n";
    std::cout << "  get <key>\n";
    std::cout << "  range <low> <high>\n";
    std::cout << "  quit\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) continue;

        if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "insert") {
            int key;
            std::string valueStr;
            if (!(iss >> key)) {
                std::cout << "Usage: insert <key> <string>\n";
                continue;
            }
            if (!std::getline(iss, valueStr)) {
                std::cout << "Usage: insert <key> <string>\n";
                continue;
            }
            // strip leading space
            if (!valueStr.empty() && valueStr[0] == ' ')
                valueStr.erase(0, 1);
            uint8_t data[VALUE_SIZE];
            fillValueFromString(data, valueStr);
            bool ok = tree.writeData(key, data);
            std::cout << (ok ? "OK\n" : "FAIL\n");
        } else if (cmd == "delete") {
            int key;
            if (!(iss >> key)) {
                std::cout << "Usage: delete <key>\n";
                continue;
            }
            bool ok = tree.deleteData(key);
            std::cout << (ok ? "OK\n" : "FAIL\n");
        } else if (cmd == "get") {
            int key;
            if (!(iss >> key)) {
                std::cout << "Usage: get <key>\n";
                continue;
            }
            uint8_t data[VALUE_SIZE];
            bool ok = tree.readData(key, data);
            if (!ok) {
                std::cout << "NOT_FOUND\n";
            } else {
                std::cout << "VALUE: ";
                printValue(data);
                std::cout << "\n";
            }
        } else if (cmd == "range") {
            int low, high;
            if (!(iss >> low >> high)) {
                std::cout << "Usage: range <low> <high>\n";
                continue;
            }
            int n = 0;
            auto vals = tree.readRangeData(low, high, n);
            std::cout << "FOUND " << n << " records\n";
            for (int i = 0; i < n; ++i) {
                std::cout << "  ";
                printValue(vals[i].data());
                std::cout << "\n";
            }
        } else {
            std::cout << "Unknown command\n";
        }
    }

    return 0;
}


