/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Include the actual adapter; observe its real presenter call/drawables only in this fixture. */
#include "runtime/graphics_metal.h"
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sched.h>
#include <vector>
static std::atomic<unsigned> captures{0},callbacks{0},positive_times{0},close_requests{0},failures{0},acquisitions{0},last_callback{0},last_positive{0},geometry_checks{0};
@interface PBOwnedWindowLayer : CAMetalLayer
@end
@implementation PBOwnedWindowLayer
- (id<CAMetalDrawable>)nextDrawable
{
    id<CAMetalDrawable> drawable=[super nextDrawable];
    if (drawable) {
        const unsigned acquisition=acquisitions.fetch_add(1)+1;
        [drawable addPresentedHandler:^(id<MTLDrawable> shown) {
            if (shown.presentedTime>0) {positive_times.fetch_add(1);last_positive.store(acquisition);}
            callbacks.fetch_add(1);last_callback.store(acquisition);
        }];
    }
    return drawable;
}
@end
static rt_metal_status observed_present(rt_metal_offscreen *,void *,const rt_metal_bgra_readback *,uint32_t,char *,size_t);
#define CAMetalLayer PBOwnedWindowLayer
#define rt_metal_offscreen_present observed_present
#include "../runtime/display/display_window_metal.mm"
#undef rt_metal_offscreen_present
#undef CAMetalLayer

static const uint8_t row_bgra[]={1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16, 17,18,19,20, 21,22,23,24, 25,26,27,28};
static const uint8_t column_bgra[]={29,31,37,41, 43,47,53,59, 61,67,71,73, 79,83,89,97, 101,103,107,109};
static rt_metal_status observed_present(rt_metal_offscreen *context,void *native_layer,
    const rt_metal_bgra_readback *capture,uint32_t timeout,char *error,size_t error_size)
{
    if (capture || !NSThread.isMainThread) {failures.fetch_add(1);return RT_METAL_INVALID_ARGUMENT;}
    CAMetalLayer *layer=(__bridge CAMetalLayer *)native_layer;
    const unsigned phase=captures.load();const size_t width=phase?1:7,height=phase?5:1;
    if (layer.drawableSize.width!=width || layer.drawableSize.height!=height) failures.fetch_add(1);
    if (phase) {
        NSWindow *window=NSApp.keyWindow;NSView *view=window.contentView;
        const auto geometry_ok=[&] {
            return window && view && view.bounds.size.width==140 && view.bounds.size.height==70 &&
                CGRectEqualToRect(layer.frame,view.bounds) && layer.contentsScale==window.backingScaleFactor &&
                layer.drawableSize.width==1 && layer.drawableSize.height==5;
        };
        const auto check_geometry=[&](const char *name) {
            const bool match=geometry_ok();
            std::printf("runtime_window_geometry name=%s content_width=%g content_height=%g layer_x=%g layer_y=%g layer_width=%g layer_height=%g scale=%g expected_scale=%g drawable_width=%g drawable_height=%g match=%d\n",
                name,(double)view.bounds.size.width,(double)view.bounds.size.height,(double)layer.frame.origin.x,
                (double)layer.frame.origin.y,(double)layer.frame.size.width,(double)layer.frame.size.height,
                (double)layer.contentsScale,(double)window.backingScaleFactor,(double)layer.drawableSize.width,
                (double)layer.drawableSize.height,match?1:0);
            if (!match) failures.fetch_add(1);
            geometry_checks.fetch_add(1);
        };
        check_geometry("actual-resize");
        // Isolate the actual handler from AppKit's possible automatic backing-layer resize.
        // A compiled deleted-handler control must leave these literal bad fields unrepaired.
        layer.frame=CGRectMake(11,13,17,19);layer.contentsScale=window.backingScaleFactor+1;
        [window.delegate windowDidResize:[NSNotification notificationWithName:NSWindowDidResizeNotification object:window]];
        check_geometry("resize-handler-repair");
        layer.frame=CGRectMake(23,29,31,37);layer.contentsScale=window.backingScaleFactor+2;
        [window.delegate windowDidChangeBackingProperties:[NSNotification notificationWithName:NSWindowDidChangeBackingPropertiesNotification object:window]];
        check_geometry("backing-handler-repair");
    }

    std::vector<uint8_t> bytes(width*height*4,0xa5);
    rt_metal_bgra_readback observed{bytes.data(),width*4,bytes.size()};
    layer.framebufferOnly=NO; /* Only this test requests the primitive's real pre-present capture. */
    const rt_metal_status result=rt_metal_offscreen_present(context,native_layer,&observed,timeout,error,error_size);
    if (result==RT_METAL_OK) {
        const uint8_t *literal=phase?column_bgra:row_bgra;
        if (std::memcmp(bytes.data(),literal,bytes.size())) failures.fetch_add(1);
        if (!phase) [NSApp.keyWindow setContentSize:NSMakeSize(140,70)];
        captures.fetch_add(1);
    }
    return result;
}
extern "C" void rt_boot_window_request_stop(const char *function,const char *reason)
{
    if (!NSThread.isMainThread || !function || !reason || !std::strstr(reason,"close")) failures.fetch_add(1);
    close_requests.fetch_add(1);
}
extern "C" void rt_fatal(const char *function,const char *reason)
{
    std::fprintf(stderr,"owned runtime window fatal: %s: %s\n",function,reason);std::_Exit(1);
}
static bool wait_for(const std::atomic<unsigned> &value,unsigned target)
{
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (value.load(std::memory_order_acquire)<target) {
        if (std::chrono::steady_clock::now()>=deadline) return false;
        sched_yield();
    }
    return true;
}
static int owned_boot(void *unused,const sigset_t *previous)
{
    (void)unused;
    if (NSThread.isMainThread || pthread_sigmask(SIG_SETMASK,previous,nullptr)) return 1;
    if (rt_display_window_open(7,1)) return 1;
    alignas(uint32_t) uint8_t row[sizeof row_bgra];std::memcpy(row,row_bgra,sizeof row);
    rt_display_window_show((const uint32_t *)row,7,1);std::memset(row,0,sizeof row);
    if (!wait_for(captures,1) || !wait_for(callbacks,1) || !wait_for(positive_times,1) || !wait_for(last_callback,1) || !wait_for(last_positive,1)) return 1;
    alignas(uint32_t) uint8_t column[sizeof column_bgra];std::memcpy(column,column_bgra,sizeof column);
    rt_display_window_show((const uint32_t *)column,1,5);std::memset(column,0,sizeof column);
    if (!wait_for(captures,2) || !wait_for(callbacks,2) || !wait_for(positive_times,2) || !wait_for(last_callback,2) || !wait_for(last_positive,2)) return 1;
    dispatch_async(dispatch_get_main_queue(),^{[NSApp.keyWindow performClose:nil];});
    if (!wait_for(close_requests,1)) return 1;
    return failures.load()?1:0;
}
int main(void)
{
    const int result=rt_display_window_run(owned_boot,nullptr);
    std::printf("Metal runtime window: captures=%u callbacks=%u positive_times=%u close_requests=%u geometry_checks=%u failures=%u rc=%d\n",
        captures.load(),callbacks.load(),positive_times.load(),close_requests.load(),geometry_checks.load(),failures.load(),result);
    return result || failures.load() || geometry_checks.load()!=3 || acquisitions.load()!=2 || last_callback.load()!=2 || last_positive.load()!=2 || captures.load()!=2 || callbacks.load()!=2 || positive_times.load()!=2 || close_requests.load()!=1 ?1:0;
}
