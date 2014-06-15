// Buffer stream data until it ends.
module.exports = function(stream, options, cb) {
    if (typeof(options) === 'function') {
        cb = options;
        options = {};
    }

    if (options.encoding)
        stream.setEncoding(options.encoding);

    var chunks = [];
    var size = 0;

    stream.on('data', onData);
    function onData(chunk) {
        size += chunk.length;
        if (options.maxLength && size > options.maxLength)
            callback(null, null);
        else
            chunks.push(chunk);
    }

    stream.on('end', onEnd);
    function onEnd() {
        var data;
        if (options.encoding)
            data = chunks.join('');
        else
            data = Buffer.concat(chunks, size);

        callback(null, data);
    }

    stream.on('error', onError);
    function onError(err) {
        callback(err, null);
    }

    function callback(err, res) {
        stream.removeListener('data', onData);
        stream.removeListener('end', onEnd);
        stream.removeListener('error', onError);
        cb(err, res);
    }
};
