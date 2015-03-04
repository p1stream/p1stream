// Wrapper class for the native audio and video mixers combined.
// FIXME: Merge this into model.

var util = require('util');
var events = require('events');
var native = require('../build/Release/native.node');

util.inherits(Mixer, events.EventEmitter);
function Mixer(scope, obj, cfg) {
    var self = this;

    self.audioMixer = new native.AudioMixer({
        onEvent: onEvent
    });

    self.videoClock = new native.SoftwareClock({
        numerator: 1,
        denominator: 30
    });

    self.videoMixer = new native.VideoMixer({
        width: 1280,
        height: 720,
        clock: self.videoClock,
        onEvent: onEvent
    });

    function onEvent(id, arg) {
        switch (id) {
            case native.EV_FAILURE:
                self.destroy();
                self.emit('error');
                break;

            case native.EV_VIDEO_HEADERS:
                self.emit('videoHeaders', arg);
                break;

            case native.EV_VIDEO_FRAME:
                self.emit('videoFrame', arg);
                break;

            case native.EV_AUDIO_HEADERS:
                self.emit('audioHeaders', arg);
                break;

            case native.EV_AUDIO_FRAME:
                self.emit('audioFrame', arg);
                break;

            default:
                scope.handleNativeEvent(obj, id, arg);
                break;
        }
    }
}

Mixer.prototype.destroy = function() {
    if (!this.zombie) {
        this.videoMixer.destroy();
        this.videoMixer = null;

        this.videoClock.destroy();
        this.videoClock = null;

        this.audioMixer.destroy();
        this.audioMixer = null;

        this.zombie = true;
    }
};

module.exports = Mixer;
