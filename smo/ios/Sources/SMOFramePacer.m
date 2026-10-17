#import "SMOFramePacer.h"
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <mach/mach.h>
#include <sys/resource.h>
#include "logbuf.h"
#import "nvn_metal_private.h"
@implementation SMOFramePacer {
    NSCondition *_condition;
    BOOL _active,_powerSaving;
    int _fps;
    double _deadline,_lastTime,_lastCPU;
    uint64_t _lastFrames;
}
+ (instancetype)shared {static SMOFramePacer *p;static dispatch_once_t once;dispatch_once(&once,^{p=[SMOFramePacer new];});return p;}
- (instancetype)init {
    if((self=[super init])){
        _condition=[NSCondition new];_active=UIApplication.sharedApplication.applicationState==UIApplicationStateActive;_fps=60;
        NSNotificationCenter *nc=NSNotificationCenter.defaultCenter;
        [nc addObserver:self selector:@selector(suspend) name:UIApplicationWillResignActiveNotification object:nil];
        [nc addObserver:self selector:@selector(resume) name:UIApplicationDidBecomeActiveNotification object:nil];
        [nc addObserver:self selector:@selector(updatePolicy) name:NSProcessInfoThermalStateDidChangeNotification object:nil];
        [nc addObserver:self selector:@selector(updatePolicy) name:NSProcessInfoPowerStateDidChangeNotification object:nil];
        [self updatePolicy];
        _lastTime=CACurrentMediaTime();
        struct rusage usage;if(!getrusage(RUSAGE_SELF,&usage))_lastCPU=usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6;
        [NSTimer scheduledTimerWithTimeInterval:10 repeats:YES block:^(NSTimer *timer){(void)timer;[self recordStats];}];
    }return self;
}
- (BOOL)powerSaving {[_condition lock];BOOL value=_powerSaving;[_condition unlock];return value;}
- (void)setPowerSaving:(BOOL)value {[_condition lock];_powerSaving=value;[_condition unlock];[self updatePolicy];}
- (void)updatePolicy {
    NSProcessInfo *info=NSProcessInfo.processInfo;
    [_condition lock];_fps=(_powerSaving || info.lowPowerModeEnabled || info.thermalState>=NSProcessInfoThermalStateSerious)?30:60;
    _deadline=0;[_condition broadcast];int fps=_fps;[_condition unlock];
    dm_log_emit("FRAME_PACING target=%d thermal=%ld low_power=%d",fps,(long)info.thermalState,info.lowPowerModeEnabled);
}
- (void)suspend {[_condition lock];_active=NO;_deadline=0;[_condition broadcast];[_condition unlock];}
- (void)resume {[_condition lock];_active=YES;_deadline=0;[_condition broadcast];[_condition unlock];}
- (void)waitForFrame:(int)interval {
    [_condition lock];
    for(;;){
        while(!_active)[_condition wait];
        double now=CACurrentMediaTime();
        double duration=MAX(1.0/_fps,interval>0?interval/60.0:0);
        if(!_deadline || now>=_deadline){_deadline=now+duration;break;}
        [_condition waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:_deadline-now]];
    }
    [_condition unlock];
}
- (void)recordStats {
    double now=CACurrentMediaTime(),cpu=_lastCPU;struct rusage usage;
    if(!getrusage(RUSAGE_SELF,&usage))cpu=usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6;
    task_vm_info_data_t memory={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
    kern_return_t result=task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&memory,&count);
    uint64_t frames=nvn_metal_offscreen_frames();
    dm_log_emit("RUNTIME_STATS fps=%.1f cpu_cores=%.2f footprint_mb=%.1f thermal=%ld",
        (frames-_lastFrames)/MAX(0.001,now-_lastTime),(cpu-_lastCPU)/MAX(0.001,now-_lastTime),
        result==KERN_SUCCESS?memory.phys_footprint/1048576.0:-1.0,(long)NSProcessInfo.processInfo.thermalState);
    _lastTime=now;_lastCPU=cpu;_lastFrames=frames;
}
@end
