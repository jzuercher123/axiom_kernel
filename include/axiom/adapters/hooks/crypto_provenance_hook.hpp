#pragma once

#include "axiom/core/boolean_hook.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace axiom::adapters {

class ICryptoBackend {
public:
    virtual ~ICryptoBackend() = default;

    // Computes SHA-256 over (raw tensor bytes || sequence_id || monotonic_ns)
    [[nodiscard]] virtual bool compute_digest(
        const std::byte* data,
        std::size_t byte_len,
        std::uint64_t sequence_id,
        std::uint64_t monotonic_ns,
        std::array<std::uint8_t, 32>& out_digest) noexcept = 0;

    // Signs a 32-byte SHA-256 digest using ECDSA P-256 (raw 64-byte R || S signature)
    [[nodiscard]] virtual bool sign_digest(
        const std::array<std::uint8_t, 32>& digest,
        std::array<std::uint8_t, 64>& out_signature) noexcept = 0;

    [[nodiscard]] virtual bool is_hardware_backed() const noexcept = 0;
};

class CryptoProvenanceHook final : public core::IBooleanHook {
public:
    explicit CryptoProvenanceHook(ICryptoBackend& backend,
                                  bool drop_on_failure = true) noexcept
        : backend_(backend), drop_on_failure_(drop_on_failure) {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return "CryptoProvenanceHook";
    }

    [[nodiscard]] bool evaluate(const core::TensorView& tensor,
                                core::SlotMetadata& meta) noexcept override {
        if (tensor.empty()) {
            return !drop_on_failure_;
        }

        const bool hashed = backend_.compute_digest(
            tensor.data,
            tensor.byte_size(),
            meta.sequence_id,
            meta.monotonic_ns,
            meta.sha256_digest
        );
        if (!hashed) {
            return !drop_on_failure_;
        }
        meta.provenance_flags |= core::kProvenanceHasDigest;

        const bool signed_ok = backend_.sign_digest(meta.sha256_digest, meta.signature);
        if (!signed_ok) {
            return !drop_on_failure_;
        }

        meta.provenance_flags |= backend_.is_hardware_backed()
            ? core::kProvenanceHardwareSig
            : core::kProvenanceSoftwareSig;

        return true;
    }

private:
    ICryptoBackend& backend_;
    bool drop_on_failure_;
};

} // namespace axiom::adapters