#pragma once

template<class T>
struct RangeTracking {
  T val = T();
  T val_min = std::numeric_limits<T>::max();
  T val_max = std::numeric_limits<T>::lowest();
  RangeTracking() : val(0), val_min(0), val_max(0) {}
  RangeTracking(T val) : val(val) {
    val_min = val;
    val_max = val;
  }
  RangeTracking& operator++(){
    val++;
    val_max = std::max(val_max,val);
    return *this;
  }
  RangeTracking& operator--(){
    val--;
    val_min = std::min(val_min,val);
    return *this;
  } 
  RangeTracking& operator+=(const T& inc){
    val+=inc;
    val_max = std::max(val_max,val);
    return *this;
  }
  RangeTracking& operator-=(const T& inc){
    val-=inc;
    val_min = std::min(val_min,val);
    return *this;
  }
  T max() const { return val_max; }
  T min() const { return val_min; }
};
