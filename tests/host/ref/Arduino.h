// ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
// Just enough of the Arduino-ESP32 API to compile rc-switch 2.6.4 and the sketch extract on the
// host. The pin and delay functions are implemented by the test (arduino_shim in test_reference.cpp).
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#define HIGH    1
#define LOW     0
#define INPUT   0
#define OUTPUT  1
#define CHANGE  1
#define DEC     10
#define HEX     16
#define IRAM_ATTR
#define DRAM_ATTR

typedef uint8_t byte;
typedef bool    boolean;

void          pinMode(int pin, int mode);
void          digitalWrite(int pin, int level);
void          delayMicroseconds(unsigned int us);
unsigned long micros(void);
void          attachInterrupt(int irq, void (*fn)(void), int mode);
void          detachInterrupt(int irq);
void          yield(void);

// Arduino String semantics that the sketch relies on (WString.cpp).
class String {
public:
    String() {}
    String(const char *s) : s_(s ? s : "") {}
    String(const std::string &s) : s_(s) {}
    explicit String(char c) : s_(1, c) {}
    String(int v, unsigned char base = DEC) : s_(num((long)v, base)) {}
    String(unsigned int v, unsigned char base = DEC) : s_(unum(v, base)) {}
    String(long v, unsigned char base = DEC) : s_(num(v, base)) {}
    String(unsigned long v, unsigned char base = DEC) : s_(unum(v, base)) {}

    const char  *c_str() const { return s_.c_str(); }
    unsigned int length() const { return (unsigned int)s_.size(); }
    void toLowerCase()
    {
        for (auto &c : s_) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    bool startsWith(const String &p) const { return s_.compare(0, p.s_.size(), p.s_) == 0 && p.s_.size() <= s_.size(); }
    bool endsWith(const String &p) const
    {
        return p.s_.size() <= s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
    }
    String substring(unsigned int left) const { return substring(left, length()); }
    String substring(unsigned int left, unsigned int right) const
    {
        if (left > right) { unsigned int t = right; right = left; left = t; }
        if (left >= length()) return String();
        if (right > length()) right = length();
        return String(s_.substr(left, right - left));
    }
    bool    operator==(const String &o) const { return s_ == o.s_; }
    bool    operator!=(const String &o) const { return s_ != o.s_; }
    String &operator+=(const String &o) { s_ += o.s_; return *this; }
    friend String operator+(const String &a, const String &b) { return String(a.s_ + b.s_); }
    friend String operator+(const String &a, const char *b) { return String(a.s_ + b); }
    friend String operator+(const char *a, const String &b) { return String(std::string(a) + b.s_); }

private:
    static std::string unum(unsigned long v, unsigned char base)
    {
        char buf[72];
        int  n = 0;
        if (base < 2) base = 10;
        do { int d = (int)(v % base); buf[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= base; } while (v);
        std::string s;
        while (n) s += buf[--n];
        return s;
    }
    static std::string num(long v, unsigned char base)
    {
        if (base == 10 && v < 0) return "-" + unum((unsigned long)(-(v + 1)) + 1u, base);
        return unum((unsigned long)v, base);
    }
    std::string s_;
};
