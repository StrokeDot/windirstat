// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

class ScanTiming
{
public:
    using Clock = std::chrono::steady_clock;
    using Seconds = std::chrono::seconds;

    void Start(const Clock::time_point now)
    {
        *this = {};
        m_started = now;
    }

    void SetPaused(const bool paused, const Clock::time_point now)
    {
        if (paused && !m_pausedAt) m_pausedAt = now;
        else if (!paused && m_pausedAt)
        {
            m_pausedTime += now - *m_pausedAt;
            m_pausedAt.reset();
        }
    }

    Seconds Elapsed(const Clock::time_point now) const
    {
        return std::chrono::duration_cast<Seconds>(m_pausedAt.value_or(now) - m_started - m_pausedTime);
    }

    std::optional<Seconds> Estimate(const Clock::time_point now, const std::uint64_t position, const std::uint64_t range)
    {
        if (m_pausedAt) return std::nullopt;

        const auto elapsed = Elapsed(now);
        if (range != m_range || position < m_lastPosition || range == 0 || position >= range)
        {
            m_range = range;
            m_samplePosition = position;
            m_lastPosition = position;
            m_sampleTime = elapsed;
            m_unitsPerSecond = 0;
            return std::nullopt;
        }

        m_lastPosition = position;
        const auto sampleDuration = elapsed - m_sampleTime;
        if (sampleDuration >= Seconds{ 10 })
        {
            m_unitsPerSecond = static_cast<double>(position - m_samplePosition) / sampleDuration.count();
            m_samplePosition = position;
            m_sampleTime = elapsed;
        }

        if (m_unitsPerSecond <= 0) return std::nullopt;
        const double remaining = std::ceil(static_cast<double>(range - position) / m_unitsPerSecond);
        if (remaining >= static_cast<double>(Seconds::max().count())) return std::nullopt;
        return Seconds{ static_cast<Seconds::rep>(remaining) };
    }

private:
    Clock::time_point m_started{};
    Clock::duration m_pausedTime{};
    std::optional<Clock::time_point> m_pausedAt;
    Seconds m_sampleTime{};
    std::uint64_t m_samplePosition = 0;
    std::uint64_t m_lastPosition = 0;
    std::uint64_t m_range = 0;
    double m_unitsPerSecond = 0;
};
