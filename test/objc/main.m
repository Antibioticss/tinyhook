#import <Foundation/Foundation.h>

@interface Example : NSObject

@end

@implementation Example : NSObject

+ (void)Hello {
    NSLog(@"Hello, World!");
}

+ (void)Intro {
    NSLog(@"This is an example.");
}

- (void)Goodbye {
    NSLog(@"Goodbye!");
}

@end

int main() {
    Example *Instance = [[Example alloc] init];
    [Example Hello];
    [Example Intro];
    [Instance Goodbye];
    [Instance release];
    return 0;
}
