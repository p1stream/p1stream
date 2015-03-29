var native = require('../../build/Release/native.node');

// Default handler for the stock set of native events.
exports.handleNativeEvent = function(id, arg) {
    switch (id) {
        case native.EV_LOG_TRACE: this._log.trace(arg); break;
        case native.EV_LOG_DEBUG: this._log.debug(arg); break;
        case native.EV_LOG_INFO:  this._log.info(arg);  break;
        case native.EV_LOG_WARN:  this._log.warn(arg);  break;
        case native.EV_LOG_ERROR: this._log.error(arg); break;
        case native.EV_LOG_FATAL: this._log.fatal(arg); break;

        case native.EV_FAILURE:
            this.fatal();
            break;

        case native.EV_STALLED:
            this._log.warn("Native event buffer dropped %d events during stall", arg);
            break;
    }
};
