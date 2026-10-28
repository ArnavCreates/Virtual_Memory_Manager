#pragma once
#include <cstddef>

// Purpose: Hand-written Doubly Linked List template for tracking memory blocks in address order.

namespace vmm {

/**
 * Node in the DoublyLinkedList storing data and pointers to previous and next nodes.
 */
template <typename T>
struct ListNode {
    T data;
    ListNode* prev = nullptr;
    ListNode* next = nullptr;

    explicit ListNode(const T& value) : data(value), prev(nullptr), next(nullptr) {}
};

/**
 * Hand-written Doubly Linked List template class.
 * Stores memory blocks in address order.
 * Provides O(1) insertion, deletion, and bidirectional traversal given a node pointer.
 */
template <typename T>
class DoublyLinkedList {
public:
    using Node = ListNode<T>;

    DoublyLinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~DoublyLinkedList() {
        clear();
    }

    DoublyLinkedList(const DoublyLinkedList&) = delete;
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;

    Node* head() const { return head_; }
    Node* tail() const { return tail_; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    Node* pushBack(const T& value) {
        Node* newNode = new Node(value);
        if (tail_ == nullptr) {
            head_ = newNode;
            tail_ = newNode;
        } else {
            tail_->next = newNode;
            newNode->prev = tail_;
            tail_ = newNode;
        }
        ++size_;
        return newNode;
    }

    Node* insertAfter(Node* position, const T& value) {
        if (position == nullptr || position == tail_) {
            return pushBack(value);
        }
        Node* newNode = new Node(value);
        // [DS: DoublyLinkedList] Rewire pointers: insert newNode between position and position->next
        newNode->prev = position;
        newNode->next = position->next;
        position->next->prev = newNode;
        position->next = newNode;
        ++size_;
        return newNode;
    }

    Node* insertBefore(Node* position, const T& value) {
        if (position == nullptr || position == head_) {
            Node* newNode = new Node(value);
            if (head_ == nullptr) {
                head_ = newNode;
                tail_ = newNode;
            } else {
                newNode->next = head_;
                head_->prev = newNode;
                head_ = newNode;
            }
            ++size_;
            return newNode;
        }
        Node* newNode = new Node(value);
        // [DS: DoublyLinkedList] Rewire pointers: insert newNode between position->prev and position
        newNode->next = position;
        newNode->prev = position->prev;
        position->prev->next = newNode;
        position->prev = newNode;
        ++size_;
        return newNode;
    }

    void erase(Node* node) {
        if (node == nullptr) return;

        // [DS: DoublyLinkedList] Unlink node from previous neighbor or update head
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            head_ = node->next;
        }

        // [DS: DoublyLinkedList] Unlink node from next neighbor or update tail
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        } else {
            tail_ = node->prev;
        }

        delete node;
        --size_;
    }

    void clear() {
        Node* current = head_;
        while (current != nullptr) {
            Node* next = current->next;
            delete current;
            current = next;
        }
        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }

private:
    Node* head_;
    Node* tail_;
    std::size_t size_;
};

} // namespace vmm
