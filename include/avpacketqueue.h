#pragma once

#include <memory>

#include <ffmpeg/avformat>

#include "lockedqueue.h"

using AVPacketQueue = LockedQueue<std::shared_ptr<AVPacket>>;
