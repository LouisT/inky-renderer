#ifndef JPEG_UTILS_H
#define JPEG_UTILS_H

#include <vector>
#include <cstdint>
#include <cstddef>

namespace jpeg_utils
{
    // JPEG classifications
    enum class JpegKind : uint8_t
    {
        INVALID = 0,
        BASELINE,
        PROGRESSIVE,
        OTHER
    };

    // Inspect raw JPEG bytes and return its kind
    JpegKind probeKind(const uint8_t *data, std::size_t len);

    // Generic predicate: true if probeKind(...) == Kind
    template <JpegKind Kind>
    inline bool isKind(const uint8_t *data, std::size_t len)
    {
        return probeKind(data, len) == Kind;
    }

    // Overload for std::vector buffers
    template <JpegKind Kind>
    inline bool isKind(const std::vector<uint8_t> &buf)
    {
        return isKind<Kind>(buf.data(), buf.size());
    }

    // Convenience aliases
    inline bool isProgressive(const uint8_t *d, std::size_t l)
    {
        return isKind<JpegKind::PROGRESSIVE>(d, l);
    }
    inline bool isProgressive(const std::vector<uint8_t> &b)
    {
        return isKind<JpegKind::PROGRESSIVE>(b);
    }

    inline bool isBaseline(const uint8_t *d, std::size_t l)
    {
        return isKind<JpegKind::BASELINE>(d, l);
    }
    inline bool isBaseline(const std::vector<uint8_t> &b)
    {
        return isKind<JpegKind::BASELINE>(b);
    }

    inline bool isInvalid(const uint8_t *d, std::size_t l)
    {
        return isKind<JpegKind::INVALID>(d, l);
    }
    inline bool isInvalid(const std::vector<uint8_t> &b)
    {
        return isKind<JpegKind::INVALID>(b);
    }

    inline bool isOther(const uint8_t *d, std::size_t l)
    {
        return isKind<JpegKind::OTHER>(d, l);
    }
    inline bool isOther(const std::vector<uint8_t> &b)
    {
        return isKind<JpegKind::OTHER>(b);
    }
}

#endif