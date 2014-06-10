var core = require('./core.node');

var videoMixer = new core.VideoMixer({
    bufferSize: 1 * 1024 * 1024,
    width: 1280,
    height: 720,
    onData: onData,
    onError: onError
});

function onData(e) {
    console.log('data', e);
}

function onError(e) {
    console.log('error', e);
}

console.log('Hello world!', videoMixer);

videoMixer.destroy();
