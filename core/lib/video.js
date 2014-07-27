var events = require('events');
var native = require('../../build/Release/core.node');
var defaults = require('./defaults');

var Video = function() {
    var self = this;

    self._clock = new defaults.VideoClock();
    self._mixer = new native.VideoMixer({
        bufferSize: 1 * 1024 * 1024,
        width: 1280,
        height: 720,
        clock: self._clock,
        onData: onData,
        onError: onError
    });

    self._source = new defaults.VideoSource();
    self._mixer.setSources([
        {
            source: self._source,
            x1: -1, y1: -1,
            x2: +1, y2: +1,
            u1: 0, v1: 0,
            u2: 1, v2: 1
        }
    ]);

    self.headers = null;

    function onData(e) {
        var buf = e.buf;
        e.frames.forEach(function(frame) {
            frame.nals.forEach(function(nal) {
                nal.buf = buf.slice(nal.start, nal.end);
            });

            if (self.headers) {
                self.emit('frame', frame);
            }
            else {
                self.headers = frame;
                self.emit('headers', frame);
            }
        });
    }

    function onError(e) {
        self.destroy();
        self.emit('error', e);
    }
};

Video.prototype = Object.create(events.EventEmitter.prototype);

Video.prototype.destroy = function() {
    if (this._mixer) {
        this._mixer.destroy();
        this._mixer = null;
    }

    if (this._source) {
        this._source.destroy();
        this._source = null;
    }

    if (this._clock) {
        this._clock.destroy();
        this._clock = null;
    }
};

module.exports = Video;
