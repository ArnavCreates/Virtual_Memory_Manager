#pragma once
#include <cstddef>

// Purpose: Hand-written FIFO Queue template for tracking waiting memory allocation requests.

namespace vmm {

/**
 * Hand-written FIFO Queue template class.
 * Built from its own singly linked nodes with head and tail pointers.
 * Guarantees O(1) enqueue at tail and O(1) dequeue at head.
 */
template <typename T>
class Queue {
private:
    struct QueueNode {
        T data;
        QueueNode* next = nullptr;
        explicit QueueNode(const T& val) : data(val), next(nullptr) {}
    };

    QueueNode* head_ = nullptr;
    QueueNode* tail_ = nullptr;
    std::size_t size_ = 0;

public:
    Queue() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~Queue() {
        clear();
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void enqueue(const T& value) {
        QueueNode* newNode = new QueueNode(value);
        if (tail_ == nullptr) {
            head_ = newNode;
            tail_ = newNode;
        } else {
            tail_->next = newNode;
            tail_ = newNode;
        }
        ++size_;
    }

    bool dequeue(T& out) {
        if (head_ == nullptr) {
            return false;
        }
        QueueNode* nodeToDelete = head_;
        out = nodeToDelete->data;
        head_ = head_->next;
        if (head_ == nullptr) {
            tail_ = nullptr;
        }
        delete nodeToDelete;
        --size_;
        return true;
    }

    bool front(T& out) const {
        if (head_ == nullptr) {
            return false;
        }
        out = head_->data;
        return true;
    }

    bool empty() const {
        return size_ == 0;
    }

    std::size_t size() const {
        return size_;
    }

    template <typename Fn>
    void forEach(Fn fn) const {
        QueueNode* current = head_;
        while (current != nullptr) {
            fn(current->data);
            current = current->next;
        }
    }

    void clear() {
        QueueNode* current = head_;
        while (current != nullptr) {
            QueueNode* next = current->next;
            delete current;
            current = next;
        }
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }
};

} // namespace vmm
