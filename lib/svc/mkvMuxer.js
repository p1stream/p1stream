module.exports = function(scope) {
    var events = require('events');
    var matroska = require('../matroska');

    // Running muxers.
    var muxers = Object.create(null);

    // Start for the given mixer.
    scope.getMkvMuxer = function(obj) {
        var muxer = muxers[obj.$id];
        if (muxer) {
            muxer.refs++;
            return muxer;
        }

        muxer = new events.EventEmitter();
        muxer.refs = 1;
        muxer.headers = null;

        var cancelFrameListener = obj.$addFrameListener({
            audioHeaders: onHeaders,
            videoHeaders: onHeaders,
            audioFrame: onAudioFrame,
            videoFrame: onVideoFrame
        }, { emitInitHeaders: true });
        muxer.unref = function() {
            if (--muxer.refs === 0)
                cancelFrameListener();
        };

        function onHeaders() {
            if (obj.$videoHeaders && obj.$audioHeaders)
                muxer.headers = matroska.headers(obj.$videoHeaders, obj.$audioHeaders);
        }

        function onAudioFrame(frame) {
            muxer.emit('frame', matroska.audioFrame(frame));
        }

        function onVideoFrame(frame) {
            muxer.emit('frame', matroska.videoFrame(frame), frame.keyframe);
        }

        return muxer;
    };
};
