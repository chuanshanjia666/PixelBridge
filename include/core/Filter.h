#ifndef FILTER_H
#define FILTER_H

#include "DataPacket.h"
#include <vector>
#include <string>

namespace pb
{
    enum class LatencyLevel
    {
        UltraLow = 0, // 追求极致延迟，可能有少量花屏
        Low = 1,      // 平衡模式
        Standard = 2  // 优先画质和稳定性
    };

    class Filter
    {
    public:
        virtual ~Filter() = default;

        virtual bool initialize() = 0;
        virtual void process(DataPacket::Ptr packet) = 0;
        virtual void start() {}
        virtual void stop() = 0;

        void setNextFilter(Filter *next)
        {
            m_next = next;
        }

        void setLatencyLevel(LatencyLevel level) { m_latencyLevel = level; }
        LatencyLevel latencyLevel() const { return m_latencyLevel; }

        std::string name() const { return m_name; }

    protected:
        Filter(const std::string &name) : m_name(name) {}
        Filter *m_next = nullptr;
        std::string m_name;
        LatencyLevel m_latencyLevel = LatencyLevel::Low;
    };

} // namespace pb

#endif // FILTER_H
