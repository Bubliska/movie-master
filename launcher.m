#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <spawn.h>
#include <unistd.h>
extern char **environ;

static int fail(NSString *message, NSError *error) {
    [NSApplication sharedApplication];
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Movie Master";
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

static int importHelper(NSString *bridge, NSURL *desktop, pid_t editor) {
    NSString *request = [bridge stringByAppendingPathComponent:@"REQUEST.TXT"];
    NSString *response = [bridge stringByAppendingPathComponent:@"RESPONSE.TXT"];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    __block NSOpenPanel *panel = nil;
    __block unsigned long active = 0;
    [NSTimer scheduledTimerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *timer) {
            (void)timer;
            if (getppid() != editor) { if (panel) [panel cancel:nil]; [[NSFileManager defaultManager] removeItemAtPath:[bridge stringByAppendingPathComponent:@"IMPORT.FTN"] error:nil]; [NSApp terminate:nil]; return; }
            NSString *message = [NSString stringWithContentsOfFile:request encoding:NSUTF8StringEncoding error:nil];
            char command = 0; unsigned long identifier = 0;
            if (sscanf(message.UTF8String ?: "", "%c %lu", &command, &identifier) != 2) return;
            if (command == 'C') {
                [@"" writeToFile:request atomically:YES encoding:NSUTF8StringEncoding error:nil];
                if (panel && identifier == active) [panel cancel:nil];
                else if (!panel) [[NSString stringWithFormat:@"C %lu\n", identifier] writeToFile:response atomically:YES encoding:NSUTF8StringEncoding error:nil];
                return;
            }
            if (command != 'I' || panel) return;
            [@"" writeToFile:request atomically:YES encoding:NSUTF8StringEncoding error:nil];
            active = identifier;
            panel = [NSOpenPanel openPanel];
            panel.title = @"Import Fountain — Movie Master";
            panel.message = @"Choose a Fountain screenplay. The original file will not be changed.";
            panel.prompt = @"Import";
            panel.directoryURL = desktop;
            panel.canChooseDirectories = NO;
            panel.allowsMultipleSelection = NO;
            panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"fountain"],
                [UTType typeWithFilenameExtension:@"ftn"], [UTType typeWithFilenameExtension:@"fou"], UTTypePlainText];
            [NSApp activate];
            [panel beginWithCompletionHandler:^(NSModalResponse result) {
                char outcome = 'C'; NSError *readError = nil;
                if (result == NSModalResponseOK) {
                    NSNumber *size = nil;
                    if ([panel.URL getResourceValue:&size forKey:NSURLFileSizeKey error:&readError] && size.unsignedLongLongValue <= 8ULL*1024*1024) {
                        NSData *data = [NSData dataWithContentsOfURL:panel.URL options:NSDataReadingMappedIfSafe error:&readError];
                        outcome = data && [data writeToFile:[bridge stringByAppendingPathComponent:@"IMPORT.FTN"] options:NSDataWritingAtomic error:&readError] ? 'O' : 'E';
                    } else outcome = 'E';
                    if (outcome == 'E') fail(@"Cannot read this Fountain file. Choose a readable file smaller than 8 MB.", readError);
                }
                [[NSString stringWithFormat:@"%c %lu\n", outcome, identifier] writeToFile:response atomically:YES encoding:NSUTF8StringEncoding error:nil];
                panel = nil;
            }];
    }];
    [NSApp run];
    return 0;
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc == 5 && !strcmp(argv[1], "--import-helper"))
            return importHelper([NSString stringWithUTF8String:argv[2]], [NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[3]]], (pid_t)strtol(argv[4], NULL, 10));
        NSFileManager *files = [NSFileManager defaultManager];
        NSError *error = nil;
        NSString *resources = [NSBundle mainBundle].resourcePath;
        NSString *executable = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
        NSURL *support = [[files URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask] firstObject];
        NSURL *desktop = [[files URLsForDirectory:NSDesktopDirectory inDomains:NSUserDomainMask] firstObject];
        if (!resources || !support || !desktop) return fail(@"Cannot find the application or your document folders.", nil);
        NSString *supportFolder = [[support URLByAppendingPathComponent:@"MovieMaster" isDirectory:YES] path];
        NSString *legacy = [[support URLByAppendingPathComponent:@"WRITER" isDirectory:YES] path];
        int lock = -1;
        if (![files fileExistsAtPath:supportFolder] && [files fileExistsAtPath:legacy]) {
            lock = open([legacy stringByAppendingPathComponent:@"session.lock"].fileSystemRepresentation, O_CREAT | O_RDWR, 0600);
            if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0)
                return fail(@"Close the previous Movie Master session before upgrading. Save your draft first.", nil);
            if (![files moveItemAtPath:legacy toPath:supportFolder error:&error])
                return fail(@"Cannot migrate your previous session. Its files have been preserved.", error);
        }
        if (![files createDirectoryAtPath:supportFolder withIntermediateDirectories:YES attributes:nil error:&error])
            return fail(@"Cannot create Movie Master's settings folder.", error);
        NSString *lockPath = [supportFolder stringByAppendingPathComponent:@"session.lock"];
        if (lock < 0) lock = open(lockPath.fileSystemRepresentation, O_CREAT | O_RDWR, 0600);
        if (lock < 0) return fail(@"Cannot open Movie Master's session lock.", nil);
        if (flock(lock, LOCK_EX | LOCK_NB) != 0) {
            close(lock);
            return fail(@"Movie Master is already open. Return to its existing window before starting another session.", nil);
        }
        /* The lock survives exec and is released when the session exits. */
        NSString *dataFolder = [supportFolder stringByAppendingPathComponent:@"Session"];
        if (![files createDirectoryAtPath:dataFolder withIntermediateDirectories:YES attributes:nil error:&error])
            return fail(@"Cannot create Movie Master's recovery folder.", error);
        NSString *runtimeFolder = [supportFolder stringByAppendingPathComponent:@"Runtime"];
        if (![files createDirectoryAtPath:runtimeFolder withIntermediateDirectories:YES attributes:nil error:&error])
            return fail(@"Cannot create Movie Master's runtime folder.", error);
        NSString *bridge = [supportFolder stringByAppendingPathComponent:@"Import"];
        if (![files createDirectoryAtPath:bridge withIntermediateDirectories:YES attributes:nil error:&error])
            return fail(@"Cannot create Movie Master's import folder.", error);
        for (NSString *name in @[@"REQUEST.TXT", @"RESPONSE.TXT"])
            if (![@"" writeToFile:[bridge stringByAppendingPathComponent:name] atomically:YES encoding:NSUTF8StringEncoding error:&error])
                return fail(@"Cannot prepare the Mac file chooser.", error);
        for (NSString *name in @[@"MOVIEMST.EXE", @"CWSDPMI.EXE", @"FONT.TTF"]) {
            if (!installFile(files, [[resources stringByAppendingPathComponent:@"DOS"] stringByAppendingPathComponent:name],
                             [runtimeFolder stringByAppendingPathComponent:name], &error))
                return fail(@"Cannot install Movie Master's DOS runtime files.", error);
        }
        NSString *mapper = [supportFolder stringByAppendingPathComponent:@"MOVIEMST.MAP"];
        if (!installFile(files, [resources stringByAppendingPathComponent:@"MOVIEMST.MAP"], mapper, &error))
            return fail(@"Cannot install the Turkish F keyboard profile.", error);
        if (!validDOSPath(dataFolder) || !validDOSPath(desktop.path) || !validDOSPath(mapper) || !validDOSPath(runtimeFolder) || !validDOSPath(bridge) || !validDOSPath(NSHomeDirectory()))
            return fail(@"Movie Master cannot use folder paths containing quotation marks or line breaks.", nil);
        NSString *config = [NSString stringWithFormat:
            @"[sdl]\nfullscreen=true\nfullresolution=desktop\nwindowresolution=original\n"
             "output=openglnb\nautolock=false\ntitlebar=Movie Master\nnocachedir=true\nmapperfile_sdl2=%@\n"
             "[render]\naspect=false\nscaler=none\nglshader=none\n"
             "[dos]\nlfn=true\nkeyboardlayout=none\n"
             "[dosbox]\nmemsize=32\n[cpu]\ncycles=25000\n"
             "[autoexec]\n@echo off\nchcp 857\nmount c \"%@\"\nmount d \"%@\"\nmount e \"%@\"\nmount f \"/Volumes\"\nmount r \"%@\"\nmount x \"%@\"\n"
             "set PATH=R:\\;Z:\\\nset MMFONT=R:\\FONT.TTF\nset MMMAC=X:/\nc:\nR:\\MOVIEMST.EXE\nexit\n",
             mapper, dataFolder, desktop.path, NSHomeDirectory(), runtimeFolder, bridge];
        NSString *configPath = [supportFolder stringByAppendingPathComponent:@"MovieMaster.conf"];
        if (![config writeToFile:configPath atomically:YES encoding:NSUTF8StringEncoding error:&error])
            return fail(@"Cannot write Movie Master's display settings.", error);
        if (chdir(dataFolder.fileSystemRepresentation) != 0) return fail(@"Cannot open Movie Master's recovery folder.", nil);
        NSString *logPath = [supportFolder stringByAppendingPathComponent:@"launch.log"];
        int log = open(logPath.fileSystemRepresentation, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }
        NSString *runtime = [executable stringByAppendingPathComponent:@"dosbox-x"];
        NSString *helper = [[NSBundle mainBundle].bundlePath stringByAppendingPathComponent:@"Contents/Helpers/MovieMasterImport.app/Contents/MacOS/MovieMasterImport"];
        char parent[32];snprintf(parent, sizeof(parent), "%ld", (long)getpid());
        char *helperArguments[] = {(char *)helper.fileSystemRepresentation, "--import-helper", (char *)bridge.fileSystemRepresentation, (char *)desktop.path.fileSystemRepresentation, parent, NULL};
        pid_t helperPID;int result = posix_spawn(&helperPID, helper.fileSystemRepresentation, NULL, NULL, helperArguments, environ);
        if (result) return fail([NSString stringWithFormat:@"Cannot start the Mac file chooser: %s", strerror(result)], nil);
        const char *arguments[] = {runtime.fileSystemRepresentation, "-conf", configPath.fileSystemRepresentation, "-noconsole", "-fastlaunch", NULL};
        execv(runtime.fileSystemRepresentation, (char *const *)arguments);
        return fail([NSString stringWithFormat:@"Cannot start DOSBox-X: %s", strerror(errno)], nil);
    }
}
