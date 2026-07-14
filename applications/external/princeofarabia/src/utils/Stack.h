#pragma once

#include <stdint.h>

template <typename Type, uint8_t Capacity>

class Stack {
public:
    static_assert(Capacity > 0, "Attempt to create a Stack with Capacity less than 1");
    static_assert(Capacity < 128, "Attempt to create a Stack with Capacity greater than 127");

    using ItemType = Type;
    using IndexType = int16_t;
    using IndexOfType = int8_t;

    constexpr static const IndexType FirstIndex = 0;
    constexpr static const IndexType LastIndex = Capacity - 1;

private:
    ItemType items[Capacity];
    IndexType next;
    uint8_t frame;

public:
    Stack(void);

    bool isEmpty(void) const;
    bool isFull(void) const;

    IndexType getCount(void) const;
    constexpr IndexType getCapacity(void) const;

    ItemType& peek(void);
    const ItemType& peek(void) const;
    bool insert(const ItemType& item);
    bool push(ItemType&& item);
    bool push(ItemType&& item1, ItemType&& item2);
    bool push(ItemType&& item1, ItemType&& item2, ItemType&& item3);
    ItemType& pop(void);

    void drop(void);
    void clear(void);
    bool contains(const ItemType& item) const;

    // Returns -1 if item not found
    IndexOfType indexOf(const ItemType& item) const;

    ItemType& operator[](const IndexType index);
    const ItemType& operator[](const IndexType index) const;
    void update();
    uint8_t getFrame() const;
    void setFrame(uint8_t val);
};

//
// Definition
//

template <typename Type, uint8_t Capacity>
Stack<Type, Capacity>::Stack(void)
    : items()
    , next(0) {
}

template <typename Type, uint8_t Capacity>
void Stack<Type, Capacity>::update() {
    if(this->frame != 0) {
        this->frame--;
    } else {
        this->frame = Constants::Animation_NumberOfFrames;
    }
}

template <typename Type, uint8_t Capacity>
uint8_t Stack<Type, Capacity>::getFrame() const {
    return this->frame;
}

template <typename Type, uint8_t Capacity>
void Stack<Type, Capacity>::setFrame(uint8_t val) {
    this->frame = val;
}

template <typename Type, uint8_t Capacity>
bool Stack<Type, Capacity>::isEmpty(void) const {
    return (this->next == FirstIndex);
}

template <typename Type, uint8_t Capacity>
bool Stack<Type, Capacity>::isFull(void) const {
    return (this->next == this->getCapacity());
}

template <typename Type, uint8_t Capacity>
typename Stack<Type, Capacity>::IndexType Stack<Type, Capacity>::getCount(void) const {
    return this->next;
}

template <typename Type, uint8_t Capacity>
constexpr typename Stack<Type, Capacity>::IndexType
    Stack<Type, Capacity>::getCapacity(void) const {
    return static_cast<IndexType>(Capacity);
}

template <typename Type, uint8_t Capacity>
typename Stack<Type, Capacity>::ItemType& Stack<Type, Capacity>::peek(void) {
    return this->items[this->next - 1];
}

template <typename Type, uint8_t Capacity>
const typename Stack<Type, Capacity>::ItemType& Stack<Type, Capacity>::peek(void) const {
    return this->items[this->next - 1];
}

template <typename Type, uint8_t Capacity>
typename Stack<Type, Capacity>::ItemType& Stack<Type, Capacity>::pop(void) {
    --this->next;
    return this->items[this->next];
}

template <typename Type, uint8_t Capacity>
bool Stack<Type, Capacity>::insert(const typename Stack<Type, Capacity>::ItemType& item) {
    if(this->isFull()) return false;

    for(uint8_t i = this->next; i > 0; i--) {
        this->items[i] = this->items[i - 1];
    }

    this->items[0] = item;
    ++this->next;

    return true;
}

template <typename Type, uint8_t Capacity>
bool Stack<Type, Capacity>::push(typename Stack<Type, Capacity>::ItemType&& item) {
    this->frame = Constants::Animation_NumberOfFrames;

    if(this->isFull()) return false;

    this->items[this->next] = item; // ought to be std::move
    ++this->next;
    return true;
}

template <typename Type, uint8_t Capacity>
bool Stack<Type, Capacity>::push(
    typename Stack<Type, Capacity>::ItemType&& item1,
    typename Stack<Type, Capacity>::ItemType&& item2) {
    this->frame = Constants::Animation_NumberOfFrames;
    if(this->isFull()) return false;

    this->items[this->next] = item1; // ought to be std::move
    ++this->next;

    if(this->isFull()) return false;
    this->items[this->next] = item2; // ought to be std::move
    ++this->next;

    return true;
}

template <typename Type, uint8_t Capacity>
bool Stack<Type, Capacity>::push(
    typename Stack<Type, Capacity>::ItemType&& item1,
    typename Stack<Type, Capacity>::ItemType&& item2,
    typename Stack<Type, Capacity>::ItemType&& item3) {
    this->frame = Constants::Animation_NumberOfFrames;
    if(this->isFull()) return false;
    this->items[this->next] = item1; // ought to be std::move
    ++this->next;

    if(this->isFull()) return false;
    this->items[this->next] = item2; // ought to be std::move
    ++this->next;

    if(this->isFull()) return false;
    this->items[this->next] = item3; // ought to be std::move
    ++this->next;
    return true;
}

template <typename Type, uint8_t Capacity>
void Stack<Type, Capacity>::drop(void) {
    --this->next;
    this->items[this->next].~ItemType();
}

template <typename Type, uint8_t Capacity>
void Stack<Type, Capacity>::clear(void) { // O(n)
    for(IndexType i = 0; i < this->next; ++i)
        (&this->items[i])->~ItemType();
    this->next = 0;
}

template <typename Type, uint8_t Capacity>
bool Stack<Type, Capacity>::contains(
    const typename Stack<Type, Capacity>::ItemType& item) const { // O(n)
    return this->indexOf(item) != -1;
}

template <typename Type, uint8_t Capacity>
typename Stack<Type, Capacity>::IndexOfType Stack<Type, Capacity>::indexOf(
    const typename Stack<Type, Capacity>::ItemType& item) const { // O(n)
    for(IndexType i = 0; i < this->next; ++i)
        if(this->items[i] == item) return i;
    return -1;
}

template <typename Type, uint8_t Capacity>
typename Stack<Type, Capacity>::ItemType&
    Stack<Type, Capacity>::operator[](const typename Stack<Type, Capacity>::IndexType index) {
    return this->items[index];
}

template <typename Type, uint8_t Capacity>
const typename Stack<Type, Capacity>::ItemType& Stack<Type, Capacity>::operator[](
    const typename Stack<Type, Capacity>::IndexType index) const {
    return this->items[index];
}
