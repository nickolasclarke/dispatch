#pragma once

#include <limits>
#include <ostream>
#include <utility>

template<class Tag> class TSI;

namespace std {
  template<class Tag>
  struct hash<TSI<Tag>> {
  public:
    size_t operator()(const TSI<Tag> &c) const; // don't define yet
  };
}

template<class Tag>
class TSI {
 public:
    typedef TSI<Tag> value;
    typedef int64_t type;

    TSI() : m_val(invalid_value) {}
    constexpr static value make(type val) { return TSI(val); }
    constexpr static value invalid() { return TSI(); }
    operator type() const { return m_val; }

    bool is_valid()   const { return !std::isnan(m_val); }
    bool is_invalid() const { return  std::isnan(m_val); }
    void invalidate() { m_val = invalid_value; }

    type raw() const { return m_val; }

    friend std::ostream& operator<<(std::ostream& out, const value& self) {
        out<<self.m_val;
        return out;
    }

    friend size_t std::hash<value>::operator ()(const value&) const;

    bool operator==(const TSI &o){ return m_val==o.m_val; }
    bool operator!=(const TSI &o){ return m_val==o.m_val; }
 private:
    constexpr explicit TSI(type val) : m_val(val) {}
    type m_val;
    constexpr static type invalid_value = std::numeric_limits<int32_t>::lowest();
};

namespace std {
  template<class Tag>
  size_t hash<TSI<Tag>>::operator()(const TSI<Tag> &c) const {
    return std::hash<typename TSI<Tag>::type>()(c.raw());
  }
}