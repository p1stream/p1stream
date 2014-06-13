var core = require('./core.node');
var mac_sources = require('../mac_sources');

var videoMixer = new core.VideoMixer({
    bufferSize: 1 * 1024 * 1024,
    width: 1280,
    height: 720,
    onData: onData,
    onError: onError
});

var clock = new mac_sources.DisplayLink();
videoMixer.setClock(clock);

setTimeout(finish, 5000);

function onData(e) {
    console.log('data', e);
}

function onError(e) {
    console.log('error', e);
    finish();
}

function finish() {
    if (videoMixer) {
        videoMixer.destroy();
        videoMixer = null;
    }

    if (clock) {
        clock.destroy();
        clock = null;
    }
}
