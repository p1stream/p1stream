var core = require('../../MacOS/core.node');

var videoMixer = new core.VideoMixer({
    width: 1280,
    height: 720,
    onFrame: onFrame,
    onError: onError
});

function onFrame(e) {
    console.log('frame', e);
}

function onError(e) {
    console.log('error', e);
}

console.log('Hello world!', videoMixer);

videoMixer.destroy();
