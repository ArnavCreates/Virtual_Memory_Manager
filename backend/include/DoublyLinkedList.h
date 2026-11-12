#pragma once
#include <cstddef>

namespace vmm {

template <typename T>
struct ListNode {
    T data;
    ListNode* prev = nullptr;
    ListNode* next = nullptr;

    explicit ListNode(const T& value) : data(value), prev(nullptr), next(nullptr) {}
};

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
        newNode->next = position;
        newNode->prev = position->prev;
        position->prev->next = newNode;
        position->prev = newNode;
        ++size_;
        return newNode;
    }

    void erase(Node* node) {
        if (node == nullptr) return;

        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            head_ = node->next;
        }

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
