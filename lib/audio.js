var events = require('events');
var native = require('../build/Release/native.node');

var Audio = function() {
    var self = this;

    self._mixer = new native.AudioMixer({
        onData: onData,
        onError: onError
    });

    self.headers = null;

    function onData(e) {
        var buf = e.buf;
        e.frames.forEach(function(frame) {
            frame.buf = buf.slice(frame.start, frame.end);

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

Audio.prototype = Object.create(events.EventEmitter.prototype);

Audio.prototype.destroy = function() {
    if (this._mixer) {
        this._mixer.destroy();
        this._mixer = null;
    }

    if (this._source) {
        this._source.destroy();
        this._source = null;
    }
};

module.exports = Audio;
