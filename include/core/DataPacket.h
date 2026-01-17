#ifndef DATAPACKET_H
#define DATAPACKET_H

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace pb {

enum class PacketType {
    UNKNOWN,
    AV_PACKET,
    AV_FRAME
};

class DataPacket {
public:
    using Ptr = std::shared_ptr<DataPacket>;

    virtual ~DataPacket() = default;
    virtual PacketType type() const = 0;
};

class AVPacketWrapper : public DataPacket {
public:
    AVPacketWrapper() {
        packet = av_packet_alloc();
    }
    ~AVPacketWrapper() {
        if (packet) av_packet_free(&packet);
    }
    PacketType type() const override { return PacketType::AV_PACKET; }
    AVPacket* get() { return packet; }

private:
    AVPacket* packet = nullptr;
};

class AVFrameWrapper : public DataPacket {
public:
    AVFrameWrapper() {
        frame = av_frame_alloc();
    }
    ~AVFrameWrapper() {
        if (frame) av_frame_free(&frame);
    }
    PacketType type() const override { return PacketType::AV_FRAME; }
    AVFrame* get() { return frame; }

private:
    AVFrame* frame = nullptr;
};

} // namespace pb

#endif // DATAPACKET_H
