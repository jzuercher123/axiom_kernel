#pragma once

#include "axiom/adapters/hooks/crypto_provenance_hook.hpp"

#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace axiom::adapters {

class OpenSslBackend final : public ICryptoBackend {
public:
    OpenSslBackend() {
        EVP_PKEY* raw_key = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
        if (raw_key == nullptr) {
            throw std::runtime_error("OpenSslBackend: failed to generate EC P-256 keypair");
        }
        key_.reset(raw_key);
    }

    [[nodiscard]] bool is_hardware_backed() const noexcept override {
        return false;
    }

    [[nodiscard]] bool compute_digest(
        const std::byte* data,
        std::size_t byte_len,
        std::uint64_t sequence_id,
        std::uint64_t monotonic_ns,
        std::array<std::uint8_t, 32>& out_digest) noexcept override {
        if (data == nullptr || byte_len == 0) {
            return false;
        }

        MdCtxPtr ctx(EVP_MD_CTX_new());
        if (!ctx) {
            return false;
        }

        unsigned int digest_len = 0;
        const bool ok =
            EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1 &&
            EVP_DigestUpdate(ctx.get(), data, byte_len) == 1 &&
            EVP_DigestUpdate(ctx.get(), &sequence_id, sizeof(sequence_id)) == 1 &&
            EVP_DigestUpdate(ctx.get(), &monotonic_ns, sizeof(monotonic_ns)) == 1 &&
            EVP_DigestFinal_ex(ctx.get(), out_digest.data(), &digest_len) == 1 &&
            digest_len == 32u;

        return ok;
    }

    [[nodiscard]] bool sign_digest(
        const std::array<std::uint8_t, 32>& digest,
        std::array<std::uint8_t, 64>& out_signature) noexcept override {
        PkeyCtxPtr ctx(EVP_PKEY_CTX_new(key_.get(), nullptr));
        if (!ctx || EVP_PKEY_sign_init(ctx.get()) <= 0) {
            return false;
        }

        std::size_t der_len = 0;
        if (EVP_PKEY_sign(ctx.get(), nullptr, &der_len, digest.data(), digest.size()) <= 0) {
            return false;
        }

        std::array<unsigned char, 128> der_buf{};
        if (der_len > der_buf.size()) {
            return false;
        }
        if (EVP_PKEY_sign(ctx.get(), der_buf.data(), &der_len, digest.data(), digest.size()) <= 0) {
            return false;
        }

        const unsigned char* p = der_buf.data();
        EcdsaSigPtr sig(d2i_ECDSA_SIG(nullptr, &p, static_cast<long>(der_len)));
        if (!sig) {
            return false;
        }

        const BIGNUM* r = nullptr;
        const BIGNUM* s = nullptr;
        ECDSA_SIG_get0(sig.get(), &r, &s);
        if (r == nullptr || s == nullptr) {
            return false;
        }

        return BN_bn2binpad(r, out_signature.data(), 32) == 32 &&
               BN_bn2binpad(s, out_signature.data() + 32, 32) == 32;
    }

    // Verification helper for unit tests and downstream provenance checks
    [[nodiscard]] bool verify_digest(
        const std::array<std::uint8_t, 32>& digest,
        const std::array<std::uint8_t, 64>& signature) const noexcept {
        BIGNUM* r = BN_bin2bn(signature.data(), 32, nullptr);
        BIGNUM* s = BN_bin2bn(signature.data() + 32, 32, nullptr);
        if (r == nullptr || s == nullptr) {
            BN_free(r);
            BN_free(s);
            return false;
        }

        EcdsaSigPtr sig(ECDSA_SIG_new());
        if (!sig || ECDSA_SIG_set0(sig.get(), r, s) != 1) {
            BN_free(r);
            BN_free(s);
            return false;
        }

        std::array<unsigned char, 128> der_buf{};
        unsigned char* p = der_buf.data();
        const int der_len = i2d_ECDSA_SIG(sig.get(), &p);
        if (der_len <= 0) {
            return false;
        }

        PkeyCtxPtr ctx(EVP_PKEY_CTX_new(key_.get(), nullptr));
        if (!ctx || EVP_PKEY_verify_init(ctx.get()) <= 0) {
            return false;
        }

        return EVP_PKEY_verify(
                   ctx.get(),
                   der_buf.data(),
                   static_cast<std::size_t>(der_len),
                   digest.data(),
                   digest.size()) == 1;
    }

private:
    struct EvpPkeyDeleter { void operator()(EVP_PKEY* p) const noexcept { EVP_PKEY_free(p); } };
    struct EvpMdCtxDeleter { void operator()(EVP_MD_CTX* p) const noexcept { EVP_MD_CTX_free(p); } };
    struct EvpPkeyCtxDeleter { void operator()(EVP_PKEY_CTX* p) const noexcept { EVP_PKEY_CTX_free(p); } };
    struct EcdsaSigDeleter { void operator()(ECDSA_SIG* p) const noexcept { ECDSA_SIG_free(p); } };

    using PkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
    using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;
    using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;
    using EcdsaSigPtr = std::unique_ptr<ECDSA_SIG, EcdsaSigDeleter>;

    PkeyPtr key_;
};

} // namespace axiom::adapters