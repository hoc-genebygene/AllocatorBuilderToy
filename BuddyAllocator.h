#pragma once

#include <stdlib.h>

#include <cassert>
#include <memory>
#include <stack>
#include <utility>

#include "AlignedAllocator.h"

namespace AllocatorBuilder {
namespace BuddyAllocator {
template <class T>
constexpr T RoundUpPowerOf2(T n) {
    // (Modified) Hack to get the next power of 2:
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2

    n--;
    for (int shift = 1; shift < sizeof(T) * 8; shift *= 2) {
        n |= n >> shift;
    }

    n++;
    return n;
}

constexpr bool IsPowerOf2(uint64_t n) {
    // Hack to see if n is a power of 2
    // http://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
    return n && !(n & (n - 1));
}

namespace detail {
template<size_t MinSize, size_t MaxSize>
class BuddyTree {
public:
    BuddyTree() {
        void * mem;
        int res = posix_memalign(&mem, MaxSize, MaxSize);
        if (res != 0) {
            throw std::bad_alloc();
        }

        root_ = std::make_unique<BuddyTreeNode>(reinterpret_cast<char *>(mem), MaxSize);
    }

    char * allocate(size_t n) {
        size_t needed_size = std::max(RoundUpPowerOf2(n), MinSize);

        BuddyTreeNode * current_node = root_.get();
        std::stack<BuddyTreeNode *> unvisited_nodes;

        auto getLastUnvisitedNode = [&unvisited_nodes, &current_node]() -> BuddyTreeNode * {
            if (unvisited_nodes.empty()) {
                return nullptr;
            } else {
                // Go back to the last unvisited node, and rerun this loop
                BuddyTreeNode * last_unvisited_node = unvisited_nodes.top();
                unvisited_nodes.pop();
                return last_unvisited_node;
            }
        };

        while (current_node != nullptr) {
            assert(current_node->size() >= MinSize);
            if (current_node->size() == needed_size) {
                if (current_node->is_occupied()) {
                    current_node = getLastUnvisitedNode();
                } else {
                    return current_node->allocate();
                }
            } else {
                // current_node has size bigger than what we want
                if (current_node->is_allocated()) {
                    current_node = getLastUnvisitedNode();
                    // Next loop iteration
                } else if (current_node->is_occupied()) {
                    // At this point, left child and right child both exist
                    unvisited_nodes.push(current_node->right_child());
                    current_node = current_node->left_child();
                    // Next loop iteration
                } else {
                    // Split node
                    BuddyTreeNode * left_child;
                    BuddyTreeNode * right_child;
                    std::tie(left_child, right_child) = current_node->SplitNode();
                    unvisited_nodes.push(right_child);
                    current_node = left_child;
                    // Next loop iteration
                }
            }
        }

        // If we got here then we couldn't find a space big enough
        return nullptr;
    }

    void deallocate(char * mem) {
        // TODO
    }

private:
    class BuddyTreeNode {
    public:
        BuddyTreeNode(char * mem, size_t size) : mem_(mem), size_(size) {
            assert(IsPowerOf2(size));
        }

        char * allocate() {
            allocated_ = true;
            return mem_;
        }

        bool is_occupied() const {
            return !(left_child_ == nullptr && right_child_ == nullptr && !allocated_);
        }

        bool is_allocated() const {
            return allocated_;
        }

        size_t size() const { return size_; }

        BuddyTreeNode * left_child() {
            return left_child_.get();
        }

        BuddyTreeNode * right_child() {
            return right_child_.get();
        }

        std::pair<BuddyTreeNode *, BuddyTreeNode *> SplitNode() {
            assert(size_ >= 2);

            left_child_ = std::make_unique<BuddyTreeNode>(mem_, size_/2);
            right_child_ = std::make_unique<BuddyTreeNode>(mem_+size_/2, size_/2);

            return {left_child_.get(), right_child_.get()};
        }

    private:
        char * mem_;
        size_t size_;

        bool allocated_ = false;

        std::unique_ptr<BuddyTreeNode> left_child_ = nullptr;
        std::unique_ptr<BuddyTreeNode> right_child_ = nullptr;
    };

    std::unique_ptr<BuddyTreeNode> root_;
};
} // namespace detail

template <class T, size_t MinSize, size_t MaxSize>
class BuddyAllocator {
public:
    // std::allocator_traits
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    template< class U, size_t OtherMinSize, size_t OtherMaxSize >
    struct rebind {
        typedef BuddyAllocator<U, OtherMinSize, OtherMaxSize> other;
    };

    using is_always_equal = std::true_type;

    // custom allocator traits
    using thread_safe = std::false_type;

    static_assert(IsPowerOf2(MinSize), "MinSize must be power of 2");
    static_assert(IsPowerOf2(MaxSize), "MaxSize must be power of 2");
    static_assert(MinSize >= sizeof(T), "MinSize must be larger than sizeof(T)");

//    // TODO Copy and Move functions, for now delete them so we dont misuse
//    BuddyAllocator(const BuddyAllocator &) = delete;
//    BuddyAllocator & operator=(const BuddyAllocator &) = delete;
//
//    BuddyAllocator(BuddyAllocator &&) = delete;
//    BuddyAllocator & operator=(BuddyAllocator &&) = delete;


    pointer address(reference x) const noexcept {
        return std::addressof(x);
    }

    const_pointer address(const_reference x) const noexcept {
        return std::addressof(x);
    }

    T* allocate(std::size_t n, const void * hint) {
        // purposefully ignore hint
        return allocate(n);
    }

    T* allocate(std::size_t n) {
        return reinterpret_cast<T*>(buddy_tree_.allocate(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t n) {
        buddy_tree_.deallocate(p);
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

    detail::BuddyTree<MinSize, MaxSize> buddy_tree_;
};
} // namespace BuddyAllocator
} // namespace AllocatorBuilder
