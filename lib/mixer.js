// Wrapper class for the native audio and video mixers combined.
// FIXME: Merge this into model.

var util = require('util');
var events = require('events');
var avc = require('./avc');
var native = require('../build/Release/native.node');

util.inherits(Mixer, events.EventEmitter);
function Mixer(scope, obj, cfg, clock) {
    var self = this;

    self.audioMixer = new native.AudioMixer({
        onEvent: onEvent
    });

    self.videoMixer = new native.VideoMixer({
        width: 1280,
        height: 720,
        clock: clock,
        onEvent: onEvent
    });

    function onEvent(id, arg) {
        switch (id) {
            case native.EV_FAILURE:
                self.destroy();
                self.emit('error');
                break;

            case native.EV_VIDEO_HEADERS:
                // Convenience: Build AVC decoder config
                arg.avc = avc.config(arg);
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

        this.audioMixer.destroy();
        this.audioMixer = null;

        this.zombie = true;
    }
};

module.exports = Mixer;
