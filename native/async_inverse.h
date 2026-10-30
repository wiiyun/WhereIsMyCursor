#pragma once
#include "core.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

struct InverseSpan {int y,x,count;uint32_t andMask,xorMask;};
struct RenderPlan {
    u64 generation=0;int width=0,height=0,offsetX=0,offsetY=0;
    std::vector<uint32_t> base;std::vector<InverseSpan> spans;RECT inverseBounds{};
};
struct InverseFrame {Dib pixels,background;std::shared_ptr<const RenderPlan> plan;};
class AsyncInverse {
    struct Job {std::shared_ptr<const RenderPlan> plan;POINT position{};};
    std::mutex mutex;std::condition_variable wake;std::thread thread;
    Job pending;std::unique_ptr<InverseFrame> ready,spare;
    bool stopping=false,active=false,failed=false;u64 epoch=0;
    void loop();
public:
    AsyncInverse();~AsyncInverse();
    void submit(std::shared_ptr<const RenderPlan> plan,POINT position);
    std::unique_ptr<InverseFrame> take();
    void recycle(std::unique_ptr<InverseFrame> frame);
    void cancel();
};
