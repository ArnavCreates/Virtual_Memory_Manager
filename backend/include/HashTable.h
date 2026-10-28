#pragma once
#include <cstddef>
#include <string>
#include <vector>

// Purpose: Hand-written Hash Table with separate chaining (17 buckets, djb2 hash) for O(1) process lookup.

namespace vmm {

/**
 * Hand-written Hash Table template class using separate chaining.
 * Fixed 17 buckets (a prime number) with djb2 hashing algorithm.
 * Provides O(1) average time lookup, insertion, and deletion of process blocks by process ID.
 * Separate chaining ensures collision resolution and correctness without resizing under any load.
 */
template <typename V>
class HashTable {
public:
    struct Entry {
        std::string key;
        V value;
        Entry* next = nullptr;

        Entry(const std::string& k, const V& v, Entry* n = nullptr)
            : key(k), value(v), next(n) {}
    };

    explicit HashTable(std::size_t bucketCount = 17)
        : bucketCount_(bucketCount), size_(0) {
        buckets_ = new Entry*[bucketCount_];
        for (std::size_t i = 0; i < bucketCount_; ++i) {
            buckets_[i] = nullptr;
        }
    }

    ~HashTable() {
        clear();
        delete[] buckets_;
    }

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    std::size_t bucketIndex(const std::string& key) const {
        // djb2 hash algorithm
        unsigned long hash = 5381;
        for (char c : key) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(c); // hash * 33 + c
        }
        return hash % bucketCount_;
    }

    bool insert(const std::string& key, const V& value) {
        std::size_t idx = bucketIndex(key);
        Entry* current = buckets_[idx];
        Entry* prev = nullptr;

        while (current != nullptr) {
            if (current->key == key) {
                return false; // Key already exists
            }
            prev = current;
            current = current->next;
        }

        Entry* newEntry = new Entry(key, value, nullptr);
        if (prev == nullptr) {
            buckets_[idx] = newEntry;
        } else {
            prev->next = newEntry;
        }
        ++size_;
        return true;
    }

    V* find(const std::string& key) {
        std::size_t idx = bucketIndex(key);
        Entry* current = buckets_[idx];
        while (current != nullptr) {
            if (current->key == key) {
                return &(current->value);
            }
            current = current->next;
        }
        return nullptr;
    }

    const V* find(const std::string& key) const {
        std::size_t idx = bucketIndex(key);
        Entry* current = buckets_[idx];
        while (current != nullptr) {
            if (current->key == key) {
                return &(current->value);
            }
            current = current->next;
        }
        return nullptr;
    }

    bool erase(const std::string& key) {
        std::size_t idx = bucketIndex(key);
        Entry* current = buckets_[idx];
        Entry* prev = nullptr;

        while (current != nullptr) {
            if (current->key == key) {
                if (prev == nullptr) {
                    buckets_[idx] = current->next;
                } else {
                    prev->next = current->next;
                }
                delete current;
                --size_;
                return true;
            }
            prev = current;
            current = current->next;
        }
        return false;
    }

    bool contains(const std::string& key) const {
        return find(key) != nullptr;
    }

    std::size_t size() const {
        return size_;
    }

    std::size_t bucketCount() const {
        return bucketCount_;
    }

    std::vector<std::string> keysInBucket(std::size_t index) const {
        std::vector<std::string> keys;
        if (index >= bucketCount_) {
            return keys;
        }
        Entry* current = buckets_[index];
        while (current != nullptr) {
            keys.push_back(current->key);
            current = current->next;
        }
        return keys;
    }

    void clear() {
        for (std::size_t i = 0; i < bucketCount_; ++i) {
            Entry* current = buckets_[i];
            while (current != nullptr) {
                Entry* next = current->next;
                delete current;
                current = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }

private:
    std::size_t bucketCount_;
    std::size_t size_;
    Entry** buckets_;
};

} // namespace vmm
