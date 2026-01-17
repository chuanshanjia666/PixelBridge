#ifndef FILTER_H
#define FILTER_H

#include "DataPacket.h"
#include <vector>
#include <string>

namespace pb
{

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

        std::string name() const { return m_name; }

    protected:
        Filter(const std::string &name) : m_name(name) {}
        Filter *m_next = nullptr;
        std::string m_name;
    };

} // namespace pb

#endif // FILTER_H
