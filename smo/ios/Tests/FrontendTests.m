#import <XCTest/XCTest.h>
#import <QuartzCore/QuartzCore.h>
#import "SMOTouchController.h"
#import "SMOFramePacer.h"
#include "dm_input.h"
#include "audio_output.h"
#include <math.h>
@interface UIControl (SMOTestingTracking)
- (BOOL)beginTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event;
- (BOOL)continueTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event;
- (void)endTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event;
@end
static uint64_t buttons;
static int32_t axes[4];
void dm_input_set_buttons(uint64_t value){buttons=value;}
void dm_input_set_sticks(int32_t lx,int32_t ly,int32_t rx,int32_t ry){axes[0]=lx;axes[1]=ly;axes[2]=rx;axes[3]=ry;}
void dm_input_set_state(uint64_t value,int32_t lx,int32_t ly,int32_t rx,int32_t ry){dm_input_set_buttons(value);dm_input_set_sticks(lx,ly,rx,ry);}
void smo_audio_set_output(smo_audio_output_fn fn){(void)fn;}
void smo_audio_set_suspended(int value){(void)value;}
void dm_log_emit(const char *format,...){(void)format;}
uint64_t nvn_metal_offscreen_frames(void){return 0;}
@interface SMOFramePacer (TestLifecycle)
- (void)suspend;
- (void)resume;
@end
@interface SMOFakeTouch : NSObject
@property(nonatomic) CGPoint point;
@end
@implementation SMOFakeTouch
- (CGPoint)locationInView:(UIView *)view {(void)view;return _point;}
@end
@interface FrontendTests : XCTestCase @end
@implementation FrontendTests
- (SMOTouchController *)controller {
    SMOTouchController *pad=[[SMOTouchController alloc] initWithFrame:CGRectMake(0,0,844,390)];
    [pad layoutIfNeeded];return pad;
}
- (UIControl *)button:(NSString *)name in:(UIView *)pad {
    for(UIView *view in pad.subviews)if([view.accessibilityLabel isEqualToString:name])return (UIControl *)view;
    XCTFail(@"Missing button %@",name);return nil;
}
- (void)testSimultaneousButtonsAndCancellation {
    SMOTouchController *pad=[self controller];
    UIControl *a=[self button:@"A" in:pad],*b=[self button:@"B" in:pad];
    [a beginTrackingWithTouch:nil withEvent:nil];[b beginTrackingWithTouch:nil withEvent:nil];
    XCTAssertEqual(buttons,DM_BTN_A|DM_BTN_B);
    [a endTrackingWithTouch:nil withEvent:nil];XCTAssertEqual(buttons,DM_BTN_B);
    [b cancelTrackingWithEvent:nil];XCTAssertEqual(buttons,0ull);
    [a beginTrackingWithTouch:nil withEvent:nil];pad.hidden=YES;XCTAssertEqual(buttons,0ull);
    pad.hidden=NO;XCTAssertEqual(buttons,0ull);
}
- (void)testStickDirectionDeadzoneAndLifecycle {
    SMOTouchController *pad=[self controller];UIControl *stick=(UIControl *)pad.subviews[16];
    SMOFakeTouch *touch=[SMOFakeTouch new];touch.point=CGPointMake(stick.bounds.size.width,0);
    [stick beginTrackingWithTouch:(UITouch *)touch withEvent:nil];
    XCTAssertGreaterThan(axes[0],22000);XCTAssertGreaterThan(axes[1],22000);
    XCTAssertLessThanOrEqual(hypot(axes[0],axes[1]),32768.0);
    touch.point=CGPointMake(CGRectGetMidX(stick.bounds)+1,CGRectGetMidY(stick.bounds));
    [stick continueTrackingWithTouch:(UITouch *)touch withEvent:nil];XCTAssertEqual(axes[0],0);XCTAssertEqual(axes[1],0);
    touch.point=CGPointZero;[stick continueTrackingWithTouch:(UITouch *)touch withEvent:nil];
    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationWillResignActiveNotification object:nil];
    XCTAssertEqual(axes[0],0);XCTAssertEqual(axes[1],0);
    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationDidBecomeActiveNotification object:nil];
    XCTAssertEqual(buttons,0ull);
}
- (void)testControlsDoNotOverlapAndEmptyAreaPassesThrough {
    for(NSValue *size in @[[NSValue valueWithCGSize:CGSizeMake(844,390)],[NSValue valueWithCGSize:CGSizeMake(667,340)]]){
        SMOTouchController *pad=[self controller];pad.frame=(CGRect){CGPointZero,size.CGSizeValue};[pad layoutIfNeeded];
        for(NSUInteger a=0;a<pad.subviews.count;a++){
            XCTAssertTrue(CGRectContainsRect(pad.bounds,pad.subviews[a].frame));
            for(NSUInteger b=a+1;b<pad.subviews.count;b++){
                CGRect intersection=CGRectIntersection(pad.subviews[a].frame,pad.subviews[b].frame);
                XCTAssertFalse(!CGRectIsNull(intersection) && intersection.size.width>0.5 && intersection.size.height>0.5);
            }
        }
        XCTAssertNil([pad hitTest:CGPointMake(300,10) withEvent:nil]);
    }
}
- (void)testPacingAndBackgroundWait {
    SMOFramePacer *p=SMOFramePacer.shared;[p resume];p.powerSaving=YES;
    XCTestExpectation *done=[self expectationWithDescription:@"three paced frames"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        CFTimeInterval start=CACurrentMediaTime();for(int i=0;i<3;i++)[p waitForFrame:1];
        XCTAssertGreaterThanOrEqual(CACurrentMediaTime()-start,0.06);[done fulfill];
    });
    [self waitForExpectations:@[done] timeout:3];
    [p suspend];dispatch_semaphore_t finished=dispatch_semaphore_create(0);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{[p waitForFrame:1];dispatch_semaphore_signal(finished);});
    XCTAssertNotEqual(dispatch_semaphore_wait(finished,dispatch_time(DISPATCH_TIME_NOW,80*NSEC_PER_MSEC)),0l);
    [p resume];XCTAssertEqual(dispatch_semaphore_wait(finished,dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC)),0l);
    p.powerSaving=NO;
}
@end
