#import <UIKit/UIKit.h>
#import <Metal/Metal.h>

@interface SMOMetalView : UIView
@property(nonatomic,copy) void (^onFirstFrame)(void);
- (void)offerTexture:(id<MTLTexture>)texture interval:(int)interval ordinal:(uint64_t)ordinal;
@end
