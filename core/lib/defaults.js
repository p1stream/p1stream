var sources;
switch (process.platform) {
    case 'darwin':
        sources = require('../../mac_sources');
        exports.VideoClock = sources.DisplayLink;
        exports.VideoSource = sources.DisplayStream;
        exports.AudioSource = sources.AudioQueue;
        break;
    default:
        throw new Error("Platform " + process.platform + " unsupported");
}
