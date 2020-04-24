#pragma once

#include <cassert>
#include <cmath>
#include <limits>
#include <ostream>
#include <utility>

template<class Tag> class TSV;

namespace std {
  template<class Tag>
  struct hash<TSV<Tag>> {
  public:
    size_t operator()(const TSV<Tag> &c) const; // don't define yet
  };
}

template<class Tag>
class TSV {
 public:
    typedef TSV<Tag> value;
    typedef double type;

    TSV() : m_val(invalid_value) {}
    constexpr static value make(type val) { return TSV(val); }
    constexpr static value invalid() { return TSV(); }
    operator type() const { return m_val; }

    bool is_valid()   const { return !std::isnan(m_val); }
    bool is_invalid() const { return  std::isnan(m_val); }

    void invalidate() { m_val = invalid_value; }

    value operator+(const value &o) const {
        assert(is_valid() && o.is_valid());
        return value(m_val+o.m_val);
    }
    value operator-(const value &o) const {
        assert(is_valid() && o.is_valid());
        return value(m_val-o.m_val);
    }
    value& operator+=(const value &o){
        assert(is_valid && o.is_valid());
        m_val+=o.m_val;
        return *this;
    }
    value& operator-=(const value &o){
        assert(is_valid && o.is_valid());
        m_val-=o.m_val;
        return *this;
    }
    value& operator++(){
        assert(is_valid());
        m_val++;
        return *this;
    }
    value& operator--(){
        assert(is_valid());
        m_val--;
        return *this;
    }
    value& operator-(){
        assert(is_valid());
        m_val = -m_val;
        return *this;
    }

    type raw() const { return m_val; }

    friend std::ostream& operator<<(std::ostream& out, const value& self) {
        out<<self.m_val;
        return out;
    }

    std::string to_string() {
        return std::to_string(m_val);
    }

    friend size_t std::hash<value>::operator ()(const value&) const;

    bool operator==(const TSV &o){
        assert(is_valid() && o.is_valid());
        return m_val==o.m_val;
    }
    bool operator!=(const TSV &o){
        assert(is_valid() && o.is_valid());
        return m_val!=o.m_val;
    }
 private:
    constexpr explicit TSV(type val) : m_val(val) {}
    type m_val;
    constexpr static type invalid_value = std::numeric_limits<type>::quiet_NaN();
};

namespace std {
  template<class Tag>
  size_t hash<TSV<Tag>>::operator()(const TSV<Tag> &c) const {
    return std::hash<typename TSV<Tag>::type>()(c.raw());
  }
}