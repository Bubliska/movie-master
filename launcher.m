#import <Cocoa/Cocoa.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

static int fail(NSString *message, NSError *error) {
    [NSApplication sharedApplication];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"WRITER";
    alert.informativeText = error ? [message stringByAppendingFormat:@"\n\n%@", error.localizedDescription] : message;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
    return 1;
}

static BOOL installFile(NSFileManager *files, NSString *source, NSString *destination, NSError **error) {
    if ([files fileExistsAtPath:destination]) {
        if ([files contentsEqualAtPath:source andPath:destination]) return YES;
    }
    NSData *data = [NSData dataWithContentsOfFile:source options:0 error:error];
    return data && [data writeToFile:destination options:NSDataWritingAtomic error:error];
}

static BOOL validDOSPath(NSString *path) {
    return [path rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"\"\r\n"]].location == NSNotFound;
}

int main(void) {
    @autoreleasepool {
        NSFileManager *files = [NSFileManager defaultManager];
        NSError *error = nil;
        NSString *resources = [NSBundle mainBundle].resourcePath;
        NSString *executable = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
        NSURL *support = [[files URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask] firstObject];
        NSURL *desktop = [[files URLsForDirectory:NSDesktopDirectory inDomains:NSUserDomainMask] firstObject];
        if (!resources || !support || !desktop) return fail(@"Cannot find the application or your document folders.", nil);
        NSString *supportFolder = [[support URLByAppendingPathComponent:@"WRITER" isDirectory:YES] path];
        if (![files createDirectoryAtPath:supportFolder withIntermediateDirectories:YES attributes:nil error:&error])
            return fail(@"Cannot create WRITER's settings folder.", error);
        NSString *lockPath = [supportFolder stringByAppendingPathComponent:@"session.lock"];
        int lock = open(lockPath.fileSystemRepresentation, O_CREAT | O_RDWR, 0600);
        if (lock < 0) return fail(@"Cannot open WRITER's session lock.", nil);
        if (flock(lock, LOCK_EX | LOCK_NB) != 0) {
            close(lock);
            return fail(@"WRITER is already open. Return to its existing window before starting another session.", nil);
        }
        /* The lock survives exec and is released when the emulator exits. */
        NSString *dataFolder = [supportFolder stringByAppendingPathComponent:@"Session"];
        if (![files createDirectoryAtPath:dataFolder withIntermediateDirectories:YES attributes:nil error:&error])
            return fail(@"Cannot create WRITER's recovery folder.", error);
        NSString *runtimeFolder = [supportFolder stringByAppendingPathComponent:@"Runtime"];
        if (![files createDirectoryAtPath:runtimeFolder withIntermediateDirectories:YES attributes:nil error:&error])
            return fail(@"Cannot create WRITER's runtime folder.", error);
        for (NSString *name in @[@"WRITER.EXE", @"CWSDPMI.EXE", @"FONT.TTF"]) {
            if (!installFile(files, [[resources stringByAppendingPathComponent:@"DOS"] stringByAppendingPathComponent:name],
                             [runtimeFolder stringByAppendingPathComponent:name], &error))
                return fail(@"Cannot install WRITER's DOS runtime files.", error);
        }
        NSString *mapper = [supportFolder stringByAppendingPathComponent:@"WRITER.MAP"];
        if (!installFile(files, [resources stringByAppendingPathComponent:@"WRITER.MAP"], mapper, &error))
            return fail(@"Cannot install the Turkish F keyboard profile.", error);
        if (!validDOSPath(dataFolder) || !validDOSPath(desktop.path) || !validDOSPath(mapper) || !validDOSPath(runtimeFolder) || !validDOSPath(NSHomeDirectory()))
            return fail(@"WRITER cannot use folder paths containing quotation marks or line breaks.", nil);
        NSString *config = [NSString stringWithFormat:
            @"[sdl]\nfullscreen=true\nfullresolution=desktop\nwindowresolution=original\n"
             "output=openglnb\nautolock=false\nmapperfile_sdl2=%@\n"
             "[render]\naspect=false\nscaler=none\nglshader=none\n"
             "[dos]\nlfn=true\nkeyboardlayout=none\n"
             "[dosbox]\nmemsize=32\n[cpu]\ncycles=25000\n"
             "[autoexec]\n@echo off\nchcp 857\nmount c \"%@\"\nmount d \"%@\"\nmount e \"%@\"\nmount f \"/Volumes\"\nmount r \"%@\"\n"
             "set PATH=R:\\;Z:\\\nset WRFONT=R:\\FONT.TTF\nc:\nR:\\WRITER.EXE\nexit\n",
             mapper, dataFolder, desktop.path, NSHomeDirectory(), runtimeFolder];
        NSString *configPath = [supportFolder stringByAppendingPathComponent:@"WRITER.conf"];
        if (![config writeToFile:configPath atomically:YES encoding:NSUTF8StringEncoding error:&error])
            return fail(@"Cannot write WRITER's display settings.", error);
        if (chdir(dataFolder.fileSystemRepresentation) != 0) return fail(@"Cannot open WRITER's recovery folder.", nil);
        NSString *logPath = [supportFolder stringByAppendingPathComponent:@"launch.log"];
        int log = open(logPath.fileSystemRepresentation, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }
        NSString *runtime = [executable stringByAppendingPathComponent:@"dosbox-x"];
        const char *arguments[] = {runtime.fileSystemRepresentation, "-conf", configPath.fileSystemRepresentation,
                                   "-noconsole", "-fastlaunch", NULL};
        execv(runtime.fileSystemRepresentation, (char *const *)arguments);
        return fail([NSString stringWithFormat:@"Cannot start the bundled DOSBox runtime: %s", strerror(errno)], nil);
    }
}
