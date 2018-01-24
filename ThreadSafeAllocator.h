#pragma once

#include <mutex>

namespace AllocatorBuilder {
namespace ThreadSafeAllocator {
// Turns any allocator into a thread-safe allocator by serializing all accesses
template <class BaseAllocator>
class ThreadSafeAllocator {
public:
    // std::allocator_traits
    using value_type = typename BaseAllocator::value_type;
    using pointer = typename BaseAllocator::pointer;
    using const_pointer = typename BaseAllocator::const_pointer;
    using reference = typename BaseAllocator::reference;
    using const_reference = typename BaseAllocator::const_reference;
    using size_type = typename BaseAllocator::size_type;
    using difference_type = typename BaseAllocator::difference_type;
    using propagate_on_container_move_assignment = typename BaseAllocator::propagate_on_container_move_assignment;

//    template<class U>
//    using rebind = typename BaseAllocator::template rebind<U>;

    using is_always_equal = typename BaseAllocator::is_always_equal;

    // custom allocator traits
    using thread_safe = std::true_type;

    template <class... Args>
    ThreadSafeAllocator(Args&&... args) : allocator_(std::forward<Args>(args)...) {}

    // TODO copy and move need to be thread-safe

    pointer address(reference x) const noexcept {
        return std::addressof(x);
    }

    const_pointer address(const_reference x) const noexcept {
        return std::addressof(x);
    }

    pointer allocate(std::size_t n, const void * hint) {
        // purposefully ignore hint
        return allocate(n);
    }

    pointer allocate(std::size_t n) {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocator_.allocate(n);
    }

    void deallocate(pointer p, std::size_t n) {
        std::lock_guard<std::mutex> lock(mutex_);
        allocator_.free(p);
    }

    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(value_type);
    }

    template <class U, class... Args>
    void construct(U * p, Args&&... args) {
        ::new((void *)p) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U * p) {
        p->~U();
    }

private:
    std::mutex mutex_;
    BaseAllocator allocator_;
};
} // namespace ThreadSafeAllocator
} // namespace AllocatorBuilder

