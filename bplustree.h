// Simple disk-backed B+ tree interface
// Page size: 4096 bytes, integer keys, 100-byte values

#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <cstdint>
#include <string>
#include <vector>

static constexpr uint32_t PAGE_SIZE = 4096;
static constexpr uint32_t VALUE_SIZE = 100;

// Public API wrapper around the on-disk B+ tree
class BPlusTree {
public:
    explicit BPlusTree(const std::string &filename);
    ~BPlusTree();

    // disable copy
    BPlusTree(const BPlusTree &) = delete;
    BPlusTree &operator=(const BPlusTree &) = delete;

    // Writing API
    bool writeData(int32_t key, const uint8_t data[VALUE_SIZE]);
    bool deleteData(int32_t key);

    // Reading API
    // Returns true and fills outData if found, false otherwise.
    bool readData(int32_t key, uint8_t outData[VALUE_SIZE]);

    // Range read: returns vector of values for keys in [lowerKey, upperKey]
    // n is set to the number of results.
    std::vector<std::array<uint8_t, VALUE_SIZE>> readRangeData(int32_t lowerKey,
                                                               int32_t upperKey,
                                                               int &n);

private:
    int m_fd;
    std::string m_filename;
    bool m_ok;

    // --- On-disk structures ---

    struct FileHeader {
        uint32_t magic;        // magic number to identify file
        uint32_t pageSize;     // should be 4096
        uint32_t rootPage;     // page id of root node
        uint32_t freeListHead; // first free page id or 0xFFFFFFFF if none
    };

    enum class NodeType : uint8_t {
        INTERNAL = 0,
        LEAF = 1
    };

    struct NodeHeader {
        uint8_t type;       // NodeType
        uint32_t numKeys;   // number of valid keys
        uint32_t reserved;  // padding / future use
    };

    // Layout decisions:
    // - Internal node stores: header + [keys][children]
    // - Leaf node stores: header + nextLeaf + array of (key,value)

    static constexpr uint32_t INVALID_PAGE = 0xFFFFFFFFu;

    // capacity settings (computed from PAGE_SIZE)
    static constexpr uint32_t INTERNAL_MAX_KEYS = 128;
    static constexpr uint32_t LEAF_MAX_KEYS = 30;

    struct InternalNode {
        NodeHeader hdr;
        int32_t keys[INTERNAL_MAX_KEYS];
        uint32_t children[INTERNAL_MAX_KEYS + 1];
    };

    struct LeafNode {
        NodeHeader hdr;
        uint32_t nextLeaf; // page id of next leaf or INVALID_PAGE
        int32_t keys[LEAF_MAX_KEYS];
        uint8_t values[LEAF_MAX_KEYS][VALUE_SIZE];
    };

    FileHeader m_header;

    // low-level IO
    bool openFile(const std::string &filename);
    void closeFile();
    bool readPage(uint32_t pageId, void *page);
    bool writePage(uint32_t pageId, const void *page);
    uint32_t allocatePage();
    void initEmptyTree();
    bool loadHeader();
    bool flushHeader();

    // helpers
    bool isOk() const { return m_ok; }

    // tree navigation
    bool readInternal(uint32_t pageId, InternalNode &node);
    bool readLeaf(uint32_t pageId, LeafNode &node);
    bool writeInternal(uint32_t pageId, const InternalNode &node);
    bool writeLeaf(uint32_t pageId, const LeafNode &node);

    uint32_t findLeafPage(int32_t key, std::vector<uint32_t> *path = nullptr);

    // insertion helpers
    bool insertInLeaf(uint32_t leafPage, int32_t key, const uint8_t value[VALUE_SIZE],
                      int32_t &promotedKey, uint32_t &newRightPage);
    bool insertInParent(const std::vector<uint32_t> &path,
                        uint32_t leftPage,
                        int32_t key,
                        uint32_t rightPage);

    // deletion helpers
    bool deleteFromLeaf(uint32_t leafPage, int32_t key);

    // search helper
    bool searchInLeaf(const LeafNode &leaf, int32_t key, uint32_t &index) const;

    // utility
    static uint64_t pageOffset(uint32_t pageId) {
        return static_cast<uint64_t>(pageId) * PAGE_SIZE;
    }
};

#endif // BPLUSTREE_H


