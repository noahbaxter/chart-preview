#pragma once

#include <cstdint>

class PPQ
{
public:
    static constexpr double PPQ_RESOLUTION = 960.0;

    // Constructors
    PPQ() : scaledValue(0) {}
    PPQ(float ppq) : scaledValue(static_cast<int64_t>(ppq * PPQ_RESOLUTION)) {}
    PPQ(double ppq) : scaledValue(static_cast<int64_t>(ppq * PPQ_RESOLUTION)) {}
    PPQ(int scaledPPQ) : scaledValue(static_cast<int64_t>(scaledPPQ)) {}
    PPQ(int64_t scaledPPQ) : scaledValue(scaledPPQ) {}

    // Conversion operators
    operator double() const { return static_cast<double>(scaledValue) / PPQ_RESOLUTION; }
    operator int64_t() const { return scaledValue; }

    // Arithmetic operations
    PPQ operator+(const PPQ &other) const { return PPQ(scaledValue + other.scaledValue); }
    PPQ operator-(const PPQ &other) const { return PPQ(scaledValue - other.scaledValue); }
    PPQ operator+(double ppq) const { return PPQ(scaledValue + static_cast<int64_t>(ppq * PPQ_RESOLUTION)); }
    PPQ operator-(double ppq) const { return PPQ(scaledValue - static_cast<int64_t>(ppq * PPQ_RESOLUTION)); }

    // Comparison operators
    bool operator<(const PPQ &other) const { return scaledValue < other.scaledValue; }
    bool operator<=(const PPQ &other) const { return scaledValue <= other.scaledValue; }
    bool operator>(const PPQ &other) const { return scaledValue > other.scaledValue; }
    bool operator>=(const PPQ &other) const { return scaledValue >= other.scaledValue; }
    bool operator==(const PPQ &other) const { return scaledValue == other.scaledValue; }

    // Comparison operators (PPQ vs double)
    bool operator>(double other) const { return scaledValue > static_cast<int64_t>(other * PPQ_RESOLUTION); }
    bool operator<(double other) const { return scaledValue < static_cast<int64_t>(other * PPQ_RESOLUTION); }
    bool operator>=(double other) const { return scaledValue >= static_cast<int64_t>(other * PPQ_RESOLUTION); }
    bool operator<=(double other) const { return scaledValue <= static_cast<int64_t>(other * PPQ_RESOLUTION); }
    bool operator==(double other) const { return scaledValue == static_cast<int64_t>(other * PPQ_RESOLUTION); }
    bool operator!=(double other) const { return scaledValue != static_cast<int64_t>(other * PPQ_RESOLUTION); }

    // Comparison operators (PPQ vs int64_t)
    bool operator>(int64_t other) const { return scaledValue > other; }
    bool operator<(int64_t other) const { return scaledValue < other; }
    bool operator>=(int64_t other) const { return scaledValue >= other; }
    bool operator<=(int64_t other) const { return scaledValue <= other; }
    bool operator==(int64_t other) const { return scaledValue == other; }
    bool operator!=(int64_t other) const { return scaledValue != other; }

    // Assignment operators
    PPQ &operator-=(const PPQ &other)
    {
        scaledValue -= other.scaledValue;
        return *this;
    }
    PPQ &operator-=(double ppq)
    {
        scaledValue -= static_cast<int64_t>(ppq * PPQ_RESOLUTION);
        return *this;
    }
    PPQ &operator-=(int scaledPPQ)
    {
        scaledValue -= scaledPPQ;
        return *this;
    }

    // Utility methods
    double toDouble() const { return static_cast<double>(scaledValue) / PPQ_RESOLUTION; }
    int64_t toScaled() const { return scaledValue; }

private:
    int64_t scaledValue;
};

// Global comparison operators for double vs PPQ
inline bool operator<(double lhs, const PPQ &rhs) { return rhs > lhs; }
inline bool operator<=(double lhs, const PPQ &rhs) { return rhs >= lhs; }
inline bool operator>(double lhs, const PPQ &rhs) { return rhs < lhs; }
inline bool operator>=(double lhs, const PPQ &rhs) { return rhs <= lhs; }
inline bool operator==(double lhs, const PPQ &rhs) { return rhs == lhs; }
inline bool operator!=(double lhs, const PPQ &rhs) { return rhs != lhs; }

// Global comparison operators for int64_t vs PPQ
inline bool operator<(int64_t lhs, const PPQ &rhs) { return rhs > lhs; }
inline bool operator<=(int64_t lhs, const PPQ &rhs) { return rhs >= lhs; }
inline bool operator>(int64_t lhs, const PPQ &rhs) { return rhs < lhs; }
inline bool operator>=(int64_t lhs, const PPQ &rhs) { return rhs <= lhs; }
inline bool operator==(int64_t lhs, const PPQ &rhs) { return rhs == lhs; }
inline bool operator!=(int64_t lhs, const PPQ &rhs) { return rhs != lhs; }
