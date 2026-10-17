#import <Foundation/Foundation.h>
@interface SMOFramePacer : NSObject
@property(nonatomic) BOOL powerSaving;
+ (instancetype)shared;
- (void)waitForFrame:(int)interval;
@end
