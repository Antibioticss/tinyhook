#import <Foundation/Foundation.h>
#import <stdio.h>

#import "../../include/tinyhook.h"

@interface Hook : NSObject

@end

@implementation Hook : NSObject

+ (void)MyIntro {
    fprintf(stderr, "=== +[Example Intro] hooked!\n");
    NSLog(@"This is tinyhook.");
}

@end

__attribute__((constructor(0))) int load() {
    fprintf(stderr, "=== libexample loading...\n");

    // get objc function imp
    void *func_hello = ocrt_impl("Example", "Hello", CLASS_METHOD);
    void *func_goodbye = ocrt_impl("Example", "Goodbye", INSTANCE_METHOD);
    fprintf(stderr, "=== +[Example Hello] IMP: %p\n", func_hello);
    fprintf(stderr, "=== -[Example Goodbye] IMP: %p\n", func_goodbye);

    // exchange objc functions
    ocrt_swap("Example", "Intro", "Hook", "MyIntro");
    return 0;
}
