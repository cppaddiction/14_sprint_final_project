#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept { buffer_ = std::move(other.buffer_); capacity_ = std::move(other.capacity_); other.buffer_ = nullptr; other.capacity_ = 0; }
    RawMemory& operator=(RawMemory&& rhs) noexcept { buffer_ = std::move(rhs.buffer_); capacity_ = std::move(rhs.capacity_); rhs.buffer_ = nullptr; rhs.capacity_ = 0; return *this; }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other, size_t size, const T& elem)
        : data_(size)
        , size_(other.size_ + 1)
    {
        new (data_ + other.size_) T(elem);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        }
        else {
            std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        }
    }

    Vector(Vector&& other, size_t size, T&& elem)
        : data_(size)
        , size_(other.size_ + 1)
    {
        new (data_ + other.size_) T(std::move(elem));
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        }
        else {
            std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        }
    }

    template<typename... Args>
    Vector(Vector&& other, size_t size, Args&&... args)
        : data_(size)
        , size_(other.size_ + 1)
    {
        new (data_ + other.size_) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        }
        else {
            std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        }
    }

    template<typename... Args>
    Vector(Vector&& other, size_t size, int64_t dist, Args&&... args)
        : data_(size)
        , size_(other.size_ + 1)
    {
        if (other.insrt_cpy_called)
        {
            if constexpr (std::is_copy_constructible_v<T>)
            {
                new (data_ + dist) T(std::forward<Args>(args)...);
            }
            else
            {
                throw;
            }
        }
        else if (other.insrt_mve_called)
        {
            new (data_ + dist) T(std::move(args)...);
        }
        else
        {
            if constexpr (std::is_copy_constructible_v<T>)
            {
                new (data_ + dist) T(std::forward<Args>(args)...);
            }
            else
            {
                new (data_ + dist) T(std::move(args)...);
            }
        }
        
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(other.data_.GetAddress(), dist, data_.GetAddress());
            }
            else {
                std::uninitialized_copy_n(other.data_.GetAddress(), dist, data_.GetAddress());
            }
        }
        catch (...)
        {
            std::destroy_n(data_.GetAddress() + dist, 1);
        }
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(other.data_.GetAddress() + dist, other.size_ - dist, data_.GetAddress() + dist + 1);
            }
            else {
                std::uninitialized_copy_n(other.data_.GetAddress() + dist, other.size_ - dist, data_.GetAddress() + dist + 1);
            }
        }
        catch (...)
        {
            std::destroy_n(data_.GetAddress(), dist + 1);
        }
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::move(other.size_))
    {

    }

    Vector& operator=(const Vector& rhs)
    {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (rhs.size_ < size_)
                {
                    for (size_t i = 0; i < rhs.size_; i++)
                    {
                        data_[i] = rhs.data_[i];
                    }
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else
                {
                    for (size_t i = 0; i < size_; i++)
                    {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept
    {
        data_ = std::move(rhs.data_);
        size_ = std::move(rhs.size_);
        return *this;
    }

    void Swap(Vector& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Resize(size_t new_size)
    {
        if (new_size < size_)
        {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        if (new_size > size_)
        {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value)
    {
        if (data_.Capacity() == size_)
        {
            Vector<T> new_vec(std::move(*this), size_ == 0 ? 1 : 2 * size_, value);
            Swap(new_vec);
        }
        else
        {
            new (data_ + size_) T(value);
            ++size_;
        }
    }

    void PushBack(T&& value)
    {
        if (data_.Capacity() == size_)
        {
            Vector<T> new_vec(std::move(*this), size_ == 0 ? 1 : 2 * size_, std::move(value));
            Swap(new_vec);
        }
        else
        {
            new (data_ + size_) T(std::move(value));
            ++size_;
        }
    }

    void PopBack() noexcept
    {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        size_--;
    }

    ~Vector() {
        if (data_.GetAddress() != nullptr && size_ > 0)
        {
            std::destroy_n(data_.GetAddress(), size_);
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {

        if (data_.Capacity() == size_)
        {
            Vector<T> new_vec(std::move(*this), size_ == 0 ? 1 : 2 * size_, std::forward<Args>(args)...);
            Swap(new_vec);
        }
        else
        {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
        }

        return data_[size_ - 1];
    }

    iterator begin() noexcept { return data_.GetAddress(); }
    iterator end() noexcept { return data_.GetAddress() + size_; }
    const_iterator begin() const noexcept { return data_.GetAddress(); }
    const_iterator end() const noexcept { return data_.GetAddress() + size_; }
    const_iterator cbegin() const noexcept { return data_.GetAddress(); }
    const_iterator cend() const noexcept { return data_.GetAddress() + size_; }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args)
    {
        auto dist = std::distance(cbegin(), pos);
        if (data_.Capacity() == size_)
        {
            Vector<T> new_vec(std::move(*this), size_ == 0 ? 1 : 2 * size_, dist, std::forward<Args>(args)...);
            Swap(new_vec);
            iterator fin = begin();
            std::advance(fin, dist);
            return fin;
        }
        else
        {
            if (pos != cend())
            {
                if (insrt_cpy_called)
                {
                    if constexpr (std::is_copy_constructible_v<T>)
                    {
                        T&& temp = T(std::forward<Args>(args)...);
                        new (data_ + size_) T(std::move(*end()));
                        iterator from = begin();
                        std::advance(from, dist);
                        std::move_backward(from, end() - 1, end());
                        iterator fin = begin();
                        std::advance(fin, dist);
                        *fin = std::move(temp);
                        size_ += 1;
                        return fin;
                    }
                    else
                    {
                        throw;
                    }
                }
                else if (insrt_mve_called)
                {
                    T&& temp = T(std::move(args)...);
                    new (data_ + size_) T(std::move(*end()));
                    iterator from = begin();
                    std::advance(from, dist);
                    std::move_backward(from, end() - 1, end());
                    iterator fin = begin();
                    std::advance(fin, dist);
                    *fin = std::move(temp);
                    size_ += 1;
                    return fin;
                }
                else
                {
                    if constexpr (std::is_copy_constructible_v<T>)
                    {
                        T&& temp = T(std::forward<Args>(args)...);
                        new (data_ + size_) T(std::move(*end()));
                        iterator from = begin();
                        std::advance(from, dist);
                        std::move_backward(from, end() - 1, end());
                        iterator fin = begin();
                        std::advance(fin, dist);
                        *fin = std::move(temp);
                        size_ += 1;
                        return fin;
                    }
                    else
                    {
                        T&& temp = T(std::move(args)...);
                        new (data_ + size_) T(std::move(*end()));
                        iterator from = begin();
                        std::advance(from, dist);
                        std::move_backward(from, end() - 1, end());
                        iterator fin = begin();
                        std::advance(fin, dist);
                        *fin = std::move(temp);
                        size_ += 1;
                        return fin;
                    }
                }
            }
            else
            {
                new (data_ + size_) T(std::move(args)...);
                iterator fin = begin();
                std::advance(fin, dist);
                size_ += 1;
                return fin;
            }
        }
    }

    iterator Insert(const_iterator pos, const T& value)
    {
        insrt_cpy_called = true;
        return Emplace(pos, value);
        insrt_cpy_called = false;
    }

    iterator Insert(const_iterator pos, T&& value)
    {
        insrt_mve_called = true;
        return Emplace(pos, value);
        insrt_mve_called = false;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        iterator from = begin();
        auto dist = std::distance(cbegin(), pos) + 1;
        std::advance(from, dist);
        iterator to = std::next(from, -1);
        std::move(from, end(), to);
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        size_ -= 1;
        iterator fin = begin();
        std::advance(fin, dist - 1);
        return fin;
    }

private:
    bool insrt_mve_called = false;
    bool insrt_cpy_called = false;

    RawMemory<T> data_;
    size_t size_ = 0;

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};