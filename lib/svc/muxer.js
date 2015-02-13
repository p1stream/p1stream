module.exports = function(scope) {
    var events = require('events');

    var types = Object.create(null);
    types.matroska = {
        mod: require('../matroska'),
        running: Object.create(null)
    };
    types.mpegts = {
        mod: require('../mpegts'),
        running: Object.create(null)
    };

    scope.getMuxer = function(type, mixer) {
        var obj = types[type];
        var ctx = obj.running[mixer.$id];
        if (ctx) {
            ctx.refs++;
            return ctx;
        }

        var mod = obj.mod;
        ctx = obj.running[mixer.$id] = new events.EventEmitter();
        ctx.refs = 1;
        ctx.headers = null;
        if (mod.init)
            mod.init(ctx);

        var cancelFrameListener = mixer.$addFrameListener({
            audioHeaders: onHeaders,
            videoHeaders: onHeaders,
            audioFrame: onAudioFrame,
            videoFrame: onVideoFrame
        }, { emitInitHeaders: true });
        ctx.unref = function() {
            if (--ctx.refs === 0) {
                cancelFrameListener();
                delete obj.running[mixer.$id];
            }
        };

        function onHeaders() {
            if (mixer.$videoHeaders && mixer.$audioHeaders)
                ctx.headers = mod.headers(mixer.$videoHeaders, mixer.$audioHeaders, ctx);
        }

        function onAudioFrame(frame) {
            if (ctx.headers)
                ctx.emit('frame', mod.audioFrame(frame, ctx));
        }

        function onVideoFrame(frame) {
            if (ctx.headers)
                ctx.emit('frame', mod.videoFrame(frame, ctx), frame.keyframe);
        }

        return ctx;
    };
};
