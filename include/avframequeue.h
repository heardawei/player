#pragma once

#include <memory>

#include <ffmpeg/avformat>

#include "lockedqueue.h"

using AVFrameQueue = LockedQueue<std::shared_ptr<AVFrame>>;
