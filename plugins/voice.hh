#pragma once

#include <cstdint>

#include "smoothed-value.hh"

namespace clap {
   class Voice {
   public:
      static constexpr const uint32_t max_voices = 16;

      Voice(uint32_t voiceIndex) : _voiceIndex(voiceIndex) {}
      Voice(const Voice &) = delete;
      Voice(Voice &&) = delete;
      Voice &operator=(const Voice &) = delete;
      Voice &operator=(Voice &&) = delete;

      [[nodiscard]] uint32_t voiceIndex() const noexcept;
      [[nodiscard]] bool isAssigned() const noexcept;

      // key and channel info
      [[nodiscard]] int32_t channel() const noexcept;
      [[nodiscard]] int32_t key() const noexcept;
      [[nodiscard]] int32_t lowestKey() const noexcept;
      [[nodiscard]] int32_t highestKey() const noexcept;
      [[nodiscard]] int32_t startKey() const noexcept;

      // note expressions for this voice
      [[nodiscard]] double pitch() const noexcept;
      [[nodiscard]] double brightness() const noexcept;
      [[nodiscard]] double pressure() const noexcept;
      [[nodiscard]] double vibrato() const noexcept;
      [[nodiscard]] double expression() const noexcept;
      [[nodiscard]] double velocity() const noexcept;
      [[nodiscard]] double gain() const noexcept;
      [[nodiscard]] double pan() const noexcept;

      void reset() noexcept;

   private:
      const uint32_t _voiceIndex;
      bool _isAssigned = false;

      double _velocity;
      SmoothedValue _pitch;
      SmoothedValue _brigthness;
      SmoothedValue _pressure;
      SmoothedValue _vibrato;
      SmoothedValue _expression;
      SmoothedValue _gain;
      SmoothedValue _pan;
   };

   template <typename T>
   using Voices = std::array<T, Voice::max_voices>;
} // namespace clap
