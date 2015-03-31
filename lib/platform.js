var native = require('../build/Release/native.node');

if (process.platform === 'darwin') {
    // On Mac, we wrap the libuv loop in a CFRunLoop, so that plugins can
    // safely use functionality reliant on a Core Foundation main loop being
    // available (e.g., code that uses NSDistributedNotificationCenter). Of
    // course, this is a bit of hack, and tied closely to the IO.js
    // main loop implementation.
    exports.setup = function(cb) {
        // We cannot call CFRunLoopRun at this point, because Node setup is not
        // completely finished. We cannot do it from nextTick or similar,
        // because uv_run is not reentrant. The safe spot is to wait until
        // beforeExit.
        process.once('beforeExit', function() {
            cb();
            native.mainLoop(process);
        });
    };
}
else {
    // Currently no-op on other platforms.
    exports.setup = function(cb) {
        cb();
    };
}
