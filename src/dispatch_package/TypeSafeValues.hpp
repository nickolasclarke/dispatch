#pragma once

#include <limits>
#include <ostream>

struct NoTag{};
template<class Tag=NoTag>
class TSI {
 public:
    typedef TSI<Tag> value;

    TSI() : m_val(invalid_value) {}
    constexpr static TSI make(double val) { return TSI(val); }
    operator double() const { return m_val; }

    bool valid()   const { return !std::isnan(m_val); }
    bool invalid() const { return  std::isnan(m_val); }

    value operator+(const value &o) const { 
        assert(valid() && o.valid());
        value temp; temp.m_val=m_val+o.m_val; return temp; 
    }
    value operator-(const value &o) const { 
        assert(valid() && o.valid());
        value temp; temp.m_val=m_val-o.m_val; return temp; 
    }
    value& operator+=(const value &o){ 
        assert(valid && o.valid());
        m_val+=o.m_val; return *this; 
    }
    value& operator-=(const value &o){ 
        assert(valid && o.valid());
        m_val-=o.m_val; return *this; 
    }
    value& operator++(){ 
        assert(valid());
        m_val++; return *this; 
    }
    value& operator--(){ 
        assert(valid());
        m_val--; return *this; 
    }
    value& operator-(){
        assert(valid());
        m_val = -m_val;
        return *this;
    }

    double raw() const { return m_val; }

    friend std::ostream& operator<<(std::ostream& out, const value& self) {
        out<<self.m_val;
        return out;
    }

    bool operator==(const TSI &o){ return m_val==o.m_val; }
    bool operator!=(const TSI &o){ return m_val==o.m_val; }
 private:
    constexpr explicit TSI(double val) : m_val(val) {}
    double m_val;
    constexpr static double invalid_value = std::numeric_limits<double>::quiet_NaN();
};
