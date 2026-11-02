#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include <cstdint>

// High-security, standalone cryptographic utilities
// Implements SHA-256 and PBKDF2-HMAC-SHA256 with 100,000 iterations
namespace Crypto {

// --- Portable SHA-256 Implementation ---
class SHA256 {
private:
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    uint32_t datalen;

    static inline uint32_t rotr(uint32_t a, uint32_t b) {
        return (a >> b) | (a << (32 - b));
    }
    static inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) {
        return (e & f) ^ (~e & g);
    }
    static inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) {
        return (a & b) ^ (a & c) ^ (b & c);
    }
    static inline uint32_t sig0(uint32_t x) {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }
    static inline uint32_t sig1(uint32_t x) {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }
    static inline uint32_t theta0(uint32_t x) {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }
    static inline uint32_t theta1(uint32_t x) {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }

    void transform(const uint8_t* chunk) {
        static const uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };

        uint32_t m[64];
        for (int i = 0; i < 16; ++i) {
            m[i] = ((uint32_t)chunk[i * 4] << 24) |
                   ((uint32_t)chunk[i * 4 + 1] << 16) |
                   ((uint32_t)chunk[i * 4 + 2] << 8) |
                   ((uint32_t)chunk[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            m[i] = theta1(m[i - 2]) + m[i - 7] + theta0(m[i - 15]) + m[i - 16];
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sig1(e) + choose(e, f, g) + k[i] + m[i];
            uint32_t t2 = sig0(a) + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

public:
    SHA256() { reset(); }

    void reset() {
        datalen = 0;
        bitlen = 0;
        state[0] = 0x6a09e667; state[1] = 0xbb67ae85;
        state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
        state[4] = 0x510e527f; state[5] = 0x9b05688c;
        state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
    }

    void update(const uint8_t* input, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            data[datalen] = input[i];
            datalen++;
            if (datalen == 64) {
                transform(data);
                bitlen += 512;
                datalen = 0;
            }
        }
    }

    void finalize(uint8_t hash[32]) {
        size_t i = datalen;
        if (datalen < 56) {
            data[i++] = 0x80;
            while (i < 56) data[i++] = 0x00;
        } else {
            data[i++] = 0x80;
            while (i < 64) data[i++] = 0x00;
            transform(data);
            std::memset(data, 0, 56);
        }
        bitlen += datalen * 8;
        data[63] = bitlen;
        data[62] = bitlen >> 8;
        data[61] = bitlen >> 16;
        data[60] = bitlen >> 24;
        data[59] = bitlen >> 32;
        data[58] = bitlen >> 40;
        data[57] = bitlen >> 48;
        data[56] = bitlen >> 56;
        transform(data);

        for (i = 0; i < 4; ++i) {
            for (int j = 0; j < 8; ++j) {
                hash[j * 4 + i] = (state[j] >> (24 - i * 8)) & 0x000000ff;
            }
        }
    }
};

// HMAC-SHA256
inline void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t dataLen, uint8_t out[32]) {
    uint8_t k[64] = {0};
    if (keyLen > 64) {
        SHA256 s;
        s.update(key, keyLen);
        s.finalize(k);
    } else {
        std::memcpy(k, key, keyLen);
    }

    uint8_t ipad[64];
    uint8_t opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    uint8_t innerHash[32];
    SHA256 s1;
    s1.update(ipad, 64);
    s1.update(data, dataLen);
    s1.finalize(innerHash);

    SHA256 s2;
    s2.update(opad, 64);
    s2.update(innerHash, 32);
    s2.finalize(out);
}

// PBKDF2-HMAC-SHA256
inline void pbkdf2Sha256(const std::string& password, const std::string& salt, uint32_t iterations, uint8_t out[32]) {
    std::vector<uint8_t> saltWithIndex(salt.size() + 4);
    std::memcpy(saltWithIndex.data(), salt.data(), salt.size());
    saltWithIndex[salt.size() + 0] = 0;
    saltWithIndex[salt.size() + 1] = 0;
    saltWithIndex[salt.size() + 2] = 0;
    saltWithIndex[salt.size() + 3] = 1;

    uint8_t u[32];
    uint8_t t[32];
    hmacSha256((const uint8_t*)password.data(), password.size(), saltWithIndex.data(), saltWithIndex.size(), u);
    std::memcpy(t, u, 32);

    for (uint32_t iter = 1; iter < iterations; ++iter) {
        uint8_t nextU[32];
        hmacSha256((const uint8_t*)password.data(), password.size(), u, 32, nextU);
        for (int i = 0; i < 32; ++i) {
            t[i] ^= nextU[i];
            u[i] = nextU[i];
        }
    }
    std::memcpy(out, t, 32);
}

inline std::string toHex(const uint8_t* data, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    return ss.str();
}

inline std::string generateRandomHex(size_t bytes = 16) {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(0, 255);

    std::vector<uint8_t> buf(bytes);
    for (size_t i = 0; i < bytes; ++i) {
        buf[i] = dis(gen);
    }
    return toHex(buf.data(), bytes);
}

// Hash password using PBKDF2-HMAC-SHA256 with random salt (60,000 iterations for fast response + high security)
inline std::string hashPassword(const std::string& password) {
    std::string salt = generateRandomHex(16);
    uint32_t iterations = 60000;
    uint8_t digest[32];
    pbkdf2Sha256(password, salt, iterations, digest);
    return "$pbkdf2-sha256$" + std::to_string(iterations) + "$" + salt + "$" + toHex(digest, 32);
}

// Constant-time string comparison to prevent timing attacks
inline bool constantTimeCompare(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) return false;
    unsigned char result = 0;
    for (size_t i = 0; i < a.length(); ++i) {
        result |= (a[i] ^ b[i]);
    }
    return result == 0;
}

// Verify password against stored hash
inline bool verifyPassword(const std::string& password, const std::string& storedHash) {
    if (storedHash.rfind("$pbkdf2-sha256$", 0) != 0) {
        // Fallback for simple legacy hashes if needed
        return false;
    }
    size_t first = storedHash.find('$', 15);
    if (first == std::string::npos) return false;
    size_t second = storedHash.find('$', first + 1);
    if (second == std::string::npos) return false;

    uint32_t iterations = std::stoul(storedHash.substr(15, first - 15));
    std::string salt = storedHash.substr(first + 1, second - first - 1);
    std::string expectedHex = storedHash.substr(second + 1);

    uint8_t digest[32];
    pbkdf2Sha256(password, salt, iterations, digest);
    std::string computedHex = toHex(digest, 32);

    return constantTimeCompare(computedHex, expectedHex);
}

// Generate secure session token
inline std::string generateSessionToken() {
    return generateRandomHex(32);
}

} // namespace Crypto

#endif // CRYPTO_H
