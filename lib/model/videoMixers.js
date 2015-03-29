var _ = require('lodash');
var native = require('../../build/Release/native.node');

module.exports = function(app) {
    // Define the video mixer type.
    app.store.onCreate('video-mixer', function(obj) {
        obj._sources = [];
        obj._hooks = [];

        obj.setSources = function(list) {
            _.each(obj._sources, function(source) {
                source._obj.unref(obj);
            });

            obj._sources = list || [];

            _.each(obj._sources, function(source) {
                source._obj.ref(obj);
            });

            obj.emit('sourcesChanged', obj._sources);
        };

        obj.setHooks = function(list) {
            obj._hooks = list || [];
            obj.emit('hooksChanged', obj._hooks);
        };

        obj.on('destroy', function() {
            _.each(obj._sources, function(source) {
                source._obj.unref(obj);
            });
        });

        obj.activation('video clock object', {
            start: function() {
                obj._clock = app.store.create(obj.cfg.clock);
                obj._clock._ephemeral = true;
                obj._clock.ref(obj);
            },
            stop: function() {
                obj._clock.unref(obj);
                obj._clock = null;
            }
        });

        obj.activation('native video mixer', {
            cond: function() {
                return obj.activationCond() &&
                    obj._clock && obj._clock._instance;
            },
            start: function(lg) {
                obj._instance = new native.VideoMixer({
                    width: 1280,
                    height: 720,
                    clock: obj._clock._instance,
                    onEvent: onEvent
                });
                app.mark();

                updateSources(obj._sources);
                lg.listen(obj, 'sourcesChanged', updateSources);
                function updateSources(list) {
                    obj._instance.setSources(list);
                }

                updateHooks(obj._hooks);
                lg.listen(obj, 'hooksChanged', updateHooks);
                function updateHooks(list) {
                    obj._instance.setHooks(list);
                }
            },
            stop: function() {
                obj._instance.destroy();
                obj._instance = null;
                app.mark();
            }
        });

        function onEvent(id, arg) {
            switch (id) {
                case native.EV_VIDEO_HEADERS:
                    arg.avc = buildAvcConfig(arg);

                    obj._headers = arg;
                    app.mark();

                    obj.emit('headers', arg);
                    break;

                case native.EV_VIDEO_FRAME:
                    obj.emit('frame', arg);
                    break;

                default:
                    obj.handleNativeEvent(id, arg);
                    break;
            }
        }
    });
};

// Build AVC config from NALUs.
function buildAvcConfig(headers) {
    var parts = [];
    var b;

    var sps = headers.nals.filter(function(nal) {
        return nal.type === 7;
    });
    var pps = headers.nals.filter(function(nal) {
        return nal.type === 8;
    });

    b = new Buffer(6);
    b.writeUInt8(1, 0); // version
    b.writeUInt8(sps[0].buf[5], 1); // profile
    b.writeUInt8(sps[0].buf[6], 2); // profile compat
    b.writeUInt8(sps[0].buf[7], 3); // level
    b.writeUInt8(0xFF, 4); // 6: reserved, 2: Size of NALU lengths, minus 1
    b.writeUInt8(0xE0 | sps.length, 5); // 3: reserved, 5: num SPS
    parts.push(b);

    sps.forEach(function(nal) {
        b = new Buffer(2);
        b.writeUInt16BE(nal.buf.length - 4, 0);
        parts.push(b);
        parts.push(nal.buf.slice(4));
    });

    b = new Buffer(1);
    b.writeUInt8(pps.length, 0);
    parts.push(b);

    pps.forEach(function(nal) {
        b = new Buffer(2);
        b.writeUInt16BE(nal.buf.length - 4, 0);
        parts.push(b);
        parts.push(nal.buf.slice(4));
    });

    return Buffer.concat(parts);
}
