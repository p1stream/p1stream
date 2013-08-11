## P1stream

This is early work on a utility to livestream your desktop and camera sources
on a Mac. It will target OS X 10.8 (Mountain Lion) and later.

There's [a Trello board][todo] with a rough to-do list.

 [todo]: https://trello.com/b/mPsCUmiF/p1stream

### Build

The only requirements are Xcode 4 and OS X 10.8. If the project fails to
build, please file an issue.

Check out the code as follows:

    git clone https://github.com/stephank/P1stream.git
    cd P1stream
    git submodule update --init

When updating an existing clone, don't forget about submodules:

    git pull
    git submodule update

Open the workspace, and build the ‘P1stream’ scheme. Or from the command line:

    xcodebuild -workspace P1stream.xcworkspace -scheme P1stream

The binary can be found in the ‘Products’ group of the project navigator. The
filesystem location for this is somewhere in `~/Library/Developer/Xcode/DerivedData`.

### Running

Currently, many parameters are hardcoded. The utility will stream at 720p,
30 FPS, with an average bitrate of 4 Mbps.

The first and only parameter on the commandline is the stream URL. If you're
running from Xcode, this is set to `rtmp://127.0.0.1/app/test`.

If you need this to be a different URL, make a non-shared copy of the
‘P1stream’ scheme and edit the arguments. (The shared scheme lives in git.)

A good way to test is using [nginx-rtmp-module].

 [nginx-rtmp-module]: https://github.com/arut/nginx-rtmp-module

### License

All original code here is licensed [GPLv3](LICENSE). This is pretty much
everything in this repository (not submodules) except the contents of the
`generated` directories, which is based on their respective projects.

© 2013 — Stéphan Kochen
