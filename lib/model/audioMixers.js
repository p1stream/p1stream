var _ = require('lodash');
var native = require('../../build/Release/native.node');

module.exports = function(app) {
    // Define the audio mixer type.
    app.store.onCreate('audio-mixer', function(obj) {
        obj._sources = [];

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

        obj.on('destroy', function() {
            _.each(obj._sources, function(source) {
                source._obj.unref(obj);
            });
        });

        obj.activation('native audio mixer', {
            start: function(lg) {
                obj._instance = new native.AudioMixer({
                    onEvent: onEvent
                });
                app.mark();

                updateSources(obj._sources);
                lg.listen(obj, 'sourcesChanged', updateSources);
                function updateSources(list) {
                    obj._instance.setSources(list);
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
                case native.EV_AUDIO_HEADERS:
                    obj._headers = arg;
                    app.mark();

                    obj.emit('headers', arg);
                    break;

                case native.EV_AUDIO_FRAME:
                    obj.emit('frame', arg);
                    break;

                default:
                    obj.handleNativeEvent(id, arg);
                    break;
            }
        }
    });
};
