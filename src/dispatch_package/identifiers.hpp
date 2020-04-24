#pragma once

#include <type_safe/strong_typedef.hpp>

template<class Tag, class T>
struct Identifier : type_safe::strong_typedef<Identifier<Tag, T>, T>,
                    type_safe::strong_typedef_op::equality_comparison<Identifier<Tag, T>>,
                    type_safe::strong_typedef_op::output_operator<Identifier<Tag, T>>,
                    type_safe::strong_typedef_op::relational_comparison<Identifier<Tag,T>>
{
    using type_safe::strong_typedef<Identifier<Tag, T>, T>::strong_typedef;
    typedef T type;
    static constexpr Identifier<Tag, T> invalid() { return Identifier<Tag,T>(std::numeric_limits<T>::lowest()); }
    bool is_valid()   const { return (*this)!=invalid(); }
    bool is_invalid() const { return (*this)==invalid(); }
};

namespace std
{
  // we want to use it with the std::unordered_* containers
  template <class Tag, class T>
  struct hash<Identifier<Tag,T>> : type_safe::hashable<Identifier<Tag,T>>{};
}

struct StopIDTag{};
struct DepotIDTag{};
struct BlockIDTag{};

using depot_id_t = Identifier<DepotIDTag, int64_t>;
using block_id_t = Identifier<BlockIDTag, int64_t>;
using stop_id_t  = Identifier<StopIDTag, int64_t>;