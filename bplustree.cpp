// Disk-backed B+ tree implementation (simplified but functional)

#include "bplustree.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

namespace {

constexpr uint32_t MAGIC = 0x42505431u; // "BPT1"

bool fileExists(const std::string &path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

} // namespace

BPlusTree::BPlusTree(const std::string &filename)
    : m_fd(-1), m_filename(filename), m_ok(false) {
    m_ok = openFile(filename);
    if (!m_ok) return;

    if (!fileExists(filename) || lseek(m_fd, 0, SEEK_END) == 0) {
        // New file or empty file: initialize header and empty tree
        initEmptyTree();
    } else {
        m_ok = loadHeader();
    }
}

BPlusTree::~BPlusTree() {
    if (m_fd >= 0) {
        flushHeader();
        closeFile();
    }
}

bool BPlusTree::openFile(const std::string &filename) {
    m_fd = ::open(filename.c_str(), O_RDWR | O_CREAT, 0644);
    if (m_fd < 0) {
        perror("open");
        return false;
    }
    return true;
}

void BPlusTree::closeFile() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool BPlusTree::readPage(uint32_t pageId, void *page) {
    uint64_t off = pageOffset(pageId);
    if (lseek(m_fd, static_cast<off_t>(off), SEEK_SET) < 0) return false;
    ssize_t n = ::read(m_fd, page, PAGE_SIZE);
    if (n != static_cast<ssize_t>(PAGE_SIZE)) return false;
    return true;
}

bool BPlusTree::writePage(uint32_t pageId, const void *page) {
    uint64_t off = pageOffset(pageId);
    if (lseek(m_fd, static_cast<off_t>(off), SEEK_SET) < 0) return false;
    ssize_t n = ::write(m_fd, page, PAGE_SIZE);
    if (n != static_cast<ssize_t>(PAGE_SIZE)) return false;
    return true;
}

uint32_t BPlusTree::allocatePage() {
    // Simple allocator: append at end; ignore free list for now
    off_t end = lseek(m_fd, 0, SEEK_END);
    if (end < 0) return INVALID_PAGE;
    uint32_t pageId = static_cast<uint32_t>(end / PAGE_SIZE);
    // Ensure file extended
    std::array<uint8_t, PAGE_SIZE> zero{};
    if (!writePage(pageId, zero.data())) return INVALID_PAGE;
    return pageId;
}

void BPlusTree::initEmptyTree() {
    // Initialize header
    std::memset(&m_header, 0, sizeof(m_header));
    m_header.magic = MAGIC;
    m_header.pageSize = PAGE_SIZE;
    m_header.freeListHead = INVALID_PAGE;

    // Page 0 is header
    std::array<uint8_t, PAGE_SIZE> page0{};
    std::memcpy(page0.data(), &m_header, sizeof(m_header));
    writePage(0, page0.data());

    // Root is a single empty leaf node at page 1
    LeafNode leaf{};
    leaf.hdr.type = static_cast<uint8_t>(NodeType::LEAF);
    leaf.hdr.numKeys = 0;
    leaf.nextLeaf = INVALID_PAGE;

    m_header.rootPage = 1;
    std::array<uint8_t, PAGE_SIZE> buf{};
    std::memcpy(buf.data(), &leaf, sizeof(leaf));
    writePage(1, buf.data());

    flushHeader();
}

bool BPlusTree::loadHeader() {
    std::array<uint8_t, PAGE_SIZE> buf{};
    if (!readPage(0, buf.data())) return false;
    std::memcpy(&m_header, buf.data(), sizeof(m_header));
    if (m_header.magic != MAGIC || m_header.pageSize != PAGE_SIZE) {
        std::cerr << "Invalid index file header\n";
        return false;
    }
    return true;
}

bool BPlusTree::flushHeader() {
    std::array<uint8_t, PAGE_SIZE> buf{};
    std::memcpy(buf.data(), &m_header, sizeof(m_header));
    return writePage(0, buf.data());
}

bool BPlusTree::readInternal(uint32_t pageId, InternalNode &node) {
    std::array<uint8_t, PAGE_SIZE> buf{};
    if (!readPage(pageId, buf.data())) return false;
    std::memcpy(&node, buf.data(), sizeof(node));
    return true;
}

bool BPlusTree::readLeaf(uint32_t pageId, LeafNode &node) {
    std::array<uint8_t, PAGE_SIZE> buf{};
    if (!readPage(pageId, buf.data())) return false;
    std::memcpy(&node, buf.data(), sizeof(node));
    return true;
}

bool BPlusTree::writeInternal(uint32_t pageId, const InternalNode &node) {
    std::array<uint8_t, PAGE_SIZE> buf{};
    std::memcpy(buf.data(), &node, sizeof(node));
    return writePage(pageId, buf.data());
}

bool BPlusTree::writeLeaf(uint32_t pageId, const LeafNode &node) {
    std::array<uint8_t, PAGE_SIZE> buf{};
    std::memcpy(buf.data(), &node, sizeof(node));
    return writePage(pageId, buf.data());
}

uint32_t BPlusTree::findLeafPage(int32_t key, std::vector<uint32_t> *path) {
    uint32_t page = m_header.rootPage;
    if (path) path->clear();
    while (true) {
        if (path) path->push_back(page);
        std::array<uint8_t, PAGE_SIZE> buf{};
        if (!readPage(page, buf.data())) return INVALID_PAGE;

        NodeHeader nh{};
        std::memcpy(&nh, buf.data(), sizeof(nh));
        if (nh.type == static_cast<uint8_t>(NodeType::LEAF)) {
            return page;
        } else {
            InternalNode inode{};
            std::memcpy(&inode, buf.data(), sizeof(inode));
            uint32_t i = 0;
            while (i < inode.hdr.numKeys && key >= inode.keys[i]) {
                ++i;
            }
            page = inode.children[i];
        }
    }
}

bool BPlusTree::searchInLeaf(const LeafNode &leaf, int32_t key, uint32_t &index) const {
    uint32_t lo = 0, hi = leaf.hdr.numKeys;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (leaf.keys[mid] == key) {
            index = mid;
            return true;
        } else if (leaf.keys[mid] < key) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    index = lo;
    return false;
}

bool BPlusTree::writeData(int32_t key, const uint8_t data[VALUE_SIZE]) {
    if (!isOk()) return false;
    std::vector<uint32_t> path;
    uint32_t leafPage = findLeafPage(key, &path);
    if (leafPage == INVALID_PAGE) return false;

    int32_t promotedKey = 0;
    uint32_t newRightPage = INVALID_PAGE;
    if (!insertInLeaf(leafPage, key, data, promotedKey, newRightPage)) {
        return false;
    }
    if (newRightPage != INVALID_PAGE) {
        // need to insert into parent
        if (!insertInParent(path, leafPage, promotedKey, newRightPage)) return false;
    }
    return true;
}

bool BPlusTree::insertInLeaf(uint32_t leafPage, int32_t key, const uint8_t value[VALUE_SIZE],
                             int32_t &promotedKey, uint32_t &newRightPage) {
    LeafNode leaf{};
    if (!readLeaf(leafPage, leaf)) return false;

    uint32_t idx = 0;
    bool found = searchInLeaf(leaf, key, idx);
    if (found) {
        // overwrite existing
        std::memcpy(leaf.values[idx], value, VALUE_SIZE);
        return writeLeaf(leafPage, leaf);
    }

    // insert into leaf
    if (leaf.hdr.numKeys < LEAF_MAX_KEYS) {
        for (uint32_t i = leaf.hdr.numKeys; i > idx; --i) {
            leaf.keys[i] = leaf.keys[i - 1];
            std::memcpy(leaf.values[i], leaf.values[i - 1], VALUE_SIZE);
        }
        leaf.keys[idx] = key;
        std::memcpy(leaf.values[idx], value, VALUE_SIZE);
        ++leaf.hdr.numKeys;
        newRightPage = INVALID_PAGE;
        return writeLeaf(leafPage, leaf);
    }

    // Need to split
    LeafNode newLeaf{};
    newLeaf.hdr.type = static_cast<uint8_t>(NodeType::LEAF);
    newLeaf.hdr.numKeys = 0;
    newLeaf.nextLeaf = leaf.nextLeaf;

    // temp arrays
    int32_t tmpKeys[LEAF_MAX_KEYS + 1];
    uint8_t tmpValues[LEAF_MAX_KEYS + 1][VALUE_SIZE];

    for (uint32_t i = 0; i < leaf.hdr.numKeys; ++i) {
        tmpKeys[i] = leaf.keys[i];
        std::memcpy(tmpValues[i], leaf.values[i], VALUE_SIZE);
    }
    // insert new key/value in temp
    uint32_t total = leaf.hdr.numKeys;
    for (uint32_t i = total; i > idx; --i) {
        tmpKeys[i] = tmpKeys[i - 1];
        std::memcpy(tmpValues[i], tmpValues[i - 1], VALUE_SIZE);
    }
    tmpKeys[idx] = key;
    std::memcpy(tmpValues[idx], value, VALUE_SIZE);
    ++total;

    uint32_t split = total / 2;
    leaf.hdr.numKeys = split;
    newLeaf.hdr.numKeys = total - split;

    for (uint32_t i = 0; i < split; ++i) {
        leaf.keys[i] = tmpKeys[i];
        std::memcpy(leaf.values[i], tmpValues[i], VALUE_SIZE);
    }
    for (uint32_t i = split; i < total; ++i) {
        newLeaf.keys[i - split] = tmpKeys[i];
        std::memcpy(newLeaf.values[i - split], tmpValues[i], VALUE_SIZE);
    }

    // link leaves
    uint32_t newPage = allocatePage();
    if (newPage == INVALID_PAGE) return false;
    newLeaf.nextLeaf = leaf.nextLeaf;
    leaf.nextLeaf = newPage;

    promotedKey = newLeaf.keys[0];
    newRightPage = newPage;

    if (!writeLeaf(leafPage, leaf)) return false;
    if (!writeLeaf(newPage, newLeaf)) return false;
    return true;
}

bool BPlusTree::insertInParent(const std::vector<uint32_t> &path,
                               uint32_t leftPage,
                               int32_t key,
                               uint32_t rightPage) {
    // Case 1: tree was a single leaf and it just split
    if (path.size() == 1 && path[0] == m_header.rootPage) {
        InternalNode root{};
        root.hdr.type = static_cast<uint8_t>(NodeType::INTERNAL);
        root.hdr.numKeys = 1;
        root.keys[0] = key;
        root.children[0] = leftPage;
        root.children[1] = rightPage;

        uint32_t newRootPage = allocatePage();
        if (newRootPage == INVALID_PAGE) return false;
        if (!writeInternal(newRootPage, root)) return false;
        m_header.rootPage = newRootPage;
        return flushHeader();
    }

    // parent is the last internal node in path before the splitting child
    if (path.size() < 2) return false;
    uint32_t parentPage = path[path.size() - 2];
    InternalNode parent{};
    if (!readInternal(parentPage, parent)) return false;

    // find index of leftPage in parent's children
    uint32_t idxChild = 0;
    while (idxChild <= parent.hdr.numKeys && parent.children[idxChild] != leftPage) {
        ++idxChild;
    }
    if (idxChild > parent.hdr.numKeys) return false;

    // Case 2: parent has space, just insert key/rightPage
    if (parent.hdr.numKeys < INTERNAL_MAX_KEYS) {
        for (uint32_t i = parent.hdr.numKeys; i > idxChild; --i) {
            parent.keys[i] = parent.keys[i - 1];
        }
        for (uint32_t i = parent.hdr.numKeys + 1; i > idxChild + 1; --i) {
            parent.children[i] = parent.children[i - 1];
        }
        parent.keys[idxChild] = key;
        parent.children[idxChild + 1] = rightPage;
        ++parent.hdr.numKeys;
        return writeInternal(parentPage, parent);
    }

    // Case 3: parent is full – split internal node and propagate upwards recursively
    InternalNode newParent{};
    newParent.hdr.type = static_cast<uint8_t>(NodeType::INTERNAL);
    newParent.hdr.numKeys = 0;

    int32_t tmpKeys[INTERNAL_MAX_KEYS + 1];
    uint32_t tmpChildren[INTERNAL_MAX_KEYS + 2];

    for (uint32_t i = 0; i < parent.hdr.numKeys; ++i) {
        tmpKeys[i] = parent.keys[i];
    }
    for (uint32_t i = 0; i <= parent.hdr.numKeys; ++i) {
        tmpChildren[i] = parent.children[i];
    }

    // insert the new key/child into temporary arrays
    for (uint32_t i = parent.hdr.numKeys; i > idxChild; --i) {
        tmpKeys[i] = tmpKeys[i - 1];
    }
    for (uint32_t i = parent.hdr.numKeys + 1; i > idxChild + 1; --i) {
        tmpChildren[i] = tmpChildren[i - 1];
    }
    tmpKeys[idxChild] = key;
    tmpChildren[idxChild + 1] = rightPage;

    uint32_t total = parent.hdr.numKeys + 1; // total keys in temp
    uint32_t mid = total / 2;
    int32_t midKey = tmpKeys[mid];

    // left (existing parent) keeps first 'mid' keys
    parent.hdr.numKeys = mid;
    for (uint32_t i = 0; i < mid; ++i) {
        parent.keys[i] = tmpKeys[i];
        parent.children[i] = tmpChildren[i];
    }
    parent.children[mid] = tmpChildren[mid];

    // right (newParent) gets keys after midKey
    newParent.hdr.numKeys = total - mid - 1;
    for (uint32_t i = 0; i < newParent.hdr.numKeys; ++i) {
        newParent.keys[i] = tmpKeys[mid + 1 + i];
        newParent.children[i] = tmpChildren[mid + 1 + i];
    }
    newParent.children[newParent.hdr.numKeys] = tmpChildren[total];

    uint32_t newPage = allocatePage();
    if (newPage == INVALID_PAGE) return false;
    if (!writeInternal(parentPage, parent)) return false;
    if (!writeInternal(newPage, newParent)) return false;

    // If parent was root, create a new root
    if (parentPage == m_header.rootPage) {
        InternalNode newRoot{};
        newRoot.hdr.type = static_cast<uint8_t>(NodeType::INTERNAL);
        newRoot.hdr.numKeys = 1;
        newRoot.keys[0] = midKey;
        newRoot.children[0] = parentPage;
        newRoot.children[1] = newPage;

        uint32_t rootPage = allocatePage();
        if (rootPage == INVALID_PAGE) return false;
        if (!writeInternal(rootPage, newRoot)) return false;
        m_header.rootPage = rootPage;
        return flushHeader();
    }

    // Non-root internal split: recursively insert promoted key into grandparent
    auto it = std::find(path.begin(), path.end(), parentPage);
    if (it == path.end() || it == path.begin()) {
        // Should not happen if 'path' is a valid root-to-leaf path
        return false;
    }

    std::vector<uint32_t> newPath(path.begin(), std::next(it)); // up to and including parentPage
    return insertInParent(newPath, parentPage, midKey, newPage);
}

bool BPlusTree::readData(int32_t key, uint8_t outData[VALUE_SIZE]) {
    if (!isOk()) return false;
    uint32_t leafPage = findLeafPage(key, nullptr);
    if (leafPage == INVALID_PAGE) return false;
    LeafNode leaf{};
    if (!readLeaf(leafPage, leaf)) return false;
    uint32_t idx = 0;
    bool found = searchInLeaf(leaf, key, idx);
    if (!found) return false;
    std::memcpy(outData, leaf.values[idx], VALUE_SIZE);
    return true;
}

std::vector<std::array<uint8_t, VALUE_SIZE>>
BPlusTree::readRangeData(int32_t lowerKey, int32_t upperKey, int &n) {
    std::vector<std::array<uint8_t, VALUE_SIZE>> result;
    n = 0;
    if (!isOk()) return result;

    uint32_t leafPage = findLeafPage(lowerKey, nullptr);
    if (leafPage == INVALID_PAGE) return result;

    while (leafPage != INVALID_PAGE) {
        LeafNode leaf{};
        if (!readLeaf(leafPage, leaf)) break;
        for (uint32_t i = 0; i < leaf.hdr.numKeys; ++i) {
            int32_t k = leaf.keys[i];
            if (k < lowerKey) continue;
            if (k > upperKey) {
                n = static_cast<int>(result.size());
                return result;
            }
            std::array<uint8_t, VALUE_SIZE> v{};
            std::memcpy(v.data(), leaf.values[i], VALUE_SIZE);
            result.push_back(v);
        }
        leafPage = leaf.nextLeaf;
    }
    n = static_cast<int>(result.size());
    return result;
}

bool BPlusTree::deleteFromLeaf(uint32_t leafPage, int32_t key) {
    LeafNode leaf{};
    if (!readLeaf(leafPage, leaf)) return false;
    uint32_t idx = 0;
    bool found = searchInLeaf(leaf, key, idx);
    if (!found) return false;

    for (uint32_t i = idx + 1; i < leaf.hdr.numKeys; ++i) {
        leaf.keys[i - 1] = leaf.keys[i];
        std::memcpy(leaf.values[i - 1], leaf.values[i], VALUE_SIZE);
    }
    --leaf.hdr.numKeys;
    return writeLeaf(leafPage, leaf);
}

bool BPlusTree::deleteData(int32_t key) {
    if (!isOk()) return false;
    uint32_t leafPage = findLeafPage(key, nullptr);
    if (leafPage == INVALID_PAGE) return false;
    // Simplified: delete from leaf only, no rebalancing
    return deleteFromLeaf(leafPage, key);
}


