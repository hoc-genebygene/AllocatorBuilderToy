#include <cassert>
#include <iostream>
#include <utility>
#include <vector>

struct block {
    void * ptr;
    size_t length;
    
    block() noexcept
    : ptr(nullptr), length(0) {
    }
    
    constexpr block(void * ptr, size_t length) noexcept
    : ptr(ptr), length(length) {
    }
    
    block(const block & other) noexcept = default;
    block & operator=(const block & other) noexcept = default;
    
    block(block && other) {
        ptr = other.ptr;
        length = other.length;
    }
    
    block & operator=(block && other) noexcept {
        ptr = other.ptr;
        length = other.length;
        
        return *this;
    }
    
    ~block() {}
    
    void reset() noexcept {
        ptr = nullptr;
        length = 0;
    }
    
    explicit operator bool() const {
        return length != 0;
    }
    
    bool operator==(const block & other) {
        return ptr == other.ptr && length == other.length;
    }
};

class null_allocator {
public:
    block allocate(size_t) noexcept {
        return {nullptr, 0};
    }
    
    bool owns(const block & b) noexcept {
        return !b;
    }
    
    bool expand(block & b, size_t) noexcept {
        assert(!b);
        return false;
    }
    
    bool reallocate(block & b, size_t) noexcept {
        assert(!b);
        return false;
    }
    
    void deallocate(block & b) noexcept {
        assert(!b);
    }
    
    void deallocateAll() noexcept {}
};

class mallocator {
public:
    static constexpr const size_t alignment = 4;
    
    block allocate(size_t length) noexcept {
        block blk;
        if (length == 0) {
            return blk;
        }
        
        auto ptr = ::malloc(length);
        if (ptr != nullptr) {
            blk.ptr = ptr;
            blk.length = length;
        }
        
        return blk;
    }
    
    void deallocate(block & b) noexcept {
        if (b) {
            ::free(b.ptr);
            b.reset();
        }
    }
};

template <size_t Threshold, class SmallAllocator, class LargeAllocator>
class Segreagator {
public:
    block allocate(size_t n) noexcept {
        if (n <= Threshold) {
            return SmallAllocator::allocate(n);
        } else {
            return LargeAllocator::allocate(n);
        }
    }
    
    void deallocate(block & b) noexcept {
        if (!b) {
            return;
        }
        
        if (b.length <= Threshold) {
            return SmallAllocator::deallocate(b);
        } else {
            return LargeAllocator::deallocate(b);
        }
    }
    
    bool reallocate(block & b, size_t n) {
        if (b.length <= Threshold) {
            if (n <= Threshold) {
                return SmallAllocator::reallocate(b, n);
            } else {
                // Allocate larger memory
                auto larger_block = LargeAllocator::allocate(n);
                if (!larger_block) {
                    return false;
                }
                // Copy from original memory into larger memory
                memcpy(larger_block.ptr, b.ptr, b.length);

                // Deallocate original memory
                SmallAllocator::deallocate(b);

                b = std::move(larger_block);
            }
        } else {
            if (n <= Threshold) {
                // Allocate smaller memory
                auto smaller_block = SmallAllocator::allocate(n);
                if (!smaller_block) {
                    return false;
                }

                // Copy from original memory to smaller memory, truncating at end of smaller memory
                memcpy(smaller_block.ptr, b.ptr, n);

                // Deallocate original memory
                LargeAllocator::deallocate(b, n);

                b = std::move(smaller_block);
            } else {
                return LargeAllocator::reallocate(b, n);
            }
        }

        return true;
    }
    
    bool owns(const block & b) {
        if (b.length <= Threshold) {
            return SmallAllocator::owns(b);
        } else {
            return LargeAllocator::owns(b);
        }
    }
    
    void deallocate_all() {
        SmallAllocator::deallocate_all();
        LargeAllocator::deallocate_all();
    }
};

template <class Allocator, size_t MinSize, size_t MaxSize, size_t StepSize>
class Bucketizer {

};

template <class T, class Allocator>
class std_allocator_adapter;

template <class Allocator>
class std_allocator_adapter<void, Allocator> {
public:
    using pointer = void *;
    using const_pointer = const void *;
    using value_type = void;
    template <class U> struct rebind {
        typedef std_allocator_adapter<U, Allocator> other;
    };
};

template <class T, class Allocator>
class std_allocator_adapter {
    const Allocator & allocator_;
public:
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using value_type = T;
    
    template <class U> struct rebind {
        typedef std_allocator_adapter<U, Allocator> other;
    };
    
    explicit std_allocator_adapter(const Allocator & allocator) noexcept
    : allocator_(allocator) {
    }
    
    ~std_allocator_adapter() noexcept = default;
    
    const Allocator & allocator() const {
        return allocator_;
    }
    
    template <class U>
    explicit std_allocator_adapter(const std_allocator_adapter<U, Allocator> & other)
    : allocator_(other.allocator()) {
    }
    
    pointer address(reference r) const {
        return &r;
    }
    
    const_pointer address(const_reference r) const {
        return &r;
    }
    
    T* allocate(std::size_t num_elems, const void * = nullptr) {
        auto b = const_cast<Allocator &>(allocator_).allocate(num_elems * sizeof(T));
        if (b) {
            return static_cast<T*>(b.ptr);
        }
        return nullptr;
    }
    
    void deallocate(T * ptr, std::size_t n) {
        block blk(ptr, n);
        const_cast<Allocator &>(allocator_).deallocate(blk);
    }
    
    size_t max_size() const {
        return ((size_t)(-1)) / sizeof(T);
    }
    
    template <class... U>
    void construct(pointer ptr, U&&... val) {
        ::new(static_cast<void *>(ptr)) T(std::forward<U>(val)...);
    }
    
    void destroy(pointer p) {
        p->T::~T();
    }
};

int main() {
    using Allocator = std_allocator_adapter<int, mallocator>;
    Allocator allocator{mallocator{}};
    std::vector<int, Allocator> vec(allocator);

    vec.push_back(1);
    
    std::cout << vec.front() << std::endl;
    
    return 0;
}
