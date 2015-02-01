// Wrapper class for the native audio and video mixers combined.

var util = require('util');
var events = require('events');
var native = require('../build/Release/native.node');

util.inherits(Mixer, events.EventEmitter);
function Mixer(cfg) {
    var self = this;

    var audioHeadersEmitted = false;
    var videoHeadersEmitted = false;

    self.audioMixer = new native.AudioMixer({
        onData: onAudioData,
        onError: onError
    });

    self.videoClock = new native.SoftwareClock({
        numerator: 1,
        denominator: 30
    });

    self.videoMixer = new native.VideoMixer({
        bufferSize: 1 * 1024 * 1024,
        width: 1280,
        height: 720,
        clock: self.videoClock,
        onData: onVideoData,
        onError: onError
    });

    function onAudioData(e) {
        var buf = e.buf;
        e.frames.forEach(function(frame) {
            frame.buf = buf.slice(frame.start, frame.end);

            if (audioHeadersEmitted) {
                self.emit('audioFrame', frame);
            }
            else {
                audioHeadersEmitted = true;
                self.emit('audioHeaders', frame);
            }
        });
    }

    function onVideoData(e) {
        var buf = e.buf;
        e.frames.forEach(function(frame) {
            frame.nals.forEach(function(nal) {
                nal.buf = buf.slice(nal.start, nal.end);
            });

            if (videoHeadersEmitted) {
                self.emit('videoFrame', frame);
            }
            else {
                videoHeadersEmitted = true;
                self.emit('videoHeaders', frame);
            }
        });
    }

    function onError(err) {
        self.destroy();
        self.emit('error', err);
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
