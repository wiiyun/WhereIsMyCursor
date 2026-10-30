#include "async_inverse.h"

AsyncInverse::AsyncInverse(){thread=std::thread([this]{loop();});}
AsyncInverse::~AsyncInverse(){
    {std::lock_guard<std::mutex> lock(mutex);stopping=true;active=false;failed=false;pending={};ready.reset();spare.reset();++epoch;}
    wake.notify_one();thread.join();
}
void AsyncInverse::submit(std::shared_ptr<const RenderPlan> plan,POINT position){
    {std::lock_guard<std::mutex> lock(mutex);active=true;pending={std::move(plan),position};}wake.notify_one();
}
std::unique_ptr<InverseFrame> AsyncInverse::take(){std::lock_guard<std::mutex> lock(mutex);if(failed){failed=false;throw std::runtime_error("Cannot update inverse cursor background");}return std::move(ready);}
void AsyncInverse::recycle(std::unique_ptr<InverseFrame> frame){std::lock_guard<std::mutex> lock(mutex);if(active&&!stopping)spare=std::move(frame);}
void AsyncInverse::cancel(){
    {std::lock_guard<std::mutex> lock(mutex);active=false;failed=false;pending={};ready.reset();spare.reset();++epoch;}wake.notify_one();
}
void AsyncInverse::loop(){
    std::unique_ptr<InverseFrame> work;
    for(;;){
        Job job;u64 jobEpoch;
        {
            std::unique_lock<std::mutex> lock(mutex);
            while(!stopping&&!pending.plan){if(!active)work.reset();wake.wait(lock);}
            if(stopping)return;
            job=std::move(pending);pending={};jobEpoch=epoch;
            if(!work)work=std::move(spare);
        }
        try{
            if(!work)work=std::make_unique<InverseFrame>();
            const auto& plan=*job.plan;const auto box=plan.inverseBounds;
            int bw=box.right-box.left,bh=box.bottom-box.top;
            work->pixels.ensure(plan.width,plan.height);work->background.ensure(bw,bh);work->background.fill(0);
            std::copy(plan.base.begin(),plan.base.end(),work->pixels.pixels);
            RECT desired{job.position.x+box.left,job.position.y+box.top,job.position.x+box.right,job.position.y+box.bottom};
            RECT screen{GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),0,0};screen.right=screen.left+GetSystemMetrics(SM_CXVIRTUALSCREEN);screen.bottom=screen.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);RECT clip{};
            if(IntersectRect(&clip,&desired,&screen)){
                HDC desktop=GetDC(nullptr);BOOL ok=BitBlt(work->background.dc,clip.left-desired.left,clip.top-desired.top,clip.right-clip.left,clip.bottom-clip.top,desktop,clip.left,clip.top,SRCCOPY|CAPTUREBLT);GdiFlush();ReleaseDC(nullptr,desktop);require(ok,"Cannot sample cursor background");
            }
            for(const auto& span:plan.spans){
                const uint32_t* source=work->background.pixels+size_t(span.y-box.top)*bw+span.x-box.left;
                uint32_t* target=work->pixels.pixels+size_t(span.y)*plan.width+span.x;
                for(int i=0;i<span.count;i++)target[i]=0xFF000000|((source[i]&span.andMask)^span.xorMask);
            }
            work->plan=std::move(job.plan);
            {
                std::lock_guard<std::mutex> lock(mutex);
                if(stopping)return;
                if(active&&epoch==jobEpoch){ready=std::move(work);work=std::move(spare);}
                else work.reset();
            }
        }catch(...){work.reset();std::lock_guard<std::mutex> lock(mutex);if(active&&epoch==jobEpoch)failed=true;}
    }
}
