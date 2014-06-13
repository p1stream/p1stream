var core = require('./core.node');
var mac_sources = require('../mac_sources');

var mixer = new core.VideoMixer({
    bufferSize: 1 * 1024 * 1024,
    width: 1280,
    height: 720,
    onData: onData,
    onError: onError
});

var source = new mac_sources.DisplayStream();
mixer.setSources([
    {
        source: source,
        x1: -1, y1: -1,
        x2: +1, y2: +1,
        u1: 0, v1: 0,
        u2: 1, v2: 1
    }
]);

var clock = new mac_sources.DisplayLink();
mixer.setClock(clock);

setTimeout(finish, 5000);

function onData(e) {
    console.log('data', e);
}

function onError(e) {
    console.log('error', e);
    finish();
}

function finish() {
    if (mixer) {
        mixer.destroy();
        mixer = null;
    }

    if (source) {
        source.destroy();
        source = null;
    }

    if (clock) {
        clock.destroy();
        clock = null;
    }
}
